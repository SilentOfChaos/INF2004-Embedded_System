/*
 *  tlm_msgs.h
 *  Message formats of subsystem 1 (Command and Telemetry): IF-10 and IF-12.
 *  Pack and read functions: tlm_msgs.c.
 *
 *  Pure C (no kernel headers), so host unit tests can use these types.
 */

#ifndef TLM_MSGS_H
#define TLM_MSGS_H

#include <stdbool.h>
#include <stdint.h>

/* A command's "seq": cmd_parse() reads at most 9 digits and ignores a
   negative one, so 0..999,999,999 (30 bits). */
#define CMD_SEQ_MAX             999999999u

/* ---------------------------------------------------------------------
 *  IF-10  Command -> Navigation: nav_cmd_t in Navigation's mbf_nav_cmd.
 *  Packed into two words (minimum-bit change, D-91):
 *    word 0  bit 0       type - 1 (NAV_CMD_xxx, 1..2)
 *            bits 1-5    letter: 0 none, 1..26 for 'A'..'Z' (REMAP)
 *            bits 6-8    action, letter_action_t 0..4 (REMAP)
 *            bits 9-31   0
 *    word 1  bits 0-29   the command's "seq", 0..CMD_SEQ_MAX, for the ack
 *            bits 30-31  0
 *  Build one with nav_cmd_pack(), read it with nav_cmd_type() and the
 *  others.
 * --------------------------------------------------------------------- */
#define NAV_CMD_REMAP           1u  /* set one letter's action (FR6) */
#define NAV_CMD_MAP_DEFAULT     2u  /* back to the default map */
#define NAV_CMD_WORDS           2u

typedef struct
{
    uint32_t word[NAV_CMD_WORDS];
} nav_cmd_t;

bool     nav_cmd_pack(nav_cmd_t * p_cmd, uint8_t type, char letter,
                      uint8_t action, uint32_t cmd_seq);
uint8_t  nav_cmd_type(nav_cmd_t const * p_cmd);
char     nav_cmd_letter(nav_cmd_t const * p_cmd);
uint8_t  nav_cmd_action(nav_cmd_t const * p_cmd);
uint32_t nav_cmd_seq(nav_cmd_t const * p_cmd);

/* ---------------------------------------------------------------------
 *  IF-12  every task -> Telemetry: tlm_msg_t in mbf_tlm (owned here).
 *  Status records every 200 ms; route-log entries, acks, fault records
 *  and summary requests when they happen.  Use tlm_post(): it never
 *  blocks, and a full queue only drops the record (telemetry is not
 *  needed to drive, NFR10) and counts it.
 *
 *  Packed (minimum-bit change, D-92): each record is whole uint32_t
 *  words, built with its pack function and read with its read functions
 *  (tlm_msgs.c).  Bit 0 is the least significant bit; unused bits are 0.
 *  A record carries only what Telemetry reads.  The time stamp travels
 *  only with the records whose time Telemetry prints (tlm_msg_t u.ev).
 * --------------------------------------------------------------------- */
#define TLM_MSG_LINE            1u
#define TLM_MSG_MOTION          2u
#define TLM_MSG_IMU             3u
#define TLM_MSG_SCAN            4u
#define TLM_MSG_NAV             5u
#define TLM_MSG_SAFETY          6u
#define TLM_MSG_ROUTE           7u  /* route-log entry (FR13) */
#define TLM_MSG_ACK             8u  /* answer on the ack topic */
#define TLM_MSG_SUMMARY         9u  /* publish the run summary */
#define TLM_MSG_FAULT           10u /* fault latched or released */
#define TLM_MSG_LINK            11u /* MQTT link state (Command) */
#define TLM_MSG_OBSTACLE        12u /* obstacle profile (Scan, FR10) */

/* Route-log entry kinds (FR13: distances, turns, barcodes, humps,
   obstacles), plus the run's start and end. */
#define ROUTE_RUN_START         1u
#define ROUTE_BARCODE           2u  /* letter, action */
#define ROUTE_TURN              3u  /* action, commanded and measured */
#define ROUTE_HUMP              4u  /* height, peak roll */
#define ROUTE_OBSTACLE          5u  /* side, width, closest point */
#define ROUTE_LINE_SEARCH       6u  /* value 1 = started, 0 = found */
#define ROUTE_SAFE_HOLD         7u  /* reason */
#define ROUTE_COMPLETE          8u  /* line not found after 5 s */
#define ROUTE_NO_COMMAND        9u  /* junction with no command (FR5) */
#define ROUTE_FAULT             10u /* fault state entered */
#define ROUTE_BC_FAIL           11u /* barcode seen but not decoded (dashboard) */

