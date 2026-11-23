/*
 *  line_track.c
 *  The LINE TRACKING region of the Line statechart.  See line_track.h.
 *
 *  The state is packed (D-86).  line_track_sample() takes a working copy
 *  of the fields into local variables, runs the statechart on them and
 *  packs them back, so the statechart reads as before.
 */

#include <stddef.h>

#include "../app_config.h"
#include "../common/bitpack.h"
#include "../common/fixmath.h"
#include "line_track.h"

#define LINE_FULL_PM        1000
#define LINE_ADC_MAX        4095u   /* 12-bit readings and levels */

/* Packed layout (line_track.h). */
#define LT_LEVEL_BITS       12u
#define LT_WHITE_SHIFT      0u      /* words 0 (left) and 1 (right) */
#define LT_BLACK_SHIFT      12u
#define LT_DARK_RUN_SHIFT   24u     /* word 0 */
#define LT_DARK_RUN_BITS    2u
#define LT_REARM_SHIFT      26u
#define LT_REARM_BITS       3u
#define LT_ARMED_BIT        29u
#define LT_LOST_BIT         30u
#define LT_SEEN_BIT         31u
#define LT_FAILS_SHIFT      24u     /* word 1 */
#define LT_FAILS_BITS       3u
#define LT_ERROR_SHIFT      0u      /* word 2 */
#define LT_ERROR_BITS       11u
#define LT_WHITE_MS_SHIFT   11u
#define LT_WHITE_MS_BITS    9u

/* The layout holds every value it must. */
BITPACK_ASSERT(BITPACK_BITS_FOR(LINE_ADC_MAX) == LT_LEVEL_BITS,
               lt_level_bits);
BITPACK_ASSERT((CFG_LINE_L_WHITE >= 0) && (CFG_LINE_L_WHITE <= 4095) &&
               (CFG_LINE_L_BLACK >= 0) && (CFG_LINE_L_BLACK <= 4095) &&
               (CFG_LINE_R_WHITE >= 0) && (CFG_LINE_R_WHITE <= 4095) &&
               (CFG_LINE_R_BLACK >= 0) && (CFG_LINE_R_BLACK <= 4095),
               lt_stored_levels_12_bit);
BITPACK_ASSERT(BITPACK_BITS_FOR(CFG_LINE_JUNCTION_SAMPLES) ==
               LT_DARK_RUN_BITS, lt_dark_run_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(CFG_LINE_JUNCTION_REARM) == LT_REARM_BITS,
               lt_rearm_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(CFG_LINE_READ_FAIL_LIMIT) == LT_FAILS_BITS,
               lt_fails_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR_SIGNED(LINE_FULL_PM) == LT_ERROR_BITS,
               lt_error_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(CFG_LINE_LOST_MS) == LT_WHITE_MS_BITS,
               lt_white_ms_bits);
BITPACK_ASSERT((LT_REARM_SHIFT + LT_REARM_BITS) == LT_ARMED_BIT,
               lt_layout_1);
BITPACK_ASSERT((LT_ERROR_SHIFT + LT_ERROR_BITS) == LT_WHITE_MS_SHIFT,
               lt_layout_2);

/* The fields while one sample is processed (a local working copy). */
typedef struct
{
    int32_t  white_l;
    int32_t  black_l;
    int32_t  white_r;
    int32_t  black_r;
    int32_t  error_pm;
    uint32_t dark_run;
    uint32_t rearm_run;
    uint32_t white_ms;
    bool     b_junction_armed;
    bool     b_lost;
    bool     b_line_seen;
} line_track_work_t;

static void line_track_adapt(int32_t raw, int32_t dark_pm,
                             int32_t * p_white, int32_t * p_black,
                             int32_t stored_white, int32_t stored_black);
static int32_t line_track_step(int32_t level, int32_t raw);
static void line_track_load(line_track_t const * p_track,
                            line_track_work_t * p_work);
static void line_track_store(line_track_t * p_track,
                             line_track_work_t const * p_work);
static uint32_t line_track_up_to(uint32_t value, uint32_t limit);

