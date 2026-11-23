/*
 *  line_msgs.c
 *  Pack and read the IF-01 line events and the IF-02 line record.  See
 *  line_msgs.h for the layouts.
 */

#include <stddef.h>

#include "../common/bitpack.h"
#include "line_msgs.h"

#define LINE_LETTERS        26u     /* 'A'..'Z' as 1..26, 0 = none */
#define LINE_ADC_MAX        4095u   /* 12-bit conversions */

/* The layouts in line_msgs.h hold every value they must. */
BITPACK_ASSERT(BITPACK_BITS_FOR(LINE_EVT_CAPTURE_FAILED) ==
               LINE_EVT_TYPE_BITS, evt_type_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(LINE_LETTERS) == LINE_EVT_LETTER_BITS,
               evt_letter_bits);
BITPACK_ASSERT((LINE_EVT_TYPE_SHIFT + LINE_EVT_TYPE_BITS) ==
               LINE_EVT_LETTER_SHIFT, evt_layout_1);
BITPACK_ASSERT((LINE_EVT_LETTER_SHIFT + LINE_EVT_LETTER_BITS) ==
               LINE_EVT_REVERSED_BIT, evt_layout_2);
BITPACK_ASSERT((LINE_EVT_REVERSED_BIT + 1u) == LINE_EVT_USED_BITS,
               evt_layout_3);
BITPACK_ASSERT(LINE_EVT_USED_BITS <= 16u, evt_fits_16_bits);

BITPACK_ASSERT(BITPACK_BITS_FOR_SIGNED(LINE_ERR_ERROR_MAX_PM) ==
               LINE_ERR_ERROR_BITS, err_error_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(LINE_ADC_MAX) == LINE_ERR_RAW_BITS,
               err_raw_bits);
BITPACK_ASSERT((LINE_ERR_ERROR_SHIFT + LINE_ERR_ERROR_BITS) ==
               LINE_ERR_SEEN_BIT, err_layout_1);
BITPACK_ASSERT(((LINE_ERR_SEEN_BIT + 1u) == LINE_ERR_VALID_BIT) &&
               ((LINE_ERR_VALID_BIT + 1u) == LINE_ERR_SEQ_SHIFT),
               err_layout_2);
BITPACK_ASSERT((LINE_ERR_SEQ_SHIFT + LINE_ERR_SEQ_BITS) <= 32u,
               err_layout_3);
BITPACK_ASSERT((LINE_ERR_RAW_L_SHIFT + LINE_ERR_RAW_BITS) ==
               LINE_ERR_RAW_R_SHIFT, err_layout_4);
BITPACK_ASSERT((LINE_ERR_RAW_R_SHIFT + LINE_ERR_RAW_BITS) <= 32u,
               err_layout_5);

static uint32_t line_err_next_seq(uint32_t word0);

/* ------------------------------------------------------------------ */
/*  IF-01 line events                                                  */
/* ------------------------------------------------------------------ */

/*
 * Build one event.  letter is 'A'..'Z' for LINE_EVT_LETTER and '\0'
 * otherwise.  Returns false if a value did not fit its field (then the
 * nearest value that fits is stored, and any other letter is stored as
 * none); the Line task only ever passes values that fit.
 */
bool
line_evt_pack (line_evt_t * p_evt, uint8_t type, char letter,
               bool b_reversed)
{
    bool b_ok = false;

    if (NULL != p_evt)
    {
        uint32_t word        = 0u;
        uint32_t letter_code = 0u;
        bool     b_letter_ok = true;

        if ((letter >= 'A') && (letter <= 'Z'))
        {
            letter_code = (uint32_t) (letter - 'A') + 1u;   /* 1..26 */
        }
        else
        {
            b_letter_ok = ('\0' == letter);
        }

        b_ok = bitpack_set(&word, LINE_EVT_TYPE_SHIFT, LINE_EVT_TYPE_BITS,
                           (uint32_t) type);
        b_ok = bitpack_set(&word, LINE_EVT_LETTER_SHIFT,
                           LINE_EVT_LETTER_BITS, letter_code) && b_ok;
        bitpack_set_flag(&word, LINE_EVT_REVERSED_BIT, b_reversed);

        /* LINE_EVT_USED_BITS (9) fit 16 bits: nothing is lost. */
        p_evt->bits = (uint16_t) word;
        b_ok = b_ok && b_letter_ok;
    }

    return (b_ok);
}

/* LINE_EVT_xxx; 0 for no event. */
uint8_t
line_evt_type (line_evt_t const * p_evt)
{
    uint8_t type = 0u;

    if (NULL != p_evt)
    {
        /* A 3-bit field: fits uint8_t. */
        type = (uint8_t) bitpack_get((uint32_t) p_evt->bits,
                                     LINE_EVT_TYPE_SHIFT,
                                     LINE_EVT_TYPE_BITS);
    }

    return (type);
}

