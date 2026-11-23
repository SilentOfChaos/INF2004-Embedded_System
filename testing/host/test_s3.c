/*
 *  test_s3.c
 *  Host tests for Buddy 3's pure modules: barcode decoder and capture,
 *  line tracking, steering and the letter map.
 *  Results: testing/subsystem3/TEST_LOG.md.
 *
 *  The Code 39 patterns below are typed in from docs/notes/05 (measured
 *  on the sample barcodes), not copied from bc_decode.c, so the decoder's
 *  own table is checked too.  Design-note test IDs (UT-B3-xx) are given
 *  next to each S3 test.
 */

#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "ref/ref_bc_capture.h"
#include "ref/ref_letter_map.h"
#include "ref/ref_line_track.h"
#include "ref/ref_steering.h"
#include "subsystem3/barcode_edge.h"
#include "subsystem3/bc_capture.h"
#include "subsystem3/bc_decode.h"
#include "subsystem3/letter_map.h"
#include "subsystem3/line_keep.h"
#include "subsystem3/line_msgs.h"
#include "subsystem3/line_track.h"
#include "subsystem3/steering.h"
#include "unit.h"

#define TEST_NARROW_US      10000u  /* 3 mm at 0.3 m/s */
#define TEST_WIDE_US        30000u  /* 3:1, as printed */
#define TEST_EDGES          (BC_SYMBOL_ELEMENTS + 1u)
#define TEST_START_US       500000u /* after a long quiet zone */
#define TEST_STABLE_US      2000u   /* > CFG_BC_GLITCH_US */
#define TEST_MAX_ELEMENTS   40u

/* docs/notes/05 section 3: b s b s b s b s b, w = wide, n = narrow. */
static char const * const g_letters[26] =
{
    "wnnnnwnnw", "nnwnnwnnw", "wnwnnwnnn", "nnnnwwnnw", "wnnnwwnnn",
    "nnwnwwnnn", "nnnnnwwnw", "wnnnnwwnn", "nnwnnwwnn", "nnnnwwwnn",
    "wnnnnnnww", "nnwnnnnww", "wnwnnnnwn", "nnnnwnnww", "wnnnwnnwn",
    "nnwnwnnwn", "nnnnnnwww", "wnnnnnwwn", "nnwnnnwwn", "nnnnwnwwn",
    "wwnnnnnnw", "nwwnnnnnw", "wwwnnnnnn", "nwnnwnnnw", "wwnnwnnnn",
    "nwwnwnnnn"
};
static char const g_star[] = "nwnnwnwnn";

/* Not letters (standard Code 39 table). */
static char const * const g_not_letters[5] =
{
    "wnnwnnnnw",    /* '1' */
    "nwnnnnwnw",    /* '-' */
    "wwnnnnwnn",    /* '.' */
    "nwwnnnwnn",    /* ' ' */
    "nwnnwnwnn"     /* '*' */
};

/* Widths of '*', a middle character and '*', with a narrow gap after
   the first and second characters. */
static void
test_symbol (char const * p_middle, uint32_t bar_n, uint32_t bar_w,
             uint32_t space_n, uint32_t space_w, uint32_t * p_widths)
{
    char const * const chars[3] = { g_star, p_middle, g_star };
    uint32_t           pos      = 0u;

    for (uint32_t chr = 0u; chr < 3u; chr++)
    {
        for (uint32_t elem = 0u; elem < BC_CHAR_ELEMENTS; elem++)
        {
            bool const b_wide = ('w' == chars[chr][elem]);

            if (0u == (elem % 2u))
            {
                p_widths[pos] = b_wide ? bar_w : bar_n;
            }
            else
            {
                p_widths[pos] = b_wide ? space_w : space_n;
            }

            pos++;
        }

        if (chr < 2u)
        {
            p_widths[pos] = space_n;
            pos++;
        }
    }
}

static void
test_reverse (uint32_t * p_widths, uint32_t count)
{
    for (uint32_t idx = 0u; idx < (count / 2u); idx++)
    {
        uint32_t const keep = p_widths[idx];

        p_widths[idx] = p_widths[count - 1u - idx];
        p_widths[count - 1u - idx] = keep;
    }
}

/* Edges for the given elements, from start_us; returns the last time. */
static uint32_t
test_feed (bc_capture_t * p_cap, uint32_t const * p_widths, uint32_t count,
           uint32_t start_us)
{
    uint32_t time_us = start_us;

    for (uint32_t edge = 0u; edge <= count; edge++)
    {
        bc_capture_edge(p_cap, time_us, (0u == (edge % 2u)));

        if (edge < count)
        {
            time_us += p_widths[edge];
        }
    }

    return (time_us);
}

/* ---- The Line task's barcode cycle (line_task.c), for D-78 tests ---- */

#define TEST_LINE_PERIOD_US (CFG_PERIOD_LINE_MS * 1000u)
#define TEST_LINE_POLL_MAX  3u      /* LINE_POLL_MAX in line_task.c */
#define TEST_ISR_EDGES_MAX  80u

/* An edge as the Barcode ISR saw it.  A dropped edge never reaches the
   Line task (the queue was full); the ISR's overflow flag reports it. */
typedef struct
{
    uint32_t time_us;
    bool     b_dark;
    bool     b_dropped;
} test_isr_edge_t;

typedef struct
{
    uint32_t letters;
    uint32_t failed;
    char     letter;            /* the last letter */
    uint8_t  reason;            /* the last failure reason */
} test_line_out_t;

/* The 30 edges of a symbol from start_us, appended at p_edges[*p_count]. */
static void
test_isr_symbol (uint32_t const * p_widths, uint32_t start_us,
                 test_isr_edge_t * p_edges, uint32_t * p_count)
{
    uint32_t time_us = start_us;

    for (uint32_t edge = 0u; edge <= BC_SYMBOL_ELEMENTS; edge++)
    {
        p_edges[*p_count].time_us   = time_us;
        p_edges[*p_count].b_dark    = (0u == (edge % 2u));
        p_edges[*p_count].b_dropped = false;
        (*p_count)++;

        if (edge < BC_SYMBOL_ELEMENTS)
        {
            time_us += p_widths[edge];
        }
    }
}

/* One more edge (edges must be added in time order). */
static void
test_isr_add (test_isr_edge_t * p_edges, uint32_t * p_count,
              uint32_t time_us, bool b_dark, bool b_dropped)
{
    p_edges[*p_count].time_us   = time_us;
    p_edges[*p_count].b_dark    = b_dark;
    p_edges[*p_count].b_dropped = b_dropped;
    (*p_count)++;
}

/*
 * The Line task's barcode work, one 5 ms cycle at a time from from_us
 * to until_us, in its order: a dropped edge wakes the task at once,
 * which marks the capture while the other edges are still queued; each
 * cycle takes every queued edge, marks the capture again if an edge was
 * dropped since the last cycle, then polls up to 3 times.
 */
static test_line_out_t
test_line_cycles (bc_capture_t * p_cap, test_isr_edge_t const * p_edges,
                  uint32_t count, uint32_t from_us, uint32_t until_us)
{
    test_line_out_t out  = { 0u, 0u, '\0', BC_FAIL_NONE };
    uint32_t        next = 0u;

    for (uint32_t now_us = from_us; now_us <= until_us;
         now_us += TEST_LINE_PERIOD_US)
    {
        bool b_dropped = false;

        for (uint32_t idx = next;
             (idx < count) && (p_edges[idx].time_us <= now_us); idx++)
        {
            if (p_edges[idx].b_dropped && !b_dropped)
            {
                bc_capture_mark_lost(p_cap);        /* overflow wake */
                b_dropped = true;
            }
        }

        while ((next < count) && (p_edges[next].time_us <= now_us))
        {
            if (!p_edges[next].b_dropped)
            {
                bc_capture_edge(p_cap, p_edges[next].time_us,
                                p_edges[next].b_dark);
            }

            next++;
        }

        if (b_dropped)
        {
            bc_capture_mark_lost(p_cap);            /* after the drain */
        }

        for (uint32_t poll = 0u; poll < TEST_LINE_POLL_MAX; poll++)
        {
            bc_out_t const res = bc_capture_poll(p_cap, now_us);

            if (BC_OUT_LETTER == res.type)
            {
                out.letters++;
                out.letter = res.result.letter;
            }
            else if (BC_OUT_FAILED == res.type)
            {
                out.failed++;
                out.reason = res.result.reason;
            }
            else if (BC_OUT_NONE == res.type)
            {
                break;
            }
            else
            {
                /* STARTED: Navigation would hold the speed. */
            }
        }
    }

    return (out);
}

/* Poll until nothing more comes out; returns the last output and
   whether STARTED was reported first. */
static bc_out_t
test_poll (bc_capture_t * p_cap, uint32_t now_us, bool * p_b_started)
{
    bc_out_t last = { BC_OUT_NONE, { '\0', false, BC_FAIL_NONE } };
    bc_out_t out  = bc_capture_poll(p_cap, now_us);

    while (BC_OUT_NONE != out.type)
    {
        if (BC_OUT_STARTED == out.type)
        {
            *p_b_started = true;
        }
        else
        {
            last = out;
        }

        out = bc_capture_poll(p_cap, now_us);
    }

    return (last);
}

static void
test_decode_forward (void)
{
    uint32_t widths[BC_SYMBOL_ELEMENTS];
    bool     b_ok = true;

    UNIT_BEGIN("S3-U01", "all 26 letters forward (UT-B3-01)");

    for (uint32_t idx = 0u; idx < 26u; idx++)
    {
        bc_result_t result;

        test_symbol(g_letters[idx], TEST_NARROW_US, TEST_WIDE_US,
                    TEST_NARROW_US, TEST_WIDE_US, widths);
        result = bc_decode(widths, BC_SYMBOL_ELEMENTS);
        b_ok = b_ok && (BC_FAIL_NONE == result.reason) &&
               (('A' + (char) idx) == result.letter) && !result.b_reversed;
    }

    UNIT_CHECK(b_ok);
    UNIT_END();
}

static void
test_decode_reversed (void)
{
    uint32_t widths[BC_SYMBOL_ELEMENTS];
    bool     b_ok = true;

    UNIT_BEGIN("S3-U02", "all 26 letters backwards: same letter, reversed "
                         "(UT-B3-02, UT-B3-03)");

    for (uint32_t idx = 0u; idx < 26u; idx++)
    {
        bc_result_t result;

        test_symbol(g_letters[idx], TEST_NARROW_US, TEST_WIDE_US,
                    TEST_NARROW_US, TEST_WIDE_US, widths);
        test_reverse(widths, BC_SYMBOL_ELEMENTS);
        result = bc_decode(widths, BC_SYMBOL_ELEMENTS);
        b_ok = b_ok && (BC_FAIL_NONE == result.reason) &&
               (('A' + (char) idx) == result.letter) && result.b_reversed;
    }

    UNIT_CHECK(b_ok);

    /* The dangerous ones: backwards Z, K, O look like T, U, X. */
    test_symbol(g_letters['Z' - 'A'], TEST_NARROW_US, TEST_WIDE_US,
                TEST_NARROW_US, TEST_WIDE_US, widths);
    test_reverse(widths, BC_SYMBOL_ELEMENTS);
    UNIT_EQ(bc_decode(widths, BC_SYMBOL_ELEMENTS).letter, 'Z');
    test_symbol(g_letters['K' - 'A'], TEST_NARROW_US, TEST_WIDE_US,
                TEST_NARROW_US, TEST_WIDE_US, widths);
    test_reverse(widths, BC_SYMBOL_ELEMENTS);
    UNIT_EQ(bc_decode(widths, BC_SYMBOL_ELEMENTS).letter, 'K');
    test_symbol(g_letters['O' - 'A'], TEST_NARROW_US, TEST_WIDE_US,
                TEST_NARROW_US, TEST_WIDE_US, widths);
    test_reverse(widths, BC_SYMBOL_ELEMENTS);
    UNIT_EQ(bc_decode(widths, BC_SYMBOL_ELEMENTS).letter, 'O');
    UNIT_END();
}

