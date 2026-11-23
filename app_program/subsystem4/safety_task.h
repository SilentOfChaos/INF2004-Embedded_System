/*
 *  safety_task.h
 *  The Safety Task (report section 3): priority 5 (highest application
 *  task), core 0, every 10 ms and on a fault event.
 *  Private to subsystem 4.
 */

#ifndef SAFETY_TASK_H
#define SAFETY_TASK_H

#include "../app.h"

ER safety_task_init(void);
ER safety_task_start(void);

#endif /* SAFETY_TASK_H */

/*** end of file ***/
