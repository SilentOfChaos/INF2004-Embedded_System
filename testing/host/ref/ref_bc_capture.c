/*
 *  ref_bc_capture.c
 *  TEST ONLY: the barcode capture before the minimum-bit change, from
 *  app_program/subsystem3/bc_capture.c at c644b68 with only its public
 *  names prefixed ref_ (see ref_bc_capture.h).  Do not change it.
 */
#include <stddef.h>

#include "ref_bc_capture.h"

#define BC_US_PER_MS        1000u
#define BC_EDGES_NEEDED     (BC_SYMBOL_ELEMENTS + 1u)   /* 30 */

static void ref_bc_capture_end(ref_bc_capture_t * p_cap);
static bool ref_bc_capture_should_end(ref_bc_capture_t const * p_cap,
                                  uint32_t now_us);
static uint8_t ref_bc_capture_end_reason(ref_bc_capture_t const * p_cap,
                                     uint32_t now_us);
static bc_out_t ref_bc_capture_try_decode(ref_bc_capture_t * p_cap);

/* Start in WAITING with the current signal level. */
void
ref_bc_capture_init (ref_bc_capture_t * p_cap, bool b_dark_now, uint32_t now_us)
{
    if (NULL != p_cap)
    {
        p_cap->state          = BC_STATE_WAITING;
        p_cap->b_dark         = b_dark_now;
        p_cap->quiet_us       = (uint32_t) CFG_BC_QUIET_MS * BC_US_PER_MS;
        p_cap->white_since_us = now_us;
        p_cap->quiet_from_us  = now_us;
        p_cap->edge_count     = 0u;
        p_cap->b_started_sent = false;
        p_cap->b_decode_tried = false;
        p_cap->fail_reason    = BC_FAIL_NONE;
        p_cap->b_edge_lost    = false;
        p_cap->b_overflow     = false;

        for (uint32_t idx = 0u; idx < CFG_BC_MAX_EDGES; idx++)
        {
            p_cap->edges_us[idx] = 0u;
        }
    }
}

/*
 * One edge from the Barcode ISR: its time stamp and the level after it.
 * Edges must be given in the order they happened.
 */
void
ref_bc_capture_edge (ref_bc_capture_t * p_cap, uint32_t time_us, bool b_dark)
{
    if (NULL == p_cap)
    {
        /* Nothing to update. */
    }
    else if (BC_STATE_WAITING == p_cap->state)
    {
        if (b_dark && !p_cap->b_dark)
        {
            /* First bar: CAPTURING (report 6.3).  With the optional quiet
               zone (quiet_us > 0, not approved, D-25) only after that
               much white. */
            bool const b_quiet = ((time_us - p_cap->white_since_us) >=
                                  p_cap->quiet_us);

            if (b_quiet)
            {
                p_cap->state          = BC_STATE_CAPTURING;
                p_cap->quiet_from_us  = p_cap->white_since_us;
                p_cap->edges_us[0]    = time_us;
                p_cap->edge_count     = 1u;
                p_cap->b_started_sent = false;
                p_cap->b_decode_tried = false;
                p_cap->fail_reason    = BC_FAIL_NONE;
                p_cap->b_edge_lost    = false;
                p_cap->b_overflow     = false;
            }
        }
        else if (!b_dark && p_cap->b_dark)
        {
            p_cap->white_since_us = time_us;
        }
        else
        {
            /* Same level twice: an edge was missed; nothing to undo. */
        }

        p_cap->b_dark = b_dark;
    }
    else
    {
        bool const b_expect_dark = (0u == (p_cap->edge_count % 2u));
        uint32_t const last_us   = p_cap->edges_us[p_cap->edge_count - 1u];

        if (b_dark != b_expect_dark)
        {
            /* Levels must alternate, so an edge was lost.  Inside the
               symbol (before its 30th edge) that spoils the read; after
               it, the edge belongs to what follows the barcode (D-78). */
            if (p_cap->edge_count < BC_EDGES_NEEDED)
            {
                p_cap->b_edge_lost = true;
            }
        }
        else if ((time_us - last_us) < CFG_BC_GLITCH_US)
        {
            /* A pulse shorter than the glitch limit: drop it, and the
               level goes back to what it was before that pulse. */
            p_cap->edge_count--;

            if (0u == p_cap->edge_count)
            {
                /* The first bar itself was a glitch: back to WAITING
                   with the white time that came before it, so a real
                   bar right after the glitch can still start (this
                   matters only with the optional quiet zone). */
                p_cap->state          = BC_STATE_WAITING;
                p_cap->white_since_us = p_cap->quiet_from_us;
            }
        }
        else if (p_cap->edge_count < CFG_BC_MAX_EDGES)
        {
            p_cap->edges_us[p_cap->edge_count] = time_us;
            p_cap->edge_count++;
        }
        else
        {
            p_cap->b_overflow = true;
        }

        p_cap->b_dark = b_dark;
    }
}

/*
 * Called every 5 ms by the Line task.  Returns at most one output; call
 * again until it returns BC_OUT_NONE.
 */
