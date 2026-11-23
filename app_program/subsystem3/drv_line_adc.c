/*
 *  drv_line_adc.c
 *  Line sensor ADC driver (kernel "adca" device, 12-bit conversions).
 */

#include "../app.h"
#include "../common/hw_regs.h"
#include "drv_line_adc.h"

#define LINE_ADC_DEVICE     "adca"
#define LINE_ADC_MASK       0x0FFFu     /* 12-bit result */

static ID g_adc_dd = 0;                 /* device descriptor */

static void line_adc_pad_analog(uint32_t pin);
static ER   line_adc_read_channel(INT channel, uint32_t * p_value);

/*
 * Open the kernel ADC driver and set both pads for analogue use: digital
 * input and output off, no pulls (the A0 output drives the pin).
 */
ER
line_adc_open (void)
{
    ER ercd = E_OK;

    line_adc_pad_analog(CFG_PIN_LINE_L);
    line_adc_pad_analog(CFG_PIN_LINE_R);

    g_adc_dd = tk_opn_dev((UB *) LINE_ADC_DEVICE, TD_READ);
    if (g_adc_dd < E_OK)
    {
        ercd = g_adc_dd;
    }

    return (ercd);
}

/* Read both line sensors, left then right. */
ER
line_adc_read (uint32_t * p_left, uint32_t * p_right)
{
    ER ercd = line_adc_read_channel(CFG_ADC_CH_LINE_L, p_left);

    if (E_OK == ercd)
    {
        ercd = line_adc_read_channel(CFG_ADC_CH_LINE_R, p_right);
    }

    return (ercd);
}

/* One conversion.  The driver takes the channel as the start address. */
static ER
line_adc_read_channel (INT channel, uint32_t * p_value)
{
    UW sample = 0u;
    SZ asize  = 0;
    ER ercd   = tk_srea_dev(g_adc_dd, channel, &sample, 1, &asize);

    if ((ercd >= E_OK) && (1 == asize))
    {
        *p_value = (uint32_t) (sample & LINE_ADC_MASK);
        ercd = E_OK;
    }
    else if (ercd >= E_OK)
    {
        ercd = E_IO;
    }
    else
    {
        /* Pass the driver's error code on. */
    }

    return (ercd);
}

static void
line_adc_pad_analog (uint32_t pin)
{
    clr_w(GPIO(pin), (HW_PAD_IE | HW_PAD_PUE | HW_PAD_PDE));
    set_w(GPIO(pin), HW_PAD_OD);
}

/*** end of file ***/
