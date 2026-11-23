/*
 *  drv_ultrasonic.c
 *  HC-SR04P driver: 10 us trigger pulse, echo timed in the Echo ISR.
 */

#include "../app.h"
#include "../common/app_gpio.h"
#include "../common/app_time.h"
#include "../common/gpio_irq.h"
#include "drv_ultrasonic.h"

#define US_BOTH_EDGES   (GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL)

/* Message buffer for the ISR: written once before the interrupt is
   enabled, only read afterwards. */
static ID g_echo_mbf = 0;

/* Echo edge-tracking state, owned by the ISR but reset per ping by the
   scan task (ultrasonic_ping_reset).  Keeping it at file scope lets the
   task resynchronise it: a missed or coalesced edge during a jolt can
   otherwise latch b_echo_high and pair later falling edges against a
   stale rise time, which freezes the reported range until the next
   reflash.  Volatile: written in the ISR (core 0), reset from the task. */
static volatile uint32_t g_saved_rise_us = 0u;
static volatile bool     g_echo_high     = false;

static void ultrasonic_echo_isr(uint32_t pin, uint32_t events,
                                uint32_t time_us);

/*
 * Trig low as an output; Echo an input with a pull-down so an unplugged
 * sensor reads "no echo" (A5: the HC-SR04P is powered from 3.3 V, so
 * Echo never exceeds 3.3 V, NFR11).
 */
ER
ultrasonic_init (ID mbf_echo)
{
    g_echo_mbf = mbf_echo;
    app_gpio_output(CFG_PIN_US_TRIG, false);
    app_gpio_input(CFG_PIN_US_ECHO, APP_GPIO_PULL_DOWN);

    return ((ER) gpio_irq_register(CFG_PIN_US_ECHO, US_BOTH_EDGES,
                                   ultrasonic_echo_isr));
}

/*
 * Clear the Echo ISR's edge state, called by the scan task just before
 * each trigger (after draining any stale echo).  The next ping has not
 * been sent yet, so there is no pulse in progress worth keeping: the
 * only meaningful edge is the rising edge that the upcoming trigger will
 * produce.  Forcing the state to "waiting for a rising edge" therefore
 * discards any stale latch from an earlier ping.
 *
 * This matters because a single lost falling edge (a jolt, or the Echo
 * line held high through the sensor's ~38 ms timeout) would otherwise
 * leave g_echo_high stuck true with an old rise time: every later
 * falling edge then pairs against that stale time, the width is wrong,
 * and the range freezes -- the stuck-at-4000 (or stuck-tiny) state that
 * previously cleared only on a reflash, since a soft reset never re-runs
 * ultrasonic_init.  Clearing unconditionally here lets the very next
 * ping recover, so the freeze can no longer outlive one 65 ms cycle.
 *
 * Note it is deliberately NOT set from the live pin level: a line stuck
 * high at reset time would make app_gpio_read() return true and re-arm
 * the very latch we are trying to drop.  A rising edge from the real
 * ping re-establishes g_echo_high = true with a correct timestamp; the
 * task's own (echo.rise_us - trigger_us) >= 0 guard rejects any echo
 * that somehow predates the trigger.
 *
 * This runs on core 1 while the ISR writes the same two words on core 0,
 * but the call site makes the window race-free: the task has drained
 * every stale echo and has not yet sent the trigger, so the sensor can
 * raise no new edge until after this returns.  Each word is written
 * atomically (single 32-bit store) and marked volatile, so the ISR
 * never sees a torn value.
 */
void
ultrasonic_ping_reset (void)
{
    g_echo_high     = false;
    g_saved_rise_us = 0u;
}

/* Send one ping; returns the time of the trigger pulse. */
uint32_t
ultrasonic_trigger (void)
{
    uint32_t const start = app_time_us();

    app_gpio_write(CFG_PIN_US_TRIG, true);

    while ((app_time_us() - start) < CFG_US_TRIG_US)
    {
        /* 10 us trigger pulse (HC-SR04 datasheet). */
    }

    app_gpio_write(CFG_PIN_US_TRIG, false);

    return (start);
}

/* True while Echo is high: a pulse has started and not ended yet. */
bool
ultrasonic_echo_high (void)
{
    return (app_gpio_read(CFG_PIN_US_ECHO));
}

/*
 * Echo ISR (core 0).  Remembers the rising edge and sends the pulse on
 * the falling edge.  The rise time is kept between the two interrupts
 * in a static local, used only here.  Both edges pending at once means
 * a pulse shorter than the interrupt latency: ignored.
 */
static void
ultrasonic_echo_isr (uint32_t pin, uint32_t events, uint32_t time_us)
{
    (void) pin;

    if (US_BOTH_EDGES == (events & US_BOTH_EDGES))
    {
        g_echo_high = false;
    }
    else if (0u != (events & GPIO_IRQ_EDGE_RISE))
    {
        g_saved_rise_us = time_us;
        g_echo_high     = true;
    }
    else if (g_echo_high)
    {
        /* Widths from 23200 us up all read as 4 m: a longer one is kept
           at 32767 us, which changes no range (D-90). */
        ultrasonic_echo_t const echo =
            ultrasonic_echo_pack(g_saved_rise_us, time_us - g_saved_rise_us);

        g_echo_high = false;
        (void) tk_snd_mbf(g_echo_mbf, &echo, (INT) sizeof(echo), TMO_POL);
    }
    else
    {
        /* Falling edge without a rising one: ignored. */
    }
}

/*** end of file ***/
