/*
 *  barcode_edge.h
 *  The Barcode ISR's edge record: one word in the Line task's message
 *  buffer, instead of 8 bytes (minimum-bit change, D-85).
 *    bits  0-30  time   app_time_us() modulo 2^31 (about 35.8 minutes)
 *    bit   31    dark   the level after the edge is "over a bar"
 *  The Line task empties the queue every 5 ms cycle (also in RUN OVER),
 *  so an edge normally waits less than one cycle, and it rebuilds the
 *  full 32-bit time from its own clock: barcode_edge_time() is exact for
 *  any wait under 2^31 us (35.8 minutes).  Read the clock after taking
 *  the edge from the queue, so the edge is never newer than the clock.
 *
 *  Pure C and inline (the ISR packs the record), so the host unit tests
 *  can use it.
 */

#ifndef BARCODE_EDGE_H
#define BARCODE_EDGE_H

#include <stdbool.h>
#include <stdint.h>

#include "../common/bitpack.h"

#define BARCODE_EDGE_TIME_SHIFT     0u
#define BARCODE_EDGE_TIME_BITS      31u
#define BARCODE_EDGE_DARK_BIT       31u

typedef struct
{
    uint32_t bits;
} barcode_edge_t;

/* The ISR's record of one edge. */
static inline barcode_edge_t
barcode_edge_pack (uint32_t time_us, bool b_dark)
{
    barcode_edge_t edge;

    edge.bits = 0u;
    (void) bitpack_set(&edge.bits, BARCODE_EDGE_TIME_SHIFT,
                       BARCODE_EDGE_TIME_BITS,
                       time_us & bitpack_mask(BARCODE_EDGE_TIME_BITS));
    bitpack_set_flag(&edge.bits, BARCODE_EDGE_DARK_BIT, b_dark);

    return (edge);
}

/* The level after the edge: true = over a bar. */
static inline bool
barcode_edge_dark (barcode_edge_t const * p_edge)
{
    return (bitpack_get_flag(p_edge->bits, BARCODE_EDGE_DARK_BIT));
}

/* The edge's full app_time_us() time, rebuilt from the clock now_us
   (read after the edge was taken from the queue). */
static inline uint32_t
barcode_edge_time (barcode_edge_t const * p_edge, uint32_t now_us)
{
    /* How long ago the edge happened, modulo 2^31: the dark bit drops
       out because 2^31 is a whole number of wraps. */
    uint32_t const age_us = bitpack_sub(now_us, p_edge->bits,
                                        BARCODE_EDGE_TIME_BITS);

    return (now_us - age_us);
}

#endif /* BARCODE_EDGE_H */

/*** end of file ***/
