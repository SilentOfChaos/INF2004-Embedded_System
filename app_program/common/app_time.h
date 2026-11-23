/*
 *  app_time.h
 *  Time stamps shared by all IS19 subsystems.
 *
 *  app_time_us() reads the RP2040's free-running 1 MHz timer (the same
 *  counter the kernel uses for knl_arch_time_us).  It is a single
 *  register read, so it is safe in interrupt handlers and on both cores.
 *  The 32-bit value wraps every 71.6 minutes; always subtract two time
 *  stamps as unsigned values to get an interval.
 *
 *  app_time_ms() is the kernel's operating time in milliseconds.
 */

#ifndef APP_TIME_H
#define APP_TIME_H

#include <stdint.h>

#include "hw_regs.h"

/* Microseconds since boot, modulo 2^32.  NFR2: 1 us resolution. */
static inline uint32_t
app_time_us (void)
{
    /* The address is a fixed RP2040 peripheral register. */
    return (*(uint32_t volatile *) HW_TIMER_TIMERAWL);
}

/* Start the 1 us time base; first thing in usermain(). */
void app_time_init(void);

/* Milliseconds since boot, modulo 2^32 (kernel operating time). */
uint32_t app_time_ms(void);

#endif /* APP_TIME_H */

/*** end of file ***/
