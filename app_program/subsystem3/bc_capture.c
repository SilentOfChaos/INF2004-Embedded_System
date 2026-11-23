/*
 *  bc_capture.c
 *  The BARCODE region of the Line statechart.  See bc_capture.h.
 *
 *  Edge k of a capture (k = 0, 1, ...) is the start of a bar when k is
 *  even and the start of a space when k is odd, so the 29 elements of a
 *  symbol are the gaps between 30 edges.
 *
 *  The state is packed (D-85): the small helpers at the end read and
 *  write its fields; the statechart itself is unchanged.
 */

#include <stddef.h>

#include "bc_capture.h"

#define BC_US_PER_MS        1000u
#define BC_EDGES_NEEDED     (BC_SYMBOL_ELEMENTS + 1u)   /* 30 */

/* The packed layout holds every value it must. */
BITPACK_ASSERT(BITPACK_BITS_FOR(BC_FAIL_EDGE_LOST) == BC_CAP_REASON_BITS,
               cap_reason_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(CFG_BC_MAX_EDGES) == BC_CAP_COUNT_BITS,
               cap_count_bits);
BITPACK_ASSERT(CFG_BC_QUIET_MS <= 255, cap_quiet_fits);
BITPACK_ASSERT((BC_CAP_QUIET_SHIFT + BC_CAP_QUIET_BITS) <= 32u,
               cap_flags_fit);
BITPACK_ASSERT(BC_CAP_OFFSET_BITS == 22u, cap_offset_bits);

static void     bc_capture_end(bc_capture_t * p_cap);
static bool     bc_capture_should_end(bc_capture_t const * p_cap,
                                      uint32_t now_us);
static uint8_t  bc_capture_end_reason(bc_capture_t const * p_cap,
                                      uint32_t now_us);
static bc_out_t bc_capture_try_decode(bc_capture_t * p_cap);

static bool     bc_flag(bc_capture_t const * p_cap, uint32_t bit);
static void     bc_set_flag(bc_capture_t * p_cap, uint32_t bit, bool b_on);
static uint32_t bc_count(bc_capture_t const * p_cap);
static void     bc_set_count(bc_capture_t * p_cap, uint32_t count);
static uint8_t  bc_reason(bc_capture_t const * p_cap);
static void     bc_set_reason(bc_capture_t * p_cap, uint8_t reason);
static bool     bc_capturing(bc_capture_t const * p_cap);
static uint32_t bc_edge_us(bc_capture_t const * p_cap, uint32_t index);
static bool     bc_store_edge(bc_capture_t * p_cap, uint32_t index,
                              uint32_t time_us);

/* Start in WAITING with the current signal level. */
void
bc_capture_init (bc_capture_t * p_cap, bool b_dark_now, uint32_t now_us)
{
    if (NULL != p_cap)
    {
        p_cap->flags          = 0u;     /* WAITING, nothing stored */
        bc_set_flag(p_cap, BC_CAP_DARK_BIT, b_dark_now);
        (void) bc_capture_set_quiet_ms(p_cap, (uint32_t) CFG_BC_QUIET_MS);
        p_cap->white_since_us = now_us;
        p_cap->quiet_from_us  = now_us;
        p_cap->first_us       = 0u;

        for (uint32_t idx = 0u; idx < BC_CAP_OFFSET_WORDS; idx++)
        {
            p_cap->offsets[idx] = 0u;
        }
    }
}

/*
 * One edge from the Barcode ISR: its time stamp and the level after it.
 * Edges must be given in the order they happened.
 */
