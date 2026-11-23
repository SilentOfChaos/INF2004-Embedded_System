/*
 *  imu_msgs.h
 *  Message formats of subsystem 4 (IMU and Safety): IF-03 and IF-04, each
 *  field packed into the bits its range needs (minimum-bit change, D-89).
 *  Build and read them with the functions below.
 *
 *  Pure C (no kernel headers), so host unit tests can use these types.
 */

#ifndef IMU_MSGS_H
#define IMU_MSGS_H

#include <stdbool.h>
#include <stdint.h>

/* ---------------------------------------------------------------------
 *  IF-03  IMU -> Navigation: imu_evt_t in Navigation's mbf_imu_evt.
 * --------------------------------------------------------------------- */
#define IMU_EVT_HEADING         1u  /* periodic heading, turn rate, class */
#define IMU_EVT_HUMP_START      2u  /* roll above threshold: ON HUMP */
#define IMU_EVT_HUMP_END        3u  /* level again: peak roll and height */
#define IMU_EVT_CLASS           4u  /* motion class changed (FR17) */

/* Motion classes, FR17, plus "steady" for constant-speed driving
   (docs/decisions.md D-58). */
typedef enum
{
    IMU_CLASS_STATIONARY = 0,
    IMU_CLASS_ACCELERATING,
    IMU_CLASS_TURNING,
    IMU_CLASS_CLIMBING,
    IMU_CLASS_DESCENDING,
    IMU_CLASS_IMPACT,
    IMU_CLASS_STEADY,
    IMU_CLASS_COUNT
} imu_class_t;

/*
 * One word; the fields after the type depend on the type, and carry
 * what Navigation reads:
 *   bits  0-1   type - 1        IMU_EVT_xxx, 1..4
 *   HEADING
 *   bits  2-17  heading_cdeg    tilt-compensated, (-18000, 18000]
 *   bit   18    level ground    compass usable (not on a hump)
 *   HUMP_END
 *   bits  2-16  peak_roll_cdeg  largest |roll| on the hump, 0..18000
 *   bits 17-23  height_mm       from the table, 0..127 (its largest
 *                               entry is 65 mm)
 *   CLASS
 *   bits  2-4   motion class    imu_class_t, 0..6
 *   HUMP_START: the type only.  Unused bits are 0.
 * The turn rate, roll, pitch and class of each sample go to Telemetry in
 * the IMU status record (IF-12), not here.
 */
typedef struct
{
    uint32_t bits;
} imu_evt_t;

bool        imu_evt_heading(imu_evt_t * p_evt, int32_t heading_cdeg,
                            bool b_level_ground);
void        imu_evt_hump_start(imu_evt_t * p_evt);
bool        imu_evt_hump_end(imu_evt_t * p_evt, int32_t peak_roll_cdeg,
                             int32_t height_mm);
bool        imu_evt_class(imu_evt_t * p_evt, imu_class_t motion_class);
uint8_t     imu_evt_type(imu_evt_t const * p_evt);
int32_t     imu_evt_heading_cdeg(imu_evt_t const * p_evt);
bool        imu_evt_level_ground(imu_evt_t const * p_evt);
int32_t     imu_evt_peak_roll_cdeg(imu_evt_t const * p_evt);
int32_t     imu_evt_height_mm(imu_evt_t const * p_evt);
imu_class_t imu_evt_motion_class(imu_evt_t const * p_evt);

