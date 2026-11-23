/*
 *  motion_ctl.c
 *  Buddy 2's motion maths.  See motion_ctl.h.
 *
 *  Geometry (wheel diameter D, track width T, CPR counts per wheel
 *  turn) comes from app_config.h, all UNVALIDATED until measured (A2).
 *  pi is taken as 355/113 (error 0.00001%).
 */

#include <stddef.h>

#include "../app_config.h"
#include "../common/bitpack.h"
#include "../common/fixmath.h"
#include "motion_ctl.h"

#define MOTION_PI_NUM           355
#define MOTION_PI_DEN           113
#define MOTION_MS_PER_S         1000
#define MOTION_US_PER_S         1000000
#define MOTION_SCALE            1000
#define MOTION_CDEG_HALF_TURN   18000
#define MOTION_CDEG_FULL_TURN   36000
#define MOTION_PERCENT          100
#define MS_SILENT_L_SHIFT       0u      /* stall watch */
#define MS_SILENT_R_SHIFT       10u
#define MS_SILENT_BITS          10u
#define MS_REPORTED_BIT         20u

/* mm per count, pi x D / CPR, is below 1/2: a count below 2^31 in size
   gives a distance below 2^30 mm, which MOTION_DISTANCE_BITS holds. */
BITPACK_ASSERT((2 * MOTION_PI_NUM * CFG_WHEEL_DIAMETER_MM) <
               (MOTION_PI_DEN * CFG_ENC_COUNTS_PER_REV), motion_mm_per_count);
BITPACK_ASSERT(31u == MOTION_DISTANCE_BITS, motion_distance_bits);
/* The stall watch's silent times fit their fields. */
BITPACK_ASSERT(BITPACK_BITS_FOR(CFG_MOTION_STALL_MS) <= MS_SILENT_BITS,
               ms_silent_bits);
BITPACK_ASSERT((MS_SILENT_L_SHIFT + MS_SILENT_BITS) <= MS_SILENT_R_SHIFT,
               ms_layout);
BITPACK_ASSERT((MS_SILENT_R_SHIFT + MS_SILENT_BITS) <= MS_REPORTED_BIT,
               ms_layout_flag);

static uint32_t motion_stall_time(uint32_t silent_ms, int32_t duty,
                                  int32_t delta, uint32_t dt_ms);

/* Forget the PID history (after a stop, or on Safety's reset request). */
void
motion_pid_reset (motion_pid_t * p_pid)
{
    if (NULL != p_pid)
    {
        p_pid->integral_x1000  = 0;
        p_pid->prev_error_mmps = 0;
        p_pid->b_have_prev     = false;
    }
}

/*
 * One PID step for one wheel.  Duty = feed-forward from the target speed
 * plus P, I and D on the speed error, limited to +/-MAX duty.  A zero
 * target gives zero duty (brake) and clears the history.
 */
int32_t
motion_pid_step (motion_pid_t * p_pid, int32_t target_mmps,
                 int32_t measured_mmps, uint32_t dt_ms)
{
    int32_t duty = 0;

    if ((NULL != p_pid) && (0 != target_mmps))
    {
        int32_t const error = target_mmps - measured_mmps;
        int32_t const change = p_pid->b_have_prev
                               ? (error - p_pid->prev_error_mmps) : 0;
        int32_t const limit = CFG_PID_I_LIMIT_PM * MOTION_SCALE;
        int32_t const ff = fixmath_div_round(
                               target_mmps * CFG_PID_FF_PM_PER_100MMPS,
                               MOTION_PERCENT);
        int32_t const p_term = fixmath_div_round(error * CFG_PID_KP_X1000,
                                                 MOTION_SCALE);
        int32_t const d_term = fixmath_div_round(change * CFG_PID_KD_X1000,
                                                 MOTION_SCALE);

        /* integral += Ki x error x dt; dt is 20 ms, so no overflow. */
        p_pid->integral_x1000 = fixmath_clamp(
            p_pid->integral_x1000 +
            fixmath_div_round(error * CFG_PID_KI_X1000 * (int32_t) dt_ms,
                              MOTION_MS_PER_S),
            -limit, limit);

        duty = ff + p_term + d_term +
               fixmath_div_round(p_pid->integral_x1000, MOTION_SCALE);
        duty = fixmath_clamp(duty, -CFG_MOTOR_MAX_DUTY_PM,
                             CFG_MOTOR_MAX_DUTY_PM);

        p_pid->prev_error_mmps = error;
        p_pid->b_have_prev     = true;
    }
    else
    {
        motion_pid_reset(p_pid);
    }

    return (duty);
}

/*
 * Soft start (D-30): a duty may grow by at most CFG_MOTOR_RAMP_PM per
 * 20 ms cycle.  Reductions and stops are applied at once so braking
 * distance is not lengthened.
 */