void
bc_capture_edge (bc_capture_t * p_cap, uint32_t time_us, bool b_dark)
{
    if (NULL == p_cap)
    {
        /* Nothing to update. */
    }
    else if (!bc_capturing(p_cap))
    {
        bool const b_was_dark = bc_flag(p_cap, BC_CAP_DARK_BIT);

        if (b_dark && !b_was_dark)
        {
            /* First bar: CAPTURING (report 6.3).  With the optional quiet
               zone (not approved, D-25) only after that much white. */
            bool const b_quiet = ((time_us - p_cap->white_since_us) >=
                                  (bc_capture_quiet_ms(p_cap) *
                                   BC_US_PER_MS));

            if (b_quiet)
            {
                bc_set_flag(p_cap, BC_CAP_STATE_BIT, true);
                p_cap->quiet_from_us = p_cap->white_since_us;
                (void) bc_store_edge(p_cap, 0u, time_us);
                bc_set_count(p_cap, 1u);
                bc_set_flag(p_cap, BC_CAP_STARTED_BIT, false);
                bc_set_flag(p_cap, BC_CAP_TRIED_BIT, false);
                bc_set_reason(p_cap, BC_FAIL_NONE);
                bc_set_flag(p_cap, BC_CAP_LOST_BIT, false);
                bc_set_flag(p_cap, BC_CAP_OVERFLOW_BIT, false);
            }
        }
        else if (!b_dark && b_was_dark)
        {
            p_cap->white_since_us = time_us;
        }
        else
        {
            /* Same level twice: an edge was missed; nothing to undo. */
        }

        bc_set_flag(p_cap, BC_CAP_DARK_BIT, b_dark);
    }
    else
    {
        uint32_t const count         = bc_count(p_cap);
        bool const     b_expect_dark = (0u == (count % 2u));
        uint32_t const last_us       = bc_edge_us(p_cap, count - 1u);

        if (b_dark != b_expect_dark)
        {
            /* Levels must alternate, so an edge was lost.  Inside the
               symbol (before its 30th edge) that spoils the read; after
               it, the edge belongs to what follows the barcode (D-78). */
            if (count < BC_EDGES_NEEDED)
            {
                bc_set_flag(p_cap, BC_CAP_LOST_BIT, true);
            }
        }
        else if ((time_us - last_us) < CFG_BC_GLITCH_US)
        {
            /* A pulse shorter than the glitch limit: drop it, and the
               level goes back to what it was before that pulse. */
            bc_set_count(p_cap, count - 1u);

            if (1u == count)
            {
                /* The first bar itself was a glitch: back to WAITING
                   with the white time that came before it, so a real
                   bar right after the glitch can still start (this
                   matters only with the optional quiet zone). */
                bc_set_flag(p_cap, BC_CAP_STATE_BIT, false);
                p_cap->white_since_us = p_cap->quiet_from_us;
            }
        }
        else if ((count < CFG_BC_MAX_EDGES) &&
                 bc_store_edge(p_cap, count, time_us))
        {
            bc_set_count(p_cap, count + 1u);
        }
        else
        {
            /* The buffer is full, or (D-85) the edge is too long after
               the first bar to store: the capture is over. */
            bc_set_flag(p_cap, BC_CAP_OVERFLOW_BIT, true);
        }

        bc_set_flag(p_cap, BC_CAP_DARK_BIT, b_dark);
    }
}

/*
 * Called every 5 ms by the Line task.  Returns at most one output; call
 * again until it returns BC_OUT_NONE.
 */