static void
test_decode_speed (void)
{
    uint32_t widths[BC_SYMBOL_ELEMENTS];
    bool     b_ok = true;

    UNIT_BEGIN("S3-U03", "speed change of +/-25% across the barcode "
                         "(UT-B3-04)");

    for (uint32_t idx = 0u; idx < 26u; idx++)
    {
        for (uint32_t way = 0u; way < 2u; way++)
        {
            test_symbol(g_letters[idx], TEST_NARROW_US, TEST_WIDE_US,
                        TEST_NARROW_US, TEST_WIDE_US, widths);

            for (uint32_t elem = 0u; elem < BC_SYMBOL_ELEMENTS; elem++)
            {
                /* Speeding up (75% -> 125% of the width) or slowing. */
                uint32_t const pct = (0u == way)
                                     ? (75u + ((50u * elem) / 28u))
                                     : (125u - ((50u * elem) / 28u));

                widths[elem] = (widths[elem] * pct) / 100u;
            }

            b_ok = b_ok && (('A' + (char) idx) ==
                            bc_decode(widths, BC_SYMBOL_ELEMENTS).letter);
        }
    }

    UNIT_CHECK(b_ok);
    UNIT_END();
}

static void
test_decode_bias (void)
{
    uint32_t widths[BC_SYMBOL_ELEMENTS];
    bool     b_ok = true;

    UNIT_BEGIN("S3-U04", "comparator bias: bars +40%, spaces -40% "
                         "(UT-B3-05)");

    for (uint32_t idx = 0u; idx < 26u; idx++)
    {
        bc_result_t forward;
        bc_result_t backward;

        test_symbol(g_letters[idx], 14000u, 42000u, 6000u, 18000u, widths);
        forward = bc_decode(widths, BC_SYMBOL_ELEMENTS);
        test_reverse(widths, BC_SYMBOL_ELEMENTS);
        backward = bc_decode(widths, BC_SYMBOL_ELEMENTS);
        b_ok = b_ok && (('A' + (char) idx) == forward.letter) &&
               (('A' + (char) idx) == backward.letter);

        /* And the opposite bias. */
        test_symbol(g_letters[idx], 6000u, 18000u, 14000u, 42000u, widths);
        b_ok = b_ok && (('A' + (char) idx) ==
                        bc_decode(widths, BC_SYMBOL_ELEMENTS).letter);
    }

    UNIT_CHECK(b_ok);
    UNIT_END();
}

static void
test_decode_rejects (void)
{
    uint32_t    widths[BC_SYMBOL_ELEMENTS];
    bc_result_t result;
    bool        b_ok = true;

    UNIT_BEGIN("S3-U05", "no letter from bad captures (UT-B3-06, UT-B3-07)");

    /* Wrong element counts. */
    test_symbol(g_letters[0], TEST_NARROW_US, TEST_WIDE_US, TEST_NARROW_US,
                TEST_WIDE_US, widths);
    UNIT_EQ(bc_decode(widths, 28u).reason, BC_FAIL_COUNT);
    UNIT_EQ(bc_decode(widths, 30u).reason, BC_FAIL_COUNT);
    UNIT_EQ(bc_decode(NULL, BC_SYMBOL_ELEMENTS).reason, BC_FAIL_COUNT);

    /* Middle character not a letter. */
    for (uint32_t idx = 0u; idx < 5u; idx++)
    {
        test_symbol(g_not_letters[idx], TEST_NARROW_US, TEST_WIDE_US,
                    TEST_NARROW_US, TEST_WIDE_US, widths);
        result = bc_decode(widths, BC_SYMBOL_ELEMENTS);
        b_ok = b_ok && ('\0' == result.letter) &&
               (BC_FAIL_NOT_LETTER == result.reason);
    }

    UNIT_CHECK(b_ok);

    /* All widths equal: no wide/narrow split. */
    for (uint32_t idx = 0u; idx < BC_SYMBOL_ELEMENTS; idx++)
    {
        widths[idx] = TEST_NARROW_US;
    }

    UNIT_EQ(bc_decode(widths, BC_SYMBOL_ELEMENTS).reason, BC_FAIL_WIDTHS);

    /* A wide gap between characters. */
    test_symbol(g_letters[0], TEST_NARROW_US, TEST_WIDE_US, TEST_NARROW_US,
                TEST_WIDE_US, widths);
    widths[9] = TEST_WIDE_US;
    UNIT_EQ(bc_decode(widths, BC_SYMBOL_ELEMENTS).letter, '\0');

    /* A letter at both ends instead of '*'. */
    {
        char const * const keep = g_letters[1];

        test_symbol(keep, TEST_NARROW_US, TEST_WIDE_US, TEST_NARROW_US,
                    TEST_WIDE_US, widths);

        for (uint32_t elem = 0u; elem < BC_CHAR_ELEMENTS; elem++)
        {
            bool const b_wide = ('w' == g_letters[0][elem]);

            widths[elem] = b_wide ? TEST_WIDE_US : TEST_NARROW_US;
        }

        UNIT_EQ(bc_decode(widths, BC_SYMBOL_ELEMENTS).reason, BC_FAIL_ENDS);
    }

    UNIT_EQ(bc_decode_pattern(0x094u), '*');
    UNIT_EQ(bc_decode_pattern(0x1FFu), '\0');
    UNIT_END();
}

static void
test_capture_letter (void)
{
    static bc_capture_t cap;
    uint32_t            widths[BC_SYMBOL_ELEMENTS];
    uint32_t            last_us = 0u;
    bool                b_started = false;
    bc_out_t            out;

    UNIT_BEGIN("S3-U06", "capture: letter right after the 30th edge");
    bc_capture_init(&cap, false, 0u);
    test_symbol(g_letters['B' - 'A'], TEST_NARROW_US, TEST_WIDE_US,
                TEST_NARROW_US, TEST_WIDE_US, widths);
    last_us = test_feed(&cap, widths, BC_SYMBOL_ELEMENTS, TEST_START_US);

    /* Just after the last edge (not 120 ms later): the letter. */
    out = test_poll(&cap, last_us + TEST_STABLE_US, &b_started);
    UNIT_CHECK(b_started);
    UNIT_EQ(out.type, BC_OUT_LETTER);
    UNIT_EQ(out.result.letter, 'B');
    UNIT_EQ(bc_capture_state(&cap), BC_STATE_WAITING);

    /* Every new barcode is tried again from WAITING (report 6.3). */
    bc_capture_edge(&cap, last_us + 50000u, true);
    UNIT_EQ(bc_capture_state(&cap), BC_STATE_CAPTURING);
    UNIT_END();
}

static void
test_capture_glitch (void)
{
    static bc_capture_t cap;
    uint32_t            widths[BC_SYMBOL_ELEMENTS];
    uint32_t            time_us = TEST_START_US;
    bool                b_started = false;
    bc_out_t            out;

    UNIT_BEGIN("S3-U07", "capture: glitches under 1 ms are removed "
                         "(UT-B3-06)");
    bc_capture_init(&cap, false, 0u);
    test_symbol(g_letters['C' - 'A'], TEST_NARROW_US, TEST_WIDE_US,
                TEST_NARROW_US, TEST_WIDE_US, widths);

    for (uint32_t edge = 0u; edge <= BC_SYMBOL_ELEMENTS; edge++)
    {
        bool const b_dark = (0u == (edge % 2u));

        bc_capture_edge(&cap, time_us, b_dark);

        if (edge < BC_SYMBOL_ELEMENTS)
        {
            /* A 300 us flicker of the other level inside every element. */
            bc_capture_edge(&cap, time_us + 3000u, !b_dark);
            bc_capture_edge(&cap, time_us + 3300u, b_dark);
            time_us += widths[edge];
        }
    }

    out = test_poll(&cap, time_us + TEST_STABLE_US, &b_started);
    UNIT_EQ(out.type, BC_OUT_LETTER);
    UNIT_EQ(out.result.letter, 'C');

    /* A glitch on the very first bar: back to WAITING, and the real
       barcode right after it still decodes. */
    bc_capture_init(&cap, false, 0u);
    bc_capture_edge(&cap, TEST_START_US, true);
    bc_capture_edge(&cap, TEST_START_US + 300u, false);
    UNIT_EQ(bc_capture_state(&cap), BC_STATE_WAITING);
    time_us = test_feed(&cap, widths, BC_SYMBOL_ELEMENTS,
                        TEST_START_US + 5000u);
    out = test_poll(&cap, time_us + TEST_STABLE_US, &b_started);
    UNIT_EQ(out.result.letter, 'C');
    UNIT_END();
}

static void
test_capture_quiet (void)
{
    static bc_capture_t cap;

    UNIT_BEGIN("S3-U08", "capture: starts at the first bar (report 6.3); "
                         "the optional quiet zone waits for white");

    /* Configured behaviour: CFG_BC_QUIET_MS is 0, the report's rule. */
    UNIT_EQ(CFG_BC_QUIET_MS, 0);
    bc_capture_init(&cap, false, 0u);
    UNIT_EQ(bc_capture_quiet_ms(&cap), 0);
    bc_capture_edge(&cap, 5000u, true);         /* first bar: capture */
    UNIT_EQ(bc_capture_state(&cap), BC_STATE_CAPTURING);

    /* The proposal (D-25, not approved): 120 ms of white first. */
    bc_capture_init(&cap, false, 0u);
    UNIT_CHECK(bc_capture_set_quiet_ms(&cap, 120u));
    bc_capture_edge(&cap, 50000u, true);        /* 50 ms of white: no */
    UNIT_EQ(bc_capture_state(&cap), BC_STATE_WAITING);
    bc_capture_edge(&cap, 60000u, false);
    bc_capture_edge(&cap, 170000u, true);       /* 110 ms: no */
    UNIT_EQ(bc_capture_state(&cap), BC_STATE_WAITING);
    bc_capture_edge(&cap, 180000u, false);
    bc_capture_edge(&cap, 300000u, true);       /* 120 ms: yes */
    UNIT_EQ(bc_capture_state(&cap), BC_STATE_CAPTURING);
    UNIT_END();
}

static void
test_capture_fail (void)
{
    static bc_capture_t cap;
    uint32_t            widths[TEST_MAX_ELEMENTS];
    uint32_t            last_us = 0u;
    bool                b_started = false;
    bc_out_t            out;

    UNIT_BEGIN("S3-U09", "capture: crossbar, too few, extra, lost edges "
                         "fail (UT-B3-06, UT-B3-08)");

    /* An 18 mm junction crossbar: one long dark region. */
    bc_capture_init(&cap, false, 0u);
    bc_capture_edge(&cap, TEST_START_US, true);
    bc_capture_edge(&cap, TEST_START_US + 60000u, false);
    out = test_poll(&cap, TEST_START_US + 60000u + 50000u, &b_started);
    UNIT_EQ(out.type, BC_OUT_NONE);             /* still waiting */
    out = test_poll(&cap, TEST_START_US + 60000u + 120000u, &b_started);
    UNIT_CHECK(b_started);
    UNIT_EQ(out.type, BC_OUT_FAILED);
    UNIT_EQ(out.result.reason, BC_FAIL_TOO_FEW);

    /* 28 elements. */
    bc_capture_init(&cap, false, 0u);
    test_symbol(g_letters[0], TEST_NARROW_US, TEST_WIDE_US, TEST_NARROW_US,
                TEST_WIDE_US, widths);
    last_us = test_feed(&cap, widths, 28u, TEST_START_US);
    out = test_poll(&cap, last_us + 120000u, &b_started);
    UNIT_EQ(out.type, BC_OUT_FAILED);
    UNIT_EQ(out.result.reason, BC_FAIL_TOO_FEW);

    /* An extra element in the middle: a wide bar split in two. */
    bc_capture_init(&cap, false, 0u);
    test_symbol(g_letters[0], TEST_NARROW_US, TEST_WIDE_US, TEST_NARROW_US,
                TEST_WIDE_US, widths);
    (void) memmove(&widths[12], &widths[10],
                   (BC_SYMBOL_ELEMENTS - 10u) * sizeof(widths[0]));
    widths[10] = 14000u;
    widths[11] = 2000u;
    widths[12] = 14000u;
    last_us = test_feed(&cap, widths, BC_SYMBOL_ELEMENTS + 2u,
                        TEST_START_US);
    out = test_poll(&cap, last_us + TEST_STABLE_US, &b_started);
    UNIT_CHECK(BC_OUT_LETTER != out.type);
    out = test_poll(&cap, last_us + 120000u, &b_started);
    UNIT_EQ(out.type, BC_OUT_FAILED);

    /* Two edges of the same level: an edge was lost. */
    bc_capture_init(&cap, false, 0u);
    bc_capture_edge(&cap, TEST_START_US, true);
    bc_capture_edge(&cap, TEST_START_US + 10000u, true);
    out = test_poll(&cap, TEST_START_US + 12000u, &b_started);
    UNIT_EQ(out.type, BC_OUT_FAILED);
    UNIT_EQ(out.result.reason, BC_FAIL_EDGE_LOST);

    /* The ISR could not queue an edge. */
    bc_capture_init(&cap, false, 0u);
    bc_capture_edge(&cap, TEST_START_US, true);
    bc_capture_edge(&cap, TEST_START_US + 10000u, false);
    bc_capture_mark_lost(&cap);
    out = test_poll(&cap, TEST_START_US + 12000u, &b_started);
    UNIT_EQ(out.result.reason, BC_FAIL_EDGE_LOST);
    UNIT_END();
}

