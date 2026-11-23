/*
 *  bitpack.h
 *  Packing application data into the fewest bits (minimum-bit change,
 *  docs/decisions.md section I).
 *
 *  A packed record is one or more uint32_t words.  A field is named by its
 *  lowest bit (shift) and its width in bits; bit 0 is the least
 *  significant bit of the word.  Only masks and shifts on unsigned values
 *  are used: no C bit-fields, whose layout the compiler chooses, and no
 *  bitwise operators on signed values (Barr-C 5.3, 5.5).
 *
 *  Signed fields are two's complement in their own width: w bits hold
 *  -2^(w-1)..2^(w-1)-1.  The conversions use arithmetic only, so they are
 *  defined behaviour in C.  Signed widths are 1..31; unsigned 1..32.
 *
 *  A value that does not fit is never cut silently: the set functions
 *  store the nearest value that fits and return false, so the caller can
 *  count it.  Every record's pack function checks its ranges, and its
 *  tests check the boundaries.
 *
 *  Wrapping counters and time stamps: bitpack_sub() gives the difference
 *  of two w-bit values modulo 2^w, which is right as long as the true
 *  difference is below 2^w (bitpack_sub_signed(): magnitude below
 *  2^(w-1)).
 *
 *  Bit arrays (bitpack.c) hold equal fields back to back, so a field may
 *  span two words.  They are for data one task owns; data shared between
 *  tasks is copied as whole words.
 *
 *  Pure C: no kernel headers, so the host unit tests can build it.
 */

#ifndef BITPACK_H
#define BITPACK_H

#include <stdbool.h>
#include <stdint.h>

#define BITPACK_WORD_BITS   32u

/* Bits needed for the unsigned values 0..max, at compile time (1 for a
   max of 0 or 1). */
#define BITPACK_BITS8_(v)                                                 \
    (((v) < 0x2u) ? 1u : ((v) < 0x4u) ? 2u : ((v) < 0x8u) ? 3u :          \
     ((v) < 0x10u) ? 4u : ((v) < 0x20u) ? 5u : ((v) < 0x40u) ? 6u :       \
     ((v) < 0x80u) ? 7u : 8u)
#define BITPACK_BITS_FOR(max)                                             \
    (((max) < 0x100u) ? BITPACK_BITS8_(max) :                             \
     ((max) < 0x10000u) ? (8u + BITPACK_BITS8_((max) >> 8)) :             \
     ((max) < 0x1000000u) ? (16u + BITPACK_BITS8_((max) >> 16)) :         \
     (24u + BITPACK_BITS8_((max) >> 24)))

/* Bits needed for the signed values -maxabs..maxabs. */
#define BITPACK_BITS_FOR_SIGNED(maxabs)  (1u + BITPACK_BITS_FOR(maxabs))

/* Words needed for count fields of width bits in a bit array. */
#define BITPACK_ARRAY_WORDS(count, width)                                 \
    ((((count) * (width)) + (BITPACK_WORD_BITS - 1u)) / BITPACK_WORD_BITS)

/* Fails the build if cond is false (at file scope, for layouts). */
#define BITPACK_ASSERT(cond, name)                                        \
    typedef char bitpack_assert_##name[(cond) ? 1 : -1]

/* Mask of the low width bits (width 1..32). */
static inline uint32_t
bitpack_mask (uint32_t width)
{
    return ((width >= BITPACK_WORD_BITS) ? UINT32_MAX
                                         : ((1u << width) - 1u));
}

/* A w-bit two's complement field as a signed value (width 1..31). */
static inline int32_t
bitpack_to_signed (uint32_t field, uint32_t width)
{
    uint32_t const half  = 1u << (width - 1u);
    int32_t        value = 0;

    if (field >= half)
    {
        /* field - half and half are below 2^30: both fit int32_t. */
        value = (int32_t) (field - half) - (int32_t) half;
    }
    else
    {
        value = (int32_t) field;
    }

    return (value);
}

/* A signed value as a w-bit two's complement field (width 1..31; the
   value must already be in range, see bitpack_set_signed). */
