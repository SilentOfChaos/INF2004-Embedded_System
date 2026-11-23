/*
 *  drv_buzzer.c
 *  Piezo buzzer: a CFG_BUZZER_HZ square wave at 50% duty, or silence.
 */

#include "../app.h"
#include "../common/app_pwm.h"
#include "drv_buzzer.h"

#define BUZZER_TOP      ((APP_PWM_CLK_HZ / CFG_BUZZER_HZ) - 1u)

/* The slice counter is 16 bits wide (no clock divider is used here). */
#if ((APP_PWM_CLK_HZ / CFG_BUZZER_HZ) > 65536u)
#error "CFG_BUZZER_HZ too low for a 16-bit PWM counter without a divider"
#endif
#define BUZZER_HALF     ((BUZZER_TOP + 1u) / 2u)

/* Set up slice 3 at the tone frequency, silent. */
void
buzzer_init (void)
{
    uint32_t const slice = app_pwm_slice(CFG_PIN_BUZZER);

    app_pwm_setup(slice, BUZZER_TOP, 1u);
    app_pwm_set_level(CFG_PIN_BUZZER, 0u);
    app_pwm_enable(slice, true);
    app_pwm_attach(CFG_PIN_BUZZER);
}

void
buzzer_on (bool b_on)
{
    app_pwm_set_level(CFG_PIN_BUZZER, b_on ? BUZZER_HALF : 0u);
}

/*** end of file ***/
