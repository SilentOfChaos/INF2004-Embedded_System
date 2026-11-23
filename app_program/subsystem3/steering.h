/*
 *  steering.h
 *  Buddy 3's steering calculation (report section 3: owned by Buddy 3,
 *  run by Navigation on the line error every 5 ms).
 *
 *  PD control on the line error.  Output is a forward speed and a wheel
 *  speed difference for Motion: left = speed - diff, right = speed + diff.
 *  A positive error (line under the left sensor) gives a positive diff,
 *  which turns the car left, back over the line.
 *
 *  The state is 16 bits (minimum-bit change, D-86):
 *    bits  0-10  previous error  -1000..1000 (two's complement)
 *    bit   11    have previous   the previous error is valid
 *    bits 12-15  0 (spare)
 *
 *  Pure C, no kernel headers: tested on the host (testing/host/).
 */

#ifndef STEERING_H
#define STEERING_H

#include <stdbool.h>
#include <stdint.h>

#define STEERING_PREV_SHIFT     0u
#define STEERING_PREV_BITS      11u
#define STEERING_HAVE_PREV_BIT  11u

typedef struct
{
    uint16_t bits;              /* layout above */
} steering_t;

void steering_reset(steering_t * p_state);
void steering_compute(steering_t * p_state, int32_t error_pm,
                      int32_t base_mmps, int32_t * p_speed_mmps,
                      int32_t * p_diff_mmps);

#endif /* STEERING_H */

/*** end of file ***/
