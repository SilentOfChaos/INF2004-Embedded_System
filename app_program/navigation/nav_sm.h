/*
 *  nav_sm.h
 *  Navigation's system statechart (report section 5, Fig 5) as pure
 *  logic: inputs are the events and the 5 ms tick, outputs are a list of
 *  actions (Motion commands, scan requests, route-log entries ...) that
 *  the Navigation task carries out.  Keeping the kernel calls out of
 *  here lets the host unit tests drive the whole statechart.
 *
 *  Modes: BOOT -> SELF-TEST -> READY -> RUNNING { LINE FOLLOWING,
 *  HUMP CROSSING, LINE SEARCH, AT JUNCTION, AVOIDING OBSTACLE } ->
 *  SAFE HOLD / COMPLETE / FAULT.
 */

#ifndef NAV_SM_H
#define NAV_SM_H

#include <stdbool.h>
#include <stdint.h>

#include "../subsystem1/tlm_msgs.h"
#include "../subsystem2/motion_msgs.h"
#include "../subsystem3/letter_map.h"
#include "../subsystem3/line_msgs.h"
#include "../subsystem3/steering.h"
#include "../subsystem4/imu_msgs.h"
#include "../subsystem5/scan_msgs.h"
#include "nav_types.h"

#define NAV_ACT_MAX             16u

/* Actions for the Navigation task. */
typedef enum
{
    NAV_ACT_DRIVE = 1,          /* a = speed, b = diff */
    NAV_ACT_FORWARD,            /* a = mm, id */
    NAV_ACT_BACKWARD,           /* a = mm, id */
    NAV_ACT_TURN_LEFT,          /* a = cdeg, id */
    NAV_ACT_TURN_RIGHT,         /* a = cdeg, id */
    NAV_ACT_STOP,
    NAV_ACT_HOLD_SPEED,         /* a = 1 hold, 0 release */
    NAV_ACT_KEEPALIVE,
    NAV_ACT_SCAN,               /* kind = SCAN_REQ_xxx, side */
    NAV_ACT_ROUTE,              /* route-log entry, see tlm_route_t */
    NAV_ACT_ACK,                /* a = cmd_seq, kind = CMD_CODE, b = ok */
    NAV_ACT_SUMMARY,            /* publish the run summary */
    NAV_ACT_SELF_TEST_FAIL,     /* report FAULT_CAUSE_SELF_TEST */
    NAV_ACT_LINE_ADAPT,         /* a = 1 adapt, 0 freeze */
    NAV_ACT_LINE_RUN_OVER,      /* a = 1 run over, 0 active */
    NAV_ACT_LINE_STALE          /* report FAULT_CAUSE_LINE_DATA: the line
                                   record stopped changing, a = ms since
                                   its count last moved */
} nav_act_type_t;

typedef struct
{
    uint8_t type;               /* nav_act_type_t */
    uint8_t move_id;
    uint8_t kind;               /* ROUTE_xxx or SCAN_REQ_xxx or CMD_CODE */
    uint8_t side;               /* OBST_SIDE_xxx */
    char    letter;
    uint8_t action;             /* letter_action_t */
    uint8_t reserved;
    int32_t a_value;
    int32_t b_value;
    int32_t c_value;
} nav_act_t;

/* Steps inside AT JUNCTION, LINE SEARCH and AVOIDING OBSTACLE. */
typedef enum
{
    NAV_STEP_NONE = 0,
    NAV_STEP_TO_AXLE,           /* junction: move sensors -> axle */
    NAV_STEP_TURN,              /* junction: commanded turn */
    NAV_STEP_FIND_LINE,         /* junction: look for the new line */
    NAV_STEP_SWEEP,             /* search: sweeping on the spot */
    NAV_STEP_STOPPING,          /* obstacle: wait for the car to stop */
    NAV_STEP_SCANNING,          /* obstacle: Obstacle Scan is working */
    NAV_STEP_REVERSING,         /* obstacle: back 10 cm, once */
    NAV_STEP_TURN_OUT,          /* obstacle: turn 90 towards the side */
    NAV_STEP_LEG_OUT,           /* obstacle: sideways leg */
    NAV_STEP_TURN_PARALLEL,     /* obstacle: turn back parallel */
    NAV_STEP_PASSING,           /* obstacle: drive until it is behind */
    NAV_STEP_PAST_EXTRA,        /* obstacle: clear the rear of the car */
    NAV_STEP_TURN_IN,           /* obstacle: turn towards the line */
    NAV_STEP_REJOINING,         /* obstacle: up to 30 cm to the line */
    NAV_STEP_ALIGN_MOVE,        /* obstacle: sensors -> axle */
    NAV_STEP_ALIGN_TURN,        /* obstacle: turn back along the line */
    NAV_STEP_CLEAR_TURN,        /* obstacle: rejoin blocked, turn parallel */
    NAV_STEP_CLEAR_MOVE         /* obstacle: further past, then turn in */
} nav_step_t;