int32_t
motion_ramp (int32_t current_pm, int32_t wanted_pm)
{
    int32_t result = wanted_pm;
    bool const b_same_direction = (((current_pm >= 0) && (wanted_pm >= 0))
                                   || ((current_pm <= 0) &&
                                       (wanted_pm <= 0)));

    if (b_same_direction &&
        (fixmath_abs(wanted_pm) > fixmath_abs(current_pm)))
    {
        int32_t const step = (wanted_pm > 0) ? CFG_MOTOR_RAMP_PM
                                             : -CFG_MOTOR_RAMP_PM;

        if (fixmath_abs(wanted_pm - current_pm) > CFG_MOTOR_RAMP_PM)
        {
            result = current_pm + step;
        }
    }
    else if (!b_same_direction)
    {
        /* Reversing: stop first, then ramp up the other way. */
        result = 0;
    }
    else
    {
        /* Slowing down: at once. */
    }

    return (result);
}

/*
 * The PID's duty made safe to drive (24 Oct review, R-06).  Never
 * against the target's direction: to slow down the wheel gets less
 * drive, down to zero (brake), never reverse drive.  At least
 * CFG_MOTOR_MIN_DUTY_PM only while the wheel stands still, to break
 * static friction; once it turns, the PID may ask for less.  A zero
 * target gives zero.
 */
int32_t
motion_output_duty (int32_t pid_duty, int32_t target_mmps,
                    int32_t measured_mmps)
{
    int32_t duty = 0;

    if (0 != target_mmps)
    {
        bool const b_forward = (target_mmps > 0);
        int32_t    drive     = b_forward ? pid_duty : -pid_duty;

        if (drive < 0)
        {
            drive = 0;
        }

        if ((0 == measured_mmps) && (drive < CFG_MOTOR_MIN_DUTY_PM))
        {
            drive = CFG_MOTOR_MIN_DUTY_PM;
        }

        duty = b_forward ? drive : -drive;
    }

    return (duty);
}

/* Nothing silent, nothing reported. */
void
motion_stall_reset (motion_stall_t * p_watch)
{
    if (NULL != p_watch)
    {
        p_watch->bits = 0u;
    }
}

/*
 * One cycle of the stall and encoder-fault watch (motion_ctl.h).  The
 * duties are those driven while the counts delta_l and delta_r were
 * made, over dt_ms.  *p_wheels gets the wheels silent for
 * CFG_MOTION_STALL_MS (MOTION_STALL_LEFT, MOTION_STALL_RIGHT).  Returns
 * what to report: MOTION_STALL_NONE but once per stall.
 */
motion_stall_out_t
motion_stall_step (motion_stall_t * p_watch, int32_t duty_l, int32_t duty_r,
                   int32_t delta_l, int32_t delta_r, uint32_t dt_ms,
                   uint8_t * p_wheels)
{
    motion_stall_out_t out    = MOTION_STALL_NONE;
    uint8_t            wheels = 0u;

    if (NULL != p_watch)
    {
        uint32_t const silent_l = motion_stall_time(
                                      bitpack_get(p_watch->bits,
                                                  MS_SILENT_L_SHIFT,
                                                  MS_SILENT_BITS),
                                      duty_l, delta_l, dt_ms);
        uint32_t const silent_r = motion_stall_time(
                                      bitpack_get(p_watch->bits,
                                                  MS_SILENT_R_SHIFT,
                                                  MS_SILENT_BITS),
                                      duty_r, delta_r, dt_ms);
        bool           b_reported = bitpack_get_flag(p_watch->bits,
                                                     MS_REPORTED_BIT);
        uint32_t       word       = 0u;

        wheels = (uint8_t) (((silent_l >= CFG_MOTION_STALL_MS)
                             ? MOTION_STALL_LEFT : 0u) |
                            ((silent_r >= CFG_MOTION_STALL_MS)
                             ? MOTION_STALL_RIGHT : 0u));

        if (0u == wheels)
        {
            b_reported = false;     /* a new stall is reported again */
        }
        else if (!b_reported)
        {
            /* One wheel silent while the other counts: its encoder (or
               it is held, R5); otherwise a stall. */
            int32_t const other = (MOTION_STALL_LEFT == wheels) ? delta_r
                                                                : delta_l;
            bool const    b_one = (MOTION_STALL_LEFT == wheels) ||
                                  (MOTION_STALL_RIGHT == wheels);

            out = (b_one && (0 != other)) ? MOTION_STALL_ENCODER
                                          : MOTION_STALL_MOTOR;
            b_reported = true;
        }
        else
        {
            /* Already reported: Safety has latched it. */
        }

        /* Each time is at most CFG_MOTION_STALL_MS: it fits. */
        (void) bitpack_set(&word, MS_SILENT_L_SHIFT, MS_SILENT_BITS,
                           silent_l);
        (void) bitpack_set(&word, MS_SILENT_R_SHIFT, MS_SILENT_BITS,
                           silent_r);
        bitpack_set_flag(&word, MS_REPORTED_BIT, b_reported);
        p_watch->bits = word;
    }

    if (NULL != p_wheels)
    {
        *p_wheels = wheels;
    }

    return (out);
}