static void
test_capture_limits (void)
{
    static bc_capture_t cap;
    uint32_t            time_us = TEST_START_US;
    bool                b_started = false;
    bc_out_t            out;

    UNIT_BEGIN("S3-U10", "capture: edge and time limits always end it");

    /* More edges than the buffer holds. */
    bc_capture_init(&cap, false, 0u);

    for (uint32_t edge = 0u; edge < (CFG_BC_MAX_EDGES + 10u); edge++)
    {
        bc_capture_edge(&cap, time_us, (0u == (edge % 2u)));
        time_us += 2000u;
    }

    out = test_poll(&cap, time_us, &b_started);
    UNIT_EQ(out.type, BC_OUT_FAILED);

    /* Edges that never stop, slower than the 120 ms timeout. */
    bc_capture_init(&cap, false, 0u);
    time_us = TEST_START_US;

    for (uint32_t edge = 0u; edge < 25u; edge++)
    {
        bc_capture_edge(&cap, time_us, (0u == (edge % 2u)));
        out = test_poll(&cap, time_us + 5000u, &b_started);

        if (BC_OUT_FAILED == out.type)
        {
            break;
        }

        time_us += 100000u;
    }

    UNIT_EQ(out.type, BC_OUT_FAILED);
    UNIT_EQ(out.result.reason, BC_FAIL_TOO_LONG);
    UNIT_CHECK((time_us - TEST_START_US) <=
               ((CFG_BC_MAX_CAPTURE_MS * 1000u) + 100000u));
    UNIT_END();
}

static void
test_capture_loss_inside (void)
{
    static bc_capture_t    cap;
    static test_isr_edge_t edges[TEST_ISR_EDGES_MAX];
    uint32_t               widths[BC_SYMBOL_ELEMENTS];
    uint32_t               count = 0u;
    uint32_t               t29 = 0u;
    test_line_out_t        out;

    UNIT_BEGIN("S3-U16", "capture: an edge lost inside the symbol fails it, "
                         "even if the widths decode (D-78)");
    test_symbol(g_letters['B' - 'A'], TEST_NARROW_US, TEST_WIDE_US,
                TEST_NARROW_US, TEST_WIDE_US, widths);

    /* The sensor chatters at the end of the last bar and the ISR's queue
       overflows: the Line task is woken while the 30th edge is still
       queued (5 ms cycles, edges 2.5 ms before each cycle). */
    bc_capture_init(&cap, false, 0u);
    count = 0u;
    test_isr_symbol(widths, TEST_START_US, edges, &count);
    t29 = edges[count - 1u].time_us;
    test_isr_add(edges, &count, t29 + 100u, true, true);
    test_isr_add(edges, &count, t29 + 200u, false, true);
    out = test_line_cycles(&cap, edges, count, TEST_START_US + 2500u,
                           t29 + 200000u);
    UNIT_EQ(out.letters, 0);
    UNIT_EQ(out.failed, 1);
    UNIT_EQ(out.reason, BC_FAIL_EDGE_LOST);

    /* Levels that do not alternate before the 30th edge, which arrives
       in the same 5 ms cycle: the decode is not tried. */
    bc_capture_init(&cap, false, 0u);
    count = 0u;
    test_isr_symbol(widths, TEST_START_US, edges, &count);
    edges[count] = edges[count - 1u];                   /* edge 29 */
    edges[count - 1u].time_us   = edges[count - 2u].time_us + 8000u;
    edges[count - 1u].b_dark    = true;     /* dark again: a lost edge */
    edges[count - 1u].b_dropped = false;
    count++;
    out = test_line_cycles(&cap, edges, count, TEST_START_US + 2500u,
                           edges[count - 1u].time_us + 200000u);
    UNIT_EQ(out.letters, 0);
    UNIT_EQ(out.failed, 1);
    UNIT_EQ(out.reason, BC_FAIL_EDGE_LOST);

    /* The queue overflows while the first bar's edge is still queued:
       the capture those edges start is marked after the drain. */
    bc_capture_init(&cap, false, 0u);
    count = 0u;
    test_isr_symbol(widths, TEST_START_US, edges, &count);
    (void) memmove(&edges[3], &edges[1], (count - 1u) * sizeof(edges[0]));
    edges[1].time_us   = TEST_START_US + 300u;
    edges[1].b_dark    = false;
    edges[1].b_dropped = true;
    edges[2].time_us   = TEST_START_US + 400u;
    edges[2].b_dark    = true;
    edges[2].b_dropped = true;
    count += 2u;
    out = test_line_cycles(&cap, edges, 3u, TEST_START_US + 2500u,
                           TEST_START_US + 7500u);
    UNIT_EQ(out.failed, 1);
    UNIT_EQ(out.reason, BC_FAIL_EDGE_LOST);
    out = test_line_cycles(&cap, &edges[3], count - 3u,
                           TEST_START_US + 12500u,
                           edges[count - 1u].time_us + 200000u);
    UNIT_EQ(out.letters, 0);                /* the rest is never a letter */
    UNIT_END();
}

static void
test_capture_after_symbol (void)
{
    static bc_capture_t    cap;
    static test_isr_edge_t edges[TEST_ISR_EDGES_MAX];
    uint32_t               widths[BC_SYMBOL_ELEMENTS];
    uint32_t               count = 0u;
    uint32_t               last_us = 0u;
    bool                   b_started = false;
    bc_out_t               res;
    test_line_out_t        out;

    UNIT_BEGIN("S3-U17", "capture: edges and losses after the 30th edge do "
                         "not spoil a complete symbol (D-78)");
    test_symbol(g_letters['B' - 'A'], TEST_NARROW_US, TEST_WIDE_US,
                TEST_NARROW_US, TEST_WIDE_US, widths);

    /* A loss reported once all 30 edges are stored came after them. */
    bc_capture_init(&cap, false, 0u);
    last_us = test_feed(&cap, widths, BC_SYMBOL_ELEMENTS, TEST_START_US);
    bc_capture_mark_lost(&cap);
    res = test_poll(&cap, last_us + TEST_STABLE_US, &b_started);
    UNIT_EQ(res.type, BC_OUT_LETTER);
    UNIT_EQ(res.result.letter, 'B');

    /* 5 ms cycles: the 30th edge is taken 0.5 ms after it happened, too
       soon to decode; the queue overflows in the next cycle. */
    bc_capture_init(&cap, false, 0u);
    count = 0u;
    test_isr_symbol(widths, TEST_START_US + 2000u, edges, &count);
    last_us = edges[count - 1u].time_us;
    test_isr_add(edges, &count, last_us + 1000u, true, true);
    test_isr_add(edges, &count, last_us + 1100u, false, true);
    out = test_line_cycles(&cap, edges, count, TEST_START_US + 2500u,
                           last_us + 200000u);
    UNIT_EQ(out.letters, 1);
    UNIT_EQ(out.letter, 'B');
    UNIT_EQ(out.failed, 0);

    /* The same, with a same-level edge just after the 30th edge. */
    bc_capture_init(&cap, false, 0u);
    count = 0u;
    test_isr_symbol(widths, TEST_START_US + 2000u, edges, &count);
    last_us = edges[count - 1u].time_us;
    test_isr_add(edges, &count, last_us + 300u, false, false);
    out = test_line_cycles(&cap, edges, count, TEST_START_US + 2500u,
                           last_us + 200000u);
    UNIT_EQ(out.letters, 1);
    UNIT_EQ(out.letter, 'B');
    UNIT_EQ(out.failed, 0);

    /* Trailing edges in the same cycle as the 30th edge. */
    bc_capture_init(&cap, false, 0u);
    count = 0u;
    test_isr_symbol(widths, TEST_START_US, edges, &count);
    last_us = edges[count - 1u].time_us;
    test_isr_add(edges, &count, last_us + 1200u, true, false);
    test_isr_add(edges, &count, last_us + 2400u, false, false);
    out = test_line_cycles(&cap, edges, count, TEST_START_US + 2500u,
                           last_us + 200000u);
    UNIT_EQ(out.letters, 1);
    UNIT_EQ(out.letter, 'B');

    /* More edges than the buffer holds after a valid symbol, all before
       the first poll: the symbol still decodes. */
    bc_capture_init(&cap, false, 0u);
    last_us = test_feed(&cap, widths, BC_SYMBOL_ELEMENTS, TEST_START_US);

    for (uint32_t extra = 0u; extra < 40u; extra++)
    {
        last_us += 2000u;
        bc_capture_edge(&cap, last_us, (0u == (extra % 2u)));
    }

    UNIT_CHECK(bc_capture_overflowed(&cap));
    res = test_poll(&cap, last_us + TEST_STABLE_US, &b_started);
    UNIT_EQ(res.type, BC_OUT_LETTER);
    UNIT_EQ(res.result.letter, 'B');
    UNIT_END();
}

static void
test_line_error (void)
{
    line_track_t track;

    UNIT_BEGIN("S3-U11", "line error sign and limits (UT-B3-09)");
    line_track_init(&track);

    UNIT_EQ(line_track_darkness(CFG_LINE_L_WHITE, CFG_LINE_L_WHITE,
                                CFG_LINE_L_BLACK), 0);
    UNIT_EQ(line_track_darkness(CFG_LINE_L_BLACK, CFG_LINE_L_WHITE,
                                CFG_LINE_L_BLACK), 1000);
    UNIT_EQ(line_track_darkness(0, 300, 3000), 0);          /* clamped */
    UNIT_EQ(line_track_darkness(4095, 300, 3000), 1000);    /* clamped */
    UNIT_EQ(line_track_darkness(1650, 300, 3000), 500);
    UNIT_EQ(line_track_darkness(1650, 3000, 300), 500);     /* inverted */
    UNIT_EQ(line_track_darkness(1000, 300, 300), 0);        /* no span */

    /* Line under the left sensor only: full positive error. */
    (void) line_track_sample(&track, CFG_LINE_L_BLACK, CFG_LINE_R_WHITE,
                             5u, false);
    UNIT_EQ(line_track_error_pm(&track), 1000);
    (void) line_track_sample(&track, CFG_LINE_L_WHITE, CFG_LINE_R_BLACK,
                             5u, false);
    UNIT_EQ(line_track_error_pm(&track), -1000);
    (void) line_track_sample(&track, 1650u, 1650u, 5u, false);
    UNIT_EQ(line_track_error_pm(&track), 0);
    UNIT_CHECK(line_track_line_seen(&track));
    UNIT_END();
}

