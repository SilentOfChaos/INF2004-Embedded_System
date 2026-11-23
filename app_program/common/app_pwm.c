/*
 *  app_pwm.c
 *  Small helpers for the RP2040 PWM block.  See app_pwm.h.
 */

#include "../app.h"
#include "app_pwm.h"
#include "hw_regs.h"

#define APP_PWM_SLICES      8u
#define APP_PWM_CC_MASK_A   0x0000FFFFu
#define APP_PWM_CC_SHIFT_B  16u
#define APP_PWM_DIV_MAX     255u

static void app_pwm_unreset(void);

/* PWM slice of a pin: (pin / 2) mod 8 (RP2040 datasheet). */
uint32_t
app_pwm_slice (uint32_t pin)
{
    return ((pin / 2u) % APP_PWM_SLICES);
}

/*
 * Set a slice's wrap value and integer clock divider, counter running
 * from zero.  The PWM block is taken out of reset first if needed; it is
 * never put back into reset, because other slices may be in use.
 */
void
app_pwm_setup (uint32_t slice, uint32_t top, uint32_t div_int)
{
    uint32_t divider = div_int;

    if (0u == divider)
    {
        divider = 1u;
    }
    else if (divider > APP_PWM_DIV_MAX)
    {
        divider = APP_PWM_DIV_MAX;
    }
    else
    {
        /* Divider already in range. */
    }

    app_pwm_unreset();
    out_w(HW_PWM_CSR(slice), 0u);
    out_w(HW_PWM_DIV(slice), (divider << HW_PWM_DIV_INT_SHIFT));
    out_w(HW_PWM_TOP(slice), top);
    out_w(HW_PWM_CC(slice), 0u);
    out_w(HW_PWM_CTR(slice), 0u);
}

/* Connect a pin to its PWM output (pad: output enabled, no pulls). */
void
app_pwm_attach (uint32_t pin)
{
    clr_w(GPIO(pin), (HW_PAD_OD | HW_PAD_PUE | HW_PAD_PDE));
    out_w(HW_GPIO_CTRL(pin), HW_FUNCSEL_PWM);
}

/*
 * Set the compare level of one channel (A = even pin, B = odd pin).  Only
 * the slice's owning task calls this, so the read-modify-write of the
 * shared CC register is not interrupted by another writer.
 */
void
app_pwm_set_level (uint32_t pin, uint32_t level)
{
    uint32_t const slice = app_pwm_slice(pin);
    uint32_t       cc    = in_w(HW_PWM_CC(slice));

    if (0u != (pin % 2u))
    {
        cc = (cc & APP_PWM_CC_MASK_A) |
             ((level & APP_PWM_CC_MASK_A) << APP_PWM_CC_SHIFT_B);
    }
    else
    {
        cc = (cc & ~APP_PWM_CC_MASK_A) | (level & APP_PWM_CC_MASK_A);
    }

    out_w(HW_PWM_CC(slice), cc);
}

/* Start or stop a slice's counter. */
void
app_pwm_enable (uint32_t slice, bool b_enable)
{
    if (b_enable)
    {
        set_w(HW_PWM_CSR(slice), HW_PWM_CSR_EN);
    }
    else
    {
        clr_w(HW_PWM_CSR(slice), HW_PWM_CSR_EN);
    }
}

/* Release the PWM block from reset (idempotent, atomic clear alias). */
static void
app_pwm_unreset (void)
{
    clr_w(HW_RESETS_RESET, HW_RESETS_PWM);

    while (0u == (in_w(HW_RESETS_DONE) & HW_RESETS_PWM))
    {
        /* Wait for the block to come out of reset (a few cycles). */
    }
}

/*** end of file ***/
