/*
 *  scan_logic.c
 *  Buddy 5's scan geometry.  See scan_logic.h.
 *
 *  The obstacle is the run of neighbouring readings (by angle) around
 *  the closest return, while each reading is within CFG_SCAN_OBSTACLE_MM
 *  and close in range to its neighbour.  Returns outside that run (a
 *  wall, another object) set the clearance on their side (D-48, D-62).
 */

#include <stddef.h>

#include "../app_config.h"
#include "../common/fixmath.h"
#include "scan_logic.h"
#include "scan_msgs.h"

#define SCAN_STRAIGHT_DEG   90
#define SCAN_CDEG_PER_DEG   100
#define SCAN_MM_PER_CM      10
#define SCAN_HALF           2

static void scan_sorted(scan_set_t const * p_set, scan_set_t * p_out);
static void scan_point(scan_sample_t const * p_sample, int32_t * p_x,
                       int32_t * p_y);
static bool scan_is_hit(scan_sample_t const * p_sample);
static void scan_clearances(scan_set_t const * p_sorted, uint32_t first,
                            uint32_t last, scan_profile_t * p_out);

/* BYPASSING starts: nothing seen beside the car yet. */
void
scan_side_init (scan_side_t * p_side)
{
    if (NULL != p_side)
    {
        p_side->b_seen_beside = false;
        p_side->clear_count   = 0u;
    }
}

/*
 * One side-scan reading while bypassing.  Returns true when the obstacle
 * is behind: it was seen beside the car (a valid range within
 * CFG_SCAN_SIDE_CLEAR_MM), then CFG_SCAN_BEHIND_COUNT valid readings
 * were clear with no near reading between them.  An invalid reading
 * (missing, malformed or timed out) is neither: clearance cannot be
 * confirmed from it (report 6.5), so it changes nothing here; three in
 * a row end the scan with NO SAFE ROUTE in the Obstacle Scan task.
 */
bool
scan_side_step (scan_side_t * p_side, bool b_valid, int32_t range_mm)
{
    bool b_behind = false;

    if ((NULL == p_side) || !b_valid)
    {
        /* No state, or an invalid reading: nothing to learn (D-74). */
    }
    else if (range_mm <= CFG_SCAN_SIDE_CLEAR_MM)
    {
        p_side->b_seen_beside = true;
        p_side->clear_count   = 0u;
    }
    else if (p_side->b_seen_beside)
    {
        p_side->clear_count++;
        b_behind = (p_side->clear_count >= CFG_SCAN_BEHIND_COUNT);
    }
    else
    {
        /* Not level with the obstacle yet. */
    }

    return (b_behind);
}

/*
 * True when the set holds readings and every one of them is valid.  The
 * coarse scan reports the path clear only then (D-80): an invalid
 * reading is space that was never seen.
 */
bool
scan_all_valid (scan_set_t const * p_set)
{
    bool b_all = ((NULL != p_set) && (p_set->count > 0u));

    for (uint32_t idx = 0u; b_all && (idx < p_set->count); idx++)
    {
        b_all = p_set->samples[idx].b_valid;
    }

    return (b_all);
}

void
scan_set_clear (scan_set_t * p_set)
{
    if (NULL != p_set)
    {
        p_set->count = 0u;
    }
}

/* Add a reading; readings beyond SCAN_MAX_SAMPLES are dropped. */
void
scan_set_add (scan_set_t * p_set, int32_t angle_deg, int32_t range_mm,
              bool b_valid)
{
    if ((NULL != p_set) && (p_set->count < SCAN_MAX_SAMPLES))
    {
        p_set->samples[p_set->count].angle_deg = angle_deg;
        p_set->samples[p_set->count].range_mm  = range_mm;
        p_set->samples[p_set->count].b_valid   = b_valid;
        p_set->count++;
    }
}

/* Angular extent of the readings that hit something close. */
bool
scan_hit_range (scan_set_t const * p_set, int32_t * p_min_deg,
                int32_t * p_max_deg)
{
    bool b_hit = false;

    for (uint32_t idx = 0u; (NULL != p_set) && (idx < p_set->count); idx++)
    {
        scan_sample_t const * p_sample = &p_set->samples[idx];

        if (scan_is_hit(p_sample))
        {
            if (!b_hit || (p_sample->angle_deg < *p_min_deg))
            {
                *p_min_deg = p_sample->angle_deg;
            }

            if (!b_hit || (p_sample->angle_deg > *p_max_deg))
            {
                *p_max_deg = p_sample->angle_deg;
            }

            b_hit = true;
        }
    }

    return (b_hit);
}

