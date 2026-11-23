/*
 *  line_keep.c
 *  What the Line task keeps between cycles.  See line_keep.h.
 */

#include <stddef.h>

#include "../common/bitpack.h"
#include "line_keep.h"

#define LK_RAW_BITS         12u
#define LK_RAW_L_SHIFT      0u
#define LK_RAW_R_SHIFT      12u
#define LK_CYCLE_SHIFT      24u
#define LK_CYCLE_BITS       6u
#define LK_WINDOW_BIT       30u
#define LK_DROPPED_BIT      31u
#define LK_COUNT_BITS       8u
#define LK_LETTERS_SHIFT    0u
#define LK_LAST_SHIFT       8u
#define LK_LAST_BITS        5u
#define LK_LETTERS          26u     /* 'A'..'Z' as 1..26, 0 = none */

/* The layout holds every value it must. */
BITPACK_ASSERT(BITPACK_BITS_FOR(4095u) == LK_RAW_BITS, lk_raw_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(LINE_STATUS_EVERY - 1u) <= LK_CYCLE_BITS,
               lk_cycle_bits);
BITPACK_ASSERT((LK_CYCLE_SHIFT + LK_CYCLE_BITS) == LK_WINDOW_BIT,
               lk_layout_1);
BITPACK_ASSERT(BITPACK_BITS_FOR(LK_LETTERS) == LK_LAST_BITS, lk_last_bits);
BITPACK_ASSERT((LK_LETTERS_SHIFT + LK_COUNT_BITS) == LK_LAST_SHIFT,
               lk_layout_2);

/* No readings yet, cycle 0, window ok, nothing dropped or counted. */
void
line_keep_init (line_keep_t * p_keep)
{
    if (NULL != p_keep)
    {
        p_keep->word[0] = 0u;
        p_keep->word[1] = 0u;
        bitpack_set_flag(&p_keep->word[0], LK_WINDOW_BIT, true);
    }
}

/* The last readings (12-bit).  Returns false if one did not fit. */
bool
line_keep_set_raw (line_keep_t * p_keep, uint32_t raw_l, uint32_t raw_r)
{
    bool b_ok = false;

    if (NULL != p_keep)
    {
        b_ok = bitpack_set(&p_keep->word[0], LK_RAW_L_SHIFT, LK_RAW_BITS,
                           raw_l);
        b_ok = bitpack_set(&p_keep->word[0], LK_RAW_R_SHIFT, LK_RAW_BITS,
                           raw_r) && b_ok;
    }

    return (b_ok);
}

uint32_t
line_keep_raw_l (line_keep_t const * p_keep)
{
    return ((NULL != p_keep)
            ? bitpack_get(p_keep->word[0], LK_RAW_L_SHIFT, LK_RAW_BITS)
            : 0u);
}

uint32_t
line_keep_raw_r (line_keep_t const * p_keep)
{
    return ((NULL != p_keep)
            ? bitpack_get(p_keep->word[0], LK_RAW_R_SHIFT, LK_RAW_BITS)
            : 0u);
}

/* Count one cycle; true on the LINE_STATUS_EVERY-th call, the
   2 x LINE_STATUS_EVERY-th and so on, when a status is due. */
bool
line_keep_next_cycle (line_keep_t * p_keep)
{
    bool b_due = false;

    if (NULL != p_keep)
    {
        uint32_t cycle = bitpack_get(p_keep->word[0], LK_CYCLE_SHIFT,
                                     LK_CYCLE_BITS) + 1u;

        if (cycle >= LINE_STATUS_EVERY)
        {
            cycle = 0u;
            b_due = true;
        }

        (void) bitpack_set(&p_keep->word[0], LK_CYCLE_SHIFT, LK_CYCLE_BITS,
                           cycle);
    }

    return (b_due);
}

bool
line_keep_window_ok (line_keep_t const * p_keep)
{
    return ((NULL != p_keep) &&
            bitpack_get_flag(p_keep->word[0], LK_WINDOW_BIT));
}

void
line_keep_set_window_ok (line_keep_t * p_keep, bool b_ok)
{
    if (NULL != p_keep)
    {
        bitpack_set_flag(&p_keep->word[0], LK_WINDOW_BIT, b_ok);
    }
}

/*
 * One sample for the window (b_ok: read without error and both readings
 * in range).  Returns true on the first bad sample of a window: the
 * published line health must drop now, not at the window's end, so
 * SELF-TEST never passes on a window older than a bad sample (D-96).
 * The window's end still publishes the whole window, so the health
 * comes back only after a whole window of good samples.
 */
bool
line_keep_note_sample (line_keep_t * p_keep, bool b_ok)
{
    bool const b_was_ok = line_keep_window_ok(p_keep);

    line_keep_set_window_ok(p_keep, b_was_ok && b_ok);

    return (b_was_ok && !b_ok);
}

bool
line_keep_edges_dropped (line_keep_t const * p_keep)
{
    return ((NULL != p_keep) &&
            bitpack_get_flag(p_keep->word[0], LK_DROPPED_BIT));
}

void
line_keep_set_edges_dropped (line_keep_t * p_keep, bool b_dropped)
{
    if (NULL != p_keep)
    {
        bitpack_set_flag(&p_keep->word[0], LK_DROPPED_BIT, b_dropped);
    }
}

/* One letter read ('A'..'Z'): counted modulo 256 and kept as the last. */
void
line_keep_count_letter (line_keep_t * p_keep, char letter)
{
    if (NULL != p_keep)
    {
        uint32_t const count = bitpack_get(p_keep->word[1], LK_LETTERS_SHIFT,
                                           LK_COUNT_BITS);
        uint32_t const code  = ((letter >= 'A') && (letter <= 'Z'))
                               ? ((uint32_t) (letter - 'A') + 1u) : 0u;

        (void) bitpack_set(&p_keep->word[1], LK_LETTERS_SHIFT, LK_COUNT_BITS,
                           (count + 1u) & bitpack_mask(LK_COUNT_BITS));
        (void) bitpack_set(&p_keep->word[1], LK_LAST_SHIFT, LK_LAST_BITS,
                           code);
    }
}

/* Letters read, modulo 256. */
uint32_t
line_keep_letters (line_keep_t const * p_keep)
{
    return ((NULL != p_keep)
            ? bitpack_get(p_keep->word[1], LK_LETTERS_SHIFT, LK_COUNT_BITS)
            : 0u);
}

/* The last letter read, or LINE_KEEP_NO_LETTER before the first. */
char
line_keep_last_letter (line_keep_t const * p_keep)
{
    char letter = LINE_KEEP_NO_LETTER;

    if (NULL != p_keep)
    {
        uint32_t const code = bitpack_get(p_keep->word[1], LK_LAST_SHIFT,
                                          LK_LAST_BITS);

        if ((code >= 1u) && (code <= LK_LETTERS))
        {
            letter = (char) ('A' + (int32_t) (code - 1u));  /* 'A'..'Z' */
        }
    }

    return (letter);
}

/*** end of file ***/
