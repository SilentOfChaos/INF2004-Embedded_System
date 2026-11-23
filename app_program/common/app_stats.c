/*
 *  app_stats.c
 *  Measurement for the minimum-bit change.  See app_stats.h.
 */

#include "../app.h"
#include "app_stats.h"
#include "app_time.h"

#if (0 != CFG_STATS_ENABLE)

#define STATS_PAINT         0x5AA5C33Cu /* unused stack word */
#define STATS_PAINT_GAP     128u        /* bytes left unpainted below SP */
#define STATS_PINS          30u
#define STATS_QUEUES        16u
#define STATS_STACKS        16u
#define STATS_WORD_BYTES    4u

/*
 * Kernel facts for the stack figures (ARMv6-M port):
 *  - tk_cre_tsk() allocates stksz + DEFAULT_SYS_STKSZ bytes, rounded up to
 *    a multiple of 8 (kernel/tkernel/task_manage.c);
 *  - a new task's saved stack pointer is the top of that area less the
 *    start frame, SStackFrame, 17 words (kernel/sysdepend/cpu/core/armv6m/
 *    cpu_task.h); tk_get_reg() returns it while the task is DORMANT.
 */
#define STATS_START_FRAME   68u
#define STATS_STACK_ALIGN   8u

/* One task's figures.  Written only by that task, read by the report. */
typedef struct
{
    uint32_t last_us;           /* latest periodic release */
    uint32_t period_min_us;
    uint32_t period_max_us;
    uint32_t work_max_us;
    uint32_t releases;
    uint32_t stack_bottom;      /* lowest address of the task's stack */
    uint32_t stack_size;        /* bytes, as the kernel allocated them */
} stats_task_t;

/* One message buffer.  Registered in the init phase, before the tasks
   start; free_min is updated only by the report. */
typedef struct
{
    char const * p_name;
    ID           mbfid;
    uint32_t     bytes;
    uint32_t     free_min;
} stats_queue_t;

static char const * const g_task_names[APP_STATS_TASKS] =
{
    "safety", "line", "nav", "imu", "motion", "scan", "command", "telemetry"
};

/* One kernel stack, noted when its task is created. */
typedef struct
{
    ID       tid;
    uint32_t bottom;
    uint32_t size;
} stats_stack_t;

static stats_task_t  g_tasks[APP_STATS_TASKS];
static stats_stack_t g_stacks[STATS_STACKS];       /* init phase only */
static uint32_t      g_stack_count;
static uint32_t      g_isr_max_us[STATS_PINS];     /* written by the ISR */
static uint32_t      g_dispatch_max_us;            /* written by the ISR */
static stats_queue_t g_queues[STATS_QUEUES];
static uint32_t      g_queue_count;
static uint32_t      g_next_report_ms;

static uint32_t stats_stack_used(stats_task_t const * p_task);
static void     stats_sample_queues(void);
static void     stats_print(void);

/*
 * Called by app_start_task_stack() between tk_cre_tsk() and tk_sta_tsk(),
 * while the task is DORMANT: note where the kernel put its stack.
 */
void
app_stats_stack (ID tid, SZ stksz)
{
    T_CREGS cregs;

    if ((g_stack_count < STATS_STACKS) && (stksz > 0) &&
        (E_OK == tk_get_reg(tid, NULL, NULL, &cregs)))
    {
        /* The RP2040 has 32-bit addresses. */
        uint32_t const top  = (uint32_t) (uintptr_t) cregs.ssp +
                              STATS_START_FRAME;
        uint32_t const size = ((uint32_t) stksz + DEFAULT_SYS_STKSZ +
                               (STATS_STACK_ALIGN - 1u)) &
                              ~(STATS_STACK_ALIGN - 1u);

        g_stacks[g_stack_count].tid    = tid;
        g_stacks[g_stack_count].bottom = top - size;
        g_stacks[g_stack_count].size   = size;
        g_stack_count++;
    }
}

/*
 * First call in a task: find its stack and fill the unused part (below
 * the current stack pointer, less a gap) with a pattern, so the report
 * can find the deepest point ever used.  Interrupts taken while this task
 * runs use the same stack, so they count too.
 */
void
app_stats_task_start (app_stats_task_t task)
{
    ID const  self = tk_get_tid();
    uintptr_t sp   = 0u;

    /* The current stack pointer (thread mode, main stack). */
    __asm volatile ("mov %0, sp" : "=r" (sp));

    for (uint32_t idx = 0u; (task < APP_STATS_TASKS) &&
                            (idx < g_stack_count); idx++)
    {
        uint32_t const bottom = g_stacks[idx].bottom;
        uint32_t const top    = bottom + g_stacks[idx].size;

        /* Only a stack that really holds the stack pointer is painted. */
        if ((self == g_stacks[idx].tid) && ((uint32_t) sp > bottom) &&
            ((uint32_t) sp <= top) &&
            (((uint32_t) sp - bottom) > STATS_PAINT_GAP))
        {
            g_tasks[task].stack_bottom = bottom;
            g_tasks[task].stack_size   = g_stacks[idx].size;

            for (uint32_t addr = bottom;
                 addr < ((uint32_t) sp - STATS_PAINT_GAP);
                 addr += STATS_WORD_BYTES)
            {
                *(uint32_t volatile *) addr = STATS_PAINT;
            }
        }
    }
}

