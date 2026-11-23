/*
 *  drv_encoder.c
 *  Quadrature wheel encoders.  See drv_encoder.h.
 *
 *  CFG_ENC_QUADRATURE_X4 = 0 (default, D-28): interrupt on the rising
 *  edge of channel A only; channel B's level gives the direction.  This
 *  keeps the interrupt rate low (A8: encoder interrupts must not delay
 *  the barcode edge time stamps).
 *  CFG_ENC_QUADRATURE_X4 = 1: interrupt on every edge of A and B and
 *  decode the Gray-code sequence, 4x the counts.
 */

#include "../app.h"
#include "../common/app_gpio.h"
#include "../common/gpio_irq.h"
#include "drv_encoder.h"

#define ENC_BOTH_EDGES      (GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL)
#define ENC_STATES          16u
#define ENC_PULL            ((0 != CFG_ENC_PULL_UP) ? APP_GPIO_PULL_UP \
                                                    : APP_GPIO_PULL_NONE)

/* Shared with the Motion task, justified in drv_encoder.h: each half
   written only by its wheel's Encoder ISR (core 0), the word read by
   Motion (core 0) in one load. */
static encoder_counts_t volatile g_counts;

/* The pair at Motion's last read; used only by encoder_add_counts()
   (Motion task).  Zero, like the counters, at boot. */
static encoder_counts_t g_last;

#if CFG_ENC_QUADRATURE_X4
/* Count change for (previous AB << 2) | new AB; invalid jumps count 0. */
static int8_t const g_quad_table[ENC_STATES] =
{
    0, -1,  1,  0,
    1,  0,  0, -1,
   -1,  0,  0,  1,
    0,  1, -1,  0
};

/* Previous AB state per wheel, used only inside the ISR. */
static uint8_t g_state_l = 0u;
static uint8_t g_state_r = 0u;
#endif

static void encoder_left_isr(uint32_t pin, uint32_t events,
                             uint32_t time_us);
static void encoder_right_isr(uint32_t pin, uint32_t events,
                              uint32_t time_us);

/*
 * Configure the four pins as inputs and register the Encoder ISR with
 * the shared dispatcher.  Pull-ups as set by CFG_ENC_PULL_UP
 * (app_config.h, UNVALIDATED: open-collector outputs assumed).
 */
void
encoder_init (void)
{
    app_gpio_input(CFG_PIN_ENC_L_A, ENC_PULL);
    app_gpio_input(CFG_PIN_ENC_L_B, ENC_PULL);
    app_gpio_input(CFG_PIN_ENC_R_A, ENC_PULL);
    app_gpio_input(CFG_PIN_ENC_R_B, ENC_PULL);

#if CFG_ENC_QUADRATURE_X4
    g_state_l = (uint8_t) (((app_gpio_read(CFG_PIN_ENC_L_A) ? 2u : 0u)) |
                           (app_gpio_read(CFG_PIN_ENC_L_B) ? 1u : 0u));
    g_state_r = (uint8_t) (((app_gpio_read(CFG_PIN_ENC_R_A) ? 2u : 0u)) |
                           (app_gpio_read(CFG_PIN_ENC_R_B) ? 1u : 0u));
    (void) gpio_irq_register(CFG_PIN_ENC_L_A, ENC_BOTH_EDGES,
                             encoder_left_isr);
    (void) gpio_irq_register(CFG_PIN_ENC_L_B, ENC_BOTH_EDGES,
                             encoder_left_isr);
    (void) gpio_irq_register(CFG_PIN_ENC_R_A, ENC_BOTH_EDGES,
                             encoder_right_isr);
    (void) gpio_irq_register(CFG_PIN_ENC_R_B, ENC_BOTH_EDGES,
                             encoder_right_isr);
#else
    (void) gpio_irq_register(CFG_PIN_ENC_L_A, GPIO_IRQ_EDGE_RISE,
                             encoder_left_isr);
    (void) gpio_irq_register(CFG_PIN_ENC_R_A, GPIO_IRQ_EDGE_RISE,
                             encoder_right_isr);
#endif
}

/*
 * Add each wheel's counts since the last call to Motion's 32-bit totals
 * (which start at 0 at boot, like the counters).  The pair is copied in
 * one 32-bit load, so it is consistent; each wheel's signed change is
 * exact below 32768 counts between calls.  Motion task only.
 */
void
encoder_add_counts (int32_t * p_total_l, int32_t * p_total_r)
{
    encoder_add_pair(&g_counts, &g_last, p_total_l, p_total_r);
}

#if CFG_ENC_QUADRATURE_X4
/* Encoder ISR, X4: decode the new AB state against the previous one. */
static int32_t
encoder_quad_step (uint8_t * p_state, uint32_t pin_a, uint32_t pin_b)
{
    uint8_t const now_ab = (uint8_t) ((app_gpio_read(pin_a) ? 2u : 0u) |
                                      (app_gpio_read(pin_b) ? 1u : 0u));
    uint8_t const index  = (uint8_t) ((uint8_t) (*p_state << 2u) | now_ab);

    *p_state = now_ab;

    return ((int32_t) g_quad_table[index & (ENC_STATES - 1u)]);
}
#endif

/* Encoder ISR for the left wheel. */
static void
encoder_left_isr (uint32_t pin, uint32_t events, uint32_t time_us)
{
    int32_t step = 0;

    (void) pin;
    (void) events;
    (void) time_us;

#if CFG_ENC_QUADRATURE_X4
    step = encoder_quad_step(&g_state_l, CFG_PIN_ENC_L_A, CFG_PIN_ENC_L_B);
#else
    /* Rising A: B low = one direction, B high = the other. */
    step = app_gpio_read(CFG_PIN_ENC_L_B) ? -1 : 1;
#endif

    encoder_count_step(&g_counts, ENCODER_LEFT,
                       (0 != CFG_ENC_L_INVERT) ? -step : step);
}

/* Encoder ISR for the right wheel. */
static void
encoder_right_isr (uint32_t pin, uint32_t events, uint32_t time_us)
{
    int32_t step = 0;

    (void) pin;
    (void) events;
    (void) time_us;

#if CFG_ENC_QUADRATURE_X4
    step = encoder_quad_step(&g_state_r, CFG_PIN_ENC_R_A, CFG_PIN_ENC_R_B);
#else
    step = app_gpio_read(CFG_PIN_ENC_R_B) ? -1 : 1;
#endif

    encoder_count_step(&g_counts, ENCODER_RIGHT,
                       (0 != CFG_ENC_R_INVERT) ? -step : step);
}

/*** end of file ***/
