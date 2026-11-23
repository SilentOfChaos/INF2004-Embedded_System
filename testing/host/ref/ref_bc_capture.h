/*
 *  ref_bc_capture.h
 *  TEST ONLY.  A frozen copy of the barcode capture as it was before the
 *  minimum-bit change (app_program/subsystem3/bc_capture.c at c644b68),
 *  renamed ref_bc_capture_*, so the host tests can run the old and the
 *  packed versions side by side and compare every output (S3-U43).
 *  Never used in the firmware.  Do not change it: it is the reference.
 */

#ifndef REF_BC_CAPTURE_H
#define REF_BC_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"
#include "subsystem3/bc_capture.h"  /* bc_state_t, bc_out_t: unchanged */

typedef struct
{
    bc_state_t state;
    bool       b_dark;                  /* current signal level */
    uint32_t   quiet_us;                /* white needed before a capture;
                                           CFG_BC_QUIET_MS, 0 = none */
    uint32_t   white_since_us;          /* start of the current white */
    uint32_t   quiet_from_us;           /* white start before this capture */
    uint32_t   edges_us[CFG_BC_MAX_EDGES];
    uint32_t   edge_count;
    bool       b_started_sent;          /* BC_OUT_STARTED reported */
    bool       b_decode_tried;          /* tried at 29 elements */
    uint8_t    fail_reason;             /* reason of that try */
    bool       b_edge_lost;             /* an edge inside the symbol was
                                           lost: no decode (D-78) */
    bool       b_overflow;
} ref_bc_capture_t;

void     ref_bc_capture_init(ref_bc_capture_t * p_cap, bool b_dark_now,
                             uint32_t now_us);
void     ref_bc_capture_edge(ref_bc_capture_t * p_cap, uint32_t time_us,
                             bool b_dark);
bc_out_t ref_bc_capture_poll(ref_bc_capture_t * p_cap, uint32_t now_us);
void     ref_bc_capture_mark_lost(ref_bc_capture_t * p_cap);

#endif /* REF_BC_CAPTURE_H */

/*** end of file ***/
