/*
 *  app_time.c
 *  Millisecond time stamps from the kernel's operating time.
 */

#include "../app.h"
#include "app_time.h"

/*
 * Start the watchdog tick generator at 1 us (12 cycles of the 12 MHz
 * clk_ref).  The TIMER block behind app_time_us() and the hardware
 * watchdog both count this tick (RP2040 datasheet 4.6, 4.7.2); the Pico
 * SDK starts it in its runtime init, but the micro T-Kernel BSP does not.
 *
 * Without it the timer only ran at 1 MHz when the bootrom had left the
 * tick configured (after a UF2 flash).  The RST button (RUN pin) resets
 * the whole chip, tick included, after which the timer ran about 43x
 * slow (S3-U20 measured 1 s as 23449 us): every echo width was too
 * short, so the ultrasonic read invalid and reported 4000 mm.
 *
 * Writing the register is harmless when the tick is already running.
 */
void
app_time_init (void)
{
    out_w(HW_WATCHDOG_TICK, (HW_WATCHDOG_TICK_ENABLE | HW_XOSC_MHZ));

    while (0u == (in_w(HW_WATCHDOG_TICK) & HW_WATCHDOG_TICK_RUNNING))
    {
        /* Wait for the tick generator to start (a few clk_ref cycles). */
    }
}

/*
 * Return the kernel operating time in milliseconds.  Only the low 32 bits
 * are kept, which wrap after 49.7 days; callers subtract as unsigned.
 */
uint32_t
app_time_ms (void)
{
    SYSTIM  now;

    (void) tk_get_otm(&now);

    return ((uint32_t) now.lo);     /* UW and uint32_t are both 32 bits. */
}

/*** end of file ***/
