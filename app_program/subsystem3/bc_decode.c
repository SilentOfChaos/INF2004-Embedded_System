/*
 *  bc_decode.c
 *  Code 39 decoder for one '*'-letter-'*' barcode.  See bc_decode.h.
 *
 *  Wide/narrow classification (CFG_BC_CLASSIFY_PER_CHAR, decision D-27):
 *    1  per character, bars and spaces apart: '*' and every letter have
 *       exactly 2 wide bars (of 5) and 1 wide space (of 4), read in
 *       either direction, so the 2 widest bars and the widest space are
 *       wide, each clearly wider than the widest narrow one of its kind.
 *       Tolerates a speed change along the barcode and a comparator that
 *       makes bars wider and spaces narrower (host tests S3-U03 and
 *       S3-U04).
 *    0  the report's rule: wide if more than twice the narrowest element
 *       of the whole capture.
 */

#include <stddef.h>

#include "../app_config.h"
#include "bc_decode.h"

#define BC_CHARS            3u
#define BC_CHAR_STRIDE      10u     /* 9 elements + 1 gap */
#define BC_GAP_FIRST        9u
#define BC_GAP_SECOND       19u
#define BC_PATTERN_COUNT    44u
#define BC_RATIO_SCALE      100u
#define BC_BARS             5u      /* bars per character */
#define BC_SPACES           4u      /* spaces per character */
#define BC_BAR_NARROW_TOP   2u      /* sorted index of the widest narrow
                                       bar: 3 narrow, then 2 wide */
#define BC_SPACE_NARROW_TOP 2u      /* widest narrow space: 3 narrow,
                                       then 1 wide */

typedef struct
{
    uint32_t bar;       /* wide above this, for bars */
    uint32_t space;     /* and for spaces */
} bc_threshold_t;

typedef struct
{
    uint16_t pattern;   /* bit 8 = first element, 1 = wide */
    char     symbol;
} bc_symbol_t;

/* Code 39 character set (wide = 1), checked against the measured sample
   barcodes (*A*, *Z*) and the standard table; see docs/notes/05. */
static bc_symbol_t const g_symbols[BC_PATTERN_COUNT] =
{
    { 0x034u, '0' }, { 0x121u, '1' }, { 0x061u, '2' }, { 0x160u, '3' },
    { 0x031u, '4' }, { 0x130u, '5' }, { 0x070u, '6' }, { 0x025u, '7' },
    { 0x124u, '8' }, { 0x064u, '9' }, { 0x109u, 'A' }, { 0x049u, 'B' },
    { 0x148u, 'C' }, { 0x019u, 'D' }, { 0x118u, 'E' }, { 0x058u, 'F' },
    { 0x00Du, 'G' }, { 0x10Cu, 'H' }, { 0x04Cu, 'I' }, { 0x01Cu, 'J' },
    { 0x103u, 'K' }, { 0x043u, 'L' }, { 0x142u, 'M' }, { 0x013u, 'N' },
    { 0x112u, 'O' }, { 0x052u, 'P' }, { 0x007u, 'Q' }, { 0x106u, 'R' },
    { 0x046u, 'S' }, { 0x016u, 'T' }, { 0x181u, 'U' }, { 0x0C1u, 'V' },
    { 0x1C0u, 'W' }, { 0x091u, 'X' }, { 0x190u, 'Y' }, { 0x0D0u, 'Z' },
    { 0x085u, '-' }, { 0x184u, '.' }, { 0x0C4u, ' ' }, { 0x0A8u, '$' },
    { 0x0A2u, '/' }, { 0x08Au, '+' }, { 0x02Au, '%' }, { 0x094u, '*' }
};

static uint8_t bc_decode_one_way(uint32_t const * p_widths, char * p_letter);
static bool    bc_decode_classify(uint32_t const * p_widths,
                                  bc_threshold_t * p_threshold);
