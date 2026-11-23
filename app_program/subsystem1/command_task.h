/*
 *  command_task.h
 *  The Command Task (report section 3): priority 11, core 1, on each
 *  MQTT command and every 100 ms for the link check.  Private to
 *  subsystem 1.
 */

#ifndef COMMAND_TASK_H
#define COMMAND_TASK_H

#include "../app.h"

ER command_task_start(void);

#endif /* COMMAND_TASK_H */

/*** end of file ***/
