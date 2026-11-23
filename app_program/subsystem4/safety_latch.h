/*
 *  safety_latch.h
 *  Safety's fault latch and its RESET handshake (report 6.4, SD2) as
 *  pure logic, so the host tests can run every order of faults, RESET
 *  presses and Motion's acknowledgement (S4-U07).  safety_task.c acts on
 *  what these functions return: pins, flags, alarm, messages.
 *
 *  A fault latches when none is latched.  While one is latched, a later
 *  fault is only counted, except while a RESET waits for Motion's
 *  acknowledgement: then the reset is cancelled and that fault becomes
 *  the latched cause, so RESET must be pressed again and its "cause
 *  cleared" check applies to the new fault (G20, D-95).  Motion's
 *  acknowledgement releases the latch only while a reset is waiting; at
 *  any other time it is ignored.
 *
 *  The fault mask (Change Requirement 1; imu_msgs.h has the fault
 *  table): every latching cause reported since the last release sets
 *  its bit, the first one and the counted ones alike, and the release
 *  clears them.  A status cause (no echo, Wi-Fi loss) only sets or
 *  clears its own bit: it never latches, never counts as a fault and
 *  never cancels a waiting reset.
 *
 *  Pure C, no kernel headers: tested on the host (testing/host/).
 */

#ifndef SAFETY_LATCH_H
#define SAFETY_LATCH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool     b_latched;
    bool     b_reset_pending;   /* RESET pressed: waiting for Motion */
    uint8_t  cause;             /* the latched fault: FAULT_CAUSE_xxx */
    uint8_t  source;            /* FAULT_SRC_xxx */
    uint16_t detail;
    uint16_t mask;              /* the fault mask, FAULT_MASK_BITS bits */
} safety_latch_t;

typedef enum
{
    SAFETY_LATCH_COUNTED = 0,       /* already latched: counted only */
    SAFETY_LATCH_LATCHED,           /* latched now */
    SAFETY_LATCH_RESET_CANCELLED,   /* a waiting reset was cancelled: this
                                       fault is the latched cause now */
    SAFETY_LATCH_STATUS             /* a status cause: its bit set or
                                       cleared, nothing latched */
} safety_latch_out_t;

void               safety_latch_init(safety_latch_t * p_latch);
safety_latch_out_t safety_latch_report(safety_latch_t * p_latch,
                                       uint8_t cause, uint8_t source,
                                       uint16_t detail, bool b_cleared);
bool               safety_latch_reset(safety_latch_t * p_latch,
                                      bool b_cause_cleared);
bool               safety_latch_ack(safety_latch_t * p_latch);
void               safety_latch_ack_timeout(safety_latch_t * p_latch);

#endif /* SAFETY_LATCH_H */

/*** end of file ***/
