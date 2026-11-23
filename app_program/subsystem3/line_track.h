/*
 *  line_track.h
 *  The LINE TRACKING region of the Line subsystem statechart (report
 *  6.3, Fig 8): TRACKING <-> LINE LOST, with junction detection and the
 *  level adaptation.
 *
 *  The two line sensors sit over the two edges of the 18 mm line, so a
 *  centred car reads both partly dark, a junction crossbar both fully
 *  dark, and lost both white (report 6.3, A9).  Each 5 ms sample:
 *    darkness d = (reading - white) / (black - white), 0..1000 per mille
 *    error      = d_left - d_right, -1000..1000 (+ = line under left)
 *    junction   = both fully dark for 3 samples in a row
 *    line lost  = both white for 300 ms; found again when either is not
 *  A failed sensor read is not a sample: CFG_LINE_READ_FAIL_LIMIT of them
 *  in a row are reported once, so the Line task can latch a fault (D-79).
 *
 *  The state is packed into three words (minimum-bit change, D-86):
 *    word 0  bits  0-11  white level, left    ADC counts, 0..4095
 *            bits 12-23  black level, left
 *            bits 24-25  dark run   both fully dark, samples in a row,
 *                                   kept at most CFG_LINE_JUNCTION_SAMPLES
 *            bits 26-28  rearm run  samples since the crossbar ended,
 *                                   0..CFG_LINE_JUNCTION_REARM
 *            bit  29     junction armed
 *            bit  30     lost       LINE LOST
 *            bit  31     line seen  at least one sensor not white
 *    word 1  bits  0-11  white level, right
 *            bits 12-23  black level, right
 *            bits 24-26  failed reads in a row, 0..CFG_LINE_READ_FAIL_LIMIT
 *            bits 27-31  0 (spare)
 *    word 2  bits  0-10  error_pm   -1000..1000 (two's complement)
 *            bits 11-19  white ms   time both white, kept at most
 *                                   CFG_LINE_LOST_MS
 *            bits 20-31  0 (spare)
 *  The two runs and the white time are only ever compared with their
 *  limit, so keeping them at the limit changes nothing.  Readings must be
 *  12-bit (0..4095), as the ADC gives them; the levels then stay within
 *  0..4095.
 *
 *  Pure C, no kernel headers: tested on the host (testing/host/).
 */

#ifndef LINE_TRACK_H
#define LINE_TRACK_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    LINE_TRACK_EVT_NONE = 0,
    LINE_TRACK_EVT_JUNCTION,
    LINE_TRACK_EVT_LOST,
    LINE_TRACK_EVT_FOUND
} line_track_evt_t;

typedef enum
{
    LINE_TRACK_WHITE_L = 0,
    LINE_TRACK_BLACK_L,
    LINE_TRACK_WHITE_R,
    LINE_TRACK_BLACK_R
} line_track_level_t;

#define LINE_TRACK_WORDS    3u

typedef struct
{
    uint32_t word[LINE_TRACK_WORDS];    /* layout above */
} line_track_t;

void             line_track_init(line_track_t * p_track);
line_track_evt_t line_track_sample(line_track_t * p_track, uint32_t raw_l,
                                   uint32_t raw_r, uint32_t period_ms,
                                   bool b_adapt);
int32_t          line_track_darkness(int32_t raw, int32_t white,
                                     int32_t black);
bool             line_track_read_result(line_track_t * p_track,
                                        bool b_read_ok);
int32_t          line_track_error_pm(line_track_t const * p_track);
bool             line_track_line_seen(line_track_t const * p_track);
bool             line_track_lost(line_track_t const * p_track);
int32_t          line_track_level(line_track_t const * p_track,
                                  line_track_level_t level);
uint32_t         line_track_read_fails(line_track_t const * p_track);

#endif /* LINE_TRACK_H */

/*** end of file ***/
