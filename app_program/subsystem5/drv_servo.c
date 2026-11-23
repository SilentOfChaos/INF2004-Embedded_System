/*
 *  drv_servo.c
 *  SG90 servo: 50 Hz frame, pulse width from CFG_SERVO_US_AT_0 (0 deg)
 *  to CFG_SERVO_US_AT_180 (180 deg).  The PWM counter runs at 1 MHz, so
 *  the compare level is the pulse width in microseconds.
 */

#include "../app.h"
#include "../common/app_pwm.h"
#include "../common/fixmath.h"
#include "drv_servo.h"

#define SERVO_COUNT_HZ      1000000u
#define SERVO_FRAME_HZ      50u
#define SERVO_DIVIDER       (APP_PWM_CLK_HZ / SERVO_COUNT_HZ)    /* 125 */
#define SERVO_TOP           ((SERVO_COUNT_HZ / SERVO_FRAME_HZ) - 1u)
#define SERVO_FULL_DEG      180

/* 50 Hz on slice 6, servo centred. */
void
servo_init (void)
{
    uint32_t const slice = app_pwm_slice(CFG_PIN_SERVO);

    app_pwm_setup(slice, SERVO_TOP, SERVO_DIVIDER);
    servo_set_angle(SERVO_FULL_DEG / 2);
    app_pwm_enable(slice, true);
    app_pwm_attach(CFG_PIN_SERVO);
}

/* Point the servo; angles outside the servo's range are limited. */
void
servo_set_angle (int32_t angle_deg)
{
    int32_t angle = fixmath_clamp(angle_deg, CFG_SCAN_SERVO_MIN_DEG,
                                  CFG_SCAN_SERVO_MAX_DEG);
    int32_t pulse = 0;

    if (0 != CFG_SERVO_INVERT)
    {
        angle = SERVO_FULL_DEG - angle;
    }

    pulse = CFG_SERVO_US_AT_0 +
            fixmath_div_round(angle * (CFG_SERVO_US_AT_180 - CFG_SERVO_US_AT_0),
                              SERVO_FULL_DEG);

    /* The pulse is 500-2400 us, positive and below the 20000 us frame. */
    app_pwm_set_level(CFG_PIN_SERVO, (uint32_t) pulse);
}

/*** end of file ***/
