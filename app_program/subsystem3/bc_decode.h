/*
 *  bc_decode.h
 *  Code 39 decoder for one barcode: '*', a letter, '*' (report 6.3).
 *
 *  Input is the 29 element widths of a capture in the order they passed
 *  the sensor: 9 elements per character (5 bars, 4 spaces, 3 of them
 *  wide) with a narrow gap after the first and second characters.  The
 *  widths are times in microseconds, but only their ratios matter, so no
 *  printed bar size is assumed (docs/notes/05).
 *
 *  The direction comes from the end characters: '*' at both ends is a
 *  forward read; otherwise the element order is reversed and decoded
 *  once more (read backwards, '*' looks like 'P' and every letter looks
 *  like another valid character, so the middle character alone proves
 *  nothing).
 *
 *  Pure C, no kernel headers: tested on the host (testing/host/).
 */

#ifndef BC_DECODE_H
#define BC_DECODE_H

#include <stdbool.h>
#include <stdint.h>

#define BC_CHAR_ELEMENTS    9u
#define BC_SYMBOL_ELEMENTS  29u     /* CFG_BC_ELEMENTS: 3 x 9 + 2 gaps */

/* Why a capture gave no letter (line_evt_t.reason). */
#define BC_FAIL_NONE        0u
#define BC_FAIL_COUNT       1u  /* not exactly 29 elements */
#define BC_FAIL_WIDTHS      2u  /* no clear wide/narrow split */
#define BC_FAIL_PATTERN     3u  /* a character is not Code 39 */
#define BC_FAIL_ENDS        4u  /* not '*' at both ends, either way */
#define BC_FAIL_NOT_LETTER  5u  /* middle character is not A-Z */
#define BC_FAIL_TOO_FEW     6u  /* capture ended before 29 elements */
#define BC_FAIL_TOO_LONG    7u  /* capture hit the edge or time limit */
#define BC_FAIL_EDGE_LOST   8u  /* two edges of the same level: one lost */

typedef struct
{
    char    letter;         /* 'A'..'Z', or '\0' on failure */
    bool    b_reversed;     /* decoded after reversing the order */
    uint8_t reason;         /* BC_FAIL_xxx, BC_FAIL_NONE on success */
} bc_result_t;

bc_result_t bc_decode(uint32_t const * p_widths, uint32_t count);
char        bc_decode_pattern(uint32_t pattern);

#endif /* BC_DECODE_H */

/*** end of file ***/