static void
test_line_events (void)
{
    line_track_t     track;
    line_track_evt_t evt = LINE_TRACK_EVT_NONE;
    uint32_t         junctions = 0u;
    uint32_t         samples = 0u;

    UNIT_BEGIN("S3-U12", "junction once per crossbar; lost after 300 ms");
    line_track_init(&track);

    /* Crossbar: 10 samples both dark, then the line again. */
    for (uint32_t idx = 0u; idx < 10u; idx++)
    {
        evt = line_track_sample(&track, CFG_LINE_L_BLACK, CFG_LINE_R_BLACK,
                                5u, false);
        junctions += (LINE_TRACK_EVT_JUNCTION == evt) ? 1u : 0u;

        if ((LINE_TRACK_EVT_JUNCTION == evt) && (0u == samples))
        {
            samples = idx + 1u;
        }
    }

    UNIT_EQ(junctions, 1);
    UNIT_EQ(samples, CFG_LINE_JUNCTION_SAMPLES);

    /* Re-armed after a few samples off the crossbar. */
    for (uint32_t idx = 0u; idx < CFG_LINE_JUNCTION_REARM; idx++)
    {
        (void) line_track_sample(&track, 1650u, 1650u, 5u, false);
    }

    for (uint32_t idx = 0u; idx < 3u; idx++)
    {
        evt = line_track_sample(&track, CFG_LINE_L_BLACK, CFG_LINE_R_BLACK,
                                5u, false);
    }

    UNIT_EQ(evt, LINE_TRACK_EVT_JUNCTION);

    /* Both white: lost after exactly 300 ms, found when seen again. */
    samples = 0u;
    evt     = LINE_TRACK_EVT_NONE;

    while ((LINE_TRACK_EVT_LOST != evt) && (samples < 1000u))
    {
        evt = line_track_sample(&track, CFG_LINE_L_WHITE, CFG_LINE_R_WHITE,
                                5u, false);
        samples++;
    }

    UNIT_EQ(samples * 5u, CFG_LINE_LOST_MS);
    UNIT_CHECK(line_track_lost(&track));
    UNIT_EQ(line_track_sample(&track, 1650u, CFG_LINE_R_WHITE, 5u, false),
            LINE_TRACK_EVT_FOUND);
    UNIT_END();
}

static void
test_line_adapt (void)
{
    line_track_t track;

    UNIT_BEGIN("S3-U13", "level adaptation follows slowly, within limits");
    line_track_init(&track);

    for (uint32_t idx = 0u; idx < 1000u; idx++)
    {
        (void) line_track_sample(&track, 2620u, 2620u, 5u, true);
    }

    UNIT_CHECK(abs(line_track_level(&track, LINE_TRACK_BLACK_L) - 2620) <= 70);
    UNIT_CHECK(line_track_level(&track, LINE_TRACK_BLACK_L) < CFG_LINE_L_BLACK);

    /* Darker readings further down pull it to its limit, no further. */
    for (uint32_t idx = 0u; idx < 1000u; idx++)
    {
        (void) line_track_sample(&track, 2400u, 2400u, 5u, true);
    }

    for (uint32_t idx = 0u; idx < 1000u; idx++)
    {
        (void) line_track_sample(&track, 2200u, 2200u, 5u, true);
    }

    UNIT_EQ(line_track_level(&track, LINE_TRACK_BLACK_L),
            CFG_LINE_L_BLACK - CFG_LINE_ADAPT_LIMIT);

    /* Frozen (b_adapt false, e.g. on a hump): no change. */
    line_track_init(&track);

    for (uint32_t idx = 0u; idx < 100u; idx++)
    {
        (void) line_track_sample(&track, 2620u, 2620u, 5u, false);
    }

    UNIT_EQ(line_track_level(&track, LINE_TRACK_BLACK_L), CFG_LINE_L_BLACK);
    UNIT_END();
}

static void
test_steering (void)
{
    steering_t steer;
    int32_t    speed = 0;
    int32_t    diff  = 0;

    UNIT_BEGIN("S3-U14", "steering: sign, gains, limits, slow-down");
    steering_reset(&steer);

    steering_compute(&steer, 1000, 250, &speed, &diff);
    UNIT_EQ(diff, CFG_STEER_KP_MMPS);       /* turn left, P only */
    UNIT_EQ(speed, 250 - ((250 * CFG_STEER_SLOWDOWN_PM) / 1000));

    steering_compute(&steer, -1000, 250, &speed, &diff);
    UNIT_EQ(diff, -CFG_STEER_MAX_DIFF_MMPS);    /* P + D, limited */

    steering_reset(&steer);
    steering_compute(&steer, 0, 250, &speed, &diff);
    UNIT_EQ(diff, 0);
    UNIT_EQ(speed, 250);

    steering_reset(&steer);
    steering_compute(&steer, 5000, 250, &speed, &diff);    /* clamped */
    UNIT_EQ(diff, CFG_STEER_KP_MMPS);

    steering_reset(&steer);
    steering_compute(&steer, 500, 250, &speed, &diff);
    UNIT_EQ(diff, CFG_STEER_KP_MMPS / 2);
    steering_compute(&steer, 600, 250, &speed, &diff);     /* D term */
    UNIT_EQ(diff, ((600 * CFG_STEER_KP_MMPS) / 1000) +
                  ((100 * CFG_STEER_KD_X1000) / 1000));
    UNIT_END();
}

static void
test_letter_map (void)
{
    letter_map_t    map;
    letter_action_t action = LETTER_ACTION_NONE;

    UNIT_BEGIN("S3-U15", "letter map: defaults, remap, bad input");
    letter_map_default(&map);
    UNIT_EQ(letter_map_get(&map, 'A'), LETTER_ACTION_STRAIGHT);
    UNIT_EQ(letter_map_get(&map, 'B'), LETTER_ACTION_LEFT);
    UNIT_EQ(letter_map_get(&map, 'C'), LETTER_ACTION_RIGHT);
    UNIT_EQ(letter_map_get(&map, 'D'), LETTER_ACTION_UTURN);
    UNIT_EQ(letter_map_get(&map, 'E'), LETTER_ACTION_NONE);
    UNIT_EQ(letter_map_get(&map, 'Z'), LETTER_ACTION_NONE);

    UNIT_CHECK(letter_map_set(&map, 'E', LETTER_ACTION_LEFT));
    UNIT_EQ(letter_map_get(&map, 'E'), LETTER_ACTION_LEFT);
    UNIT_CHECK(letter_map_set(&map, 'a', LETTER_ACTION_RIGHT));
    UNIT_EQ(letter_map_get(&map, 'A'), LETTER_ACTION_RIGHT);
    UNIT_CHECK(!letter_map_set(&map, '1', LETTER_ACTION_LEFT));
    UNIT_CHECK(!letter_map_set(&map, 'F', LETTER_ACTION_COUNT));
    UNIT_EQ(letter_map_get(&map, '*'), LETTER_ACTION_NONE);

    letter_map_default(&map);
    UNIT_EQ(letter_map_get(&map, 'E'), LETTER_ACTION_NONE);

    UNIT_CHECK(letter_map_parse_action("uturn", &action));
    UNIT_EQ(action, LETTER_ACTION_UTURN);
    UNIT_CHECK(letter_map_parse_action("Left", &action));
    UNIT_EQ(action, LETTER_ACTION_LEFT);
    UNIT_CHECK(!letter_map_parse_action("back", &action));
    UNIT_CHECK(0 == strcmp(letter_map_action_name(LETTER_ACTION_RIGHT),
                           "right"));
    UNIT_END();
}

static void
test_line_read_fail (void)
{
    line_track_t track;
    uint32_t     reports = 0u;

    UNIT_BEGIN("S3-U18", "failed sensor reads: reported once at the limit, "
                         "a good read starts the count again (D-79)");
    line_track_init(&track);

    /* One short of the limit, then a good read: nothing reported. */
    for (uint32_t idx = 1u; idx < CFG_LINE_READ_FAIL_LIMIT; idx++)
    {
        UNIT_CHECK(!line_track_read_result(&track, false));
    }

    UNIT_CHECK(!line_track_read_result(&track, true));
    UNIT_EQ(line_track_read_fails(&track), 0u);

    /* The limit is reached on exactly the 4th failure in a row, and a
       longer run does not report again. */
    for (uint32_t idx = 1u; idx <= (3u * CFG_LINE_READ_FAIL_LIMIT); idx++)
    {
        if (line_track_read_result(&track, false))
        {
            reports++;
            UNIT_EQ(idx, (uint32_t) CFG_LINE_READ_FAIL_LIMIT);
        }
    }

    UNIT_EQ(reports, 1u);

    /* After a good read a new run reports again. */
    (void) line_track_read_result(&track, true);

    for (uint32_t idx = 1u; idx <= CFG_LINE_READ_FAIL_LIMIT; idx++)
    {
        reports += line_track_read_result(&track, false) ? 1u : 0u;
    }

    UNIT_EQ(reports, 2u);
    UNIT_CHECK(!line_track_read_result(NULL, false));
    UNIT_END();
}

/* ---- Line health after a bad sample (D-96): S3-U19 ---- */

/* The Line task's health work for one sample (line_task.c): returns
   the health it publishes after the sample, given the one before. */
static bool
test_health_step (line_keep_t * p_keep, bool b_sample_ok, bool b_health)
{
    if (line_keep_note_sample(p_keep, b_sample_ok))
    {
        b_health = false;                       /* at once */
    }

    if (line_keep_next_cycle(p_keep))
    {
        b_health = line_keep_window_ok(p_keep); /* the whole window */
        line_keep_set_window_ok(p_keep, true);
    }

    return (b_health);
}

static void
test_line_health (void)
{
    line_keep_t keep;
    bool        b_health = false;               /* as at power-up */
    bool        b_ok     = true;
    uint32_t    drops    = 0u;

    UNIT_BEGIN("S3-U19", "line health: the first bad sample of a window "
                         "(a failed or out-of-range read) drops it at "
                         "once, once per window; it comes back only at "
                         "the end of a whole window of good samples "
                         "(D-96)");
    line_keep_init(&keep);

    /* A whole good window: healthy only at its end. */
    for (uint32_t idx = 1u; idx <= LINE_STATUS_EVERY; idx++)
    {
        b_health = test_health_step(&keep, true, b_health);
        b_ok     = b_ok && (b_health == (LINE_STATUS_EVERY == idx));
    }

    UNIT_CHECK(b_ok);

    /* A bad sample early in the next window drops it at that sample,
       not at the window's end; the rest of the window stays down. */
    b_health = test_health_step(&keep, true, b_health);
    UNIT_CHECK(b_health);
    b_health = test_health_step(&keep, false, b_health);
    UNIT_CHECK(!b_health);

    for (uint32_t idx = 3u; idx <= LINE_STATUS_EVERY; idx++)
    {
        b_health = test_health_step(&keep, true, b_health);
        UNIT_CHECK(!b_health);
    }

    /* Then a whole good window brings it back, at its end only. */
    b_ok = true;

    for (uint32_t idx = 1u; idx <= LINE_STATUS_EVERY; idx++)
    {
        b_health = test_health_step(&keep, true, b_health);
        b_ok     = b_ok && (b_health == (LINE_STATUS_EVERY == idx));
    }

    UNIT_CHECK(b_ok);

    /* Many bad samples in one window ask for one drop. */
    for (uint32_t idx = 1u; idx < LINE_STATUS_EVERY; idx++)
    {
        drops += line_keep_note_sample(&keep, (0u != (idx % 3u))) ? 1u
                                                                  : 0u;
    }

    UNIT_EQ(drops, 1u);
    UNIT_CHECK(!line_keep_window_ok(&keep));
    UNIT_CHECK(!line_keep_note_sample(NULL, false));
    UNIT_END();
}

/* ---- Minimum-bit change (D-84): S3-U40 and up ---- */

