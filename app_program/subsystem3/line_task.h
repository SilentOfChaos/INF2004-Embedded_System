/*
 *  line_task.h
 *  The Line Task (report section 3): priority 6, core 0, every 5 ms.
 *  Private to subsystem 3 (subsystem3.c and the on-target tests).
 */

#ifndef LINE_TASK_H
#define LINE_TASK_H

#include "../app.h"

ER line_task_init(void);
ER line_task_start(void);

#endif /* LINE_TASK_H */

/*** end of file ***/
