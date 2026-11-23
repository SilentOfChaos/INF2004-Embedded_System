/*
 *  ref_steering.c
 *  TEST ONLY: the steering calculation before the minimum-bit change,
 *  from app_program/subsystem3/steering.c at c644b68 with only its
 *  public names prefixed ref_ (see ref_steering.h).  Do not change it.
 */
#include <stddef.h>

#include "app_config.h"
#include "common/fixmath.h"
#include "ref_steering.h"

#define STEERING_FULL_PM        1000

/* Forget the previous error (after a stop, a turn or a search). */
void
ref_steering_reset (ref_steering_t * p_state)
{
    if (NULL != p_state)
    {
        p_state->prev_error_pm = 0;
        p_state->b_have_prev   = false;
    }
}

/*
 * One steering step.  The speed difference is Kp x error plus Kd x the
 * change of error since the last call, limited to +/-MAX_DIFF.  The
 * forward speed drops by up to SLOWDOWN per mille at full error so the
 * car slows on sharp curves.
 */
void
ref_steering_compute (ref_steering_t * p_state, int32_t error_pm,
                  int32_t base_mmps, int32_t * p_speed_mmps,
                  int32_t * p_diff_mmps)
{
    if ((NULL != p_state) && (NULL != p_speed_mmps) &&
        (NULL != p_diff_mmps))
    {
        int32_t const error = fixmath_clamp(error_pm, -STEERING_FULL_PM,
                                            STEERING_FULL_PM);
        int32_t const change = p_state->b_have_prev
                               ? (error - p_state->prev_error_pm) : 0;
        int32_t const p_term = fixmath_div_round(error * CFG_STEER_KP_MMPS,
                                                 STEERING_FULL_PM);
        int32_t const d_term = fixmath_div_round(change * CFG_STEER_KD_X1000,
                                                 STEERING_FULL_PM);
        int32_t const cut    = fixmath_div_round(
                                   fixmath_abs(error) * CFG_STEER_SLOWDOWN_PM,
                                   STEERING_FULL_PM);
        int32_t const speed  = base_mmps -
                               fixmath_div_round(base_mmps * cut,
                                                 STEERING_FULL_PM);

        *p_diff_mmps  = fixmath_clamp(p_term + d_term,
                                      -CFG_STEER_MAX_DIFF_MMPS,
                                      CFG_STEER_MAX_DIFF_MMPS);
        *p_speed_mmps = (speed > 0) ? speed : 0;

        p_state->prev_error_pm = error;
        p_state->b_have_prev   = true;
    }
}

/*** end of file ***/