bc_out_t
bc_capture_poll (bc_capture_t * p_cap, uint32_t now_us)
{
    bc_out_t out = { BC_OUT_NONE, { '\0', false, BC_FAIL_NONE } };

    if ((NULL != p_cap) && bc_capturing(p_cap))
    {
        uint32_t const count     = bc_count(p_cap);
        uint32_t const last_us   = bc_edge_us(p_cap, count - 1u);
        bool const b_last_stable = ((now_us - last_us) >= CFG_BC_GLITCH_US);

        if (!bc_flag(p_cap, BC_CAP_STARTED_BIT) &&
            ((count >= 2u) || b_last_stable))
        {
            bc_set_flag(p_cap, BC_CAP_STARTED_BIT, true);
            out.type = BC_OUT_STARTED;
        }
        else
        {
            /* A known loss inside the symbol comes before decoding: the
               widths of a symbol with a lost edge are never used, even
               if they would decode (D-78). */
            if (!bc_flag(p_cap, BC_CAP_TRIED_BIT) &&
                !bc_flag(p_cap, BC_CAP_LOST_BIT) &&
                (count >= BC_EDGES_NEEDED) &&
                ((count > BC_EDGES_NEEDED) || b_last_stable))
            {
                out = bc_capture_try_decode(p_cap);
            }

            /* No letter: end the capture now if it is over, so a caller
               that stops at BC_OUT_NONE never misses the failure. */
            if ((BC_OUT_NONE == out.type) && bc_capturing(p_cap) &&
                bc_capture_should_end(p_cap, now_us))
            {
                out.result.letter     = '\0';
                out.result.b_reversed = false;
                out.result.reason     = bc_capture_end_reason(p_cap, now_us);
                out.type = bc_flag(p_cap, BC_CAP_STARTED_BIT)
                           ? BC_OUT_FAILED : BC_OUT_NONE;
                bc_capture_end(p_cap);
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
bc_capture_mark_lost (bc_capture_t * p_cap)
{
    if ((NULL != p_cap) && bc_capturing(p_cap) &&
        (bc_count(p_cap) < BC_EDGES_NEEDED))
    {
        bc_set_flag(p_cap, BC_CAP_LOST_BIT, true);
    }
}

/* WAITING or CAPTURING. */
bc_state_t
bc_capture_state (bc_capture_t const * p_cap)
{
    return (((NULL != p_cap) && bc_capturing(p_cap)) ? BC_STATE_CAPTURING
                                                     : BC_STATE_WAITING);
}

/* An edge of this capture could not be stored (buffer full or, D-85,
   too long after the first bar). */
bool
bc_capture_overflowed (bc_capture_t const * p_cap)
{
    return ((NULL != p_cap) && bc_flag(p_cap, BC_CAP_OVERFLOW_BIT));
}

/* White needed before a capture may start, ms (0 = none, D-25). */
uint32_t
bc_capture_quiet_ms (bc_capture_t const * p_cap)
{
    return ((NULL != p_cap)
            ? bitpack_get(p_cap->flags, BC_CAP_QUIET_SHIFT,
                          BC_CAP_QUIET_BITS)
            : 0u);
}

/* Set the optional quiet zone, 0..255 ms (only tests use other values
   than CFG_BC_QUIET_MS).  Returns false if it did not fit. */
bool
bc_capture_set_quiet_ms (bc_capture_t * p_cap, uint32_t quiet_ms)
{
    return ((NULL != p_cap) &&
            bitpack_set(&p_cap->flags, BC_CAP_QUIET_SHIFT, BC_CAP_QUIET_BITS,
                        quiet_ms));
}

/* Decode the first 29 elements; a valid letter ends the capture. */
static bc_out_t
bc_capture_try_decode (bc_capture_t * p_cap)
{
    bc_out_t out = { BC_OUT_NONE, { '\0', false, BC_FAIL_NONE } };
    uint32_t widths[BC_SYMBOL_ELEMENTS];

    for (uint32_t idx = 0u; idx < BC_SYMBOL_ELEMENTS; idx++)
    {
        widths[idx] = bc_edge_us(p_cap, idx + 1u) - bc_edge_us(p_cap, idx);
    }

    out.result = bc_decode(widths, BC_SYMBOL_ELEMENTS);
    bc_set_flag(p_cap, BC_CAP_TRIED_BIT, true);
    bc_set_reason(p_cap, out.result.reason);

    if (BC_FAIL_NONE == out.result.reason)
    {
        out.type = BC_OUT_LETTER;
        bc_capture_end(p_cap);
    }

    return (out);
}

/* True when the capture must end: 120 ms without an edge, or a limit. */
static bool
bc_capture_should_end (bc_capture_t const * p_cap, uint32_t now_us)
{
    uint32_t const last_us  = bc_edge_us(p_cap, bc_count(p_cap) - 1u);
    uint32_t const first_us = p_cap->first_us;

    return (bc_flag(p_cap, BC_CAP_LOST_BIT) ||
            bc_flag(p_cap, BC_CAP_OVERFLOW_BIT) ||
            ((now_us - last_us) >= (CFG_BC_END_MS * BC_US_PER_MS)) ||
            ((now_us - first_us) >= (CFG_BC_MAX_CAPTURE_MS * BC_US_PER_MS)));
}

/* Why a capture ended without a letter. */
static uint8_t
bc_capture_end_reason (bc_capture_t const * p_cap, uint32_t now_us)
{
    uint8_t reason = BC_FAIL_TOO_FEW;

    if (bc_flag(p_cap, BC_CAP_LOST_BIT))
    {
        reason = BC_FAIL_EDGE_LOST;
    }
    else if (bc_flag(p_cap, BC_CAP_OVERFLOW_BIT) ||
             ((now_us - p_cap->first_us) >=
              (CFG_BC_MAX_CAPTURE_MS * BC_US_PER_MS)))
    {
        reason = BC_FAIL_TOO_LONG;
    }
    else if (bc_flag(p_cap, BC_CAP_TRIED_BIT))
    {
        reason = (bc_count(p_cap) > BC_EDGES_NEEDED)
                 ? BC_FAIL_COUNT : bc_reason(p_cap);
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
bc_capture_end (bc_capture_t * p_cap)
{
    bc_set_flag(p_cap, BC_CAP_STATE_BIT, false);

    if (!bc_flag(p_cap, BC_CAP_DARK_BIT))
    {
        p_cap->white_since_us = bc_edge_us(p_cap, bc_count(p_cap) - 1u);
    }

    bc_set_count(p_cap, 0u);
}

/* ------------------------------------------------------------------ */
/*  Packed fields (bc_capture.h)                                       */
/* ------------------------------------------------------------------ */

static bool
bc_flag (bc_capture_t const * p_cap, uint32_t bit)
{
    return (bitpack_get_flag(p_cap->flags, bit));
}

static void
bc_set_flag (bc_capture_t * p_cap, uint32_t bit, bool b_on)
{
    bitpack_set_flag(&p_cap->flags, bit, b_on);
}

/* Edges stored, 0..CFG_BC_MAX_EDGES. */
static uint32_t
bc_count (bc_capture_t const * p_cap)
{
    return (bitpack_get(p_cap->flags, BC_CAP_COUNT_SHIFT,
                        BC_CAP_COUNT_BITS));
}

/* Callers keep the count within 0..CFG_BC_MAX_EDGES, which fits. */
static void
bc_set_count (bc_capture_t * p_cap, uint32_t count)
{
    (void) bitpack_set(&p_cap->flags, BC_CAP_COUNT_SHIFT, BC_CAP_COUNT_BITS,
                       count);
}

static uint8_t
bc_reason (bc_capture_t const * p_cap)
{
    /* A 4-bit field: fits uint8_t. */
    return ((uint8_t) bitpack_get(p_cap->flags, BC_CAP_REASON_SHIFT,
                                  BC_CAP_REASON_BITS));
}

/* bc_decode() gives BC_FAIL_NONE..BC_FAIL_EDGE_LOST, which fit. */
static void
bc_set_reason (bc_capture_t * p_cap, uint8_t reason)
{
    (void) bitpack_set(&p_cap->flags, BC_CAP_REASON_SHIFT, BC_CAP_REASON_BITS,
                       (uint32_t) reason);
}

static bool
bc_capturing (bc_capture_t const * p_cap)
{
    return (bc_flag(p_cap, BC_CAP_STATE_BIT));
}

/* The time of stored edge index (0..count-1). */
static uint32_t
bc_edge_us (bc_capture_t const * p_cap, uint32_t index)
{
    uint32_t time_us = p_cap->first_us;

    if (index > 0u)
    {
        time_us += bitpack_array_get(p_cap->offsets, index - 1u,
                                     BC_CAP_OFFSET_BITS);
    }

    return (time_us);
}

/*
 * Store edge index.  Edge 0 sets the first bar's time; a later edge is
 * kept as its offset from it, and one too far from it is not stored
 * (returns false).
 */
static bool
bc_store_edge (bc_capture_t * p_cap, uint32_t index, uint32_t time_us)
{
    bool b_stored = true;

    if (0u == index)
    {
        p_cap->first_us = time_us;
    }
    else
    {
        uint32_t const offset_us = time_us - p_cap->first_us;   /* wraps */

        b_stored = (offset_us <= bitpack_mask(BC_CAP_OFFSET_BITS));

        if (b_stored)
        {
            (void) bitpack_array_set(p_cap->offsets, index - 1u,
                                     BC_CAP_OFFSET_BITS, offset_us);
        }
    }

    return (b_stored);
}

/*** end of file ***/