/* 'A'..'Z', or '\0' when the event carries no letter. */
char
line_evt_letter (line_evt_t const * p_evt)
{
    char letter = '\0';

    if (NULL != p_evt)
    {
        uint32_t const code = bitpack_get((uint32_t) p_evt->bits,
                                          LINE_EVT_LETTER_SHIFT,
                                          LINE_EVT_LETTER_BITS);

        if ((code >= 1u) && (code <= LINE_LETTERS))
        {
            letter = (char) ('A' + (int32_t) (code - 1u));  /* 'A'..'Z' */
        }
    }

    return (letter);
}

bool
line_evt_reversed (line_evt_t const * p_evt)
{
    return ((NULL != p_evt) &&
            bitpack_get_flag((uint32_t) p_evt->bits, LINE_EVT_REVERSED_BIT));
}

/* ------------------------------------------------------------------ */
/*  IF-02 line record                                                  */
/* ------------------------------------------------------------------ */

/* No sample yet: not valid, seq 0. */
void
line_err_init (line_err_t * p_rec)
{
    if (NULL != p_rec)
    {
        p_rec->word[0] = 0u;
        p_rec->word[1] = 0u;
    }
}

/*
 * A new sample: every field replaced, valid set, seq counted on.
 * Returns false if a value did not fit (the nearest value that fits is
 * stored).  The Line task's values always fit: the error is
 * -1000..1000 (line_track.c) and the readings are 12-bit
 * (drv_line_adc.c).
 */
bool
line_err_set_sample (line_err_t * p_rec, int32_t error_pm, uint32_t raw_l,
                     uint32_t raw_r, bool b_line_seen)
{
    bool b_ok = false;

    if (NULL != p_rec)
    {
        uint32_t word0 = 0u;
        uint32_t word1 = 0u;

        b_ok = bitpack_set_signed(&word0, LINE_ERR_ERROR_SHIFT,
                                  LINE_ERR_ERROR_BITS, error_pm);
        bitpack_set_flag(&word0, LINE_ERR_SEEN_BIT, b_line_seen);
        bitpack_set_flag(&word0, LINE_ERR_VALID_BIT, true);
        (void) bitpack_set(&word0, LINE_ERR_SEQ_SHIFT, LINE_ERR_SEQ_BITS,
                           line_err_next_seq(p_rec->word[0]));
        b_ok = bitpack_set(&word1, LINE_ERR_RAW_L_SHIFT, LINE_ERR_RAW_BITS,
                           raw_l) && b_ok;
        b_ok = bitpack_set(&word1, LINE_ERR_RAW_R_SHIFT, LINE_ERR_RAW_BITS,
                           raw_r) && b_ok;

        p_rec->word[0] = word0;
        p_rec->word[1] = word1;
    }

    return (b_ok);
}

/* A failed read (D-79): not valid, seq counted on, the rest kept. */
void
line_err_set_invalid (line_err_t * p_rec)
{
    if (NULL != p_rec)
    {
        uint32_t word0 = p_rec->word[0];

        bitpack_set_flag(&word0, LINE_ERR_VALID_BIT, false);
        (void) bitpack_set(&word0, LINE_ERR_SEQ_SHIFT, LINE_ERR_SEQ_BITS,
                           line_err_next_seq(p_rec->word[0]));
        p_rec->word[0] = word0;
    }
}

int32_t
line_err_error_pm (line_err_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get_signed(p_rec->word[0], LINE_ERR_ERROR_SHIFT,
                                 LINE_ERR_ERROR_BITS)
            : 0);
}

uint32_t
line_err_raw_l (line_err_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get(p_rec->word[1], LINE_ERR_RAW_L_SHIFT,
                          LINE_ERR_RAW_BITS)
            : 0u);
}

uint32_t
line_err_raw_r (line_err_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get(p_rec->word[1], LINE_ERR_RAW_R_SHIFT,
                          LINE_ERR_RAW_BITS)
            : 0u);
}

bool
line_err_line_seen (line_err_t const * p_rec)
{
    return ((NULL != p_rec) &&
            bitpack_get_flag(p_rec->word[0], LINE_ERR_SEEN_BIT));
}

bool
line_err_valid (line_err_t const * p_rec)
{
    return ((NULL != p_rec) &&
            bitpack_get_flag(p_rec->word[0], LINE_ERR_VALID_BIT));
}

/* Samples taken, modulo 256. */
uint32_t
line_err_seq (line_err_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get(p_rec->word[0], LINE_ERR_SEQ_SHIFT,
                          LINE_ERR_SEQ_BITS)
            : 0u);
}

/* Samples taken between two reads of the record, if fewer than 256. */
uint32_t
line_err_samples (line_err_t const * p_newer, line_err_t const * p_older)
{
    return (bitpack_sub(line_err_seq(p_newer), line_err_seq(p_older),
                        LINE_ERR_SEQ_BITS));
}

/* The sequence number after the one in word 0, modulo 256. */
static uint32_t
line_err_next_seq (uint32_t word0)
{
    return ((bitpack_get(word0, LINE_ERR_SEQ_SHIFT, LINE_ERR_SEQ_BITS) +
             1u) & bitpack_mask(LINE_ERR_SEQ_BITS));
}

/*** end of file ***/