/*
 * FINE SCAN angles: 5 degree steps outwards from the outermost coarse
 * hits, up to the next coarse angle, within the servo's range.
 */
uint32_t
scan_fine_angles (int32_t min_hit_deg, int32_t max_hit_deg,
                  int32_t * p_angles, uint32_t max_angles)
{
    uint32_t count = 0u;

    for (uint32_t step = 1u; step <= SCAN_FINE_PER_SIDE; step++)
    {
        /* step is at most 5, so the products are small. */
        int32_t const right = min_hit_deg -
                              ((int32_t) step * CFG_SCAN_FINE_STEP_DEG);
        int32_t const left  = max_hit_deg +
                              ((int32_t) step * CFG_SCAN_FINE_STEP_DEG);

        if ((right >= CFG_SCAN_SERVO_MIN_DEG) && (count < max_angles))
        {
            p_angles[count] = right;
            count++;
        }

        if ((left <= CFG_SCAN_SERVO_MAX_DEG) && (count < max_angles))
        {
            p_angles[count] = left;
            count++;
        }
    }

    return (count);
}

/* The obstacle profile and the side to pass on (FR10, FR11). */
void
scan_profile (scan_set_t const * p_set, scan_profile_t * p_out)
{
    scan_set_t sorted;
    uint32_t   closest = 0u;
    bool       b_found = false;

    p_out->b_found        = false;
    p_out->side           = OBST_SIDE_NONE;
    p_out->distance_mm    = CFG_US_MAX_MM;
    p_out->offset_mm      = 0;
    p_out->width_mm       = 0;
    p_out->closest_mm     = CFG_US_MAX_MM;
    p_out->edge_left_mm   = 0;
    p_out->edge_right_mm  = 0;
    p_out->clear_left_mm  = CFG_US_MAX_MM;
    p_out->clear_right_mm = CFG_US_MAX_MM;

    scan_sorted(p_set, &sorted);

    for (uint32_t idx = 0u; idx < sorted.count; idx++)
    {
        if (scan_is_hit(&sorted.samples[idx]) &&
            (!b_found ||
             (sorted.samples[idx].range_mm <
              sorted.samples[closest].range_mm)))
        {
            closest = idx;
            b_found = true;
        }
    }

    if (b_found)
    {
        uint32_t first = closest;
        uint32_t last  = closest;
        int32_t  x_pos = 0;
        int32_t  y_pos = 0;

        /* Grow the run while neighbours are close hits of similar range. */
        while ((first > 0u) && scan_is_hit(&sorted.samples[first - 1u]) &&
               (fixmath_abs(sorted.samples[first - 1u].range_mm -
                            sorted.samples[first].range_mm) <=
                CFG_SCAN_JUMP_MM))
        {
            first--;
        }

        while (((last + 1u) < sorted.count) &&
               scan_is_hit(&sorted.samples[last + 1u]) &&
               (fixmath_abs(sorted.samples[last + 1u].range_mm -
                            sorted.samples[last].range_mm) <=
                CFG_SCAN_JUMP_MM))
        {
            last++;
        }

        p_out->b_found       = true;
        p_out->closest_mm    = sorted.samples[closest].range_mm;
        scan_point(&sorted.samples[first], &x_pos, &y_pos);
        p_out->edge_right_mm = y_pos;
        scan_point(&sorted.samples[last], &x_pos, &y_pos);
        p_out->edge_left_mm  = y_pos;

        for (uint32_t idx = first; idx <= last; idx++)
        {
            scan_point(&sorted.samples[idx], &x_pos, &y_pos);
            p_out->distance_mm = (x_pos < p_out->distance_mm)
                                 ? x_pos : p_out->distance_mm;
        }

        p_out->width_mm  = p_out->edge_left_mm - p_out->edge_right_mm;
        p_out->offset_mm = (p_out->edge_left_mm + p_out->edge_right_mm) /
                           SCAN_HALF;
        scan_clearances(&sorted, first, last, p_out);
    }
}

/*
 * Range in mm from an echo width (HC-SR04 datasheet: cm = us / 58).
 * Too short a pulse is malformed (invalid); a pulse beyond the maximum
 * range means nothing is in range, a valid reading of the maximum.
 */
int32_t
scan_range_mm (uint32_t echo_width_us, bool * p_valid)
{
    uint32_t const min_width = ((uint32_t) CFG_US_MIN_MM *
                                (uint32_t) CFG_US_US_PER_CM) / SCAN_MM_PER_CM;
    int32_t        range = CFG_US_MAX_MM;

    *p_valid = (echo_width_us >= min_width);

    if (*p_valid)
    {
        /* Widths are at most tens of ms, so the product fits 32 bits. */
        uint32_t const mm = (echo_width_us * SCAN_MM_PER_CM) /
                            (uint32_t) CFG_US_US_PER_CM;

        range = (mm > (uint32_t) CFG_US_MAX_MM) ? CFG_US_MAX_MM
                                                : (int32_t) mm;
    }

    return (range);
}