/* LOAD LEVELS: start from the black and white levels stored in the lab. */
void
line_track_init (line_track_t * p_track)
{
    if (NULL != p_track)
    {
        line_track_work_t work;

        work.white_l          = CFG_LINE_L_WHITE;
        work.black_l          = CFG_LINE_L_BLACK;
        work.white_r          = CFG_LINE_R_WHITE;
        work.black_r          = CFG_LINE_R_BLACK;
        work.error_pm         = 0;
        work.dark_run         = 0u;
        work.rearm_run        = 0u;
        work.white_ms         = 0u;
        work.b_junction_armed = true;
        work.b_lost           = false;
        work.b_line_seen      = false;

        p_track->word[0] = 0u;
        p_track->word[1] = 0u;      /* no failed reads */
        p_track->word[2] = 0u;
        line_track_store(p_track, &work);
    }
}

/*
 * Process one sample of both sensors (12-bit readings).  Returns at most
 * one event; the error and flags in *p_track are always updated.
 */
line_track_evt_t
line_track_sample (line_track_t * p_track, uint32_t raw_l, uint32_t raw_r,
                   uint32_t period_ms, bool b_adapt)
{
    line_track_evt_t event = LINE_TRACK_EVT_NONE;

    if (NULL != p_track)
    {
        /* ADC readings are 12-bit, so they fit int32 without loss. */
        int32_t const     left         = (int32_t) raw_l;
        int32_t const     right        = (int32_t) raw_r;
        int32_t           dark_l       = 0;
        int32_t           dark_r       = 0;
        bool              b_both_dark  = false;
        bool              b_both_white = false;
        line_track_work_t work;

        line_track_load(p_track, &work);
        dark_l = line_track_darkness(left, work.white_l, work.black_l);
        dark_r = line_track_darkness(right, work.white_r, work.black_r);
        b_both_dark  = ((dark_l >= CFG_LINE_DARK_PM) &&
                        (dark_r >= CFG_LINE_DARK_PM));
        b_both_white = ((dark_l <= CFG_LINE_WHITE_PM) &&
                        (dark_r <= CFG_LINE_WHITE_PM));

        work.error_pm    = dark_l - dark_r;
        work.b_line_seen = !b_both_white;

        /* Junction: both fully dark for N samples, once per crossbar.
           The run only counts up to N: it is only compared with N. */
        if (b_both_dark)
        {
            work.dark_run  = line_track_up_to(work.dark_run + 1u,
                                              CFG_LINE_JUNCTION_SAMPLES);
            work.rearm_run = 0u;
        }
        else
        {
            work.dark_run = 0u;

            if (!work.b_junction_armed)
            {
                work.rearm_run++;

                if (work.rearm_run >= CFG_LINE_JUNCTION_REARM)
                {
                    work.b_junction_armed = true;
                }
            }
        }

        /* Line lost: both white for 300 ms; found when either is not.
           The time only counts up to 300 ms: it is only compared with
           that. */
        work.white_ms = b_both_white
                        ? line_track_up_to(work.white_ms + period_ms,
                                           CFG_LINE_LOST_MS)
                        : 0u;

        if (work.b_junction_armed &&
            (work.dark_run >= CFG_LINE_JUNCTION_SAMPLES))
        {
            work.b_junction_armed = false;
            event = LINE_TRACK_EVT_JUNCTION;
        }
        else if (!work.b_lost && (work.white_ms >= CFG_LINE_LOST_MS))
        {
            work.b_lost = true;
            event = LINE_TRACK_EVT_LOST;
        }
        else if (work.b_lost && !b_both_white)
        {
            work.b_lost = false;
            event = LINE_TRACK_EVT_FOUND;
        }
        else
        {
            /* No event this sample. */
        }

        if ((0 != CFG_LINE_ADAPT_ENABLE) && b_adapt)
        {
            line_track_adapt(left, dark_l, &work.white_l, &work.black_l,
                             CFG_LINE_L_WHITE, CFG_LINE_L_BLACK);
            line_track_adapt(right, dark_r, &work.white_r, &work.black_r,
                             CFG_LINE_R_WHITE, CFG_LINE_R_BLACK);
        }

        line_track_store(p_track, &work);
    }

    return (event);
}

/* Darkness 0..1000 per mille of a reading between two levels.  Works for
   either sensor polarity (black above or below white). */
