/*
 *  ref_steering.h
 *  TEST ONLY.  A frozen copy of the steering calculation as it was
 *  before the minimum-bit change (app_program/subsystem3/steering.h at
 *  c644b68), renamed ref_*, for side-by-side tests (S3-U46).  Never used
 *  in the firmware.  Do not change it: it is the reference.
 */
#ifndef REF_STEERING_H
#define REF_STEERING_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int32_t prev_error_pm;      /* error of the previous call */
    bool    b_have_prev;        /* prev_error_pm is valid */
} ref_steering_t;

void ref_steering_reset(ref_steering_t * p_state);
void ref_steering_compute(ref_steering_t * p_state, int32_t error_pm,
                      int32_t base_mmps, int32_t * p_speed_mmps,
                      int32_t * p_diff_mmps);

#endif /* REF_STEERING_H */

/*** end of file ***/
