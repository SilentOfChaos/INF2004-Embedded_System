/*
 *  drv_encoder.h
 *  Quadrature wheel encoders: left A/B on GP0/GP1 (Grove 1), right A/B
 *  on GP16/GP17 (Grove 4) (report section 2).  The Encoder ISR runs on
 *  core 0 and only counts (report section 4).
 *
 *  The two counters are one of the two shared variables the report
 *  justifies: an encoder can tick hundreds of times a second, too fast
 *  to queue each tick.
 *
 *  Minimum-bit change (D-88, Tribunal 1 "Version B"): the shared pair is
 *  one 32-bit word holding two 16-bit counters that wrap.  Each Encoder
 *  ISR changes only its own half, with a 16-bit store; Motion reads both
 *  halves in one 32-bit load, so the pair is always consistent without
 *  masking interrupts, and adds each half's signed 16-bit change since
 *  its last read to its own 32-bit totals (encoder_add_counts(), through
 *  encoder_add_pair()).  That change is exact below 32768 counts between
 *  two reads.  Motion reads every 20 ms (a wheel at 2 m/s gives about
 *  100 counts in 20 ms); if it stops, its 100 ms check-in and the 200 ms
 *  watchdog reset the chip within about 0.3 s (about 1,600 counts at
 *  2 m/s), once the watchdog is armed (radio up, or 15 s after boot).
 *  Before that, reaching 32768 counts would take Motion not running for
 *  6.3 s at 2 m/s (1.6 s with CFG_ENC_QUADRATURE_X4 1).  The whole word
 *  must never be added to (a carry would change the other wheel); only
 *  encoder_count_step() writes a half.
 *
 *  Pure C header (the helpers are inline), so the host tests run the
 *  same code as the car.
 */

#ifndef DRV_ENCODER_H
#define DRV_ENCODER_H

#include <stdint.h>

#include "../common/bitpack.h"

#define ENCODER_LEFT        0u      /* half[0] */
#define ENCODER_RIGHT       1u      /* half[1] */
#define ENCODER_HALF_BITS   16u

/* The shared pair.  word is read in one load; each half is written by
   its own wheel's ISR only. */
typedef union
{
    uint32_t word;
    uint16_t half[2];
} encoder_counts_t;

/* ISR side: count step (+1 or -1) on one wheel's half, modulo 2^16.
   A 16-bit read and store of that half only. */
static inline void
encoder_count_step (encoder_counts_t volatile * p_counts, uint32_t wheel,
                    int32_t step)
{
    /* Converting -1 to uint16_t gives 0xFFFF: the sum wraps as intended. */
    p_counts->half[wheel] = (uint16_t) (p_counts->half[wheel] +
                                        (uint16_t) step);
}

/* Task side: the signed change of one wheel between two copies of the
   pair (each copied from the shared word in one load). */
static inline int32_t
encoder_count_change (encoder_counts_t const * p_now,
                      encoder_counts_t const * p_last, uint32_t wheel)
{
    return (bitpack_sub_signed((uint32_t) p_now->half[wheel],
                               (uint32_t) p_last->half[wheel],
                               ENCODER_HALF_BITS));
}

/*
 * Task side: add each wheel's change since *p_last to the 32-bit totals
 * and keep the copy for the next call.  The pair is copied from the
 * shared word in one 32-bit load, so both halves are from the same
 * moment.  encoder_add_counts() calls this with the driver's own pair;
 * the host tests call it with theirs (S2-U42).
 */
static inline void
encoder_add_pair (encoder_counts_t const volatile * p_counts,
                  encoder_counts_t * p_last, int32_t * p_total_l,
                  int32_t * p_total_r)
{
    encoder_counts_t now;

    now.word = p_counts->word;          /* one load: both halves */
    *p_total_l += encoder_count_change(&now, p_last, ENCODER_LEFT);
    *p_total_r += encoder_count_change(&now, p_last, ENCODER_RIGHT);
    *p_last = now;
}

void encoder_init(void);
void encoder_add_counts(int32_t * p_total_l, int32_t * p_total_r);

#endif /* DRV_ENCODER_H */

/*** end of file ***/
