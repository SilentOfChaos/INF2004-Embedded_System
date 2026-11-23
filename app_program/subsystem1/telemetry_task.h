/*
 *  telemetry_task.h
 *  The Telemetry Task (report section 3): priority 12, core 1, every
 *  200 ms.  Private to subsystem 1.
 */

#ifndef TELEMETRY_TASK_H
#define TELEMETRY_TASK_H

#include "../app.h"

ER telemetry_task_init(void);
ER telemetry_task_start(void);

#endif /* TELEMETRY_TASK_H */

/*** end of file ***/