static void
test_line_record (void)
{
    line_err_t rec;
    line_err_t older;
    bool       b_ok = true;

    UNIT_BEGIN("S3-U40", "IF-02 line record: two words with the exact "
                         "expected bits; every value round-trips (D-84)");
    UNIT_EQ(sizeof(line_err_t), 8u);
    line_err_init(&rec);
    UNIT_CHECK(!line_err_valid(&rec));
    UNIT_EQ(line_err_seq(&rec), 0u);

    /* Full left error, left sensor at full scale. */
    UNIT_CHECK(line_err_set_sample(&rec, -1000, 4095u, 0u, true));
    UNIT_EQ(rec.word[0], 0x00003C18u);
    UNIT_EQ(rec.word[1], 0x00000FFFu);

    /* Full right error, no line seen, right sensor at full scale. */
    UNIT_CHECK(line_err_set_sample(&rec, 1000, 0u, 4095u, false));
    UNIT_EQ(rec.word[0], 0x000053E8u);
    UNIT_EQ(rec.word[1], 0x00FFF000u);
    UNIT_EQ(line_err_error_pm(&rec), 1000);
    UNIT_EQ(line_err_raw_l(&rec), 0u);
    UNIT_EQ(line_err_raw_r(&rec), 4095u);
    UNIT_CHECK(!line_err_line_seen(&rec));
    UNIT_CHECK(line_err_valid(&rec));
    UNIT_EQ(line_err_seq(&rec), 2u);

    /* A failed read (D-79): not valid, counted, the rest kept. */
    line_err_set_invalid(&rec);
    UNIT_EQ(rec.word[0], 0x000063E8u);
    UNIT_EQ(rec.word[1], 0x00FFF000u);
    UNIT_CHECK(!line_err_valid(&rec));
    UNIT_EQ(line_err_error_pm(&rec), 1000);

    /* Every error and the reading ends round-trip. */
    for (int32_t error = -1000; error <= 1000; error++)
    {
        uint32_t const raw = (uint32_t) (error + 1000) * 2u;    /* 0..4000 */

        b_ok = b_ok &&
               line_err_set_sample(&rec, error, raw, 4095u - raw,
                                   (0 != (error % 2))) &&
               (line_err_error_pm(&rec) == error) &&
               (line_err_raw_l(&rec) == raw) &&
               (line_err_raw_r(&rec) == (4095u - raw)) &&
               (line_err_line_seen(&rec) == (0 != (error % 2))) &&
               line_err_valid(&rec) &&
               (0u == (rec.word[0] & 0xFFE00000u)) &&
               (0u == (rec.word[1] & 0xFF000000u));
    }

    UNIT_CHECK(b_ok);

    /* Samples between two reads, across the 8-bit wrap: 200 samples
       (one S3-U20 window) from sample 100 to sample 300. */
    line_err_init(&rec);

    for (uint32_t idx = 0u; idx < 100u; idx++)
    {
        (void) line_err_set_sample(&rec, 0, 0u, 0u, false);
    }

    older = rec;

    for (uint32_t idx = 0u; idx < 200u; idx++)
    {
        (void) line_err_set_sample(&rec, 0, 0u, 0u, false);
    }

    UNIT_EQ(line_err_seq(&rec), 300u % 256u);
    UNIT_EQ(line_err_samples(&rec, &older), 200u);

    /* Values that cannot occur are kept at the nearest end, reported. */
    UNIT_CHECK(!line_err_set_sample(&rec, 1100, 5000u, 4096u, true));
    UNIT_EQ(line_err_error_pm(&rec), 1023);
    UNIT_EQ(line_err_raw_l(&rec), 4095u);
    UNIT_EQ(line_err_raw_r(&rec), 4095u);
    UNIT_CHECK(!line_err_set_sample(&rec, -1100, 0u, 0u, true));
    UNIT_EQ(line_err_error_pm(&rec), -1024);

    /* No record. */
    UNIT_CHECK(!line_err_valid(NULL));
    UNIT_EQ(line_err_error_pm(NULL), 0);
    UNIT_CHECK(!line_err_set_sample(NULL, 0, 0u, 0u, false));
    UNIT_END();
}

static void
test_line_events_packed (void)
{
    line_evt_t evt;
    bool       b_ok = true;

    UNIT_BEGIN("S3-U41", "IF-01 line events: 16 bits with the exact "
                         "expected bits; every field round-trips (D-84)");
    UNIT_EQ(sizeof(line_evt_t), 2u);

    UNIT_CHECK(line_evt_pack(&evt, LINE_EVT_LETTER, 'Z', true));
    UNIT_EQ(evt.bits, 0x01D5u);
    UNIT_CHECK(line_evt_pack(&evt, LINE_EVT_CAPTURE_FAILED, '\0', false));
    UNIT_EQ(evt.bits, 0x0006u);
    UNIT_CHECK(line_evt_pack(&evt, LINE_EVT_JUNCTION, '\0', false));
    UNIT_EQ(evt.bits, 0x0001u);

    /* Every type, letter and direction. */
    for (uint8_t type = LINE_EVT_JUNCTION; type <= LINE_EVT_CAPTURE_FAILED;
         type++)
    {
        for (uint32_t idx = 0u; idx <= 26u; idx++)
        {
            char const letter = (26u == idx) ? '\0'
                                             : (char) ('A' + (int) idx);

            for (uint32_t rev = 0u; rev <= 1u; rev++)
            {
                bool const b_rev = (1u == rev);

                b_ok = b_ok &&
                       line_evt_pack(&evt, type, letter, b_rev) &&
                       (line_evt_type(&evt) == type) &&
                       (line_evt_letter(&evt) == letter) &&
                       (line_evt_reversed(&evt) == b_rev) &&
                       (0u == (evt.bits & 0xFE00u));
            }
        }
    }

    UNIT_CHECK(b_ok);

    /* Values that cannot occur: reported, nearest value kept. */
    UNIT_CHECK(!line_evt_pack(&evt, LINE_EVT_LETTER, '1', false));
    UNIT_EQ(line_evt_letter(&evt), '\0');
    UNIT_CHECK(!line_evt_pack(&evt, 8u, '\0', false));
    UNIT_EQ(line_evt_type(&evt), 7u);
    UNIT_EQ(line_evt_type(NULL), 0u);
    UNIT_EQ(line_evt_letter(NULL), '\0');
    UNIT_END();
}

/* ---- Old and packed capture side by side (S3-U43) ---- */

#define TEST_EQ_SCENARIOS   4000u
#define TEST_EQ_STEPS       240u

typedef struct
{
    uint32_t state;
} test_rng_t;

/* xorshift32: the same sequence on every host. */
static uint32_t
test_rand (test_rng_t * p_rng)
{
    uint32_t x = p_rng->state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    p_rng->state = x;

    return (x);
}

static uint32_t
test_range (test_rng_t * p_rng, uint32_t low, uint32_t high)
{
    return (low + (test_rand(p_rng) % ((high - low) + 1u)));
}

typedef struct
{
    ref_bc_capture_t ref;
    bc_capture_t     cap;
    uint32_t         next_cycle_us;     /* next 5 ms Line cycle */
    uint32_t         mismatches;
    uint32_t         outputs[4];        /* by bc_out_type_t */
    uint32_t         reasons[BC_FAIL_EDGE_LOST + 1u];
    /* The Line task held up (S3-U49 only; 0 = never): */
    uint32_t         late_max_us;       /* longest hold-up */
    uint32_t         stall_from_us;     /* no Line cycle from here ... */
    uint32_t         stall_us;          /* ... for this long */
    uint32_t         high_edges;        /* edges stored past 2^21 us */
    uint32_t         high_letters;      /* letters read from such edges */
    char             last_letter;       /* the last letter read */
} test_pair_t;

/* Offsets that need bit 21 (2^21 us, about 2.1 s) of the 22. */
#define TEST_OFFSET_HIGH_US 0x200000u

/* Edges stored in the packed capture and the offset of stored edge
   index (1..63), read from the layout in bc_capture.h. */
static uint32_t
test_cap_count (bc_capture_t const * p_cap)
{
    return (bitpack_get(p_cap->flags, BC_CAP_COUNT_SHIFT, BC_CAP_COUNT_BITS));
}

static uint32_t
test_cap_offset (bc_capture_t const * p_cap, uint32_t index)
{
    return (bitpack_array_get(p_cap->offsets, index - 1u,
                              BC_CAP_OFFSET_BITS));
}

static bool
test_same_out (bc_out_t const * p_a, bc_out_t const * p_b)
{
    return ((p_a->type == p_b->type) &&
            (p_a->result.letter == p_b->result.letter) &&
            (p_a->result.b_reversed == p_b->result.b_reversed) &&
            (p_a->result.reason == p_b->result.reason));
}

static void
test_pair_check_state (test_pair_t * p_pair)
{
    if ((p_pair->ref.state != bc_capture_state(&p_pair->cap)) ||
        (p_pair->ref.b_overflow != bc_capture_overflowed(&p_pair->cap)))
    {
        p_pair->mismatches++;
    }
}

/* The Line cycles up to until_us: poll both up to 3 times each. */
static void
test_pair_cycles (test_pair_t * p_pair, uint32_t until_us,
                  test_rng_t * p_rng)
{
    while ((int32_t) (until_us - p_pair->next_cycle_us) >= 0)
    {
        uint32_t const now_us = p_pair->next_cycle_us;

        if ((0u != p_pair->stall_us) &&
            ((now_us - p_pair->stall_from_us) < p_pair->stall_us))
        {
            /* The Line task is held up: its next cycle runs when the
               hold-up ends, and the edges meanwhile wait in the queue. */
            p_pair->next_cycle_us = p_pair->stall_from_us + p_pair->stall_us;
            p_pair->stall_us = 0u;
            continue;
        }

        for (uint32_t poll = 0u; poll < TEST_LINE_POLL_MAX; poll++)
        {
            bc_out_t const old_out = ref_bc_capture_poll(&p_pair->ref, now_us);
            bc_out_t const new_out = bc_capture_poll(&p_pair->cap, now_us);

            if (!test_same_out(&old_out, &new_out))
            {
                p_pair->mismatches++;
            }

            if (BC_OUT_LETTER == new_out.type)
            {
                p_pair->last_letter = new_out.result.letter;

                if (test_cap_offset(&p_pair->cap, BC_SYMBOL_ELEMENTS) >=
                    TEST_OFFSET_HIGH_US)
                {
                    /* Read from edges past 2^21 us (the offsets stay in
                       the buffer after the capture ends). */
                    p_pair->high_letters++;
                }
            }

            p_pair->outputs[(uint32_t) old_out.type & 3u]++;

            if ((BC_OUT_FAILED == old_out.type) &&
                (old_out.result.reason <= BC_FAIL_EDGE_LOST))
            {
                p_pair->reasons[old_out.result.reason]++;
            }

            test_pair_check_state(p_pair);

            if (BC_OUT_NONE == old_out.type)
            {
                break;
            }
        }

        /* 5 ms, with the release up to 1 ms late (NFR1). */
        p_pair->next_cycle_us += TEST_LINE_PERIOD_US +
                                 test_range(p_rng, 0u, 1000u) - 500u;
    }
}

/*
 * One edge, through the queue as the Line task sees it: packed by the
 * ISR, rebuilt from the clock at the next cycle, which must give back
 * the exact time, then given to the packed capture; the old capture
 * gets the true time.
 */
static void
test_pair_edge (test_pair_t * p_pair, uint32_t time_us, bool b_dark,
                test_rng_t * p_rng)
{
    barcode_edge_t const edge = barcode_edge_pack(time_us, b_dark);
    uint32_t             now_us = time_us + test_range(p_rng, 0u, 6000u);
    uint32_t const       count  = test_cap_count(&p_pair->cap);

    test_pair_cycles(p_pair, time_us - 1u, p_rng);

    if ((0u != p_pair->late_max_us) &&
        ((int32_t) (p_pair->next_cycle_us - now_us) > 0))
    {
        now_us = p_pair->next_cycle_us;     /* taken after a hold-up */
    }

    if ((barcode_edge_time(&edge, now_us) != time_us) ||
        (barcode_edge_dark(&edge) != b_dark))
    {
        p_pair->mismatches++;
    }

    ref_bc_capture_edge(&p_pair->ref, time_us, b_dark);
    bc_capture_edge(&p_pair->cap, barcode_edge_time(&edge, now_us),
                    barcode_edge_dark(&edge));
    test_pair_check_state(p_pair);

    if ((test_cap_count(&p_pair->cap) == (count + 1u)) && (count >= 1u) &&
        (test_cap_offset(&p_pair->cap, count) >= TEST_OFFSET_HIGH_US))
    {
        p_pair->high_edges++;
    }
}

