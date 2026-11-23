/*
 *  subsystem4.c
 *  Subsystem 4 (Buddy 4): creates its objects and starts its tasks.
 *
 *  Files in this subsystem:
 *    safety_task.c    Safety Task: fault latch, pin override (A14),
 *                     RESET handshake, watchdog, alarm; IF-04 and IF-09
 *    imu_task.c       IMU Task: calibration, heading, humps, motion
 *                     class, collision; IF-03
 *    imu_calc.c       tilt, heading, hump detector, classifier (pure)
 *    drv_i2c1.c       I2C1 master on GP2/GP3
 *    drv_lsm303.c     GY-511 (LSM303DLHC)
 *    drv_watchdog.c   RP2040 watchdog
 *    drv_buzzer.c     piezo buzzer on GP22
 *    drv_neopixel.c   NeoPixel on GP18 (PIO, Pico SDK namespace)
 */

#include "imu_task.h"
#include "safety_task.h"
#include "subsystem4.h"

EXPORT ER
subsystem4_init (void)
{
    ER ercd = safety_task_init();

    if (E_OK == ercd)
    {
        ercd = imu_task_init();
    }

    return (ercd);
}

EXPORT ER
subsystem4_start (void)
{
    ER ercd = safety_task_start();

    if (E_OK == ercd)
    {
        ercd = imu_task_start();
    }

    return (ercd);
}

/*** end of file ***/
