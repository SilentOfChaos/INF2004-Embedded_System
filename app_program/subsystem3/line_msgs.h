/*
 *  line_msgs.h
 *  Message formats of subsystem 3 (Line): IF-01 and IF-02, each field
 *  packed into the bits its range needs (minimum-bit change, D-84).
 *  Use the functions below to build and read them; the layouts are
 *  given so a reader of a memory dump can decode them.
 *
 *  Pure C (no kernel headers), so host unit tests can use these types.
 */

#ifndef LINE_MSGS_H
#define LINE_MSGS_H

#include <stdbool.h>
#include <stdint.h>

/* ---------------------------------------------------------------------
 *  IF-01  Line -> Navigation: one line_evt_t in Navigation's
 *  mbf_line_evt.  16 bits:
 *    bits  0-2   type      LINE_EVT_xxx, 1..6
 *    bits  3-7   letter    LINE_EVT_LETTER: 1..26 = 'A'..'Z'; else 0
 *    bit   8     reversed  LINE_EVT_LETTER: read backwards
 *    bits  9-15  0 (spare)
 *  A failed capture's reason is not sent: Navigation treats every
 *  failure the same, and nothing else reads it (D-84).
 * --------------------------------------------------------------------- */
#define LINE_EVT_JUNCTION         1u  /* both fully dark, 3 samples */
#define LINE_EVT_LOST             2u  /* both white for 300 ms */
#define LINE_EVT_FOUND            3u  /* line seen again after LOST */
#define LINE_EVT_CAPTURE_STARTED  4u  /* first bar: hold the speed */
#define LINE_EVT_LETTER           5u  /* valid letter decoded */
#define LINE_EVT_CAPTURE_FAILED   6u  /* no valid letter either way */

#define LINE_EVT_TYPE_SHIFT       0u
#define LINE_EVT_TYPE_BITS        3u
#define LINE_EVT_LETTER_SHIFT     3u
#define LINE_EVT_LETTER_BITS      5u
#define LINE_EVT_REVERSED_BIT     8u
#define LINE_EVT_USED_BITS        9u

typedef struct
{
    uint16_t bits;
} line_evt_t;

bool    line_evt_pack(line_evt_t * p_evt, uint8_t type, char letter,
                      bool b_reversed);
uint8_t line_evt_type(line_evt_t const * p_evt);
char    line_evt_letter(line_evt_t const * p_evt);
bool    line_evt_reversed(line_evt_t const * p_evt);

/* ---------------------------------------------------------------------
 *  IF-02  Line -> Navigation: the latest line record, shared under
 *  mtx_line_err (one of the two shared variables the report justifies).
 *  Read it only with line_err_read().  Two words:
 *    word 0  bits  0-10  error_pm  -1000..1000 (two's complement), + =
 *                                  line under the left sensor, steer left
 *            bit  11     line seen at least one sensor not white
 *            bit  12     valid     the last sample was read
 *            bits 13-20  seq       samples taken, modulo 256
 *            bits 21-31  0 (spare)
 *    word 1  bits  0-11  raw_l     ADC counts, 0..4095
 *            bits 12-23  raw_r
 *            bits 24-31  0 (spare)
 *  Navigation reads the error, line seen and valid; the on-target tests
 *  read the readings and seq (S3-U20, S3-U21).  seq only tells how many
 *  samples were taken between two reads less than 256 samples (1.28 s)
 *  apart: line_err_samples().  LINE LOST is not kept here: Navigation
 *  learns it from the LOST and FOUND events (D-84).
 * --------------------------------------------------------------------- */
#define LINE_ERR_WORDS            2u
#define LINE_ERR_ERROR_SHIFT      0u
#define LINE_ERR_ERROR_BITS       11u
#define LINE_ERR_SEEN_BIT         11u
#define LINE_ERR_VALID_BIT        12u
#define LINE_ERR_SEQ_SHIFT        13u
#define LINE_ERR_SEQ_BITS         8u
#define LINE_ERR_RAW_L_SHIFT      0u
#define LINE_ERR_RAW_R_SHIFT      12u
#define LINE_ERR_RAW_BITS         12u
#define LINE_ERR_ERROR_MAX_PM     1000

typedef struct
{
    uint32_t word[LINE_ERR_WORDS];
} line_err_t;

void     line_err_init(line_err_t * p_rec);
bool     line_err_set_sample(line_err_t * p_rec, int32_t error_pm,
                             uint32_t raw_l, uint32_t raw_r,
                             bool b_line_seen);
void     line_err_set_invalid(line_err_t * p_rec);
int32_t  line_err_error_pm(line_err_t const * p_rec);
uint32_t line_err_raw_l(line_err_t const * p_rec);
uint32_t line_err_raw_r(line_err_t const * p_rec);
bool     line_err_line_seen(line_err_t const * p_rec);
bool     line_err_valid(line_err_t const * p_rec);
uint32_t line_err_seq(line_err_t const * p_rec);
uint32_t line_err_samples(line_err_t const * p_newer,
                          line_err_t const * p_older);

#endif /* LINE_MSGS_H */

/*** end of file ***/
