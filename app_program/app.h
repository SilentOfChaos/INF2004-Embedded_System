/*
 *	app.h
 *	Definitions shared by the whole team.  Changed only by team agreement.
 *
 *	Anything that belongs to ONE subsystem goes in that subsystem's own
 *	header instead (app_program/subsystemN/subsystemN.h).
 *
 *	IS19: every tunable value lives in app_config.h; task priorities,
 *	periods and cores follow the report's task table (TEAM.md section 4).
 */

#ifndef APP_H
#define APP_H

#include <stdbool.h>		/* bool (Barr-C 5.6) */
#include <stdint.h>		/* fixed-width integers (Barr-C 5.2) */

#include <tk/tkernel.h>		/* micro T-Kernel API (tk_xxx_yyy) */
#include <tm/tmonitor.h>	/* T-Monitor API: tm_printf() console output */
#include <bsp/libbsp.h>		/* Board helpers: gpio_set_pin/val, pwm_... */

#include "app_config.h"		/* IS19 central configuration */

/* ---------------------------------------------------------------------
 *  Pins: free for projects are GP0..GP22 and GP26..GP28.  GP0/GP1 are
 *  taken by UART0 when built with "make CRASH_REPORT=1" (see README).
 *  Never use GP23/24/25/29: they are wired to the Pico W radio.
 *  Claim every pin in TEAM.md section 2 before using it.
 *  IS19 pin numbers: app_config.h section 2.
 * --------------------------------------------------------------------- */

/* ---------------------------------------------------------------------
 *  Task priorities: 1 is the highest; the kernel's USB console (3) and
 *  WiFi (4) tasks sit above every application task.  IS19 uses its own
 *  order 5-12 from the report (TEAM.md section 4, app_config.h).
 * --------------------------------------------------------------------- */
#define TASK_STACK_SIZE		2048	/* bytes; tm_printf needs ~1 KB */

/* Cores for app_start_task(): the report pins every task to one core. */
#define APP_CORE0		TP_PRC1
#define APP_CORE1		TP_PRC2

/* Create and start a task.  prc = TP_PRC1 (core 0), TP_PRC2 (core 1), or
   0 for "any core".  Defined in app_main.c. */
IMPORT ID app_start_task(FP entry, PRI pri, UW prc, const char *name);

/* Same, with an explicit stack size in bytes. */
IMPORT ID app_start_task_stack(FP entry, PRI pri, UW prc, SZ stksz,
			       const char *name);

/* ---------------------------------------------------------------------
 *  Every subsystem provides these.  usermain() calls ALL the _init()
 *  functions first and only then ALL the _start() functions, so every
 *  kernel object exists before any task can use it.
 *    subsystemN_init()   create this subsystem's kernel objects
 *    subsystemN_start()  create and start this subsystem's tasks
 *    subsystemN_test()   on-target tests, only in test builds (make TEST=1)
 *  The team-shared Navigation task follows the same pattern.
 * --------------------------------------------------------------------- */
IMPORT ER subsystem1_init(void);
IMPORT ER subsystem2_init(void);
IMPORT ER subsystem3_init(void);
IMPORT ER subsystem4_init(void);
IMPORT ER subsystem5_init(void);
IMPORT ER navigation_init(void);

IMPORT ER subsystem1_start(void);
IMPORT ER subsystem2_start(void);
IMPORT ER subsystem3_start(void);
IMPORT ER subsystem4_start(void);
IMPORT ER subsystem5_start(void);
IMPORT ER navigation_start(void);

#if APP_TEST
IMPORT void subsystem1_test(void);
IMPORT void subsystem2_test(void);
IMPORT void subsystem3_test(void);
IMPORT void subsystem4_test(void);
IMPORT void subsystem5_test(void);
IMPORT void navigation_test(void);
#endif

/* Print a readable message if a kernel call failed (negative error code) */
#define CHECK(er, what)							\
	do {								\
		INT check_er_ = (INT)(er);	/* evaluate once */	\
		if (check_er_ < E_OK) {					\
			tm_printf((UB*)"ERROR %s: %d\n", (what), check_er_); \
		}							\
	} while (0)

#endif /* APP_H */