int32_t
line_track_darkness (int32_t raw, int32_t white, int32_t black)
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
 * Count failed sensor reads (G15, D-79).  A good read clears the count.
 * Returns true once, on the read that makes CFG_LINE_READ_FAIL_LIMIT
 * failures in a row, so the Line task reports a single fault; a longer
 * run of failures does not report again until a good read has come.
 */
bool
line_track_read_result (line_track_t * p_track, bool b_read_ok)
{
    bool b_limit = false;

    if (NULL == p_track)
    {
        /* Nothing to count. */
    }
    else
    {
        uint32_t const fails = line_track_read_fails(p_track);

        if (b_read_ok)
        {
            (void) bitpack_set(&p_track->word[1], LT_FAILS_SHIFT,
                               LT_FAILS_BITS, 0u);
        }
        else if (fails < CFG_LINE_READ_FAIL_LIMIT)
        {
            /* At most CFG_LINE_READ_FAIL_LIMIT: fits. */
            (void) bitpack_set(&p_track->word[1], LT_FAILS_SHIFT,
                               LT_FAILS_BITS, fails + 1u);
            b_limit = (CFG_LINE_READ_FAIL_LIMIT == (fails + 1u));
        }
        else
        {
            /* Already reported for this run of failures. */
        }
    }

    return (b_limit);
}

/* Last line error, -1000..1000 (+ = line under the left sensor). */
int32_t
line_track_error_pm (line_track_t const * p_track)
{
    return ((NULL != p_track)
            ? bitpack_get_signed(p_track->word[2], LT_ERROR_SHIFT,
                                 LT_ERROR_BITS)
            : 0);
}

bool
line_track_line_seen (line_track_t const * p_track)
{
    return ((NULL != p_track) &&
            bitpack_get_flag(p_track->word[0], LT_SEEN_BIT));
}

bool
line_track_lost (line_track_t const * p_track)
{
    return ((NULL != p_track) &&
            bitpack_get_flag(p_track->word[0], LT_LOST_BIT));
}

/* A current level, ADC counts. */
int32_t
line_track_level (line_track_t const * p_track, line_track_level_t level)
{
    int32_t value = 0;

    if (NULL != p_track)
    {
        uint32_t const word  = ((LINE_TRACK_WHITE_L == level) ||
                                (LINE_TRACK_BLACK_L == level))
                               ? p_track->word[0] : p_track->word[1];
        uint32_t const shift = ((LINE_TRACK_WHITE_L == level) ||
                                (LINE_TRACK_WHITE_R == level))
                               ? LT_WHITE_SHIFT : LT_BLACK_SHIFT;

        /* 12 bits: fits int32_t. */
        value = (int32_t) bitpack_get(word, shift, LT_LEVEL_BITS);
    }

    return (value);
}

/* Failed sensor reads in a row, 0..CFG_LINE_READ_FAIL_LIMIT. */
uint32_t
line_track_read_fails (line_track_t const * p_track)
{
    return ((NULL != p_track)
            ? bitpack_get(p_track->word[1], LT_FAILS_SHIFT, LT_FAILS_BITS)
            : 0u);
}

/*
 * Adapt one sensor's levels: a reading in the dark band pulls the black
 * level towards it, one in the white band pulls the white level.  Levels
 * stay within CFG_LINE_ADAPT_LIMIT of the stored values and keep at
 * least CFG_LINE_MIN_SPAN apart, or the stored values are restored.
 * A level only moves towards a 12-bit reading or back to a stored
 * level, so it stays within 0..4095.
 */