/*
 * A symbol of a random letter, speed, drift and direction, sometimes
 * with glitches, a lost edge or extra edges.  When the Line task can be
 * late (S3-U49) the symbol is slow: narrow 54 to 57 ms, wide about 2x,
 * +/-3% drift, so its 29 elements take 1.9 to 2.3 s and no element is
 * as long as the 120 ms that ends a capture; and the Line task is held
 * up once near the 2 s limit.
 */
static uint32_t
test_pair_symbol (test_pair_t * p_pair, uint32_t time_us, bool * p_level,
                  test_rng_t * p_rng)
{
    uint32_t       widths[BC_SYMBOL_ELEMENTS];
    bool const     b_slow = (0u != p_pair->late_max_us);
    uint32_t const narrow = b_slow ? test_range(p_rng, 54000u, 57000u)
                                   : test_range(p_rng, 1500u, 30000u);
    uint32_t const wide   = (narrow * (b_slow
                                       ? test_range(p_rng, 190u, 200u)
                                       : test_range(p_rng, 220u, 340u))) /
                            100u;
    uint32_t const letter = test_range(p_rng, 0u, 25u);
    uint32_t const flaw   = test_range(p_rng, 0u, 9u);
    char           middle[BC_CHAR_ELEMENTS + 1u];

    (void) memcpy(middle, g_letters[letter], sizeof(middle));

    if (4u == flaw)
    {
        /* A Code 39 character that is not a letter. */
        (void) memcpy(middle, g_not_letters[test_range(p_rng, 0u, 3u)],
                      sizeof(middle));
    }
    else if (5u == flaw)
    {
        /* Any three wide elements: often no Code 39 character. */
        for (uint32_t idx = 0u; idx < BC_CHAR_ELEMENTS; idx++)
        {
            middle[idx] = 'n';
        }

        for (uint32_t done = 0u; done < 3u; )
        {
            uint32_t const idx = test_range(p_rng, 0u, BC_CHAR_ELEMENTS - 1u);

            if ('n' == middle[idx])
            {
                middle[idx] = 'w';
                done++;
            }
        }
    }
    else
    {
        /* The letter as printed. */
    }

    test_symbol(middle, narrow, wide, narrow, wide, widths);

    if (0u == (test_rand(p_rng) % 2u))
    {
        test_reverse(widths, BC_SYMBOL_ELEMENTS);
    }

    if (!*p_level)
    {
        /* A symbol starts with a bar: from white. */
    }
    else
    {
        time_us += test_range(p_rng, 2000u, 50000u);
        *p_level = false;
        test_pair_edge(p_pair, time_us, false, p_rng);
    }

    time_us += test_range(p_rng, 20000u, 200000u);

    if (b_slow)
    {
        /* No Line cycle for up to late_max_us, from 1.9 to 2.0 s after
           the first bar. */
        p_pair->stall_from_us = time_us + test_range(p_rng, 1900000u,
                                                     2000000u);
        p_pair->stall_us      = test_range(p_rng, 0u, p_pair->late_max_us);
    }

    for (uint32_t edge = 0u; edge <= BC_SYMBOL_ELEMENTS; edge++)
    {
        bool     b_dark = (0u == (edge % 2u));
        uint32_t width  = 0u;

        if ((1u == flaw) && (17u == edge))
        {
            b_dark = !b_dark;           /* an edge lost inside */
        }

        test_pair_edge(p_pair, time_us, b_dark, p_rng);
        *p_level = b_dark;

        if (edge < BC_SYMBOL_ELEMENTS)
        {
            /* +/-25% speed change across the symbol (+/-3% if slow). */
            uint32_t const drift = b_slow
                                   ? (97u + test_range(p_rng, 0u, 6u))
                                   : (75u + test_range(p_rng, 0u, 50u));

            width = (widths[edge] * drift) / 100u;
        }

        if ((2u == flaw) && (width > 2500u) &&
            (0u == (test_rand(p_rng) % 6u)))
        {
            /* A short flicker inside the element (edges stay in time
               order, as the ISR gives them). */
            uint32_t const at = time_us + test_range(p_rng, 100u, 400u);

            test_pair_edge(p_pair, at, !b_dark, p_rng);
            test_pair_edge(p_pair, at + test_range(p_rng, 10u, 1100u),
                           b_dark, p_rng);
        }

        if (3u == flaw)
        {
            bc_capture_mark_lost(&p_pair->cap);     /* the ISR dropped one */
            ref_bc_capture_mark_lost(&p_pair->ref);
            test_pair_check_state(p_pair);
        }

        time_us += width;
    }

    return (time_us);
}

static void
test_capture_equivalent (void)
{
    static test_pair_t pair;
    test_rng_t         rng = { 0x2004C0DEu };
    uint32_t           total_mismatch = 0u;
    uint32_t           outputs[4] = { 0u, 0u, 0u, 0u };
    uint32_t           reasons[BC_FAIL_EDGE_LOST + 1u];

    UNIT_BEGIN("S3-U43", "barcode capture: the packed and the old version "
                         "give the same output at every poll, 4000 random "
                         "scenarios (D-85)");
    (void) memset(reasons, 0, sizeof(reasons));

    for (uint32_t scen = 0u; scen < TEST_EQ_SCENARIOS; scen++)
    {
        /* Any start time, so the 32-bit clock wraps in some runs. */
        uint32_t time_us = test_rand(&rng);
        bool     b_level = (0u == (test_rand(&rng) % 2u));

        (void) memset(&pair, 0, sizeof(pair));
        ref_bc_capture_init(&pair.ref, b_level, time_us);
        bc_capture_init(&pair.cap, b_level, time_us);
        pair.next_cycle_us = time_us + TEST_LINE_PERIOD_US;

        if (0u == (test_rand(&rng) % 5u))
        {
            /* The optional quiet zone (D-25), as S3-U08 sets it. */
            uint32_t const quiet_ms = test_range(&rng, 1u, 200u);

            pair.ref.quiet_us = quiet_ms * 1000u;
            (void) bc_capture_set_quiet_ms(&pair.cap, quiet_ms);
        }

        for (uint32_t step = 0u; step < TEST_EQ_STEPS; step++)
        {
            uint32_t const pick = test_range(&rng, 0u, 99u);

            if (pick < 8u)
            {
                time_us = test_pair_symbol(&pair, time_us, &b_level, &rng);
            }
            else if (pick < 90u)
            {
                /* Noise: element, glitch, long gap, or a same-level edge
                   (a lost edge). */
                uint32_t const kind = test_range(&rng, 0u, 9u);
                uint32_t       gap  = 0u;

                if (kind < 4u)
                {
                    gap = test_range(&rng, 1500u, 45000u);
                }
                else if (kind < 6u)
                {
                    gap = test_range(&rng, 0u, 1200u);
                }
                else if (kind < 8u)
                {
                    gap = test_range(&rng, 50000u, 400000u);
                }
                else
                {
                    gap = test_range(&rng, 400000u, 3000000u);
                }

                time_us += gap;

                if (0u != (test_rand(&rng) % 25u))
                {
                    b_level = !b_level;
                }

                test_pair_edge(&pair, time_us, b_level, &rng);
            }
            else if (pick < 95u)
            {
                bc_capture_mark_lost(&pair.cap);
                ref_bc_capture_mark_lost(&pair.ref);
                test_pair_check_state(&pair);
            }
            else
            {
                time_us += test_range(&rng, 1000u, 300000u);
                test_pair_cycles(&pair, time_us, &rng);
            }
        }

        test_pair_cycles(&pair, time_us + 3000000u, &rng);
        total_mismatch += pair.mismatches;

        for (uint32_t idx = 0u; idx < 4u; idx++)
        {
            outputs[idx] += pair.outputs[idx];
        }

        for (uint32_t idx = 0u; idx <= BC_FAIL_EDGE_LOST; idx++)
        {
            reasons[idx] += pair.reasons[idx];
        }
    }

    UNIT_EQ(total_mismatch, 0u);

    /* The scenarios really reached every kind of outcome. */
    UNIT_CHECK(outputs[BC_OUT_STARTED] > 1000u);
    UNIT_CHECK(outputs[BC_OUT_LETTER] > 500u);
    UNIT_CHECK(outputs[BC_OUT_FAILED] > 1000u);
    /* Every failure reason, except BC_FAIL_PATTERN when characters are
       classified one by one: a character then only classifies with 2
       wide bars and 1 wide space, and all 40 such patterns are Code 39
       characters, so bc_decode() cannot report it (the decoder is not
       changed here). */
    for (uint32_t idx = BC_FAIL_COUNT; idx <= BC_FAIL_EDGE_LOST; idx++)
    {
        UNIT_CHECK((reasons[idx] > 0u) ||
                   ((BC_FAIL_PATTERN == idx) &&
                    (0 != CFG_BC_CLASSIFY_PER_CHAR)));
    }
    UNIT_END();
}

/* ---- The Line task held up near the 2 s limit (S3-U49) ---- */

#define TEST_LATE_SCENARIOS 1500u
#define TEST_LATE_SYMBOLS   4u
/* About the longest the Line task can be held up once the watchdog is
   armed (D-85): its 50 ms check-in deadline, up to 10 ms for Safety to
   see it missed, the 200 ms watchdog, and a cycle. */
#define TEST_LATE_MAX_US    270000u

/*
 * S3-U43 runs every Line cycle on time, so a capture always ends at the
 * first poll after 2 s and no stored offset reaches 2^21 us (2.1 s).
 * Here the Line task is held up for up to 0.27 s from 1.9 to 2.0 s after
 * the first bar while a slow symbol goes on: the edges meanwhile are
 * stored up to about 2.2 s after the first bar, so bit 21 of the offset
 * is written and read back (element widths, glitch filter, end checks).
 */
static void
test_capture_late (void)
{
    static test_pair_t pair;
    test_rng_t         rng          = { 0x1A7E2004u };
    uint32_t           mismatches   = 0u;
    uint32_t           high_edges   = 0u;
    uint32_t           high_letters = 0u;
    uint32_t           too_long     = 0u;

    UNIT_BEGIN("S3-U49", "barcode capture with the Line task held up for "
                         "up to 0.27 s near the 2 s limit: edges stored "
                         "up to 2.2 s after the first bar (bit 21 of the "
                         "offset) are read back exactly; the packed and "
                         "the old version give the same output at every "
                         "poll (D-85)");

    for (uint32_t scen = 0u; scen < TEST_LATE_SCENARIOS; scen++)
    {
        /* Any start time, so the 32-bit clock wraps in some runs. */
        uint32_t time_us = test_rand(&rng);
        bool     b_level = false;

        (void) memset(&pair, 0, sizeof(pair));
        ref_bc_capture_init(&pair.ref, false, time_us);
        bc_capture_init(&pair.cap, false, time_us);
        pair.next_cycle_us = time_us + TEST_LINE_PERIOD_US;
        pair.late_max_us   = TEST_LATE_MAX_US;

        for (uint32_t sym = 0u; sym < TEST_LATE_SYMBOLS; sym++)
        {
            time_us = test_pair_symbol(&pair, time_us, &b_level, &rng);

            /* White long enough for the capture to end. */
            time_us += test_range(&rng, 300000u, 600000u);
            test_pair_cycles(&pair, time_us, &rng);
        }

        mismatches   += pair.mismatches;
        high_edges   += pair.high_edges;
        high_letters += pair.high_letters;
        too_long     += pair.reasons[BC_FAIL_TOO_LONG];
    }

    UNIT_EQ(mismatches, 0u);

    /* The runs really stored such edges (675) and read letters from them
       (484), and captures really ended at the limit (2859). */
    UNIT_CHECK(high_edges > 300u);
    UNIT_CHECK(high_letters > 200u);
    UNIT_CHECK(too_long > 1000u);
    UNIT_END();
}

