/*
 *  app_stats.h
 *  Measurement for the Week 13 evidence of the minimum-bit change: the
 *  same build is measured before and after the change.
 *
 *  With CFG_STATS_ENABLE 1 (app_config.h) the firmware records, on the
 *  board:
 *    - per task: shortest and longest time between periodic releases
 *      (NFR1, NFR3) and the longest work time after a release;
 *    - per interrupt pin: the longest handler time, and the longest time
 *      in the shared GPIO dispatcher (packing must not slow it);
 *    - per task: stack high-water mark.  app_start_task_stack() notes
 *      where the kernel put each stack (app_stats_stack), and each task
 *      fills the unused part of its own stack with a pattern when it
 *      starts (app_stats_task_start);
 *    - per message buffer: bytes the kernel allocated (read when it is
 *      registered, still empty), free now, least free seen.
 *  Call app_stats_release() at the start of a task's periodic work and
 *  app_stats_work_end() when it is done; the period is the time between
 *  two starts.
 *  Telemetry prints a report on the console every CFG_STATS_REPORT_MS.
 *  Timing resolution is 1 us (the RP2040 timer).
 *
 *  With CFG_STATS_ENABLE 0 (the default) every function below is an empty
 *  inline function, so normal builds carry no code or data for it.  The
 *  records here are measurement data, not application data: each word has
 *  one writer and is read only by the report, across cores and without a
 *  kernel object, so the figures in one printed line can come from
 *  different moments (diagnostic only, TEAM.md section 7).  Times are
 *  wall-clock, not CPU time; testing/README.md lists what each includes.
 */

#ifndef APP_STATS_H
#define APP_STATS_H

#include <stdint.h>

#include "../app.h"

typedef enum
{
    APP_STATS_SAFETY = 0,
    APP_STATS_LINE,
    APP_STATS_NAV,
    APP_STATS_IMU,
    APP_STATS_MOTION,
    APP_STATS_SCAN,
    APP_STATS_COMMAND,
    APP_STATS_TELEMETRY,
    APP_STATS_TASKS
} app_stats_task_t;

#if (0 != CFG_STATS_ENABLE)

void app_stats_stack(ID tid, SZ stksz);
void app_stats_task_start(app_stats_task_t task);
void app_stats_release(app_stats_task_t task);
void app_stats_work_end(app_stats_task_t task);
void app_stats_isr(uint32_t pin, uint32_t start_us, uint32_t end_us);
void app_stats_dispatch(uint32_t start_us, uint32_t end_us);
void app_stats_queue(char const * p_name, ID mbfid);
void app_stats_report_tick(void);

#else

static inline void
app_stats_stack (ID tid, SZ stksz)
{
    (void) tid;
    (void) stksz;
}

static inline void
app_stats_task_start (app_stats_task_t task)
{
    (void) task;
}

static inline void
app_stats_release (app_stats_task_t task)
{
    (void) task;
}

static inline void
app_stats_work_end (app_stats_task_t task)
{
    (void) task;
}

static inline void
app_stats_isr (uint32_t pin, uint32_t start_us, uint32_t end_us)
{
    (void) pin;
    (void) start_us;
    (void) end_us;
}

static inline void
app_stats_dispatch (uint32_t start_us, uint32_t end_us)
{
    (void) start_us;
    (void) end_us;
}

static inline void
app_stats_queue (char const * p_name, ID mbfid)
{
    (void) p_name;
    (void) mbfid;
}

static inline void
app_stats_report_tick (void)
{
}

#endif /* CFG_STATS_ENABLE */

#endif /* APP_STATS_H */

/*** end of file ***/
