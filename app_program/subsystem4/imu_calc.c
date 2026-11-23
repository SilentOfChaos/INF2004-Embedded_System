/*
 *  imu_calc.c
 *  Buddy 4's IMU maths.  See imu_calc.h.
 *
 *  Tilt-compensated heading (car frame x forward, y left, z up):
 *    roll  = atan2(ay, az)
 *    pitch = atan2(-ax, sqrt(ay^2 + az^2))
 *    Xh = mx cos(pitch) + (my sin(roll) + mz cos(roll)) sin(pitch)
 *    Yh = my cos(roll) - mz sin(roll)
 *    heading = atan2(-Yh, Xh), counter-clockwise from magnetic north
 *  Checked numerically against rotated test vectors (host test S4-U02).
 */

#include <stddef.h>

#include "../common/fixmath.h"
#include "imu_calc.h"

#define IMU_FULL_SCALE_MG   4000    /* +/-4 g range */
#define IMU_ONE_G_MG        1000    /* gravity, at rest */

static int32_t const g_hump_cdeg[CFG_IMU_HUMP_TABLE_LEN] =
    CFG_IMU_HUMP_TABLE_CDEG;
static int32_t const g_hump_mm[CFG_IMU_HUMP_TABLE_LEN] =
    CFG_IMU_HUMP_TABLE_MM;

/* Roll and pitch from the gravity reaction measured at rest. */
void
imu_calc_tilt (int32_t ax_mg, int32_t ay_mg, int32_t az_mg,
               int32_t * p_roll_cdeg, int32_t * p_pitch_cdeg)
{
    /* Each axis is within +/-4000 mg, so the squares fit in 32 bits. */
    uint32_t const yz = fixmath_isqrt((uint32_t) ((ay_mg * ay_mg) +
                                                  (az_mg * az_mg)));

    *p_roll_cdeg  = fixmath_atan2_cdeg(ay_mg, az_mg);
    *p_pitch_cdeg = fixmath_atan2_cdeg(-ax_mg, (int32_t) yz);
}

/* Tilt-compensated heading in centi-degrees, (-18000, 18000]. */
int32_t
imu_calc_heading (int32_t mx, int32_t my, int32_t mz, int32_t roll_cdeg,
                  int32_t pitch_cdeg)
{
    int32_t const sin_r = fixmath_sin_q15(roll_cdeg);
    int32_t const cos_r = fixmath_cos_q15(roll_cdeg);
    int32_t const sin_p = fixmath_sin_q15(pitch_cdeg);
    int32_t const cos_p = fixmath_cos_q15(pitch_cdeg);
    int32_t const tilt  = fixmath_mul_q15(my, sin_r) +
                          fixmath_mul_q15(mz, cos_r);
    int32_t const x_h   = fixmath_mul_q15(mx, cos_p) +
                          fixmath_mul_q15(tilt, sin_p);
    int32_t const y_h   = fixmath_mul_q15(my, cos_r) -
                          fixmath_mul_q15(mz, sin_r);

    return (fixmath_atan2_cdeg(-y_h, x_h));
}

/*
 * Sort one accelerometer sample (report 6.4, G17 decided 16 Oct, D-81):
 * a failed read or a total below CFG_IMU_MIN_G_MG is invalid data, a
 * total above CFG_IMU_MAX_G_MG is a jolt beyond the plausible range (the
 * +/-4 g range saturates in a hard collision), anything else is used.
 */
imu_sample_t
imu_calc_check_sample (bool b_read_ok, uint32_t total_mg)
{
    imu_sample_t result = IMU_SAMPLE_OK;

    if (!b_read_ok || (total_mg < (uint32_t) CFG_IMU_MIN_G_MG))
    {
        result = IMU_SAMPLE_BAD;
    }
    else if (total_mg > (uint32_t) CFG_IMU_MAX_G_MG)
    {
        result = IMU_SAMPLE_OVERRANGE;
    }
    else
    {
        /* Plausible. */
    }

    return (result);
}

/*
 * CALIBRATING expects the car at rest, |a| = 1 g.  A sample that differs
 * from 1 g by at least the collision threshold (CFG_IMU_IMPACT_MG) is a
 * jolt: calibration starts again, without a fault (D-81).
 */
bool
imu_calc_cal_jolt (uint32_t total_mg)
{
    /* |a| is at most 4000 mg x sqrt(3) (norm of a clamped reading), so
       it fits int32. */
    int32_t const diff = (int32_t) total_mg - IMU_ONE_G_MG;

    return (fixmath_abs(diff) >= CFG_IMU_IMPACT_MG);
}

