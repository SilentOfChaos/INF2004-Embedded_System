/*
 *  app_pwm.h
 *  Small helpers for the RP2040 PWM block, shared by the drivers that
 *  own a slice: Motion (slices 4 and 5), Obstacle Scan (servo, slice 6)
 *  and Safety (buzzer, slice 3).  Each slice is still set up and written
 *  only by its owning task (TEAM.md section 3).
 */

#ifndef APP_PWM_H
#define APP_PWM_H

#include <stdbool.h>
#include <stdint.h>

#include "hw_regs.h"

#define APP_PWM_CLK_HZ      APP_CLK_HZ  /* PWM runs from clk_sys */

uint32_t app_pwm_slice(uint32_t pin);
void     app_pwm_setup(uint32_t slice, uint32_t top, uint32_t div_int);
void     app_pwm_attach(uint32_t pin);
void     app_pwm_set_level(uint32_t pin, uint32_t level);
void     app_pwm_enable(uint32_t slice, bool b_enable);

#endif /* APP_PWM_H */

/*** end of file ***/
