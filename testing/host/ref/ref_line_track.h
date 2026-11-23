/*
 *  ref_line_track.h
 *  TEST ONLY.  A frozen copy of line tracking as it was before the
 *  minimum-bit change (app_program/subsystem3/line_track.h at c644b68),
 *  renamed ref_*, so the host tests can run the old and the packed
 *  versions side by side (S3-U45).  Never used in the firmware.  Do not
 *  change it: it is the reference.
 */
#ifndef REF_LINE_TRACK_H
#define REF_LINE_TRACK_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    REF_LINE_TRACK_EVT_NONE = 0,
    REF_LINE_TRACK_EVT_JUNCTION,
    REF_LINE_TRACK_EVT_LOST,
    REF_LINE_TRACK_EVT_FOUND
} ref_line_track_evt_t;

typedef struct
{
    int32_t  white_l;           /* current levels, ADC counts */
    int32_t  black_l;
    int32_t  white_r;
    int32_t  black_r;
    int32_t  error_pm;          /* last line error */
    int32_t  dark_l_pm;         /* last darkness values */
    int32_t  dark_r_pm;
    uint32_t dark_run;          /* samples in a row with both fully dark */
    uint32_t rearm_run;         /* samples since the crossbar ended */
    uint32_t white_ms;          /* time both have been white */
    uint32_t read_fail_run;     /* failed sensor reads in a row (D-79) */
    bool     b_junction_armed;
    bool     b_lost;
    bool     b_line_seen;
} ref_line_track_t;

void             ref_line_track_init(ref_line_track_t * p_track);
ref_line_track_evt_t ref_line_track_sample(ref_line_track_t * p_track, uint32_t raw_l,
                                   uint32_t raw_r, uint32_t period_ms,
                                   bool b_adapt);
int32_t          ref_line_track_darkness(int32_t raw, int32_t white,
                                     int32_t black);
bool             ref_line_track_read_result(ref_line_track_t * p_track,
                                        bool b_read_ok);

#endif /* REF_LINE_TRACK_H */

/*** end of file ***/