/* Length of a 3-axis vector (mg), inputs within the sensor range. */
uint32_t
imu_calc_norm (int32_t x_value, int32_t y_value, int32_t z_value)
{
    int32_t const xx = fixmath_clamp(x_value, -IMU_FULL_SCALE_MG,
                                     IMU_FULL_SCALE_MG);
    int32_t const yy = fixmath_clamp(y_value, -IMU_FULL_SCALE_MG,
                                     IMU_FULL_SCALE_MG);
    int32_t const zz = fixmath_clamp(z_value, -IMU_FULL_SCALE_MG,
                                     IMU_FULL_SCALE_MG);

    /* 3 x 4000^2 = 4.8e7 fits in 32 bits. */
    return (fixmath_isqrt((uint32_t) ((xx * xx) + (yy * yy) + (zz * zz))));
}

/* Hump height from the peak roll, by the calibration table (A4). */
int32_t
imu_calc_hump_height (int32_t peak_roll_cdeg)
{
    return (fixmath_interp(fixmath_abs(peak_roll_cdeg), g_hump_cdeg,
                           g_hump_mm, CFG_IMU_HUMP_TABLE_LEN));
}

void
imu_hump_init (imu_hump_t * p_hump)
{
    if (NULL != p_hump)
    {
        p_hump->b_on_hump = false;
        p_hump->above_ms  = 0u;
        p_hump->peak_cdeg = 0;
    }
}

/*
 * One sample of the hump detector.  ON HUMP needs |roll| above the ON
 * threshold for CFG_IMU_HUMP_MIN_MS (filters single jolts); it ends when
 * |roll| falls to the OFF threshold, reporting the peak.
 */
imu_hump_evt_t
imu_hump_step (imu_hump_t * p_hump, int32_t roll_cdeg, uint32_t dt_ms)
{
    imu_hump_evt_t event = IMU_HUMP_EVT_NONE;
    int32_t const  roll  = fixmath_abs(roll_cdeg);

    if (NULL == p_hump)
    {
        /* No state: no event. */
    }
    else if (!p_hump->b_on_hump)
    {
        if (roll >= CFG_IMU_HUMP_ON_CDEG)
        {
            p_hump->above_ms += dt_ms;

            if (p_hump->above_ms >= CFG_IMU_HUMP_MIN_MS)
            {
                p_hump->b_on_hump = true;
                p_hump->peak_cdeg = roll;
                event = IMU_HUMP_EVT_STARTED;
            }
        }
        else
        {
            p_hump->above_ms = 0u;
        }
    }
    else
    {
        if (roll > p_hump->peak_cdeg)
        {
            p_hump->peak_cdeg = roll;
        }

        if (roll <= CFG_IMU_HUMP_OFF_CDEG)
        {
            p_hump->b_on_hump = false;
            p_hump->above_ms  = 0u;
            event = IMU_HUMP_EVT_ENDED;
        }
    }

    return (event);
}

void
imu_classifier_init (imu_classifier_t * p_cls)
{
    if (NULL != p_cls)
    {
        for (uint32_t idx = 0u; idx < CFG_IMU_CLASS_WINDOW; idx++)
        {
            p_cls->total_mg[idx]   = 0;
            p_cls->forward_mg[idx] = 0;
            p_cls->roll_cdeg[idx]  = 0;
        }

        p_cls->index = 0u;
        p_cls->count = 0u;
    }
}

/*
 * Motion class for this sample (FR17), in priority order: sudden impact,
 * climbing / descending a hump, turning, accelerating, stationary, and
 * otherwise steady driving (D-58).  forward and lateral are the
 * accelerations with the resting offsets removed.
 */