static void
test_edge_record (void)
{
    test_rng_t     rng  = { 0x0BADC0DEu };
    bool           b_ok = true;
    barcode_edge_t edge;

    UNIT_BEGIN("S3-U42", "barcode edge record: one word with the exact "
                         "expected bits; the full time is rebuilt exactly "
                         "for any edge under 2^31 us old (D-85)");
    UNIT_EQ(sizeof(barcode_edge_t), 4u);

    edge = barcode_edge_pack(0x80000005u, true);
    UNIT_EQ(edge.bits, 0x80000005u);
    edge = barcode_edge_pack(0x80000005u, false);
    UNIT_EQ(edge.bits, 0x00000005u);
    edge = barcode_edge_pack(0x7FFFFFFFu, false);
    UNIT_EQ(edge.bits, 0x7FFFFFFFu);
    UNIT_CHECK(!barcode_edge_dark(&edge));
    edge = barcode_edge_pack(0xFFFFFFFFu, true);
    UNIT_EQ(edge.bits, 0xFFFFFFFFu);
    UNIT_CHECK(barcode_edge_dark(&edge));

    /* The ends: taken at once, and just under 2^31 us later; across the
       32-bit wrap of the clock. */
    edge = barcode_edge_pack(0xFFFFFFF0u, true);
    UNIT_EQ(barcode_edge_time(&edge, 0xFFFFFFF0u), 0xFFFFFFF0u);
    UNIT_EQ(barcode_edge_time(&edge, 0x00000010u), 0xFFFFFFF0u);
    UNIT_EQ(barcode_edge_time(&edge, 0xFFFFFFF0u + 0x7FFFFFFFu),
            0xFFFFFFF0u);

    for (uint32_t idx = 0u; idx < 200000u; idx++)
    {
        uint32_t const time_us = test_rand(&rng);
        uint32_t const age_us  = (0u == (idx % 4u))
                                 ? test_range(&rng, 0u, 20000u)
                                 : (test_rand(&rng) & 0x7FFFFFFFu);
        bool const     b_dark  = (0u != (idx % 3u));

        edge = barcode_edge_pack(time_us, b_dark);
        b_ok = b_ok &&
               (barcode_edge_time(&edge, time_us + age_us) == time_us) &&
               (barcode_edge_dark(&edge) == b_dark);
    }

    UNIT_CHECK(b_ok);
    UNIT_END();
}

/*
 * 'M' at 58 ms narrow and 116 ms wide (2.2 s long; no element reaches
 * the 120 ms that ends a capture), with a 0.5 ms flicker in its last
 * space, given to the old and the packed captures.  With stall_us > 0
 * the Line task is held up that long from 1.95 s after the first bar,
 * so the last two edges are stored 2.146 and 2.204 s after it: past
 * 2^21 us (2.097 s).
 */
static void
test_slow_letter (test_pair_t * p_pair, uint32_t start_us, uint32_t stall_us)
{
    test_rng_t rng     = { 0x5104C0DEu };
    uint32_t   time_us = start_us;
    uint32_t   widths[BC_SYMBOL_ELEMENTS];

    (void) memset(p_pair, 0, sizeof(*p_pair));
    ref_bc_capture_init(&p_pair->ref, false, start_us - 400000u);
    bc_capture_init(&p_pair->cap, false, start_us - 400000u);
    p_pair->next_cycle_us = (start_us - 400000u) + TEST_LINE_PERIOD_US;
    p_pair->late_max_us   = TEST_LATE_MAX_US;
    p_pair->stall_from_us = start_us + 1950000u;
    p_pair->stall_us      = stall_us;

    test_symbol(g_letters['M' - 'A'], 58000u, 116000u, 58000u, 116000u,
                widths);

    for (uint32_t edge = 0u; edge <= BC_SYMBOL_ELEMENTS; edge++)
    {
        test_pair_edge(p_pair, time_us, (0u == (edge % 2u)), &rng);

        if (27u == edge)
        {
            /* The flicker, which the glitch filter drops. */
            test_pair_edge(p_pair, time_us + 12000u, true, &rng);
            test_pair_edge(p_pair, time_us + 12500u, false, &rng);
        }

        if (edge < BC_SYMBOL_ELEMENTS)
        {
            time_us += widths[edge];
        }
    }

    test_pair_cycles(p_pair, time_us + 500000u, &rng);
}

static void
test_capture_packed (void)
{
    static bc_capture_t cap;
    static test_pair_t  pair;
    bool                b_started = false;
    bc_out_t            out;

    UNIT_BEGIN("S3-U44", "barcode capture state: packed words with the "
                         "exact expected bits; the edge limit of 2^22 us "
                         "after the first bar; edges stored past 2^21 us "
                         "are read back exactly (D-85)");
    UNIT_EQ(BC_CAP_OFFSET_BITS, 22u);
    UNIT_EQ(sizeof(bc_capture_t), (4u + 44u) * 4u);

    /* First bar, space, bar. */
    bc_capture_init(&cap, false, 1000u);
    UNIT_EQ(cap.flags, 0x00000000u);
    bc_capture_edge(&cap, 500000u, true);
    bc_capture_edge(&cap, 510000u, false);
    bc_capture_edge(&cap, 540000u, true);
    UNIT_EQ(cap.flags, 0x00000C03u);       /* CAPTURING, dark, 3 edges */
    UNIT_EQ(cap.first_us, 500000u);
    UNIT_EQ(cap.offsets[0], 0x10002710u);  /* 10000; low bits of 40000 */
    UNIT_EQ(cap.offsets[1], 0x00000027u);  /* high bits of 40000 */
    UNIT_EQ(cap.white_since_us, 1000u);
    UNIT_EQ(cap.quiet_from_us, 1000u);

    /* The quiet zone is kept in ms. */
    UNIT_CHECK(bc_capture_set_quiet_ms(&cap, 255u));
    UNIT_EQ(bc_capture_quiet_ms(&cap), 255u);
    UNIT_CHECK(!bc_capture_set_quiet_ms(&cap, 256u));
    UNIT_EQ(bc_capture_quiet_ms(&cap), 255u);

    /* An edge 2^22 - 1 us after the first bar is stored; 2^22 us cannot
       be, and ends the capture as too long (the old version stored it;
       it needs the Line task held up past its watchdog, D-85). */
    bc_capture_init(&cap, false, 0u);
    bc_capture_edge(&cap, 100000u, true);
    bc_capture_edge(&cap, 100000u + 0x3FFFFFu, false);
    UNIT_CHECK(!bc_capture_overflowed(&cap));
    bc_capture_init(&cap, false, 0u);
    bc_capture_edge(&cap, 100000u, true);
    bc_capture_edge(&cap, 100000u + 0x3FF000u, false);
    UNIT_CHECK(!bc_capture_overflowed(&cap));
    bc_capture_edge(&cap, 100000u + 0x400000u, true);
    UNIT_CHECK(bc_capture_overflowed(&cap));
    out = test_poll(&cap, 100000u + 0x400000u, &b_started);
    UNIT_EQ(out.type, BC_OUT_FAILED);
    UNIT_EQ(out.result.reason, BC_FAIL_TOO_LONG);
    UNIT_EQ(bc_capture_state(&cap), BC_STATE_WAITING);

    /* Bit 21 of the offset written and read back: a slow letter with the
       Line task held up 0.26 s from 1.95 s after the first bar (within
       the 0.27 s the watchdog allows).  Its last two edges are stored
       2.146 and 2.204 s after the first bar, the flicker between them is
       dropped and the letter is read, as by the old version; on time,
       the capture ends at 2 s as too long.  Also across the clock's
       32-bit wrap. */
    for (uint32_t run = 0u; run < 2u; run++)
    {
        uint32_t const start_us = (0u == run) ? 500000u : 0xFFF00000u;

        test_slow_letter(&pair, start_us, 260000u);
        UNIT_EQ(pair.mismatches, 0u);
        UNIT_EQ(test_cap_offset(&pair.cap, 28u), 2146000u);    /* 0x20BED0 */
        UNIT_EQ(test_cap_offset(&pair.cap, 29u), 2204000u);    /* 0x21A160 */
        UNIT_EQ(pair.outputs[BC_OUT_LETTER], 1u);
        UNIT_EQ(pair.last_letter, 'M');
        UNIT_EQ(pair.high_letters, 1u);
        UNIT_EQ(pair.outputs[BC_OUT_FAILED], 0u);

        /* On time: too long at 2 s; the last edges then start a new
           capture, which ends with too few elements. */
        test_slow_letter(&pair, start_us, 0u);
        UNIT_EQ(pair.mismatches, 0u);
        UNIT_EQ(pair.outputs[BC_OUT_LETTER], 0u);
        UNIT_EQ(pair.outputs[BC_OUT_FAILED], 2u);
        UNIT_EQ(pair.reasons[BC_FAIL_TOO_LONG], 1u);
        UNIT_EQ(pair.reasons[BC_FAIL_TOO_FEW], 1u);
    }

    UNIT_EQ(bc_capture_state(NULL), BC_STATE_WAITING);
    UNIT_CHECK(!bc_capture_overflowed(NULL));
    UNIT_END();
}

/* A reading near a level, kept within 0..4095 (the ADC's range). */
static uint32_t
test_reading (test_rng_t * p_rng, int32_t level, int32_t noise)
{
    int32_t value = level + (int32_t) test_range(p_rng, 0u,
                                                 (uint32_t) (2 * noise)) -
                    noise;

    value = (value < 0) ? 0 : value;
    value = (value > 4095) ? 4095 : value;

    return ((uint32_t) value);
}

static bool
test_track_same (ref_line_track_t const * p_ref, line_track_t const * p_new)
{
    return ((p_ref->error_pm == line_track_error_pm(p_new)) &&
            (p_ref->b_line_seen == line_track_line_seen(p_new)) &&
            (p_ref->b_lost == line_track_lost(p_new)) &&
            (p_ref->white_l == line_track_level(p_new, LINE_TRACK_WHITE_L)) &&
            (p_ref->black_l == line_track_level(p_new, LINE_TRACK_BLACK_L)) &&
            (p_ref->white_r == line_track_level(p_new, LINE_TRACK_WHITE_R)) &&
            (p_ref->black_r == line_track_level(p_new, LINE_TRACK_BLACK_R)) &&
            (p_ref->read_fail_run == line_track_read_fails(p_new)));
}

