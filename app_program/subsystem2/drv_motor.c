/*
 *  drv_motor.c
 *  Motor driver: two PWM slices, duty per mille, sign = direction.
 *
 *  After start-up this file only writes the PWM slice registers; the
 *  pins' output override belongs to Safety (report section 4, A14), so
 *  Motion and Safety never write the same register.
 */

#include "../app.h"
#include "../common/app_pwm.h"
#include "../common/fixmath.h"
#include "drv_motor.h"

#define MOTOR_TOP       ((APP_PWM_CLK_HZ / CFG_MOTOR_PWM_HZ) - 1u)

/* The slice counter is 16 bits wide (no clock divider is used here). */
#if ((APP_PWM_CLK_HZ / CFG_MOTOR_PWM_HZ) > 65536u)
#error "CFG_MOTOR_PWM_HZ too low for a 16-bit PWM counter without a divider"
#endif
#define MOTOR_FULL_PM   1000

static void motor_set_one(uint32_t pin_a, uint32_t pin_b, int32_t duty_pm);

/*
 * Set up both slices at CFG_MOTOR_PWM_HZ with every output at 0 (brake),
 * then connect the pins.  Before this the pins are inputs with the
 * reset pull-downs, which the driver also reads as low (motor pins held
 * safe in BOOT, report section 5).
 */
void
motor_init (void)
{
    uint32_t const slice_l = app_pwm_slice(CFG_PIN_MOTOR_L_A);
    uint32_t const slice_r = app_pwm_slice(CFG_PIN_MOTOR_R_A);

    app_pwm_setup(slice_l, MOTOR_TOP, 1u);
    app_pwm_setup(slice_r, MOTOR_TOP, 1u);
    app_pwm_enable(slice_l, true);
    app_pwm_enable(slice_r, true);

    app_pwm_attach(CFG_PIN_MOTOR_L_A);
    app_pwm_attach(CFG_PIN_MOTOR_L_B);
    app_pwm_attach(CFG_PIN_MOTOR_R_A);
    app_pwm_attach(CFG_PIN_MOTOR_R_B);
}

/* Set both motors; positive = forward for the car (after inversion). */
void
motor_set (int32_t duty_l_pm, int32_t duty_r_pm)
{
    int32_t const left  = (0 != CFG_MOTOR_L_INVERT) ? -duty_l_pm : duty_l_pm;
    int32_t const right = (0 != CFG_MOTOR_R_INVERT) ? -duty_r_pm : duty_r_pm;

    motor_set_one(CFG_PIN_MOTOR_L_A, CFG_PIN_MOTOR_L_B, left);
    motor_set_one(CFG_PIN_MOTOR_R_A, CFG_PIN_MOTOR_R_B, right);
}

/* One motor: PWM on A for forward, on B for backward, both 0 = brake. */
static void
motor_set_one (uint32_t pin_a, uint32_t pin_b, int32_t duty_pm)
{
    int32_t const  duty  = fixmath_clamp(duty_pm, -MOTOR_FULL_PM,
                                         MOTOR_FULL_PM);
    /* |duty| <= 1000 and TOP + 1 = 12500, so the product fits 32 bits. */
    uint32_t const level = ((uint32_t) fixmath_abs(duty) *
                            (MOTOR_TOP + 1u)) / (uint32_t) MOTOR_FULL_PM;

    if (duty > 0)
    {
        app_pwm_set_level(pin_b, 0u);
        app_pwm_set_level(pin_a, level);
    }
    else if (duty < 0)
    {
        app_pwm_set_level(pin_a, 0u);
        app_pwm_set_level(pin_b, level);
    }
    else
    {
        app_pwm_set_level(pin_a, 0u);
        app_pwm_set_level(pin_b, 0u);
    }
}

/*** end of file ***/