imu_class_t
imu_classifier_step (imu_classifier_t * p_cls, int32_t forward_mg,
                     int32_t lateral_mg, int32_t total_mg,
                     int32_t turn_rate_cdps, int32_t roll_cdeg,
                     bool b_on_hump)
{
    imu_class_t result = IMU_CLASS_STEADY;

    if (NULL != p_cls)
    {
        int32_t  mean_total   = 0;
        int32_t  mean_forward = 0;
        int32_t  deviation    = 0;
        int32_t  oldest_roll  = 0;
        uint32_t oldest       = 0u;
        int32_t  count        = 0;

        p_cls->total_mg[p_cls->index]   = total_mg;
        p_cls->forward_mg[p_cls->index] = forward_mg;
        p_cls->roll_cdeg[p_cls->index]  = roll_cdeg;
        p_cls->index = (p_cls->index + 1u) % CFG_IMU_CLASS_WINDOW;

        if (p_cls->count < CFG_IMU_CLASS_WINDOW)
        {
            p_cls->count++;
        }

        /* Oldest sample in the window: the next slot once it is full. */
        oldest = (p_cls->count < CFG_IMU_CLASS_WINDOW) ? 0u : p_cls->index;
        oldest_roll = p_cls->roll_cdeg[oldest];
        count = (int32_t) p_cls->count;     /* at most the window size */

        for (uint32_t idx = 0u; idx < p_cls->count; idx++)
        {
            mean_total   += p_cls->total_mg[idx];
            mean_forward += p_cls->forward_mg[idx];
        }

        mean_total   = mean_total / count;
        mean_forward = mean_forward / count;

        for (uint32_t idx = 0u; idx < p_cls->count; idx++)
        {
            deviation += fixmath_abs(p_cls->total_mg[idx] - mean_total);
        }

        deviation = deviation / count;

        if (imu_calc_norm(forward_mg, lateral_mg, 0) >=
            (uint32_t) CFG_IMU_IMPACT_MG)
        {
            result = IMU_CLASS_IMPACT;
        }
        else if (b_on_hump)
        {
            /* Roll growing over the window = climbing. */
            result = (fixmath_abs(roll_cdeg) >= fixmath_abs(oldest_roll))
                     ? IMU_CLASS_CLIMBING : IMU_CLASS_DESCENDING;
        }
        else if (fixmath_abs(turn_rate_cdps) >= CFG_IMU_TURN_CDPS)
        {
            result = IMU_CLASS_TURNING;
        }
        else if (fixmath_abs(mean_forward) >= CFG_IMU_ACCEL_MG)
        {
            result = IMU_CLASS_ACCELERATING;
        }
        else if (deviation <= CFG_IMU_STILL_MG)
        {
            result = IMU_CLASS_STATIONARY;
        }
        else
        {
            result = IMU_CLASS_STEADY;
        }
    }

    return (result);
}

/*
 * Lift / overturn detector.  The calibration mean is the gravity reaction
 * with the car upright on the floor ("up").  Each sample is projected on
 * it: the projection minus |up| is the acceleration along up (positive
 * while the car is being lifted), and projection / |a| is the cosine of
 * the tilt from upright.
 */
void
imu_lift_init (imu_lift_t * p_lift, int32_t up_x_mg, int32_t up_y_mg,
               int32_t up_z_mg)
{
    if (NULL != p_lift)
    {
        p_lift->up_x_mg    = up_x_mg;
        p_lift->up_y_mg    = up_y_mg;
        p_lift->up_z_mg    = up_z_mg;
        p_lift->up_mg      = (int32_t) imu_calc_norm(up_x_mg, up_y_mg,
                                                     up_z_mg);
        p_lift->lift_run   = 0u;
        p_lift->flip_run   = 0u;
        p_lift->last_up_mg = 0;
    }
}

imu_lift_evt_t
imu_lift_step (imu_lift_t * p_lift, int32_t ax_mg, int32_t ay_mg,
               int32_t az_mg)
{
    imu_lift_evt_t event = IMU_LIFT_NONE;

    if ((NULL != p_lift) && (p_lift->up_mg > 0))
    {
        int64_t const dot = ((int64_t) ax_mg * p_lift->up_x_mg) +
                            ((int64_t) ay_mg * p_lift->up_y_mg) +
                            ((int64_t) az_mg * p_lift->up_z_mg);
        int64_t const total = (int64_t) imu_calc_norm(ax_mg, ay_mg, az_mg);
        int32_t const along = (int32_t) (dot / p_lift->up_mg);
        bool const    b_up  = ((along - p_lift->up_mg) >= CFG_IMU_LIFT_MG);
        /* cos(tilt) = dot / (|a| |up|) < COS_PM / 1000 */
        bool const    b_flip = ((dot * 1000) <
                                ((int64_t) CFG_IMU_OVERTURN_COS_PM * total *
                                 p_lift->up_mg));

        p_lift->last_up_mg = along - p_lift->up_mg;
        p_lift->lift_run   = b_up ? (p_lift->lift_run + 1u) : 0u;
        p_lift->flip_run   = b_flip ? (p_lift->flip_run + 1u) : 0u;

        if (p_lift->flip_run >= CFG_IMU_OVERTURN_SAMPLES)
        {
            p_lift->flip_run = CFG_IMU_OVERTURN_SAMPLES;    /* no wrap */
            event = IMU_LIFT_OVERTURNED;
        }
        else if (p_lift->lift_run >= CFG_IMU_LIFT_SAMPLES)
        {
            p_lift->lift_run = CFG_IMU_LIFT_SAMPLES;
            event = IMU_LIFT_LIFTED;
        }
        else
        {
            /* Upright and not being lifted (yet). */
        }
    }

    return (event);
}

/* True when neither condition is building up: the cause has cleared. */
bool
imu_lift_clear (imu_lift_t const * p_lift)
{
    return ((NULL == p_lift) ||
            ((0u == p_lift->lift_run) && (0u == p_lift->flip_run)));
}

/*** end of file ***/
