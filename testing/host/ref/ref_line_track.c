/*
 *  ref_line_track.c
 *  TEST ONLY: line tracking before the minimum-bit change, from
 *  app_program/subsystem3/line_track.c at c644b68 with only its public
 *  names prefixed ref_ (see ref_line_track.h).  Do not change it.
 */
#include <stddef.h>

#include "app_config.h"
#include "common/fixmath.h"
#include "ref_line_track.h"

#define LINE_FULL_PM        1000

static void ref_line_track_adapt(int32_t raw, int32_t dark_pm,
                             int32_t * p_white, int32_t * p_black,
                             int32_t stored_white, int32_t stored_black);
static int32_t ref_line_track_step(int32_t level, int32_t raw);

/* LOAD LEVELS: start from the black and white levels stored in the lab. */
void
ref_line_track_init (ref_line_track_t * p_track)
{
    if (NULL != p_track)
    {
        p_track->white_l          = CFG_LINE_L_WHITE;
        p_track->black_l          = CFG_LINE_L_BLACK;
        p_track->white_r          = CFG_LINE_R_WHITE;
        p_track->black_r          = CFG_LINE_R_BLACK;
        p_track->error_pm         = 0;
        p_track->dark_l_pm        = 0;
        p_track->dark_r_pm        = 0;
        p_track->dark_run         = 0u;
        p_track->rearm_run        = 0u;
        p_track->white_ms         = 0u;
        p_track->read_fail_run    = 0u;
        p_track->b_junction_armed = true;
        p_track->b_lost           = false;
        p_track->b_line_seen      = false;
    }
}

/*
 * Process one sample of both sensors.  Returns at most one event; the
 * error, darkness values and flags in *p_track are always updated.
 */
ref_line_track_evt_t
ref_line_track_sample (ref_line_track_t * p_track, uint32_t raw_l, uint32_t raw_r,
                   uint32_t period_ms, bool b_adapt)
{
    ref_line_track_evt_t event = REF_LINE_TRACK_EVT_NONE;

    if (NULL != p_track)
    {
        /* ADC readings are 12-bit, so they fit int32 without loss. */
        int32_t const left  = (int32_t) raw_l;
        int32_t const right = (int32_t) raw_r;
        int32_t const dark_l = ref_line_track_darkness(left, p_track->white_l,
                                                   p_track->black_l);
        int32_t const dark_r = ref_line_track_darkness(right, p_track->white_r,
                                                   p_track->black_r);
        bool const b_both_dark  = ((dark_l >= CFG_LINE_DARK_PM) &&
                                   (dark_r >= CFG_LINE_DARK_PM));
        bool const b_both_white = ((dark_l <= CFG_LINE_WHITE_PM) &&
                                   (dark_r <= CFG_LINE_WHITE_PM));

        p_track->dark_l_pm   = dark_l;
        p_track->dark_r_pm   = dark_r;
        p_track->error_pm    = dark_l - dark_r;
        p_track->b_line_seen = !b_both_white;

        /* Junction: both fully dark for N samples, once per crossbar. */
        if (b_both_dark)
        {
            p_track->dark_run++;
            p_track->rearm_run = 0u;
        }
        else
        {
            p_track->dark_run = 0u;

            if (!p_track->b_junction_armed)
            {
                p_track->rearm_run++;

                if (p_track->rearm_run >= CFG_LINE_JUNCTION_REARM)
                {
                    p_track->b_junction_armed = true;
                }
            }
        }

        /* Line lost: both white for 300 ms; found when either is not. */
        p_track->white_ms = b_both_white ? (p_track->white_ms + period_ms)
                                         : 0u;

        if (p_track->b_junction_armed &&
            (p_track->dark_run >= CFG_LINE_JUNCTION_SAMPLES))
        {
            p_track->b_junction_armed = false;
            event = REF_LINE_TRACK_EVT_JUNCTION;
        }
        else if (!p_track->b_lost &&
                 (p_track->white_ms >= CFG_LINE_LOST_MS))
        {
            p_track->b_lost = true;
            event = REF_LINE_TRACK_EVT_LOST;
        }
        else if (p_track->b_lost && !b_both_white)
        {
            p_track->b_lost = false;
            event = REF_LINE_TRACK_EVT_FOUND;
        }
        else
        {
            /* No event this sample. */
        }

        if ((0 != CFG_LINE_ADAPT_ENABLE) && b_adapt)
        {
            ref_line_track_adapt(left, dark_l, &p_track->white_l,
                             &p_track->black_l, CFG_LINE_L_WHITE,
                             CFG_LINE_L_BLACK);
            ref_line_track_adapt(right, dark_r, &p_track->white_r,
                             &p_track->black_r, CFG_LINE_R_WHITE,
                             CFG_LINE_R_BLACK);
        }
    }

    return (event);
}

