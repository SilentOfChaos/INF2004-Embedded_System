/*
 *  drv_servo.h
 *  SG90 servo on GP12 (Servo 1, PWM slice 6 A), owned by the Obstacle
 *  Scan task (report section 2).  Angle 90 = straight ahead, larger =
 *  towards the left of the car.
 */

#ifndef DRV_SERVO_H
#define DRV_SERVO_H

#include <stdint.h>

void servo_init(void);
void servo_set_angle(int32_t angle_deg);

#endif /* DRV_SERVO_H */

/*** end of file ***/