/* ---------------------------------------------------------------------
 *  IF-04  any task -> Safety: fault reports (fault_evt_t in mbf_fault).
 *  Use safety_report_fault() or safety_report_status(); never send to
 *  mbf_fault directly.
 *
 *  The fault table (Change Requirement 1; PostTri1 Version C).  Each
 *  cause is one bit of Safety's fault mask, bit (cause - 1), and has a
 *  latch flag.  A latching cause latches FAULT: Motion brakes and only
 *  RESET releases it (report 6.4).  A status cause only sets or clears
 *  its bit and never brakes the car.
 *
 *    cause        latches  raised by
 *    LIFTED       yes      IMU: car lifted or overturned during a run
 *                          (detail 1 = lifted, 2 = overturned); replaces
 *                          collision detection (change, 19 Nov 2026)
 *    STOP_CMD     yes      Command: MQTT STOP (FR14, FR15)
 *    IMU_DATA     yes      IMU: invalid data (report 6.4)
 *    SELF_TEST    yes      Navigation: a sensor not healthy in SELF-TEST
 *                          (report 5)
 *    QUEUE_FULL   yes      any sender: a control queue was full (report 4)
 *    LOST_RECORD  yes      Safety: a fault report could not be queued
 *    LINE_DATA    yes      Line: reads failed (D-79); Navigation: the
 *                          line record stopped changing in a run (D-94)
 *    MOTOR_STALL  yes      Motion: the driven wheels give no counts
 *    ENCODER      yes      Motion: one driven wheel gives no counts while
 *                          the other turns (an encoder fault, or that
 *                          wheel held: risk R5)
 *    NO_ECHO      no       Scan: CFG_SCAN_INVALID_LIMIT invalid readings
 *                          in a row; in a run Navigation goes to SAFE HOLD
 *                          (NO SAFE ROUTE).  A sensor that does not answer
 *                          at all fails SELF-TEST, which latches.
 *    WIFI_LOSS    no       Command: no MQTT link (NFR10: never stops the
 *                          car)
 * --------------------------------------------------------------------- */
#define FAULT_CAUSE_LIFTED   1u
#define FAULT_CAUSE_STOP_CMD    2u
#define FAULT_CAUSE_IMU_DATA    3u
#define FAULT_CAUSE_SELF_TEST   4u
#define FAULT_CAUSE_QUEUE_FULL  5u
#define FAULT_CAUSE_LOST_RECORD 6u
#define FAULT_CAUSE_LINE_DATA   7u
#define FAULT_CAUSE_MOTOR_STALL 8u
#define FAULT_CAUSE_ENCODER     9u
#define FAULT_CAUSE_NO_ECHO     10u
#define FAULT_CAUSE_WIFI_LOSS   11u
#define FAULT_CAUSE_MAX         FAULT_CAUSE_WIFI_LOSS

/* The fault mask: one bit per cause, bit (cause - 1). */
#define FAULT_MASK_BITS         FAULT_CAUSE_MAX
#define FAULT_BIT(cause)        (1u << ((cause) - 1u))
#define FAULT_MASK_ALL          ((1u << FAULT_MASK_BITS) - 1u)
/* The latch flags: every cause latches except these status causes. */
#define FAULT_MASK_STATUS       (FAULT_BIT(FAULT_CAUSE_NO_ECHO) | \
                                 FAULT_BIT(FAULT_CAUSE_WIFI_LOSS))
#define FAULT_MASK_LATCHING     (FAULT_MASK_ALL & ~FAULT_MASK_STATUS)

#define FAULT_SRC_LINE          1u
#define FAULT_SRC_NAV           2u
#define FAULT_SRC_IMU           3u
#define FAULT_SRC_MOTION        4u
#define FAULT_SRC_SCAN          5u
#define FAULT_SRC_COMMAND       6u
#define FAULT_SRC_TELEMETRY     7u
#define FAULT_SRC_SAFETY        8u

/*
 * One word:
 *   bits  0-3   cause         FAULT_CAUSE_xxx, 1..11
 *   bits  4-6   source - 1    FAULT_SRC_xxx, 1..8
 *   bits  7-19  detail        cause-specific, 0..8191: the largest is a
 *                             collision's |a|, under 6929 mg; a SELF-TEST
 *                             failure's missing health bits are at most
 *                             0x0700, a stall's wheels 3, a queue's ID
 *                             and an error code are small
 *   bit   20    cleared       status causes: 1 = the condition has ended,
 *                             0 = it has started; 0 for the others
 *   bits 21-31  0 (spare)
 */
#define FAULT_DETAIL_MAX        8191u

typedef struct
{
    uint32_t bits;
} fault_evt_t;

bool     fault_evt_pack(fault_evt_t * p_evt, uint8_t cause, uint8_t source,
                        uint16_t detail);
bool     fault_evt_pack_status(fault_evt_t * p_evt, uint8_t cause,
                               uint8_t source, bool b_on);
uint8_t  fault_evt_cause(fault_evt_t const * p_evt);
uint8_t  fault_evt_source(fault_evt_t const * p_evt);
uint16_t fault_evt_detail(fault_evt_t const * p_evt);
bool     fault_evt_cleared(fault_evt_t const * p_evt);

uint16_t fault_cause_bit(uint8_t cause);
bool     fault_cause_latches(uint8_t cause);

#endif /* IMU_MSGS_H */

/*** end of file ***/