static bool    bc_decode_split(uint32_t * p_values, uint32_t count,
                               uint32_t narrow_top, uint32_t * p_threshold);
static void    bc_decode_sort(uint32_t * p_values, uint32_t count);

/*
 * Decode one capture.  Forward first; if the ends are not '*', reverse
 * the element order and try once more (report 6.3).
 */
bc_result_t
bc_decode (uint32_t const * p_widths, uint32_t count)
{
    bc_result_t result = { '\0', false, BC_FAIL_COUNT };

    if ((NULL != p_widths) && (BC_SYMBOL_ELEMENTS == count))
    {
        uint32_t reversed[BC_SYMBOL_ELEMENTS];
        char     letter = '\0';
        uint8_t  forward_reason = BC_FAIL_NONE;
        uint8_t  backward_reason = BC_FAIL_NONE;

        forward_reason = bc_decode_one_way(p_widths, &letter);

        if (BC_FAIL_NONE == forward_reason)
        {
            result.letter = letter;
            result.reason = BC_FAIL_NONE;
        }
        else
        {
            for (uint32_t idx = 0u; idx < BC_SYMBOL_ELEMENTS; idx++)
            {
                reversed[idx] = p_widths[BC_SYMBOL_ELEMENTS - 1u - idx];
            }

            backward_reason = bc_decode_one_way(reversed, &letter);

            if (BC_FAIL_NONE == backward_reason)
            {
                result.letter     = letter;
                result.b_reversed = true;
                result.reason     = BC_FAIL_NONE;
            }
            else
            {
                /* Report the furthest stage either direction reached:
                   WIDTHS < PATTERN < ENDS < NOT_LETTER. */
                result.reason = (forward_reason > backward_reason)
                                ? forward_reason : backward_reason;
            }
        }
    }

    return (result);
}

/* Character for a 9-bit wide/narrow pattern, or '\0' if not Code 39. */
char
bc_decode_pattern (uint32_t pattern)
{
    char symbol = '\0';

    for (uint32_t idx = 0u; idx < BC_PATTERN_COUNT; idx++)
    {
        if (pattern == (uint32_t) g_symbols[idx].pattern)
        {
            symbol = g_symbols[idx].symbol;
        }
    }

    return (symbol);
}

/* Decode the 29 widths in the given order.  Returns BC_FAIL_xxx. */
static uint8_t
bc_decode_one_way (uint32_t const * p_widths, char * p_letter)
{
    bc_threshold_t threshold[BC_CHARS];
    char           symbols[BC_CHARS] = { '\0', '\0', '\0' };
    uint8_t        reason = BC_FAIL_NONE;

    if (!bc_decode_classify(p_widths, threshold))
    {
        reason = BC_FAIL_WIDTHS;
    }

    for (uint32_t chr = 0u; (chr < BC_CHARS) && (BC_FAIL_NONE == reason);
         chr++)
    {
        uint32_t pattern = 0u;

        for (uint32_t elem = 0u; elem < BC_CHAR_ELEMENTS; elem++)
        {
            /* Even elements are bars, odd ones spaces. */
            uint32_t const limit = (0u == (elem % 2u))
                                   ? threshold[chr].bar
                                   : threshold[chr].space;

            pattern <<= 1u;

            if (p_widths[(chr * BC_CHAR_STRIDE) + elem] > limit)
            {
                pattern |= 1u;
            }
        }

        symbols[chr] = bc_decode_pattern(pattern);

        if ('\0' == symbols[chr])
        {
            reason = BC_FAIL_PATTERN;
        }
    }

    if (BC_FAIL_NONE == reason)
    {
        if (('*' != symbols[0]) || ('*' != symbols[2]))
        {
            reason = BC_FAIL_ENDS;
        }
        else if ((symbols[1] < 'A') || (symbols[1] > 'Z'))
        {
            reason = BC_FAIL_NOT_LETTER;
        }
        else
        {
            *p_letter = symbols[1];
        }
    }

    return (reason);
}