/* Command codes used in acks. */
#define CMD_CODE_UNKNOWN        0u
#define CMD_CODE_REMAP          1u
#define CMD_CODE_SUMMARY        2u
#define CMD_CODE_STOP           3u
#define CMD_CODE_BROKER         4u
#define CMD_CODE_MAP_DEFAULT    5u
#define CMD_CODE_START          6u  /* dashboard: press START remotely */
#define CMD_CODE_RESET          7u  /* dashboard: press RESET remotely */

/* A letter field of 5 bits: 0 none, 1..26 for 'A'..'Z'. */
#define TLM_NO_LETTER           '-'

/*
 * Line status (Line task, every 200 ms), two words:
 *   word 0  bits  0-11  raw_l     left reading, 0..4095 (12-bit ADC)
 *           bits 12-23  raw_r     right reading, 0..4095
 *           bits 24-31  letters   letters read since boot, modulo 256
 *   word 1  bits  0-10  error_pm  line error, -1000..1000
 *           bits 11-15  last      the last letter read (TLM_NO_LETTER
 *                                 before the first)
 */
typedef struct
{
    uint32_t word[2];
} tlm_line_t;

/*
 * Motion status (Motion task, every 200 ms), four words:
 *   word 0  bits  0-15  speed_l   measured wheel speed, mm/s,
 *                                 -32768..32767 (Motion clamps it to the
 *                                 old 16-bit field's range, D-92)
 *           bits 16-31  speed_r
 *   word 1              count_l   encoder count since boot, int32
 *   word 2              count_r
 *   word 3  bits  0-30  distance  mm since boot (MOTION_DISTANCE_BITS)
 */
typedef struct
{
    uint32_t word[4];
} tlm_motion_t;

/*
 * IMU status (IMU task, every 200 ms; its time is read for motion
 * events), two words:
 *   word 0  bits  0-15  rate        turn rate, deg/s, -32768..32767
 *           bits 16-18  class       imu_class_t, 0..6
 *           bits 19-26  humps       humps measured since boot, mod 256
 *   word 1  bits  0-6   max_height  highest hump, mm, 0..65 (the hump
 *                                   height table's largest entry)
 */
typedef struct
{
    uint32_t word[2];
} tlm_imu_t;

/*
 * Scan status (Scan task, every 200 ms), one word:
 *   bits  0-11  range      last valid range, mm, 0..4000
 *   bits 12-27  obstacles  obstacles passed since boot, modulo 65536
 */
typedef struct
{
    uint32_t word[1];
} tlm_scan_t;

/*
 * Navigation status (every 200 ms), one word:
 *   bits 0-3   mode  nav_mode_t, 0..10
 *   bits 4-8   held  the held letter (TLM_NO_LETTER when none)
 *   bits 9-10  hold  SAFE HOLD reason, nav_hold_t 0..3
 */
typedef struct
{
    uint32_t word[1];
} tlm_nav_t;

/*
 * Safety status (Safety task, every 200 ms), two words:
 *   word 0  bit   0     faulted     a fault is latched
 *           bits  1-4   cause       its FAULT_CAUSE_xxx, 0..11
 *           bit   5     wdt         watchdog armed
 *           bits  6-13  queue_full  queue-full faults since boot,
 *                                   0..255 (Safety stops at 255)
 *           bits 14-29  faults      faults since boot, 0..65535 (status
 *                                   causes are not faults)
 *   word 1  bits  0-15  missed      check-in misses since boot, 0..65535
 *           bits 16-26  mask        Safety's fault mask: bit (cause - 1)
 *                                   for each latching cause since the
 *                                   last release and each status cause
 *                                   now on (imu_msgs.h)
 */
typedef struct
{
    uint32_t word[2];
} tlm_safety_t;

/*
 * Route-log entry (Navigation, FR13), three words; its time is the
 * message's (u.ev.time_ms):
 *   word 0  bits  0-30  distance  mm since boot (MOTION_DISTANCE_BITS)
 *   word 1  bits  0-3   kind      ROUTE_xxx
 *           from bit 4, by kind:
 *             BARCODE      letter 5 (1..26), action 3 (0..4),
 *                          reversed 1
 *             TURN         action 3, commanded angle 16 (cdeg, signed)
 *             HUMP         height 7 (mm), peak |roll| 15 (cdeg)
 *             OBSTACLE     side 2, width 11 (mm, signed), closest 9 (mm)
 *             LINE_SEARCH  1 = started, 0 = found (1 bit)
 *             SAFE_HOLD    reason, nav_hold_t (2 bits)
 *   word 2  TURN      bits 0-15: measured angle (cdeg, signed)
 *           COMPLETE  the run time, ms (all 32 bits, read as signed)
 * Read with tlm_route_value() and tlm_route_value2(), which give each
 * kind's values as Navigation passed them.
 */
typedef struct
{
    uint32_t word[3];
} tlm_route_t;

