/*
 *  ref_tlm_format.h
 *  TEST ONLY.  A frozen copy of the telemetry records and formatter as
 *  they were before the minimum-bit change (app_program/subsystem1/
 *  tlm_msgs.h and tlm_format.h at c644b68), renamed ref_*, for
 *  side-by-side tests (S1-U43).  Never used in the firmware.  Do not
 *  change it: it is the reference.
 */
#ifndef REF_TLM_FORMAT_H
#define REF_TLM_FORMAT_H

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"
#include "subsystem1/tlm_msgs.h"

typedef struct
{
    int16_t  raw_l;
    int16_t  raw_r;
    int16_t  error_pm;
    uint8_t  b_lost;
    uint8_t  letters;           /* letters decoded since boot */
    uint8_t  failed;            /* captures failed since boot */
    char     last_letter;
} ref_tlm_line_t;

typedef struct
{
    int16_t  speed_l_mmps;
    int16_t  speed_r_mmps;
    int32_t  count_l;
    int32_t  count_r;
    int32_t  distance_mm;
    uint8_t  state;             /* motion_state_t */
} ref_tlm_motion_t;

typedef struct
{
    uint8_t  motion_class;      /* imu_class_t */
    uint8_t  humps;             /* humps measured since boot */
    int16_t  max_height_mm;
    int16_t  turn_rate_dps;     /* degrees per second */
    int16_t  heading_deg;
    int16_t  roll_ddeg;         /* tenths of a degree */
} ref_tlm_imu_t;

typedef struct
{
    uint8_t  state;             /* scan state number */
    uint8_t  invalid_run;       /* invalid readings in a row */
    int16_t  range_mm;          /* last valid range */
    int16_t  angle_deg;         /* servo angle */
    uint16_t obstacles;         /* obstacles profiled since boot */
} ref_tlm_scan_t;

typedef struct
{
    uint8_t  mode;              /* nav_mode_t */
    char     held_letter;       /* '-' when none */
    uint8_t  held_action;       /* letter_action_t */
    uint8_t  hold_reason;       /* SAFE HOLD reason (nav_hold_t) */
    int32_t  heading_cdeg;      /* fused heading */
    uint32_t run_ms;            /* time since START */
} ref_tlm_nav_t;

typedef struct
{
    uint8_t  b_faulted;
    uint8_t  cause;             /* FAULT_CAUSE_xxx of the latched fault */
    uint8_t  b_wdt_armed;
    uint8_t  queue_full;        /* queue-full faults since boot */
    uint16_t faults;            /* faults latched since boot */
    uint16_t missed_checkins;   /* check-in deadline misses */
} ref_tlm_safety_t;

typedef struct
{
    uint8_t  kind;              /* ROUTE_xxx */
    char     letter;
    uint8_t  action;            /* letter_action_t, or side for obstacles */
    uint8_t  reason;
    int32_t  distance_mm;       /* where on the route it happened */
    int32_t  value;             /* kind-specific, see subsystem1 tlm code */
    int32_t  value2;
} ref_tlm_route_t;

typedef struct
{
    uint32_t cmd_seq;
    uint8_t  cmd_code;          /* CMD_CODE_xxx */
    uint8_t  b_ok;
    uint8_t  detail;
    uint8_t  reserved;
} ref_tlm_ack_t;

typedef struct
{
    uint8_t  cause;             /* FAULT_CAUSE_xxx */
    uint8_t  source;            /* FAULT_SRC_xxx */
    uint16_t detail;
    uint8_t  b_latched;         /* 1 = latched, 0 = released */
} ref_tlm_fault_t;

typedef struct
{
    uint8_t  b_online;          /* MQTT connected */
    int32_t  wifi_status;       /* CYW43 link status */
} ref_tlm_link_t;

/* FR10: the obstacle profile from PLANNING (report 6.5), published as an
   event.  Positions in mm in the car frame from the sensor: x forward,
   y left.  A clearance of 4000 means nothing was seen on that side. */