/*
 * Work out the wide/narrow thresholds of each character.  Returns false
 * if the widths do not split cleanly or a gap between characters is
 * not narrow.
 */
static bool
bc_decode_classify (uint32_t const * p_widths, bc_threshold_t * p_threshold)
{
    bool b_ok = true;

#if CFG_BC_CLASSIFY_PER_CHAR
    for (uint32_t chr = 0u; (chr < BC_CHARS) && b_ok; chr++)
    {
        uint32_t bars[BC_BARS];
        uint32_t spaces[BC_SPACES];

        for (uint32_t elem = 0u; elem < BC_CHAR_ELEMENTS; elem++)
        {
            uint32_t const width = p_widths[(chr * BC_CHAR_STRIDE) + elem];

            if (0u == (elem % 2u))
            {
                bars[elem / 2u] = width;
            }
            else
            {
                spaces[elem / 2u] = width;
            }
        }

        b_ok = bc_decode_split(bars, BC_BARS, BC_BAR_NARROW_TOP,
                               &p_threshold[chr].bar) &&
               bc_decode_split(spaces, BC_SPACES, BC_SPACE_NARROW_TOP,
                               &p_threshold[chr].space);
    }
#else
    uint32_t narrowest = p_widths[0];

    for (uint32_t idx = 1u; idx < BC_SYMBOL_ELEMENTS; idx++)
    {
        if (p_widths[idx] < narrowest)
        {
            narrowest = p_widths[idx];
        }
    }

    /* Wide means more than CFG_BC_WIDE_RATIO_X100 / 100 x the narrowest. */
    for (uint32_t chr = 0u; chr < BC_CHARS; chr++)
    {
        p_threshold[chr].bar   = (narrowest *
                                  (uint32_t) CFG_BC_WIDE_RATIO_X100) /
                                 BC_RATIO_SCALE;
        p_threshold[chr].space = p_threshold[chr].bar;
    }
#endif

    /* Both gaps between characters are spaces and must be narrow. */
    if (b_ok)
    {
        uint32_t const first_limit  =
            (p_threshold[0].space < p_threshold[1].space)
            ? p_threshold[0].space : p_threshold[1].space;
        uint32_t const second_limit =
            (p_threshold[1].space < p_threshold[2].space)
            ? p_threshold[1].space : p_threshold[2].space;

        if ((p_widths[BC_GAP_FIRST] > first_limit) ||
            (p_widths[BC_GAP_SECOND] > second_limit))
        {
            b_ok = false;
        }
    }

    return (b_ok);
}

/*
 * Sort the widths of one kind and split them after index narrow_top:
 * the element above it must be clearly wider (CFG_BC_GAP_RATIO_X100).
 * The threshold is half-way between the two.
 */
static bool
bc_decode_split (uint32_t * p_values, uint32_t count, uint32_t narrow_top,
                 uint32_t * p_threshold)
{
    uint32_t narrow = 0u;
    uint32_t wide   = 0u;

    bc_decode_sort(p_values, count);
    narrow = p_values[narrow_top];
    wide   = p_values[narrow_top + 1u];
    *p_threshold = (narrow + wide) / 2u;

    /* Widths are below a few seconds in us, so the products fit. */
    return ((wide * BC_RATIO_SCALE) >=
            (narrow * (uint32_t) CFG_BC_GAP_RATIO_X100));
}

/* Insertion sort, ascending (at most 5 values). */
static void
bc_decode_sort (uint32_t * p_values, uint32_t count)
{
    for (uint32_t idx = 1u; idx < count; idx++)
    {
        uint32_t const value = p_values[idx];
        uint32_t       pos   = idx;

        while ((pos > 0u) && (p_values[pos - 1u] > value))
        {
            p_values[pos] = p_values[pos - 1u];
            pos--;
        }

        p_values[pos] = value;
    }
}

/*** end of file ***/
