/*
 *  ultrasonic_echo.h
 *  The Echo ISR's record of one echo pulse: two words in the Obstacle
 *  Scan task's message buffer mbf_echo (minimum-bit change, D-90).
 *    word 0          rise time  app_time_us() at the rising edge
 *    word 1  bits  0-14  width  pulse width in us, 0..32767
 *            bits 15-31  0 (spare)
 *  Every width from US_ECHO_FULL_RANGE_US (23200 us, CFG_US_MAX_MM) up
 *  reads as 4 m, so 15 bits are enough: a longer width is kept at 32767
 *  and gives the same range (S5-U42).  The rise time keeps 32 bits: the
 *  task compares it with the trigger time to drop echoes that rose
 *  before its ping, and a sensor stuck high can make that echo
 *  arbitrarily old.
 *
 *  Pure C and inline (the ISR packs the record), so the host unit tests
 *  can use it.
 */

#ifndef ULTRASONIC_ECHO_H
#define ULTRASONIC_ECHO_H

#include <stdbool.h>
#include <stdint.h>

#include "../app_config.h"
#include "../common/bitpack.h"

#define US_ECHO_MM_PER_CM       10u

/* The shortest width that reads as CFG_US_MAX_MM (scan_range_mm()). */
#define US_ECHO_FULL_RANGE_US   (((uint32_t) CFG_US_MAX_MM *            \
                                  (uint32_t) CFG_US_US_PER_CM) /        \
                                 US_ECHO_MM_PER_CM)

#define US_ECHO_WIDTH_SHIFT     0u
#define US_ECHO_WIDTH_BITS      BITPACK_BITS_FOR(US_ECHO_FULL_RANGE_US)

typedef struct
{
    uint32_t rise_us;       /* echo start, app_time_us() */
    uint32_t bits;          /* the width, layout above */
} ultrasonic_echo_t;

/* The ISR's record of one pulse.  A width too long for the field is
   kept at the largest that fits, which reads as the same range. */
static inline ultrasonic_echo_t
ultrasonic_echo_pack (uint32_t rise_us, uint32_t width_us)
{
    ultrasonic_echo_t echo;

    echo.rise_us = rise_us;
    echo.bits    = 0u;
    (void) bitpack_set(&echo.bits, US_ECHO_WIDTH_SHIFT, US_ECHO_WIDTH_BITS,
                       width_us);

    return (echo);
}

/* The pulse width in us, 0..32767. */
static inline uint32_t
ultrasonic_echo_width_us (ultrasonic_echo_t const * p_echo)
{
    return (bitpack_get(p_echo->bits, US_ECHO_WIDTH_SHIFT,
                        US_ECHO_WIDTH_BITS));
}

#endif /* ULTRASONIC_ECHO_H */

/*** end of file ***/