typedef struct
{
    uint8_t  side;              /* OBST_SIDE_xxx chosen, NONE = no room */
    uint8_t  b_reversed;        /* profile taken after the 10 cm reverse */
    int16_t  distance_mm;       /* position: forward distance (x) */
    int16_t  offset_mm;         /* position: lateral centre (y) */
    int16_t  width_mm;
    int16_t  closest_mm;        /* range to the closest point */
    int16_t  clear_left_mm;     /* free space beyond the left edge */
    int16_t  clear_right_mm;    /* free space beyond the right edge */
} ref_tlm_obstacle_t;

typedef struct
{
    ref_tlm_route_t entry;
    uint32_t    time_ms;
} ref_tlm_log_t;

typedef struct
{
    ref_tlm_line_t   line;
    ref_tlm_motion_t motion;
    ref_tlm_imu_t    imu;
    ref_tlm_scan_t   scan;
    ref_tlm_nav_t    nav;
    ref_tlm_safety_t safety;
    ref_tlm_link_t   link;
    char const * p_mode_name;       /* name of nav.mode */
    bool         b_run_started;     /* a ROUTE_RUN_START was seen */
    uint32_t     run_start_ms;      /* ROUTE_RUN_START time */
    int32_t      run_start_mm;      /* distance at the start */
    uint32_t     run_end_ms;        /* time of the end entry */
    int32_t      run_end_mm;        /* distance at the end */
    uint8_t      end_kind;          /* ROUTE_xxx that ended the run */
    uint8_t      end_reason;
    uint16_t     barcodes;
    uint16_t     turns;
    uint16_t     humps;
    int32_t      max_hump_mm;
    uint16_t     obstacles;
    uint16_t     faults;
    uint32_t     dropped;           /* messages that could not be sent */
    ref_tlm_log_t    log[CFG_TLM_ROUTE_MAX];
    uint32_t     log_count;
} ref_tlm_state_t;

void     ref_tlm_state_init(ref_tlm_state_t * p_state);
int32_t  ref_tlm_state_route(ref_tlm_state_t * p_state,
                             ref_tlm_route_t const * p_route,
                             uint32_t time_ms);

uint32_t ref_tlm_format_telemetry(ref_tlm_state_t const * p_state, uint32_t seq,
                                  uint32_t time_ms, char * p_buf,
                                  uint32_t size);
uint32_t ref_tlm_format_heartbeat(ref_tlm_state_t const * p_state, uint32_t seq,
                                  uint32_t time_ms, uint32_t beat,
                                  char * p_buf, uint32_t size);
uint32_t ref_tlm_format_route(ref_tlm_route_t const * p_route, uint32_t seq,
                              uint32_t time_ms, int32_t log_index,
                              bool b_replay, char * p_buf, uint32_t size);
uint32_t ref_tlm_format_ack(ref_tlm_ack_t const * p_ack, uint32_t seq,
                            uint32_t time_ms, char * p_buf, uint32_t size);
uint32_t ref_tlm_format_fault(ref_tlm_fault_t const * p_fault, uint32_t seq,
                              uint32_t time_ms, char * p_buf, uint32_t size);
uint32_t ref_tlm_format_motion_event(uint8_t motion_class, int32_t rate_dps,
                                     uint32_t seq, uint32_t time_ms,
                                     char * p_buf, uint32_t size);
uint32_t ref_tlm_format_link(ref_tlm_link_t const * p_link, uint32_t seq,
                             uint32_t time_ms, char * p_buf, uint32_t size);
uint32_t ref_tlm_format_obstacle(ref_tlm_obstacle_t const * p_obstacle,
                                 uint32_t seq, uint32_t time_ms, char * p_buf,
                                 uint32_t size);
uint32_t ref_tlm_format_summary(ref_tlm_state_t const * p_state, uint32_t seq,
                                uint32_t time_ms, char * p_buf, uint32_t size);

char const * ref_tlm_class_name(uint8_t motion_class);
char const * ref_tlm_cause_name(uint8_t cause);
char const * ref_tlm_source_name(uint8_t source);
char const * ref_tlm_cmd_name(uint8_t cmd_code);

#endif /* REF_TLM_FORMAT_H */

/*** end of file ***/
