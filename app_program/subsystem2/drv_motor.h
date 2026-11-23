/*
 *  drv_motor.h
 *  Motor driver for the Robo Pico's on-board driver (report section 2):
 *  left motor M1A/M1B on GP8/GP9 (PWM slice 4), right motor M2A/M2B on
 *  GP10/GP11 (PWM slice 5).  Owned by the Motion task.
 *
 *  Robo Pico truth table: A PWM + B low = forward, A low + B PWM =
 *  backward, both low = brake.  Duty 0 therefore always brakes, and both
 *  pins sit low between PWM pulses.
 */

#ifndef DRV_MOTOR_H
#define DRV_MOTOR_H

#include <stdint.h>

void motor_init(void);
void motor_set(int32_t duty_l_pm, int32_t duty_r_pm);

#endif /* DRV_MOTOR_H */

/*** end of file ***/
