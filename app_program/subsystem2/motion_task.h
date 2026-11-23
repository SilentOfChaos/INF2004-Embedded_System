/*
 *  motion_task.h
 *  The Motion Task (report section 3): priority 9, core 0, every 20 ms.
 *  Private to subsystem 2.
 */

#ifndef MOTION_TASK_H
#define MOTION_TASK_H

#include "../app.h"

ER motion_task_init(void);
ER motion_task_start(void);

#endif /* MOTION_TASK_H */

/*** end of file ***/