static inline uint32_t
bitpack_from_signed (int32_t value, uint32_t width)
{
    uint32_t const half  = 1u << (width - 1u);
    uint32_t       field = 0u;

    if (value < 0)
    {
        /* value >= -half, so value + half is 0..half-1. */
        field = (uint32_t) (value + (int32_t) half) + half;
    }
    else
    {
        field = (uint32_t) value;
    }

    return (field);
}

/* A whole word holding an int32_t as two's complement, back as int32_t.
   Arithmetic only: a plain cast is implementation-defined in C for a
   word above INT32_MAX.  Store with (uint32_t) value, which is defined. */
static inline int32_t
bitpack_word_signed (uint32_t word)
{
    /* The top 31 bits as a signed value (floor of value / 2), doubled,
       plus the lowest bit: -2^31 and 2^31 - 1 stay in range. */
    return ((bitpack_to_signed(word >> 1u, 31u) * 2) +
            (int32_t) (word & 1u));
}

/* Unsigned field of a word. */
static inline uint32_t
bitpack_get (uint32_t word, uint32_t shift, uint32_t width)
{
    return ((word >> shift) & bitpack_mask(width));
}

/* Signed field of a word (width 1..31). */
static inline int32_t
bitpack_get_signed (uint32_t word, uint32_t shift, uint32_t width)
{
    return (bitpack_to_signed(bitpack_get(word, shift, width), width));
}

/* One-bit flag of a word. */
static inline bool
bitpack_get_flag (uint32_t word, uint32_t bit)
{
    return (0u != ((word >> bit) & 1u));
}

/*
 * Set an unsigned field.  A value above the field's maximum stores the
 * maximum and returns false.
 */
static inline bool
bitpack_set (uint32_t * p_word, uint32_t shift, uint32_t width,
             uint32_t value)
{
    uint32_t const mask   = bitpack_mask(width);
    bool const     b_fits = (value <= mask);
    uint32_t const field  = b_fits ? value : mask;

    *p_word = (*p_word & ~(mask << shift)) | (field << shift);

    return (b_fits);
}

/*
 * Set a signed field (width 1..31).  A value outside -2^(w-1)..2^(w-1)-1
 * stores the nearest end and returns false.
 */
static inline bool
bitpack_set_signed (uint32_t * p_word, uint32_t shift, uint32_t width,
                    int32_t value)
{
    int32_t const high   = (int32_t) ((1u << (width - 1u)) - 1u);
    int32_t const low    = -high - 1;
    bool const    b_fits = ((value >= low) && (value <= high));
    int32_t const kept   = (value < low) ? low
                                         : ((value > high) ? high : value);

    (void) bitpack_set(p_word, shift, width,
                       bitpack_from_signed(kept, width));

    return (b_fits);
}

/* Set or clear a one-bit flag. */
static inline void
bitpack_set_flag (uint32_t * p_word, uint32_t bit, bool b_value)
{
    (void) bitpack_set(p_word, bit, 1u, b_value ? 1u : 0u);
}

/* newer - older for w-bit wrapping values, modulo 2^w (width 1..32). */
static inline uint32_t
bitpack_sub (uint32_t newer, uint32_t older, uint32_t width)
{
    return ((newer - older) & bitpack_mask(width));
}

/* The same as a signed difference, -2^(w-1)..2^(w-1)-1 (width 1..31). */
static inline int32_t
bitpack_sub_signed (uint32_t newer, uint32_t older, uint32_t width)
{
    return (bitpack_to_signed(bitpack_sub(newer, older, width), width));
}

/* Bit arrays: field index of width bits (1..32), back to back from bit 0
   of word 0.  The caller sizes the array with BITPACK_ARRAY_WORDS. */
uint32_t bitpack_array_get(uint32_t const * p_words, uint32_t index,
                           uint32_t width);
bool     bitpack_array_set(uint32_t * p_words, uint32_t index,
                           uint32_t width, uint32_t value);

#endif /* BITPACK_H */

/*** end of file ***/
