/*
 *  bc_capture.h
 *  The BARCODE region of the Line subsystem statechart (report 6.3,
 *  Fig 8): WAITING -> CAPTURING -> DECODING -> WAITING.
 *
 *  The Barcode ISR time-stamps every edge of the D0 signal; the Line
 *  task feeds those edges here with bc_capture_edge() and calls
 *  bc_capture_poll() every 5 ms.  WAITING ends at the first bar (report
 *  6.3).  Edges are kept, not decoded, until:
 *    - 29 elements are complete and stable: decode at once (approved
 *      improvement: decode on the 30th edge instead of waiting); or
 *    - 120 ms pass with no edge (report 6.3); or
 *    - a hard limit is hit (CFG_BC_MAX_EDGES, CFG_BC_MAX_CAPTURE_MS).
 *  The last two always end the capture, so a capture can never hang
 *  waiting for an exact edge count (the condition set when this
 *  improvement was approved, 13 Oct).
 *
 *  The symbol is the first 30 edges of a capture (D-78).  An edge lost
 *  inside it (levels that do not alternate, or the ISR's full queue
 *  reported with bc_capture_mark_lost() while fewer than 30 edges are
 *  stored) fails the capture with BC_FAIL_EDGE_LOST, and no letter is
 *  decoded even if the widths would decode.  Edges and losses after the
 *  30th edge belong to whatever follows the barcode and never stop a
 *  complete symbol from being decoded.
 *
 *  The approved glitch filter is also here: two edges closer than
 *  CFG_BC_GLITCH_US cancel each other.  An optional quiet zone (white
 *  for a set time before a capture may start) is a proposal that is not
 *  approved: it is off (CFG_BC_QUIET_MS 0, decision D-25).
 *
 *  The state is packed (minimum-bit change, D-85): one word of flags and
 *  counts, three full time stamps, and the edges after the first as
 *  offsets from it in a bit array.  Use the functions below; the layout
 *  is given so a reader of a memory dump can decode it.
 *
 *  Pure C, no kernel headers: tested on the host (testing/host/).
 */

#ifndef BC_CAPTURE_H
#define BC_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "../app_config.h"
#include "../common/bitpack.h"
#include "bc_decode.h"

typedef enum
{
    BC_STATE_WAITING = 0,
    BC_STATE_CAPTURING
} bc_state_t;

typedef enum
{
    BC_OUT_NONE = 0,
    BC_OUT_STARTED,     /* first bar seen: Navigation holds the speed */
    BC_OUT_LETTER,      /* valid letter: result.letter, b_reversed */
    BC_OUT_FAILED       /* no letter: result.reason */
} bc_out_type_t;

typedef struct
{
    bc_out_type_t type;
    bc_result_t   result;
} bc_out_t;

/*
 * flags word:
 *   bit   0     state          0 WAITING, 1 CAPTURING
 *   bit   1     dark           current signal level
 *   bit   2     started sent   BC_OUT_STARTED reported
 *   bit   3     decode tried   tried at 29 elements
 *   bit   4     edge lost      an edge inside the symbol was lost:
 *                              no decode (D-78)
 *   bit   5     overflow       an edge could not be stored
 *   bits  6-9   fail reason    BC_FAIL_xxx of that try, 0..8
 *   bits 10-16  edge count     edges stored, 0..CFG_BC_MAX_EDGES (64)
 *   bits 17-24  quiet ms       white needed before a capture, 0..255 ms
 *                              (CFG_BC_QUIET_MS, 0 = none)
 *   bits 25-31  0 (spare)
 */
#define BC_CAP_STATE_BIT        0u
#define BC_CAP_DARK_BIT         1u
#define BC_CAP_STARTED_BIT      2u
#define BC_CAP_TRIED_BIT        3u
#define BC_CAP_LOST_BIT         4u
#define BC_CAP_OVERFLOW_BIT     5u
#define BC_CAP_REASON_SHIFT     6u
#define BC_CAP_REASON_BITS      4u
#define BC_CAP_COUNT_SHIFT      10u
#define BC_CAP_COUNT_BITS       7u
#define BC_CAP_QUIET_SHIFT      17u
#define BC_CAP_QUIET_BITS       8u

/*
 * Edge k (1..63) is stored as microseconds after edge 0, in
 * BC_CAP_OFFSET_BITS bits at index k - 1 of the offsets bit array.  A
 * capture ends at the first 5 ms poll after CFG_BC_MAX_CAPTURE_MS, so a
 * stored edge is at most that long after edge 0, plus however late the
 * Line task is: once the watchdog is armed (radio up, or 15 s after
 * boot), at most its check-in deadline plus the watchdog time before
 * the watchdog resets the chip.  2000 + 50 + 200 ms need 22 bits (up to
 * 4.19 s).  An edge that would still not fit (possible only before the
 * watchdog is armed) ends the capture as too long, like a full buffer
 * (D-85).
 */
#define BC_CAP_OFFSET_MAX_US    (((uint32_t) CFG_BC_MAX_CAPTURE_MS +    \
                                  (uint32_t) CFG_CHECKIN_LINE_MS +      \
                                  (uint32_t) CFG_WDT_TIMEOUT_MS) * 1000u)
#define BC_CAP_OFFSET_BITS      BITPACK_BITS_FOR(BC_CAP_OFFSET_MAX_US)
#define BC_CAP_OFFSET_WORDS     BITPACK_ARRAY_WORDS(CFG_BC_MAX_EDGES - 1u, \
                                                    BC_CAP_OFFSET_BITS)

typedef struct
{
    uint32_t flags;                     /* layout above */
    uint32_t white_since_us;            /* start of the current white */
    uint32_t quiet_from_us;             /* white start before this capture */
    uint32_t first_us;                  /* edge 0: the first bar */
    uint32_t offsets[BC_CAP_OFFSET_WORDS];
} bc_capture_t;

void       bc_capture_init(bc_capture_t * p_cap, bool b_dark_now,
                           uint32_t now_us);
void       bc_capture_edge(bc_capture_t * p_cap, uint32_t time_us,
                           bool b_dark);
bc_out_t   bc_capture_poll(bc_capture_t * p_cap, uint32_t now_us);
void       bc_capture_mark_lost(bc_capture_t * p_cap);
bc_state_t bc_capture_state(bc_capture_t const * p_cap);
bool       bc_capture_overflowed(bc_capture_t const * p_cap);
uint32_t   bc_capture_quiet_ms(bc_capture_t const * p_cap);
bool       bc_capture_set_quiet_ms(bc_capture_t * p_cap, uint32_t quiet_ms);

#endif /* BC_CAPTURE_H */

/*** end of file ***/