/* Darkness 0..1000 per mille of a reading between two levels.  Works for
   either sensor polarity (black above or below white). */
int32_t
ref_line_track_darkness (int32_t raw, int32_t white, int32_t black)
{
    int32_t const span = black - white;
    int32_t       dark = 0;

    if (0 != span)
    {
        dark = fixmath_div_round((raw - white) * LINE_FULL_PM, span);
    }

    return (fixmath_clamp(dark, 0, LINE_FULL_PM));
}

/*
 * Adapt one sensor's levels: a reading in the dark band pulls the black
 * level towards it, one in the white band pulls the white level.  Levels
 * stay within CFG_LINE_ADAPT_LIMIT of the stored values and keep at
 * least CFG_LINE_MIN_SPAN apart, or the stored values are restored.
 */
static void
ref_line_track_adapt (int32_t raw, int32_t dark_pm, int32_t * p_white,
                  int32_t * p_black, int32_t stored_white,
                  int32_t stored_black)
{
    if (dark_pm >= (LINE_FULL_PM - CFG_LINE_ADAPT_BAND_PM))
    {
        *p_black = fixmath_clamp(ref_line_track_step(*p_black, raw),
                                 stored_black - CFG_LINE_ADAPT_LIMIT,
                                 stored_black + CFG_LINE_ADAPT_LIMIT);
    }
    else if (dark_pm <= CFG_LINE_ADAPT_BAND_PM)
    {
        *p_white = fixmath_clamp(ref_line_track_step(*p_white, raw),
                                 stored_white - CFG_LINE_ADAPT_LIMIT,
                                 stored_white + CFG_LINE_ADAPT_LIMIT);
    }
    else
    {
        /* Partly dark: over an edge, no level to learn. */
    }

    if (fixmath_abs(*p_black - *p_white) < CFG_LINE_MIN_SPAN)
    {
        *p_white = stored_white;
        *p_black = stored_black;
    }
}

/* Move a level 1/2^CFG_LINE_ADAPT_SHIFT of the way towards a reading. */
static int32_t
ref_line_track_step (int32_t level, int32_t raw)
{
    int32_t const divisor = (int32_t) (1u << CFG_LINE_ADAPT_SHIFT);

    return (level + ((raw - level) / divisor));
}

/*
 * Count failed sensor reads (G15, D-79).  A good read clears the count.
 * Returns true once, on the read that makes CFG_LINE_READ_FAIL_LIMIT
 * failures in a row, so the Line task reports a single fault; a longer
 * run of failures does not report again until a good read has come.
 */
bool
ref_line_track_read_result (ref_line_track_t * p_track, bool b_read_ok)
{
    bool b_limit = false;

    if (NULL == p_track)
    {
        /* Nothing to count. */
    }
    else if (b_read_ok)
    {
        p_track->read_fail_run = 0u;
    }
    else if (p_track->read_fail_run < CFG_LINE_READ_FAIL_LIMIT)
    {
        p_track->read_fail_run++;
        b_limit = (CFG_LINE_READ_FAIL_LIMIT == p_track->read_fail_run);
    }
    else
    {
        /* Already reported for this run of failures. */
    }

    return (b_limit);
}

/*** end of file ***/