/* Copy of the readings sorted by angle (insertion sort, <= 32). */
static void
scan_sorted (scan_set_t const * p_set, scan_set_t * p_out)
{
    p_out->count = 0u;

    for (uint32_t idx = 0u; (NULL != p_set) && (idx < p_set->count); idx++)
    {
        scan_sample_t const sample = p_set->samples[idx];
        uint32_t            pos    = p_out->count;

        while ((pos > 0u) &&
               (p_out->samples[pos - 1u].angle_deg > sample.angle_deg))
        {
            p_out->samples[pos] = p_out->samples[pos - 1u];
            pos--;
        }

        p_out->samples[pos] = sample;
        p_out->count++;
    }
}

/* Position of a reading in the car frame (from the sensor). */
static void
scan_point (scan_sample_t const * p_sample, int32_t * p_x, int32_t * p_y)
{
    int32_t const rel_cdeg = (p_sample->angle_deg - SCAN_STRAIGHT_DEG) *
                             SCAN_CDEG_PER_DEG;

    *p_x = fixmath_mul_q15(p_sample->range_mm, fixmath_cos_q15(rel_cdeg));
    *p_y = fixmath_mul_q15(p_sample->range_mm, fixmath_sin_q15(rel_cdeg));
}

static bool
scan_is_hit (scan_sample_t const * p_sample)
{
    return (p_sample->b_valid &&
            (p_sample->range_mm <= CFG_SCAN_OBSTACLE_MM));
}

/*
 * Clearance on each side: the nearest return outside the obstacle's run
 * that lies within CFG_SCAN_DEPTH_MM past the obstacle's front, measured
 * sideways from the obstacle's edge.  Nothing there = open (max range).
 * A side with no valid reading at all outside the run was never seen:
 * its clearance is SCAN_CLEAR_UNKNOWN and it has no room (report 6.5:
 * clearance cannot be confirmed from invalid readings; D-80).
 * Then the side with room for the car (width + passing gap, FR11) and
 * the most clearance is chosen.
 */
static void
scan_clearances (scan_set_t const * p_sorted, uint32_t first, uint32_t last,
                 scan_profile_t * p_out)
{
    int32_t const need = CFG_CAR_WIDTH_MM + CFG_NAV_PASS_GAP_MM;
    bool          b_left_seen  = false;
    bool          b_right_seen = false;

    for (uint32_t idx = 0u; idx < p_sorted->count; idx++)
    {
        scan_sample_t const * p_sample = &p_sorted->samples[idx];
        bool const            b_left   = (idx > last);
        int32_t               x_pos = 0;
        int32_t               y_pos = 0;

        if (((idx < first) || b_left) && p_sample->b_valid)
        {
            b_left_seen  = b_left_seen || b_left;
            b_right_seen = b_right_seen || !b_left;

            if (p_sample->range_mm < CFG_US_MAX_MM)
            {
                scan_point(p_sample, &x_pos, &y_pos);

                if (x_pos <= (p_out->distance_mm + CFG_SCAN_DEPTH_MM))
                {
                    if (b_left)
                    {
                        p_out->clear_left_mm = fixmath_clamp(
                            y_pos - p_out->edge_left_mm, 0,
                            p_out->clear_left_mm);
                    }
                    else
                    {
                        p_out->clear_right_mm = fixmath_clamp(
                            p_out->edge_right_mm - y_pos, 0,
                            p_out->clear_right_mm);
                    }
                }
            }
        }
    }

    p_out->clear_left_mm  = b_left_seen ? p_out->clear_left_mm
                                        : SCAN_CLEAR_UNKNOWN;
    p_out->clear_right_mm = b_right_seen ? p_out->clear_right_mm
                                         : SCAN_CLEAR_UNKNOWN;

    if ((p_out->clear_left_mm >= need) && (p_out->clear_right_mm >= need))
    {
        p_out->side = (p_out->clear_left_mm >= p_out->clear_right_mm)
                      ? OBST_SIDE_LEFT : OBST_SIDE_RIGHT;
    }
    else if (p_out->clear_left_mm >= need)
    {
        p_out->side = OBST_SIDE_LEFT;
    }
    else if (p_out->clear_right_mm >= need)
    {
        p_out->side = OBST_SIDE_RIGHT;
    }
    else
    {
        p_out->side = OBST_SIDE_NONE;
    }
}

/*** end of file ***/