static void
line_track_adapt (int32_t raw, int32_t dark_pm, int32_t * p_white,
                  int32_t * p_black, int32_t stored_white,
                  int32_t stored_black)
{
    if (dark_pm >= (LINE_FULL_PM - CFG_LINE_ADAPT_BAND_PM))
    {
        *p_black = fixmath_clamp(line_track_step(*p_black, raw),
                                 stored_black - CFG_LINE_ADAPT_LIMIT,
                                 stored_black + CFG_LINE_ADAPT_LIMIT);
    }
    else if (dark_pm <= CFG_LINE_ADAPT_BAND_PM)
    {
        *p_white = fixmath_clamp(line_track_step(*p_white, raw),
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
line_track_step (int32_t level, int32_t raw)
{
    int32_t const divisor = (int32_t) (1u << CFG_LINE_ADAPT_SHIFT);

    return (level + ((raw - level) / divisor));
}

/* Unpack the fields the statechart works on. */
static void
line_track_load (line_track_t const * p_track, line_track_work_t * p_work)
{
    uint32_t const left  = p_track->word[0];
    uint32_t const right = p_track->word[1];

    /* Levels are 12-bit: they fit int32_t. */
    p_work->white_l  = (int32_t) bitpack_get(left, LT_WHITE_SHIFT,
                                             LT_LEVEL_BITS);
    p_work->black_l  = (int32_t) bitpack_get(left, LT_BLACK_SHIFT,
                                             LT_LEVEL_BITS);
    p_work->white_r  = (int32_t) bitpack_get(right, LT_WHITE_SHIFT,
                                             LT_LEVEL_BITS);
    p_work->black_r  = (int32_t) bitpack_get(right, LT_BLACK_SHIFT,
                                             LT_LEVEL_BITS);
    p_work->error_pm = bitpack_get_signed(p_track->word[2], LT_ERROR_SHIFT,
                                          LT_ERROR_BITS);
    p_work->dark_run  = bitpack_get(left, LT_DARK_RUN_SHIFT,
                                    LT_DARK_RUN_BITS);
    p_work->rearm_run = bitpack_get(left, LT_REARM_SHIFT, LT_REARM_BITS);
    p_work->white_ms  = bitpack_get(p_track->word[2], LT_WHITE_MS_SHIFT,
                                    LT_WHITE_MS_BITS);
    p_work->b_junction_armed = bitpack_get_flag(left, LT_ARMED_BIT);
    p_work->b_lost           = bitpack_get_flag(left, LT_LOST_BIT);
    p_work->b_line_seen      = bitpack_get_flag(left, LT_SEEN_BIT);
}

/*
 * Pack the fields back.  Every value fits (line_track.h): the levels
 * stay within 0..4095, the error within -1000..1000, the runs and the
 * white time at most their limits.  The failed-read count in word 1 is
 * kept.
 */
static void
line_track_store (line_track_t * p_track, line_track_work_t const * p_work)
{
    uint32_t left  = 0u;
    uint32_t right = p_track->word[1];
    uint32_t times = 0u;

    (void) bitpack_set(&left, LT_WHITE_SHIFT, LT_LEVEL_BITS,
                       (uint32_t) p_work->white_l);
    (void) bitpack_set(&left, LT_BLACK_SHIFT, LT_LEVEL_BITS,
                       (uint32_t) p_work->black_l);
    (void) bitpack_set(&left, LT_DARK_RUN_SHIFT, LT_DARK_RUN_BITS,
                       p_work->dark_run);
    (void) bitpack_set(&left, LT_REARM_SHIFT, LT_REARM_BITS,
                       p_work->rearm_run);
    bitpack_set_flag(&left, LT_ARMED_BIT, p_work->b_junction_armed);
    bitpack_set_flag(&left, LT_LOST_BIT, p_work->b_lost);
    bitpack_set_flag(&left, LT_SEEN_BIT, p_work->b_line_seen);
    (void) bitpack_set(&right, LT_WHITE_SHIFT, LT_LEVEL_BITS,
                       (uint32_t) p_work->white_r);
    (void) bitpack_set(&right, LT_BLACK_SHIFT, LT_LEVEL_BITS,
                       (uint32_t) p_work->black_r);
    (void) bitpack_set_signed(&times, LT_ERROR_SHIFT, LT_ERROR_BITS,
                              p_work->error_pm);
    (void) bitpack_set(&times, LT_WHITE_MS_SHIFT, LT_WHITE_MS_BITS,
                       p_work->white_ms);

    p_track->word[0] = left;
    p_track->word[1] = right;
    p_track->word[2] = times;
}

/* value, but at most limit. */
static uint32_t
line_track_up_to (uint32_t value, uint32_t limit)
{
    return ((value < limit) ? value : limit);
}

/*** end of file ***/
