/*
 *  bitpack.c
 *  Bit arrays for the minimum-bit change.  See bitpack.h.
 */

#include <stddef.h>

#include "bitpack.h"

/* Field index of width bits; it may continue in the next word. */
uint32_t
bitpack_array_get (uint32_t const * p_words, uint32_t index, uint32_t width)
{
    uint32_t value = 0u;

    if ((NULL != p_words) && (width >= 1u) && (width <= BITPACK_WORD_BITS))
    {
        uint32_t const bit   = index * width;
        uint32_t const word  = bit / BITPACK_WORD_BITS;
        uint32_t const shift = bit % BITPACK_WORD_BITS;

        value = p_words[word] >> shift;

        if ((shift + width) > BITPACK_WORD_BITS)
        {
            /* shift is 1..31 here, so the shift below is too. */
            value |= p_words[word + 1u] << (BITPACK_WORD_BITS - shift);
        }

        value &= bitpack_mask(width);
    }

    return (value);
}

/*
 * Set field index.  A value above the field's maximum stores the maximum
 * and returns false (bitpack.h).
 */
bool
bitpack_array_set (uint32_t * p_words, uint32_t index, uint32_t width,
                   uint32_t value)
{
    bool b_fits = false;

    if ((NULL != p_words) && (width >= 1u) && (width <= BITPACK_WORD_BITS))
    {
        uint32_t const mask  = bitpack_mask(width);
        uint32_t const bit   = index * width;
        uint32_t const word  = bit / BITPACK_WORD_BITS;
        uint32_t const shift = bit % BITPACK_WORD_BITS;
        uint32_t const field = (value <= mask) ? value : mask;

        b_fits = (value <= mask);
        p_words[word] = (p_words[word] & ~(mask << shift)) |
                        (field << shift);

        if ((shift + width) > BITPACK_WORD_BITS)
        {
            /* The top (shift + width - 32) bits go to the next word. */
            uint32_t const done = BITPACK_WORD_BITS - shift;   /* 1..31 */

            p_words[word + 1u] = (p_words[word + 1u] & ~(mask >> done)) |
                                 (field >> done);
        }
    }

    return (b_fits);
}

/*** end of file ***/
