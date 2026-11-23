/*
 *  line_keep.h
 *  What the Line task keeps from one 5 ms cycle to the next, packed into
 *  two words (minimum-bit change, D-87):
 *    word 0  bits  0-11  raw_l          last left reading, 0..4095
 *            bits 12-23  raw_r          last right reading
 *            bits 24-29  cycle          cycles modulo LINE_STATUS_EVERY
 *            bit  30     window ok      every reading in range since the
 *                                       last status
 *            bit  31     edges dropped  the ISR dropped an edge since the
 *                                       queue was last emptied
 *    word 1  bits  0-7   letters        letters read, modulo 256
 *            bits  8-12  last letter    0 = none yet, 1..26 = 'A'..'Z'
 *            bits 13-31  0 (spare)
 *  The letter count is only ever reported modulo 256 (IF-12) and the
 *  cycle count only decides when a status is due, so both wrap in their
 *  width.  Failed barcode captures are not counted: nothing read that
 *  count (D-92).
 *
 *  Pure C, no kernel headers: tested on the host (testing/host/).
 */

#ifndef LINE_KEEP_H
#define LINE_KEEP_H

#include <stdbool.h>
#include <stdint.h>

#include "../app_config.h"

/* Line cycles per status record for Telemetry (FR12). */
#define LINE_STATUS_EVERY       (CFG_PERIOD_TELEMETRY_MS / CFG_PERIOD_LINE_MS)
#define LINE_KEEP_NO_LETTER     '-'     /* last letter before any read */

typedef struct
{
    uint32_t word[2];           /* layout above */
} line_keep_t;

void     line_keep_init(line_keep_t * p_keep);
bool     line_keep_set_raw(line_keep_t * p_keep, uint32_t raw_l,
                           uint32_t raw_r);
uint32_t line_keep_raw_l(line_keep_t const * p_keep);
uint32_t line_keep_raw_r(line_keep_t const * p_keep);
bool     line_keep_next_cycle(line_keep_t * p_keep);
bool     line_keep_window_ok(line_keep_t const * p_keep);
void     line_keep_set_window_ok(line_keep_t * p_keep, bool b_ok);
bool     line_keep_note_sample(line_keep_t * p_keep, bool b_ok);
bool     line_keep_edges_dropped(line_keep_t const * p_keep);
void     line_keep_set_edges_dropped(line_keep_t * p_keep, bool b_dropped);
void     line_keep_count_letter(line_keep_t * p_keep, char letter);
uint32_t line_keep_letters(line_keep_t const * p_keep);
char     line_keep_last_letter(line_keep_t const * p_keep);

#endif /* LINE_KEEP_H */

/*** end of file ***/
