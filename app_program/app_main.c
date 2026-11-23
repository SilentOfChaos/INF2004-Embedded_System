/*
 *	app_main.c
 *	START HERE.  usermain() below is your program's entry point.
 *
 *	How micro T-Kernel starts your code
 *	-----------------------------------
 *	  reset -> kernel initialises itself -> creates the "initial task"
 *	        -> initial task calls usermain()
 *
 *	usermain() runs inside that initial task at priority 1 (the highest).
 *	On a single core nothing you create runs until usermain() blocks, but
 *	on the dual-core build the OTHER core may start a new task at once.
 *	So usermain() works in two phases:
 *	  1. every subsystem creates its kernel objects  (subsystemN_init)
 *	  2. every subsystem starts its tasks             (subsystemN_start)
 *	and then puts the initial task to sleep forever.
 *	If usermain() RETURNS, the kernel shuts the whole system down.
 *
 *	IS19 autonomous robot car (Week 6 design review, TEAM.md):
 *	  subsystem1/  Buddy 1  Command + Telemetry tasks      (core 1)
 *	  subsystem2/  Buddy 2  Motion task + Encoder ISR      (core 0)
 *	  subsystem3/  Buddy 3  Line task + Barcode ISR,       (core 0)
 *	                        steering and letter-to-action map
 *	  subsystem4/  Buddy 4  Safety task (core 0), IMU task (core 1)
 *	  subsystem5/  Buddy 5  Obstacle Scan task + Echo ISR  (core 1)
 *	  navigation/  team     Navigation task, the main controller (core 0)
 *	  common/      team     GPIO interrupt dispatcher, time, maths
 *
 *	Interfaces between them: TEAM.md section 5.
 *	Every tunable value: app_config.h.
 *
 *	Output appears on the USB serial port (115200 8N1 in any terminal).
 */

#include "app.h"
#include "common/app_stats.h"
#include "common/app_time.h"
#include "common/gpio_irq.h"
#if defined(APP_BENCH) && APP_BENCH
#include "subsystem2/motion_bench.h"
#endif

/* IS19: app_config.h mirrors the kernel tick that config/config.h sets
   (report section 3, A12); the two must never disagree. */
#if (CNF_TIMER_PERIOD != CFG_TICK_MS)
#error "CFG_TICK_MS (app_config.h) must equal CNF_TIMER_PERIOD (config.h)"
#endif

/*
 * Create and start one task (declared in app.h for every subsystem).
 *   prc = TP_PRC1 (core 0), TP_PRC2 (core 1), or 0 for "any core".
 */
EXPORT ID app_start_task_stack(FP entry, PRI pri, UW prc, SZ stksz,
			       const char *name)
{
	T_CTSK	ctsk = {0};
	ID	tid;
	ER	er;

	ctsk.task    = entry;		/* function the task runs */
	ctsk.itskpri = pri;		/* initial priority (1 = highest) */
	ctsk.stksz   = stksz;		/* kernel allocates the stack */
	ctsk.tskatr  = TA_HLNG		/* written in a high-level language (C) */
		     | TA_RNG3;		/* runs at user protection level */
#if TK_SUPPORT_SMP
	if (prc != 0) {
		ctsk.tskatr |= TA_ASSPRC;	/* honour the affinity below */
		ctsk.assprc  = prc;		/* which core(s) it may run on */
	}
#else
	(void)prc;			/* single-core build: no affinity */
#endif

	/* tk_cre_tsk: create the task.  It starts in the DORMANT state. */
	tid = tk_cre_tsk(&ctsk);
	if (tid < E_OK) {
		CHECK(tid, name);
		return tid;
	}

	/* Measurement builds note where the stack is (common/app_stats.h). */
	app_stats_stack(tid, stksz);

	/* tk_sta_tsk: DORMANT -> READY.  The int is passed as stacd. */
	er = tk_sta_tsk(tid, 0);
	CHECK(er, name);
	return tid;
}

EXPORT ID app_start_task(FP entry, PRI pri, UW prc, const char *name)
{
	return app_start_task_stack(entry, pri, prc, TASK_STACK_SIZE, name);
}

