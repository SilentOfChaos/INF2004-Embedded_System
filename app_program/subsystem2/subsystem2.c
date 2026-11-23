/*
 *  subsystem2.c
 *  Subsystem 2 (Buddy 2): creates its objects and starts its task.
 *
 *  Files in this subsystem:
 *    motion_task.c    Motion Task: 20 ms loop, the motion API (IF-07),
 *                     odometry and move-done (IF-08), reset (IF-09)
 *    motion_ctl.c     PID, soft start and geometry (pure logic)
 *    drv_motor.c      motor PWM on GP8-GP11 (slices 4 and 5)
 *    drv_encoder.c    Encoder ISR on GP0/GP1 and GP16/GP17
 */

#include "motion_task.h"
#include "subsystem2.h"

EXPORT ER
subsystem2_init (void)
{
    return (motion_task_init());
}

EXPORT ER
subsystem2_start (void)
{
    return (motion_task_start());
}

/*** end of file ***/