/* A wheel's silent time, ms (for tests and logs). */
uint32_t
motion_stall_silent_ms (motion_stall_t const * p_watch, bool b_right)
{
    return ((NULL != p_watch)
            ? bitpack_get(p_watch->bits,
                          b_right ? MS_SILENT_R_SHIFT : MS_SILENT_L_SHIFT,
                          MS_SILENT_BITS)
            : 0u);
}

/* One wheel's silent time after this cycle: grows while it is driven at
   CFG_MOTION_STALL_DUTY_PM or more with no count, up to
   CFG_MOTION_STALL_MS; any count or less drive starts it again. */
static uint32_t
motion_stall_time (uint32_t silent_ms, int32_t duty, int32_t delta,
                   uint32_t dt_ms)
{
    uint32_t result = 0u;

    if ((fixmath_abs(duty) >= CFG_MOTION_STALL_DUTY_PM) && (0 == delta))
    {
        result = (dt_ms >= (CFG_MOTION_STALL_MS - silent_ms))
                 ? CFG_MOTION_STALL_MS : (silent_ms + dt_ms);
    }

    return (result);
}

/* Distance in mm for a number of encoder counts: counts x pi x D / CPR. */
int32_t
motion_counts_to_mm (int32_t counts)
{
    int64_t const num = (int64_t) counts * MOTION_PI_NUM *
                        CFG_WHEEL_DIAMETER_MM;
    int64_t const den = (int64_t) MOTION_PI_DEN * CFG_ENC_COUNTS_PER_REV;

    /* |counts| < 2^31 and the result is a distance in mm: fits int32. */
    return ((int32_t) (num / den));
}

/* Encoder counts for a distance: mm x CPR / (pi x D). */
int32_t
motion_mm_to_counts (int32_t distance_mm)
{
    int64_t const num = (int64_t) distance_mm * MOTION_PI_DEN *
                        CFG_ENC_COUNTS_PER_REV;
    int64_t const den = (int64_t) MOTION_PI_NUM * CFG_WHEEL_DIAMETER_MM;

    /* Distances on the course are far below 2^31 counts. */
    return ((int32_t) (num / den));
}

/*
 * Counts each wheel travels in a pivot turn of angle_cdeg: the arc is
 * angle x pi x T / 360, and pi cancels with the wheel circumference:
 * counts = angle_cdeg x T x CPR / (36000 x D).
 */
int32_t
motion_turn_counts (int32_t angle_cdeg)
{
    int64_t const num = (int64_t) fixmath_abs(angle_cdeg) *
                        CFG_TRACK_WIDTH_MM * CFG_ENC_COUNTS_PER_REV;
    int64_t const den = (int64_t) MOTION_CDEG_FULL_TURN *
                        CFG_WHEEL_DIAMETER_MM;

    return ((int32_t) (num / den));     /* a few hundred counts */
}

/*
 * Wheel speed in mm/s from the counts read dt_us apart.  The real time
 * between the two reads, not the nominal period (24 Oct review, R-05):
 * a late or missed release does not change the speed.
 */
int32_t
motion_speed_mmps (int32_t delta_counts, uint32_t dt_us)
{
    int32_t speed = 0;

    if (0u != dt_us)
    {
        int64_t const num = (int64_t) delta_counts * MOTION_PI_NUM *
                            CFG_WHEEL_DIAMETER_MM * MOTION_US_PER_S;
        int64_t const den = (int64_t) MOTION_PI_DEN *
                            CFG_ENC_COUNTS_PER_REV * (int64_t) dt_us;

        speed = (int32_t) (num / den);  /* well inside int32 */
    }

    return (speed);
}

/*
 * Odometry heading from the two counters, (-18000, 18000] centi-degrees.
 * The angle turned is (right - left distance) / T radians; pi cancels:
 * heading_cdeg = (count_r - count_l) x 18000 x D / (CPR x T).
 */
int32_t
motion_heading_cdeg (int32_t count_l, int32_t count_r)
{
    int64_t const diff = (int64_t) count_r - (int64_t) count_l;
    int64_t const num  = diff * MOTION_CDEG_HALF_TURN * CFG_WHEEL_DIAMETER_MM;
    int64_t const den  = (int64_t) CFG_ENC_COUNTS_PER_REV *
                         CFG_TRACK_WIDTH_MM;
    int64_t const turns_cdeg = (num / den) % MOTION_CDEG_FULL_TURN;

    /* The remainder is below 36000, so it fits int32. */
    return (fixmath_wrap_cdeg((int32_t) turns_cdeg));
}

/*** end of file ***/
