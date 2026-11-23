/*
 *  subsystem4.h
 *  Subsystem 4 (Buddy 4, Yee Sheng Chua): IMU and safety.
 *    IMU Task     priority 8, core 1, every 10 ms   (report section 3)
 *    Safety Task  priority 5, core 0, every 10 ms and on a fault event
 *
 *  Public interface of subsystem 4.  Other subsystems include it as
 *  "../subsystem4/subsystem4.h".  Interface IDs refer to TEAM.md
 *  section 5.
 */

#ifndef SUBSYSTEM4_H
#define SUBSYSTEM4_H

#include "../app.h"
#include "imu_msgs.h"

/* ---------------------------------------------------------------------
 *  Safety's event flag (owned by subsystem 4).  Other tasks only use the
 *  functions below.
 * --------------------------------------------------------------------- */
#define SAFETY_FLG_TICK         0x00000001u /* cyclic handler, 10 ms */
#define SAFETY_FLG_FAULT_EVT    0x00000002u /* a fault_evt_t was queued */
#define SAFETY_FLG_ZERO_ACK     0x00000004u /* IF-09: Motion zeroed */
#define SAFETY_FLG_FAULT_LOST   0x00000008u /* fault record not queued */
#define SAFETY_FLG_WAKE         (SAFETY_FLG_TICK | SAFETY_FLG_FAULT_EVT | \
                                 SAFETY_FLG_ZERO_ACK | SAFETY_FLG_FAULT_LOST)
#define SAFETY_FLG_FAULTED      0x00000100u /* level: FAULT latched */
#define SAFETY_FLG_IMU_OK       0x00000200u /* level: IMU data healthy */
#define SAFETY_FLG_REMOTE_RESET 0x00000400u /* level: RESET asked for over MQTT */

/* Watchdog check-in bits (report section 3), one per critical task. */
#define SAFETY_CHECKIN_LINE     0x00010000u
#define SAFETY_CHECKIN_NAV      0x00020000u
#define SAFETY_CHECKIN_MOTION   0x00040000u
#define SAFETY_CHECKIN_IMU      0x00080000u
#define SAFETY_CHECKIN_SCAN     0x00100000u
#define SAFETY_CHECKIN_ALL      (SAFETY_CHECKIN_LINE | SAFETY_CHECKIN_NAV | \
                                 SAFETY_CHECKIN_MOTION | SAFETY_CHECKIN_IMU | \
                                 SAFETY_CHECKIN_SCAN)

IMPORT ID flg_safety;
IMPORT ID mbf_fault;

/* IF-04: report a fault; never blocks, callable from tasks only. */
void safety_report_fault(uint8_t cause, uint8_t source, uint16_t detail);
/* IF-04: a status cause (FAULT_CAUSE_NO_ECHO, FAULT_CAUSE_WIFI_LOSS)
   started or ended; never latches, never blocks, tasks only.  False if
   the queue was full: report it again later. */
bool safety_report_status(uint8_t cause, uint8_t source, bool b_on);
/* True while Safety holds a fault latched (FAULT state). */
bool safety_is_faulted(void);
/* Watchdog check-in, once per cycle of each critical task. */
void safety_checkin(uint32_t checkin_bit);
/* IF-09: Motion acknowledges it has cleared queue, command and PID. */
void safety_ack_zero(void);
/* IMU health for "RESET, cause cleared" (report 6.4). */
void safety_set_imu_ok(bool b_ok);
/* Press RESET remotely (Command task, "reset" command): handled at
   Safety's next 10 ms tick exactly like a GP21 press. */
void safety_request_reset(void);

#endif /* SUBSYSTEM4_H */

/*** end of file ***/