bc_out_t
ref_bc_capture_poll (ref_bc_capture_t * p_cap, uint32_t now_us)
{
    bc_out_t out = { BC_OUT_NONE, { '\0', false, BC_FAIL_NONE } };

    if ((NULL != p_cap) && (BC_STATE_CAPTURING == p_cap->state))
    {
        uint32_t const last_us  = p_cap->edges_us[p_cap->edge_count - 1u];
        bool const b_last_stable = ((now_us - last_us) >= CFG_BC_GLITCH_US);

        if (!p_cap->b_started_sent &&
            ((p_cap->edge_count >= 2u) || b_last_stable))
        {
            p_cap->b_started_sent = true;
            out.type = BC_OUT_STARTED;
        }
        else
        {
            /* A known loss inside the symbol comes before decoding: the
               widths of a symbol with a lost edge are never used, even
               if they would decode (D-78). */
            if (!p_cap->b_decode_tried && !p_cap->b_edge_lost &&
                (p_cap->edge_count >= BC_EDGES_NEEDED) &&
                ((p_cap->edge_count > BC_EDGES_NEEDED) || b_last_stable))
            {
                out = ref_bc_capture_try_decode(p_cap);
            }

            /* No letter: end the capture now if it is over, so a caller
               that stops at BC_OUT_NONE never misses the failure. */
            if ((BC_OUT_NONE == out.type) &&
                (BC_STATE_CAPTURING == p_cap->state) &&
                ref_bc_capture_should_end(p_cap, now_us))
            {
                out.result.letter     = '\0';
                out.result.b_reversed = false;
                out.result.reason     = ref_bc_capture_end_reason(p_cap, now_us);
                out.type = p_cap->b_started_sent ? BC_OUT_FAILED
                                                 : BC_OUT_NONE;
                ref_bc_capture_end(p_cap);
            }
        }
    }

    return (out);
}

/*
 * The ISR could not queue an edge.  That edge is newer than every edge
 * given here so far, so it is inside the symbol while fewer than 30
 * edges are stored: the capture then fails with BC_FAIL_EDGE_LOST.  At
 * 30 or more it came after the symbol and changes nothing (D-78).  The
 * Line task calls this before it takes the queued edges, which counts a
 * loss that came after them as inside (the safe side), and again after
 * taking them, for a capture those edges started.
 */
void
ref_bc_capture_mark_lost (ref_bc_capture_t * p_cap)
{
    if ((NULL != p_cap) && (BC_STATE_CAPTURING == p_cap->state) &&
        (p_cap->edge_count < BC_EDGES_NEEDED))
    {
        p_cap->b_edge_lost = true;
    }
}

/* Decode the first 29 elements; a valid letter ends the capture. */
static bc_out_t
ref_bc_capture_try_decode (ref_bc_capture_t * p_cap)
{
    bc_out_t out = { BC_OUT_NONE, { '\0', false, BC_FAIL_NONE } };
    uint32_t widths[BC_SYMBOL_ELEMENTS];

    for (uint32_t idx = 0u; idx < BC_SYMBOL_ELEMENTS; idx++)
    {
        widths[idx] = p_cap->edges_us[idx + 1u] - p_cap->edges_us[idx];
    }

    out.result = bc_decode(widths, BC_SYMBOL_ELEMENTS);
    p_cap->b_decode_tried = true;
    p_cap->fail_reason    = out.result.reason;

    if (BC_FAIL_NONE == out.result.reason)
    {
        out.type = BC_OUT_LETTER;
        ref_bc_capture_end(p_cap);
    }

    return (out);
}

/* True when the capture must end: 120 ms without an edge, or a limit. */
static bool
ref_bc_capture_should_end (ref_bc_capture_t const * p_cap, uint32_t now_us)
{
    uint32_t const last_us  = p_cap->edges_us[p_cap->edge_count - 1u];
    uint32_t const first_us = p_cap->edges_us[0];

    return (p_cap->b_edge_lost || p_cap->b_overflow ||
            ((now_us - last_us) >= (CFG_BC_END_MS * BC_US_PER_MS)) ||
            ((now_us - first_us) >= (CFG_BC_MAX_CAPTURE_MS * BC_US_PER_MS)));
}

/* Why a capture ended without a letter. */
static uint8_t
ref_bc_capture_end_reason (ref_bc_capture_t const * p_cap, uint32_t now_us)
{
    uint8_t reason = BC_FAIL_TOO_FEW;

    if (p_cap->b_edge_lost)
    {
        reason = BC_FAIL_EDGE_LOST;
    }
    else if (p_cap->b_overflow ||
             ((now_us - p_cap->edges_us[0]) >=
              (CFG_BC_MAX_CAPTURE_MS * BC_US_PER_MS)))
    {
        reason = BC_FAIL_TOO_LONG;
    }
    else if (p_cap->b_decode_tried)
    {
        reason = (p_cap->edge_count > BC_EDGES_NEEDED)
                 ? BC_FAIL_COUNT : p_cap->fail_reason;
    }
    else
    {
        /* Fewer than 29 elements before the 120 ms timeout. */
    }

    return (reason);
}

/* Back to WAITING ("every new barcode is tried again from WAITING",
   report 6.3).  White is timed again from the last edge, for the
   optional quiet zone. */
static void
ref_bc_capture_end (ref_bc_capture_t * p_cap)
{
    p_cap->state = BC_STATE_WAITING;

    if (!p_cap->b_dark)
    {
        p_cap->white_since_us = p_cap->edges_us[p_cap->edge_count - 1u];
    }

    p_cap->edge_count = 0u;
}

/*** end of file ***/
