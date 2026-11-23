/*
 *  nav_task.h
 *  The Navigation task (report section 3): priority 7, core 0, every
 *  5 ms and on an event.  Private to the navigation folder.
 */

#ifndef NAV_TASK_H
#define NAV_TASK_H

#include "../app.h"

ER nav_task_init(void);
ER nav_task_start(void);

#endif /* NAV_TASK_H */

/*** end of file ***/
