/*
 *  imu_task.h
 *  The IMU Task (report section 3): priority 8, core 1, every 10 ms.
 *  Private to subsystem 4.
 */

#ifndef IMU_TASK_H
#define IMU_TASK_H

#include "../app.h"

ER imu_task_init(void);
ER imu_task_start(void);

#endif /* IMU_TASK_H */

/*** end of file ***/