/*
 * Ack (Command task, Navigation), two words:
 *   word 0  bits 0-29  seq   the command's "seq", 0..CMD_SEQ_MAX
 *   word 1  bits 0-2   code  CMD_CODE_xxx, 0..7
 *           bit  3     ok
 */
typedef struct
{
    uint32_t word[2];
} tlm_ack_t;

/*
 * Fault latched or released (Safety task), one word:
 *   bits  0-3   cause    FAULT_CAUSE_xxx, 0..11
 *   bits  4-6   source   FAULT_SRC_xxx - 1 (sources 1..8)
 *   bits  7-19  detail   0..FAULT_DETAIL_MAX (8191)
 *   bit   20    latched  1 = latched, 0 = released
 */
typedef struct
{
    uint32_t word[1];
} tlm_fault_t;

/*
 * MQTT link state (Command task), one word:
 *   bit  0    online  MQTT connected
 *   bits 1-3  wifi    CYW43 link status, -3..3 (CYW43_LINK_xxx)
 */
typedef struct
{
    uint32_t word[1];
} tlm_link_t;

/*
 * FR10: the obstacle profile from PLANNING (report 6.5), published as an
 * event (Scan task).  Positions in mm in the car frame from the sensor:
 * x forward, y left.  Every point of a profile is a hit within
 * CFG_SCAN_OBSTACLE_MM (500 mm) at a servo angle of 0..180 degrees.
 * Three words:
 *   word 0  bits  0-1   side      OBST_SIDE_xxx chosen, NONE = no room
 *           bit   2     reversed  taken after the 10 cm reverse
 *           bits  3-11  x         forward distance, 0..500
 *           bits 12-21  y         lateral centre, -500..500
 *           bits 22-30  closest   range to the closest point, 0..500
 *   word 1  bits  0-10  width     -1000..1000
 *           bits 11-22  clear_l   free space beyond the left edge, + 1:
 *                                 0 = never seen (-1), 4001 = nothing
 *                                 seen (4000)
 *   word 2  bits  0-11  clear_r   the same on the right
 */
typedef struct
{
    uint32_t word[3];
} tlm_obstacle_t;

/*
 * The message, five words: the type, then the record.  A record whose
 * time Telemetry prints has it first (u.ev).
 *   head  bits 0-3  type - 1 (TLM_MSG_xxx, 1..12); bits 4-31 0
 */
typedef struct
{
    uint32_t head;
    union
    {
        tlm_line_t      line;               /* TLM_MSG_LINE */
        tlm_motion_t    motion;             /* TLM_MSG_MOTION */
        tlm_scan_t      scan;               /* TLM_MSG_SCAN */
        tlm_nav_t       nav;                /* TLM_MSG_NAV */
        tlm_safety_t    safety;             /* TLM_MSG_SAFETY */
        struct
        {
            uint32_t    time_ms;            /* app_time_ms(), all 32 */
            union
            {
                tlm_imu_t      imu;         /* TLM_MSG_IMU */
                tlm_route_t    route;       /* TLM_MSG_ROUTE */
                tlm_ack_t      ack;         /* TLM_MSG_ACK */
                tlm_fault_t    fault;       /* TLM_MSG_FAULT */
                tlm_link_t     link;        /* TLM_MSG_LINK */
                tlm_obstacle_t obstacle;    /* TLM_MSG_OBSTACLE */
            } u;                            /* TLM_MSG_SUMMARY: time only */
        } ev;
    } u;
} tlm_msg_t;

void     tlm_msg_init(tlm_msg_t * p_msg, uint8_t type);
uint8_t  tlm_msg_type(tlm_msg_t const * p_msg);

bool     tlm_line_pack(tlm_line_t * p_rec, uint32_t raw_l, uint32_t raw_r,
                       int32_t error_pm, uint32_t letters, char last_letter);
uint32_t tlm_line_raw_l(tlm_line_t const * p_rec);
uint32_t tlm_line_raw_r(tlm_line_t const * p_rec);
int32_t  tlm_line_error_pm(tlm_line_t const * p_rec);
uint32_t tlm_line_letters(tlm_line_t const * p_rec);
char     tlm_line_last_letter(tlm_line_t const * p_rec);

bool     tlm_motion_pack(tlm_motion_t * p_rec, int32_t speed_l_mmps,
                         int32_t speed_r_mmps, int32_t count_l,
                         int32_t count_r, int32_t distance_mm);
int32_t  tlm_motion_speed_l(tlm_motion_t const * p_rec);
int32_t  tlm_motion_speed_r(tlm_motion_t const * p_rec);
int32_t  tlm_motion_count_l(tlm_motion_t const * p_rec);
int32_t  tlm_motion_count_r(tlm_motion_t const * p_rec);
int32_t  tlm_motion_distance_mm(tlm_motion_t const * p_rec);

