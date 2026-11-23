/*
 *  scan_task.h
 *  The Obstacle Scan Task (report section 3): priority 10, core 1, every
 *  65 ms.  Private to subsystem 5.
 */

#ifndef SCAN_TASK_H
#define SCAN_TASK_H

#include "../app.h"

ER scan_task_init(void);
ER scan_task_start(void);

#endif /* SCAN_TASK_H */

/*** end of file ***/