/* Print a failed start-up call; false when it failed. */
LOCAL BOOL app_init_ok(INT er, const char *what)
{
	CHECK(er, what);
	return (er >= E_OK) ? TRUE : FALSE;
}

EXPORT INT usermain(void)
{
	T_RVER	ver;
	BOOL	b_ok = TRUE;

	/* 1 us timer tick first: everything below times with it. */
	app_time_init();

	/* tk_ref_ver: ask the kernel who it is */
	tk_ref_ver(&ver);
	tm_printf((UB*)"\n=== IS19 robot car on micro T-Kernel, Pico W"
		  " (spec 0x%04x, product 0x%04x) ===\n", ver.spver, ver.prver);
#if TK_SUPPORT_SMP
	tm_printf((UB*)"dual-core (SMP) build, usermain on core %d\n",
		  tk_get_prc() - 1);	/* tk_get_prc() counts from 1 */
#else
	tm_printf((UB*)"single-core build\n");
#endif
	tm_printf((UB*)"config: hardware values are UNVALIDATED starting"
		  " values until measured (app_program/app_config.h)\n");

#if defined(APP_BENCH) && APP_BENCH
	/* Bench build (make BENCH=1): only Buddy 2's motor, encoder and
	   PID bench runs (subsystem2/motion_bench.c).  No other task
	   starts, so nothing else can steer or brake the car. */
	tm_printf((UB*)"BENCH build: only the Motion bench runs\n");
	CHECK(gpio_irq_init(), "gpio_irq_init");
	CHECK(motion_bench_start(), "motion_bench_start");
	tk_slp_tsk(TMO_FEVR);
	return 0;
#endif

	/* Phase 1: shared objects, then every subsystem's kernel objects */
	b_ok = app_init_ok(gpio_irq_init(), "gpio_irq_init") && b_ok;
	b_ok = app_init_ok(subsystem1_init(), "subsystem1_init") && b_ok;
	b_ok = app_init_ok(subsystem2_init(), "subsystem2_init") && b_ok;
	b_ok = app_init_ok(subsystem3_init(), "subsystem3_init") && b_ok;
	b_ok = app_init_ok(subsystem4_init(), "subsystem4_init") && b_ok;
	b_ok = app_init_ok(subsystem5_init(), "subsystem5_init") && b_ok;
	b_ok = app_init_ok(navigation_init(), "navigation_init") && b_ok;

	if (!b_ok) {
		/* A kernel object is missing: no task is started, so nothing
		   uses it and the motors are never driven (24 Oct review,
		   R-23).  The cause is printed above. */
		tm_printf((UB*)"init failed: no task started, the car stays"
			  " still\n");
		tk_slp_tsk(TMO_FEVR);
		return 0;
	}

	/* Phase 2: every subsystem starts its tasks.  Navigation, the only
	   task that commands the motors, starts last and only if every
	   other one started, so a missing task (Safety, say) can never
	   leave the car driving (24 Oct review, R-23). */
	b_ok = app_init_ok(subsystem1_start(), "subsystem1_start") && b_ok;
	b_ok = app_init_ok(subsystem2_start(), "subsystem2_start") && b_ok;
	b_ok = app_init_ok(subsystem3_start(), "subsystem3_start") && b_ok;
	b_ok = app_init_ok(subsystem4_start(), "subsystem4_start") && b_ok;
	b_ok = app_init_ok(subsystem5_start(), "subsystem5_start") && b_ok;

	if (b_ok) {
		CHECK(navigation_start(), "navigation_start");
		tm_printf((UB*)"all subsystems started\n\n");
	} else {
		tm_printf((UB*)"start failed: Navigation not started, the car"
			  " stays still\n");
	}

#if APP_TEST
	/* Test build (make TEST=1): run every subsystem's tests in turn,
	   here in the initial task, while the system is running.  Tests may
	   call tk_dly_tsk() to let the other tasks work for a while. */
	tm_printf((UB*)"[TEST] === test run start ===\n");
	subsystem1_test();
	subsystem2_test();
	subsystem3_test();
	subsystem4_test();
	subsystem5_test();
	navigation_test();
	tm_printf((UB*)"[TEST] === test run end ===\n");
#endif

	/* Sleep forever.  Returning from usermain() would shut down. */
	tk_slp_tsk(TMO_FEVR);
	return 0;
}