bool     tlm_imu_pack(tlm_imu_t * p_rec, uint8_t motion_class,
                      uint32_t humps, int32_t max_height_mm,
                      int32_t turn_rate_dps);
uint8_t  tlm_imu_class(tlm_imu_t const * p_rec);
uint32_t tlm_imu_humps(tlm_imu_t const * p_rec);
int32_t  tlm_imu_max_height_mm(tlm_imu_t const * p_rec);
int32_t  tlm_imu_rate_dps(tlm_imu_t const * p_rec);

bool     tlm_scan_pack(tlm_scan_t * p_rec, int32_t range_mm,
                       uint32_t obstacles);
int32_t  tlm_scan_range_mm(tlm_scan_t const * p_rec);
uint32_t tlm_scan_obstacles(tlm_scan_t const * p_rec);

bool     tlm_nav_pack(tlm_nav_t * p_rec, uint8_t mode, char held_letter,
                      uint8_t hold_reason);
uint8_t  tlm_nav_mode(tlm_nav_t const * p_rec);
char     tlm_nav_held_letter(tlm_nav_t const * p_rec);
uint8_t  tlm_nav_hold_reason(tlm_nav_t const * p_rec);

bool     tlm_safety_pack(tlm_safety_t * p_rec, bool b_faulted, uint8_t cause,
                         bool b_wdt_armed, uint32_t queue_full,
                         uint32_t faults, uint32_t missed, uint32_t mask);
bool     tlm_safety_faulted(tlm_safety_t const * p_rec);
uint8_t  tlm_safety_cause(tlm_safety_t const * p_rec);
bool     tlm_safety_wdt_armed(tlm_safety_t const * p_rec);
uint32_t tlm_safety_queue_full(tlm_safety_t const * p_rec);
uint32_t tlm_safety_faults(tlm_safety_t const * p_rec);
uint32_t tlm_safety_missed(tlm_safety_t const * p_rec);
uint32_t tlm_safety_mask(tlm_safety_t const * p_rec);

bool     tlm_route_pack(tlm_route_t * p_rec, uint8_t kind, char letter,
                        uint8_t action, int32_t distance_mm, int32_t value,
                        int32_t value2);
uint8_t  tlm_route_kind(tlm_route_t const * p_rec);
char     tlm_route_letter(tlm_route_t const * p_rec);
uint8_t  tlm_route_action(tlm_route_t const * p_rec);
int32_t  tlm_route_distance_mm(tlm_route_t const * p_rec);
int32_t  tlm_route_value(tlm_route_t const * p_rec);
int32_t  tlm_route_value2(tlm_route_t const * p_rec);

bool     tlm_ack_pack(tlm_ack_t * p_rec, uint32_t cmd_seq, uint8_t cmd_code,
                      bool b_cmd_ok);
uint32_t tlm_ack_seq(tlm_ack_t const * p_rec);
uint8_t  tlm_ack_code(tlm_ack_t const * p_rec);
bool     tlm_ack_ok(tlm_ack_t const * p_rec);

bool     tlm_fault_pack(tlm_fault_t * p_rec, uint8_t cause, uint8_t source,
                        uint16_t detail, bool b_latched);
uint8_t  tlm_fault_cause(tlm_fault_t const * p_rec);
uint8_t  tlm_fault_source(tlm_fault_t const * p_rec);
uint16_t tlm_fault_detail(tlm_fault_t const * p_rec);
bool     tlm_fault_latched(tlm_fault_t const * p_rec);

bool     tlm_link_pack(tlm_link_t * p_rec, bool b_online,
                       int32_t wifi_status);
bool     tlm_link_online(tlm_link_t const * p_rec);
int32_t  tlm_link_wifi(tlm_link_t const * p_rec);

bool     tlm_obstacle_pack(tlm_obstacle_t * p_rec, uint8_t side,
                           bool b_reversed, int32_t distance_mm,
                           int32_t offset_mm, int32_t width_mm,
                           int32_t closest_mm, int32_t clear_left_mm,
                           int32_t clear_right_mm);
uint8_t  tlm_obstacle_side(tlm_obstacle_t const * p_rec);
bool     tlm_obstacle_reversed(tlm_obstacle_t const * p_rec);
int32_t  tlm_obstacle_distance_mm(tlm_obstacle_t const * p_rec);
int32_t  tlm_obstacle_offset_mm(tlm_obstacle_t const * p_rec);
int32_t  tlm_obstacle_width_mm(tlm_obstacle_t const * p_rec);
int32_t  tlm_obstacle_closest_mm(tlm_obstacle_t const * p_rec);
int32_t  tlm_obstacle_clear_left_mm(tlm_obstacle_t const * p_rec);
int32_t  tlm_obstacle_clear_right_mm(tlm_obstacle_t const * p_rec);

#endif /* TLM_MSGS_H */

/*** end of file ***/