typedef struct
{
    nav_mode_t      mode;
    nav_hold_t      hold_reason;
    nav_step_t      step;
    uint32_t        now_ms;
    uint32_t        mode_since_ms;
    uint32_t        step_since_ms;
    uint32_t        run_start_ms;
    uint32_t        deadline_ms;        /* search or find-line limit */
    bool            b_selftest_failed;  /* failure already reported */

    letter_map_t    map;                /* FR6, remapped over MQTT */
    bool            b_command_held;
    char            held_letter;
    letter_action_t held_action;
    bool            b_speed_held;       /* holdSpeed() requested */
    bool            b_read_ignored;     /* the current barcode read was not
                                           all in LINE FOLLOWING / HUMP
                                           CROSSING: ignore it (D-53) */

    steering_t      steer;
    int32_t         last_error_pm;      /* last error with the line seen */
    bool            b_line_seq_known;   /* line_seq holds a sample count */
    bool            b_line_stale;       /* reported: the record is not used
                                           until its count moves again */
    uint8_t         line_seq;           /* the line record's last sample
                                           count (8 bits, IF-02) */
    uint32_t        line_seq_ms;        /* when that count was first seen */

    uint8_t         next_move_id;
    uint8_t         pending_move_id;    /* 0 = no move running */
    bool            b_move_done;        /* pending move finished */
    uint32_t        sweep_index;        /* position in the sweep pattern */
    bool            b_sweep_left_first;

    int32_t         distance_mm;        /* odometry */
    int32_t         avoid_closest_mm;   /* obstacle's closest range */
    int32_t         avoid_width_mm;     /* obstacle's measured width */
    int32_t         avoid_pass_from_mm; /* distance_mm when passing began */
    uint8_t         rejoin_tries;       /* blocked rejoins so far */
    int32_t         odo_heading_cdeg;
    bool            b_odo_valid;
    int32_t         fused_heading_cdeg; /* odometry + compass */
    bool            b_fused_valid;
    int32_t         turn_start_cdeg;
    int32_t         turn_command_cdeg;

    uint8_t         avoid_side;         /* OBST_SIDE_xxx */
    int32_t         avoid_lateral_mm;
    bool            b_reversed_once;

    nav_act_t       acts[NAV_ACT_MAX];
    uint32_t        act_count;
    uint32_t        act_dropped;        /* actions that did not fit */
} nav_sm_t;

void nav_sm_init(nav_sm_t * p_sm, uint32_t now_ms);
void nav_sm_tick(nav_sm_t * p_sm, uint32_t now_ms, line_err_t const * p_line,
                 uint32_t health, bool b_faulted, bool b_start_pressed);
void nav_sm_line_event(nav_sm_t * p_sm, line_evt_t const * p_evt);
void nav_sm_imu_event(nav_sm_t * p_sm, imu_evt_t const * p_evt);
void nav_sm_obstacle_event(nav_sm_t * p_sm, obstacle_evt_t const * p_evt);
void nav_sm_motion_status(nav_sm_t * p_sm, motion_sts_t const * p_sts);
void nav_sm_command(nav_sm_t * p_sm, nav_cmd_t const * p_cmd);
void nav_sm_restart(nav_sm_t * p_sm);
void nav_sm_reset(nav_sm_t * p_sm);
void nav_sm_clear_actions(nav_sm_t * p_sm);

#endif /* NAV_SM_H */

/*** end of file ***/
