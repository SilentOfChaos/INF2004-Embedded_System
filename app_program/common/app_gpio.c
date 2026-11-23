/*
 *  app_gpio.c
 *  Plain GPIO set-up and access for IS19 drivers (SIO function).
 */

#include "../app.h"
#include "app_gpio.h"
#include "hw_regs.h"

#define APP_GPIO_COUNT      30u     /* GP0..GP29 on the RP2040 */

/*
 * Make a pin a digital input with the chosen pull resistor.  The Schmitt
 * trigger stays on (reset default) so slow edges do not chatter as much.
 */
void
app_gpio_input (uint32_t pin, app_gpio_pull_t pull)
{
    if (pin < APP_GPIO_COUNT)
    {
        out_w(HW_SIO_GPIO_OE_CLR, (1u << pin));
        clr_w(GPIO(pin), (HW_PAD_OD | HW_PAD_PUE | HW_PAD_PDE));
        set_w(GPIO(pin), (HW_PAD_IE | HW_PAD_SCHMITT));

        if (APP_GPIO_PULL_UP == pull)
        {
            set_w(GPIO(pin), HW_PAD_PUE);
        }
        else if (APP_GPIO_PULL_DOWN == pull)
        {
            set_w(GPIO(pin), HW_PAD_PDE);
        }
        else
        {
            /* No pull resistor: the module drives the line itself. */
        }

        out_w(HW_GPIO_CTRL(pin), HW_FUNCSEL_SIO);
    }
}

/*
 * Make a pin a push-pull output, setting its level before the driver is
 * enabled so it never glitches to the other level.
 */
void
app_gpio_output (uint32_t pin, bool b_high)
{
    if (pin < APP_GPIO_COUNT)
    {
        app_gpio_write(pin, b_high);
        clr_w(GPIO(pin), (HW_PAD_OD | HW_PAD_PUE | HW_PAD_PDE));
        out_w(HW_GPIO_CTRL(pin), HW_FUNCSEL_SIO);
        out_w(HW_SIO_GPIO_OE_SET, (1u << pin));
    }
}

/* Read the current input level of a pin. */
bool
app_gpio_read (uint32_t pin)
{
    bool b_high = false;

    if (pin < APP_GPIO_COUNT)
    {
        b_high = (0u != (in_w(HW_SIO_GPIO_IN) & (1u << pin)));
    }

    return (b_high);
}

/* Drive an output pin high or low. */
void
app_gpio_write (uint32_t pin, bool b_high)
{
    if (pin < APP_GPIO_COUNT)
    {
        if (b_high)
        {
            out_w(HW_SIO_GPIO_OUT_SET, (1u << pin));
        }
        else
        {
            out_w(HW_SIO_GPIO_OUT_CLR, (1u << pin));
        }
    }
}

/*** end of file ***/
