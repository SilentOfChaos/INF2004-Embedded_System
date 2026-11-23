/*
 *  steering.c
 *  Buddy 3's steering calculation: PD on the line error.
 *  See steering.h for the sign convention.
 */

#include <stddef.h>

#include "../app_config.h"
#include "../common/bitpack.h"
#include "../common/fixmath.h"
#include "steering.h"

#define STEERING_FULL_PM        1000

/* The layout (steering.h) holds -1000..1000 and the flag in 16 bits. */
BITPACK_ASSERT(BITPACK_BITS_FOR_SIGNED(STEERING_FULL_PM) ==
               STEERING_PREV_BITS, steering_prev_bits);
BITPACK_ASSERT((STEERING_PREV_SHIFT + STEERING_PREV_BITS) ==
               STEERING_HAVE_PREV_BIT, steering_layout);
BITPACK_ASSERT(STEERING_HAVE_PREV_BIT < 16u, steering_fits_16_bits);

/* Forget the previous error (after a stop, a turn or a search). */
void
steering_reset (steering_t * p_state)
{
    if (NULL != p_state)
    {
        p_state->bits = 0u;     /* previous error 0, not valid */
    }
}

/*
 * One steering step.  The speed difference is Kp x error plus Kd x the
 * change of error since the last call, limited to +/-MAX_DIFF.  The
 * forward speed drops by up to SLOWDOWN per mille at full error so the
 * car slows on sharp curves.
 */
void
steering_compute (steering_t * p_state, int32_t error_pm,
                  int32_t base_mmps, int32_t * p_speed_mmps,
                  int32_t * p_diff_mmps)
{
    if ((NULL != p_state) && (NULL != p_speed_mmps) &&
        (NULL != p_diff_mmps))
    {
        uint32_t      state  = (uint32_t) p_state->bits;
        bool const    b_have_prev = bitpack_get_flag(state,
                                                     STEERING_HAVE_PREV_BIT);
        int32_t const prev   = bitpack_get_signed(state, STEERING_PREV_SHIFT,
                                                  STEERING_PREV_BITS);
        int32_t const error  = fixmath_clamp(error_pm, -STEERING_FULL_PM,
                                             STEERING_FULL_PM);
        int32_t const change = b_have_prev ? (error - prev) : 0;
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

        /* The error was clamped to -1000..1000 above: it fits. */
        (void) bitpack_set_signed(&state, STEERING_PREV_SHIFT,
                                  STEERING_PREV_BITS, error);
        bitpack_set_flag(&state, STEERING_HAVE_PREV_BIT, true);
        p_state->bits = (uint16_t) state;   /* 12 bits used */
    }
}

/*** end of file ***/