static void
test_track_equivalent (void)
{
    static ref_line_track_t ref;
    static line_track_t     track;
    test_rng_t              rng = { 0x5EED0003u };
    uint32_t                mismatches = 0u;
    uint32_t                events[4] = { 0u, 0u, 0u, 0u };
    uint32_t                reports = 0u;

    UNIT_BEGIN("S3-U45", "line tracking: packed state in three words with "
                         "the exact expected bits; the packed and the old "
                         "version agree on every sample of 600 random runs "
                         "(D-86)");
    UNIT_EQ(sizeof(line_track_t), 12u);

    line_track_init(&track);
    UNIT_EQ(track.word[0], 0x20BB812Cu);    /* levels, junction armed */
    UNIT_EQ(track.word[1], 0x00BB812Cu);
    UNIT_EQ(track.word[2], 0x00000000u);
    (void) line_track_sample(&track, CFG_LINE_L_BLACK, CFG_LINE_R_WHITE,
                             5u, false);
    UNIT_EQ(track.word[0], 0xA0BB812Cu);    /* + line seen */
    UNIT_EQ(track.word[2], 0x000003E8u);    /* error 1000 */

    for (uint32_t run = 0u; run < 600u; run++)
    {
        uint32_t regime = 0u;
        uint32_t left_for = 0u;

        ref_line_track_init(&ref);
        line_track_init(&track);

        for (uint32_t step = 0u; step < 2000u; step++)
        {
            uint32_t raw_l = 0u;
            uint32_t raw_r = 0u;
            uint32_t period = CFG_PERIOD_LINE_MS;
            bool     b_adapt = (0u != (run % 4u));
            ref_line_track_evt_t old_evt = REF_LINE_TRACK_EVT_NONE;
            line_track_evt_t     new_evt = LINE_TRACK_EVT_NONE;

            if (0u == left_for)
            {
                regime   = test_range(&rng, 0u, 7u);
                left_for = test_range(&rng, 1u, 150u);
            }

            left_for--;

            switch (regime)
            {
                case 0u:    /* on the line: both partly dark */
                    raw_l = test_reading(&rng, 1650, 400);
                    raw_r = test_reading(&rng, 1650, 400);
                break;

                case 1u:    /* junction crossbar or a dark patch */
                    raw_l = test_reading(&rng, 3000, 300);
                    raw_r = test_reading(&rng, 3000, 300);
                break;

                case 2u:    /* off the line: both white */
                    raw_l = test_reading(&rng, 300, 200);
                    raw_r = test_reading(&rng, 300, 200);
                break;

                case 3u:    /* line under the left sensor */
                    raw_l = test_reading(&rng, 2800, 400);
                    raw_r = test_reading(&rng, 400, 300);
                break;

                case 4u:    /* line under the right sensor */
                    raw_l = test_reading(&rng, 400, 300);
                    raw_r = test_reading(&rng, 2800, 400);
                break;

                case 5u:    /* lighting change: levels drift */
                    raw_l = test_reading(&rng, 2300 + (int32_t) (step % 700u),
                                         150);
                    raw_r = test_reading(&rng, 600 + (int32_t) (step % 500u),
                                         150);
                break;

                case 6u:    /* any reading */
                    raw_l = test_range(&rng, 0u, 4095u);
                    raw_r = test_range(&rng, 0u, 4095u);
                break;

                default:    /* the ends of the scale */
                    raw_l = (0u == (test_rand(&rng) % 2u)) ? 0u : 4095u;
                    raw_r = (0u == (test_rand(&rng) % 2u)) ? 0u : 4095u;
                break;
            }

            if (0u == (test_rand(&rng) % 50u))
            {
                period = test_range(&rng, 1u, 400u);    /* any period */
            }

            if (0u == (test_rand(&rng) % 40u))
            {
                /* A run of failed reads, then a good one or not. */
                uint32_t const fails = test_range(&rng, 1u, 7u);

                for (uint32_t idx = 0u; idx < fails; idx++)
                {
                    bool const b_old = ref_line_track_read_result(&ref, false);
                    bool const b_new = line_track_read_result(&track, false);

                    mismatches += (b_old != b_new) ? 1u : 0u;
                    reports += b_old ? 1u : 0u;
                }
            }

            mismatches += (ref_line_track_read_result(&ref, true) !=
                           line_track_read_result(&track, true)) ? 1u : 0u;

            old_evt = ref_line_track_sample(&ref, raw_l, raw_r, period,
                                            b_adapt);
            new_evt = line_track_sample(&track, raw_l, raw_r, period,
                                        b_adapt);

            if (((uint32_t) old_evt != (uint32_t) new_evt) ||
                !test_track_same(&ref, &track))
            {
                mismatches++;
            }

            events[(uint32_t) old_evt & 3u]++;
        }
    }

    UNIT_EQ(mismatches, 0u);
    UNIT_CHECK(events[LINE_TRACK_EVT_JUNCTION] > 1000u);
    UNIT_CHECK(events[LINE_TRACK_EVT_LOST] > 1000u);
    UNIT_CHECK(events[LINE_TRACK_EVT_FOUND] > 1000u);
    UNIT_CHECK(reports > 100u);
    UNIT_END();
}

static void
test_steering_equivalent (void)
{
    ref_steering_t ref;
    steering_t     steer;
    test_rng_t     rng = { 0x5EED0004u };
    uint32_t       mismatches = 0u;

    UNIT_BEGIN("S3-U46", "steering: 16-bit state with the exact expected "
                         "bits; the packed and the old version give the "
                         "same speed and difference for 200,000 random "
                         "steps (D-86)");
    UNIT_EQ(sizeof(steering_t), 2u);
    steering_reset(&steer);
    UNIT_EQ(steer.bits, 0x0000u);

    {
        int32_t speed = 0;
        int32_t diff  = 0;

        steering_compute(&steer, -1000, 250, &speed, &diff);
        UNIT_EQ(steer.bits, 0x0C18u);       /* -1000, have previous */
        steering_compute(&steer, 5000, 250, &speed, &diff);
        UNIT_EQ(steer.bits, 0x0BE8u);       /* clamped to 1000 */
    }

    ref_steering_reset(&ref);
    steering_reset(&steer);

    for (uint32_t step = 0u; step < 200000u; step++)
    {
        int32_t const error = (int32_t) test_range(&rng, 0u, 12000u) - 6000;
        int32_t const base  = (int32_t) test_range(&rng, 0u, 700u);
        int32_t old_speed = 0;
        int32_t old_diff  = 0;
        int32_t new_speed = 0;
        int32_t new_diff  = 0;

        if (0u == (test_rand(&rng) % 30u))
        {
            ref_steering_reset(&ref);
            steering_reset(&steer);
        }

        ref_steering_compute(&ref, error, base, &old_speed, &old_diff);
        steering_compute(&steer, error, base, &new_speed, &new_diff);
        mismatches += ((old_speed != new_speed) || (old_diff != new_diff))
                      ? 1u : 0u;
    }

    UNIT_EQ(mismatches, 0u);
    UNIT_END();
}

static void
test_letter_map_equivalent (void)
{
    ref_letter_map_t ref;
    letter_map_t     map;
    test_rng_t       rng = { 0x5EED0005u };
    uint32_t         mismatches = 0u;

    UNIT_BEGIN("S3-U47", "letter map: 26 actions of 3 bits in three words "
                         "with the exact expected bits; the packed and the "
                         "old version agree on 100,000 random remaps and "
                         "reads (D-86)");
    UNIT_EQ(sizeof(letter_map_t), 12u);
    letter_map_default(&map);
    UNIT_EQ(map.word[0], 0x000008D1u);      /* A-D: 1, 2, 3, 4 */
    UNIT_EQ(map.word[1], 0x00000000u);
    UNIT_EQ(map.word[2], 0x00000000u);

    /* Z (bits 75-77) is in the last word; K (bits 30-32) spans two. */
    UNIT_CHECK(letter_map_set(&map, 'Z', LETTER_ACTION_UTURN));
    UNIT_EQ(map.word[2], 0x00002000u);
    UNIT_CHECK(letter_map_set(&map, 'K', LETTER_ACTION_UTURN));
    UNIT_EQ(map.word[0], 0x000008D1u);      /* 4 = 100b: top bit only */
    UNIT_EQ(map.word[1], 0x00000001u);
    UNIT_CHECK(letter_map_set(&map, 'K', LETTER_ACTION_RIGHT));
    UNIT_EQ(map.word[0], 0xC00008D1u);      /* 3 = 011b: low bits only */
    UNIT_EQ(map.word[1], 0x00000000u);
    UNIT_EQ(letter_map_get(&map, 'K'), LETTER_ACTION_RIGHT);

    ref_letter_map_default(&ref);
    letter_map_default(&map);

    for (uint32_t step = 0u; step < 100000u; step++)
    {
        /* Letters, lower case, and characters around them. */
        char const letter = (char) test_range(&rng, 40u, 125u);
        uint32_t const pick = test_range(&rng, 0u, 99u);

        if (pick < 45u)
        {
            letter_action_t const action =
                (letter_action_t) test_range(&rng, 0u, 6u);

            mismatches += (ref_letter_map_set(&ref, letter, action) !=
                           letter_map_set(&map, letter, action)) ? 1u : 0u;
        }
        else if (pick < 99u)
        {
            mismatches += (ref_letter_map_get(&ref, letter) !=
                           letter_map_get(&map, letter)) ? 1u : 0u;
        }
        else
        {
            ref_letter_map_default(&ref);
            letter_map_default(&map);
        }

        for (char check = 'A'; check <= 'Z'; check++)
        {
            mismatches += (ref_letter_map_get(&ref, check) !=
                           letter_map_get(&map, check)) ? 1u : 0u;
        }
    }

    UNIT_EQ(mismatches, 0u);
    UNIT_END();
}

static void
test_line_keep (void)
{
    line_keep_t keep;
    uint32_t    old_cycle   = 0u;
    uint32_t    old_letters = 0u;
    uint32_t    mismatches  = 0u;
    test_rng_t  rng = { 0x5EED0006u };

    UNIT_BEGIN("S3-U48", "Line task's kept values packed into two words: "
                         "the exact expected bits; status every 40 cycles "
                         "and the letter count modulo 256 exactly as "
                         "before (D-87)");
    UNIT_EQ(sizeof(line_keep_t), 8u);
    UNIT_EQ(LINE_STATUS_EVERY, 40u);

    line_keep_init(&keep);
    UNIT_EQ(keep.word[0], 0x40000000u);     /* window ok */
    UNIT_EQ(keep.word[1], 0x00000000u);
    UNIT_EQ(line_keep_last_letter(&keep), '-');

    UNIT_CHECK(line_keep_set_raw(&keep, 4095u, 0x123u));
    line_keep_set_edges_dropped(&keep, true);
    UNIT_EQ(keep.word[0], 0xC0123FFFu);
    line_keep_count_letter(&keep, 'Z');
    UNIT_EQ(keep.word[1], 0x00001A01u);     /* last Z, 1 letter */
    UNIT_EQ(line_keep_last_letter(&keep), 'Z');
    UNIT_CHECK(!line_keep_set_raw(&keep, 4096u, 0u));
    UNIT_EQ(line_keep_raw_l(&keep), 4095u);

    /* The old code: cycle++, a status when cycle % 40 == 0; uint32
       counters reported as uint8_t. */
    line_keep_init(&keep);

    for (uint32_t step = 0u; step < 20000u; step++)
    {
        uint32_t const pick = test_range(&rng, 0u, 9u);
        char const     letter = (char) ('A' + (int) test_range(&rng, 0u, 25u));

        old_cycle++;
        mismatches += ((0u == (old_cycle % LINE_STATUS_EVERY)) !=
                       line_keep_next_cycle(&keep)) ? 1u : 0u;

        if (0u == pick)
        {
            old_letters++;
            line_keep_count_letter(&keep, letter);
            mismatches += (line_keep_last_letter(&keep) != letter) ? 1u : 0u;
        }
        else
        {
            /* A cycle without a letter (a failed capture keeps
               nothing). */
        }

        mismatches += ((uint32_t) (uint8_t) old_letters !=
                       line_keep_letters(&keep)) ? 1u : 0u;
    }

    UNIT_EQ(mismatches, 0u);
    UNIT_CHECK(old_letters > 256u);         /* the counter did wrap */
    UNIT_END();
}

void
test_s3 (void)
{
    test_decode_forward();
    test_decode_reversed();
    test_decode_speed();
    test_decode_bias();
    test_decode_rejects();
    test_capture_letter();
    test_capture_glitch();
    test_capture_quiet();
    test_capture_fail();
    test_capture_limits();
    test_capture_loss_inside();
    test_capture_after_symbol();
    test_line_error();
    test_line_events();
    test_line_adapt();
    test_line_read_fail();
    test_line_health();
    test_steering();
    test_letter_map();
    test_line_record();
    test_line_events_packed();
    test_edge_record();
    test_capture_equivalent();
    test_capture_late();
    test_capture_packed();
    test_track_equivalent();
    test_steering_equivalent();
    test_letter_map_equivalent();
    test_line_keep();
}

/*** end of file ***/
