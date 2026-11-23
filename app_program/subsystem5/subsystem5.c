/*
 *  subsystem5.c
 *  Subsystem 5 (Buddy 5): creates its objects and starts its task.
 *
 *  Files in this subsystem:
 *    scan_task.c       Obstacle Scan Task: Fig 10 statechart, IF-05/06
 *    scan_logic.c      obstacle profile and side choice (pure logic)
 *    drv_ultrasonic.c  HC-SR04P trigger and Echo ISR on GP4/GP5
 *    drv_servo.c       SG90 on GP12 (PWM slice 6)
 */

#include "scan_task.h"
#include "subsystem5.h"

EXPORT ER
subsystem5_init (void)
{
    return (scan_task_init());
}

EXPORT ER
subsystem5_start (void)
{
    return (scan_task_start());
}

/*** end of file ***/