/* A periodic release of the task: shortest and longest period. */
void
app_stats_release (app_stats_task_t task)
{
    if (task < APP_STATS_TASKS)
    {
        uint32_t const now    = app_time_us();
        stats_task_t * p_task = &g_tasks[task];

        if (0u != p_task->releases)
        {
            uint32_t const period = now - p_task->last_us;   /* wraps */

            if ((1u == p_task->releases) || (period < p_task->period_min_us))
            {
                p_task->period_min_us = period;
            }

            if (period > p_task->period_max_us)
            {
                p_task->period_max_us = period;
            }
        }

        p_task->last_us = now;
        p_task->releases++;
    }
}

/* The work after a release is done: longest work time. */
void
app_stats_work_end (app_stats_task_t task)
{
    if (task < APP_STATS_TASKS)
    {
        uint32_t const work = app_time_us() - g_tasks[task].last_us;

        if (work > g_tasks[task].work_max_us)
        {
            g_tasks[task].work_max_us = work;
        }
    }
}

/* One GPIO handler call (from the dispatcher, core 0). */
void
app_stats_isr (uint32_t pin, uint32_t start_us, uint32_t end_us)
{
    uint32_t const took = end_us - start_us;

    if ((pin < STATS_PINS) && (took > g_isr_max_us[pin]))
    {
        g_isr_max_us[pin] = took;
    }
}

/* One whole dispatcher entry (core 0). */
void
app_stats_dispatch (uint32_t start_us, uint32_t end_us)
{
    uint32_t const took = end_us - start_us;

    if (took > g_dispatch_max_us)
    {
        g_dispatch_max_us = took;
    }
}

/* Register a message buffer just after it is created.  It is empty, so
   its free space is the size the kernel allocated (rounded up). */
void
app_stats_queue (char const * p_name, ID mbfid)
{
    T_RMBF rmbf;

    if ((g_queue_count < STATS_QUEUES) && (mbfid > 0) &&
        (E_OK == tk_ref_mbf(mbfid, &rmbf)))
    {
        g_queues[g_queue_count].p_name   = p_name;
        g_queues[g_queue_count].mbfid    = mbfid;
        g_queues[g_queue_count].bytes    = (uint32_t) rmbf.frbufsz;
        g_queues[g_queue_count].free_min = (uint32_t) rmbf.frbufsz;
        g_queue_count++;
    }
}

/* Called by Telemetry every 200 ms: sample queues, print when due. */
void
app_stats_report_tick (void)
{
    uint32_t const now_ms = app_time_ms();

    stats_sample_queues();

    if ((int32_t) (now_ms - g_next_report_ms) >= 0)
    {
        g_next_report_ms = now_ms + CFG_STATS_REPORT_MS;
        stats_print();
    }
}

/* Bytes of the stack ever used: the size less the untouched pattern. */
static uint32_t
stats_stack_used (stats_task_t const * p_task)
{
    uint32_t free_bytes = 0u;

    while ((free_bytes < p_task->stack_size) &&
           (STATS_PAINT == *(uint32_t const volatile *)
                            (p_task->stack_bottom + free_bytes)))
    {
        free_bytes += STATS_WORD_BYTES;
    }

    return (p_task->stack_size - free_bytes);
}

/* Least free space seen in each queue (sampled, so short peaks between
   samples are missed). */
static void
stats_sample_queues (void)
{
    for (uint32_t idx = 0u; idx < g_queue_count; idx++)
    {
        T_RMBF rmbf;

        if ((E_OK == tk_ref_mbf(g_queues[idx].mbfid, &rmbf)) &&
            ((uint32_t) rmbf.frbufsz < g_queues[idx].free_min))
        {
            g_queues[idx].free_min = (uint32_t) rmbf.frbufsz;
        }
    }
}

static void
stats_print (void)
{
    tm_printf((UB *) "stats: task period_min_us period_max_us "
                     "work_max_us releases stack_used stack_size\n");

    for (uint32_t idx = 0u; idx < (uint32_t) APP_STATS_TASKS; idx++)
    {
        stats_task_t const * const p_task = &g_tasks[idx];

        /* Periods need two releases; stack figures 0 = stack not found. */
        if ((0u != p_task->releases) || (0u != p_task->stack_size))
        {
            tm_printf((UB *) "stats: %s %d %d %d %d %d %d\n",
                      g_task_names[idx],
                      (INT) p_task->period_min_us,
                      (INT) p_task->period_max_us,
                      (INT) p_task->work_max_us,
                      (INT) p_task->releases,
                      (INT) stats_stack_used(p_task),
                      (INT) p_task->stack_size);
        }
    }

    tm_printf((UB *) "stats: isr dispatch_max_us %d\n",
              (INT) g_dispatch_max_us);

    for (uint32_t pin = 0u; pin < STATS_PINS; pin++)
    {
        if (0u != g_isr_max_us[pin])
        {
            tm_printf((UB *) "stats: isr pin %d max_us %d\n", (INT) pin,
                      (INT) g_isr_max_us[pin]);
        }
    }

    for (uint32_t idx = 0u; idx < g_queue_count; idx++)
    {
        T_RMBF rmbf;

        if (E_OK == tk_ref_mbf(g_queues[idx].mbfid, &rmbf))
        {
            tm_printf((UB *) "stats: queue %s bytes %d free %d free_min %d\n",
                      g_queues[idx].p_name, (INT) g_queues[idx].bytes,
                      (INT) rmbf.frbufsz, (INT) g_queues[idx].free_min);
        }
    }
}

#else

/* Measurement is off: nothing is compiled here. */
typedef int app_stats_unused_t;

#endif /* CFG_STATS_ENABLE */

/*** end of file ***/
