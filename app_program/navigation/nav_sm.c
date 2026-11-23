/*
 *  nav_sm.c
 *  Navigation's system statechart as pure logic.  See nav_sm.h.
 *
 *  Report references: section 5 (Fig 5) for the modes, SD1 (Fig 3) for
 *  the barcode-to-turn sequence, 6.5 (Fig 10) for the obstacle steps,
 *  FR3-FR6, FR11, FR16 and FR18.  Choices the report leaves open are in
 *  docs/decisions.md (D-numbers in the comments).
 */

#include <stddef.h>

#include "../app_config.h"
#include "../common/bitpack.h"
#include "../common/fixmath.h"
#include "nav_sm.h"

#define NAV_TURN_90_CDEG        9000
#define NAV_TURN_180_CDEG       18000
#define NAV_CDEG_PER_DEG        100
#define NAV_SERVO_AHEAD_DEG     90      /* scan angles: 90 = straight */
#define NAV_SERVO_SQUARE_DEG    90      /* straight ahead to square */
#define NAV_SWEEP_PHASES        3u
#define NAV_HALF                2

/*
 * Every distance (mm) and angle (cdeg) Navigation asks Motion for fits
 * IF-07's value field (motion_msgs.h, D-88); all are sizes, the
 * direction is the command.  One check per nav_sm_move() value: a
 * setting that would no longer fit stops the build.
 */
#define NAV_VALUE_FITS(v)       (((v) >= 0) && ((v) <= MOTION_CMD_VALUE_MAX))
BITPACK_ASSERT(NAV_VALUE_FITS(CFG_NAV_REVERSE_MM) &&
               NAV_VALUE_FITS(CFG_NAV_PAST_EXTRA_MM) &&
               NAV_VALUE_FITS(CFG_NAV_SENSOR_TO_AXLE_MM) &&
               NAV_VALUE_FITS(CFG_NAV_BYPASS_MAX_MM) &&
               NAV_VALUE_FITS(CFG_NAV_REJOIN_MAX_MM), nav_moves_fit);
/* The sideways leg (nav_sm_plan()): 0..BYPASS_MAX, plus the gap and
   half the car. */
BITPACK_ASSERT(NAV_VALUE_FITS(CFG_NAV_PASS_GAP_MM +
                              (CFG_CAR_WIDTH_MM / NAV_HALF)) &&
               NAV_VALUE_FITS(CFG_NAV_BYPASS_MAX_MM + CFG_NAV_PASS_GAP_MM +
                              (CFG_CAR_WIDTH_MM / NAV_HALF)),
               nav_lateral_fits);
/* While passing, the side scan looks at most 45 degrees ahead of square
   (D-97), so its look-ahead is at most the passing distance (gap + half
   the car), and the extra travel plus that fits a move. */
BITPACK_ASSERT((CFG_SCAN_SIDE_RIGHT_DEG >= 0) &&
               (CFG_SCAN_SIDE_RIGHT_DEG <= 45) &&
               (CFG_SCAN_SIDE_LEFT_DEG >= 135) &&
               (CFG_SCAN_SIDE_LEFT_DEG <= 180), nav_side_scan_square);
BITPACK_ASSERT(NAV_VALUE_FITS(CFG_NAV_PAST_EXTRA_MM + CFG_NAV_PASS_GAP_MM +
                              (CFG_CAR_WIDTH_MM / NAV_HALF)),
               nav_past_extra_fits);
/* Turns: the sweep, twice the sweep on the way back (nav_sm_sweep()),
   a quarter turn and a U-turn. */
BITPACK_ASSERT(NAV_VALUE_FITS(CFG_NAV_SWEEP_CDEG) &&
               NAV_VALUE_FITS(NAV_HALF * CFG_NAV_SWEEP_CDEG) &&
               NAV_VALUE_FITS(NAV_TURN_90_CDEG) &&
               NAV_VALUE_FITS(NAV_TURN_180_CDEG), nav_turns_fit);

/* With both tasks healthy the line record's count is seen to move at
   least once per Line period plus a Navigation period; the stale limit
   is at least twice that, so a late sample never trips it (D-94). */
BITPACK_ASSERT(CFG_NAV_LINE_STALE_MS >=
               (2 * (CFG_PERIOD_LINE_MS + CFG_PERIOD_NAV_MS)),
               nav_stale_ms_margin);

static void nav_sm_tick_body(nav_sm_t * p_sm, uint32_t now_ms,
                             line_err_t const * p_line, uint32_t health,
                             bool b_faulted, bool b_start_pressed);
static void nav_sm_imu_body(nav_sm_t * p_sm, imu_evt_t const * p_evt);
static void nav_sm_obstacle_body(nav_sm_t * p_sm,
                                 obstacle_evt_t const * p_evt);
static void nav_sm_motion_body(nav_sm_t * p_sm, motion_sts_t const * p_sts);
static void nav_sm_set_mode(nav_sm_t * p_sm, nav_mode_t mode);
static void nav_sm_set_step(nav_sm_t * p_sm, nav_step_t step);
static void nav_sm_push(nav_sm_t * p_sm, nav_act_t const * p_act);
static void nav_sm_simple(nav_sm_t * p_sm, nav_act_type_t type,
                          int32_t a_value);
static void nav_sm_move(nav_sm_t * p_sm, nav_act_type_t type,
                        int32_t a_value);
static void nav_sm_stop(nav_sm_t * p_sm);
static void nav_sm_scan(nav_sm_t * p_sm, uint8_t kind, uint8_t side);
static void nav_sm_route(nav_sm_t * p_sm, uint8_t kind, char letter,
                         uint8_t action, int32_t value, int32_t value2);
static void nav_sm_release_hold(nav_sm_t * p_sm);
static void nav_sm_turn_toward(nav_sm_t * p_sm, uint8_t side, int32_t cdeg);
static void nav_sm_start_run(nav_sm_t * p_sm);
static void nav_sm_follow(nav_sm_t * p_sm);
static void nav_sm_enter_following(nav_sm_t * p_sm);
static void nav_sm_enter_search(nav_sm_t * p_sm);
static void nav_sm_enter_junction(nav_sm_t * p_sm);
static void nav_sm_enter_avoiding(nav_sm_t * p_sm);
static void nav_sm_enter_safe_hold(nav_sm_t * p_sm, nav_hold_t reason);
static void nav_sm_enter_complete(nav_sm_t * p_sm);
static void nav_sm_enter_fault(nav_sm_t * p_sm);
static void nav_sm_sweep(nav_sm_t * p_sm);
static void nav_sm_running_tick(nav_sm_t * p_sm, line_err_t const * p_line);
static bool nav_sm_line_fresh(nav_sm_t * p_sm, line_err_t const * p_line);
static int32_t nav_sm_past_extra_mm(uint8_t side);
static void nav_sm_move_finished(nav_sm_t * p_sm);
static void nav_sm_junction_move_finished(nav_sm_t * p_sm);
static void nav_sm_avoid_move_finished(nav_sm_t * p_sm);
static int32_t nav_sm_pass_mm(nav_sm_t const * p_sm);
static void nav_sm_barcode_event(nav_sm_t * p_sm, line_evt_t const * p_evt);
static void nav_sm_fallback_bypass(nav_sm_t * p_sm);
static void nav_sm_plan(nav_sm_t * p_sm, obstacle_evt_t const * p_evt);
static bool nav_sm_is_following(nav_sm_t const * p_sm);
static bool nav_sm_is_running(nav_sm_t const * p_sm);

/* BOOT: motor pins held safe until the first tick. */
void
nav_sm_init (nav_sm_t * p_sm, uint32_t now_ms)
{
    if (NULL != p_sm)
    {
        p_sm->mode               = NAV_MODE_BOOT;
        p_sm->hold_reason        = NAV_HOLD_NONE;
        p_sm->step               = NAV_STEP_NONE;
        p_sm->now_ms             = now_ms;
        p_sm->mode_since_ms      = now_ms;
        p_sm->step_since_ms      = now_ms;
        p_sm->run_start_ms       = now_ms;
        p_sm->deadline_ms        = now_ms;
        p_sm->b_selftest_failed  = false;
        letter_map_default(&p_sm->map);
        p_sm->b_command_held     = false;
        p_sm->held_letter        = '-';
        p_sm->held_action        = LETTER_ACTION_NONE;
        p_sm->b_speed_held       = false;
        p_sm->b_read_ignored     = false;
        steering_reset(&p_sm->steer);
        p_sm->last_error_pm      = 0;
        p_sm->b_line_seq_known   = false;
        p_sm->b_line_stale       = false;
        p_sm->line_seq           = 0u;
        p_sm->line_seq_ms        = now_ms;
        p_sm->next_move_id       = 1u;
        p_sm->pending_move_id    = 0u;
        p_sm->b_move_done        = false;
        p_sm->sweep_index        = 0u;
        p_sm->b_sweep_left_first = true;
        p_sm->distance_mm        = 0;
        p_sm->odo_heading_cdeg   = 0;
        p_sm->b_odo_valid        = false;
        p_sm->fused_heading_cdeg = 0;
        p_sm->b_fused_valid      = false;
        p_sm->turn_start_cdeg    = 0;
        p_sm->turn_command_cdeg  = 0;
        p_sm->avoid_side         = OBST_SIDE_NONE;
        p_sm->avoid_lateral_mm   = 0;
        p_sm->avoid_closest_mm   = 0;
        p_sm->avoid_width_mm     = 0;
        p_sm->avoid_pass_from_mm = 0;
        p_sm->rejoin_tries       = 0u;
        p_sm->b_reversed_once    = false;
        p_sm->act_count          = 0u;
        p_sm->act_dropped        = 0u;
    }
}

/* Forget the actions the task has already carried out. */
void
nav_sm_clear_actions (nav_sm_t * p_sm)
{
    if (NULL != p_sm)
    {
        p_sm->act_count = 0u;
    }
}

/*
 * The 5 ms tick.  p_line is the latest line-error record, health the
 * NAV_HEALTH_xxx bits, b_faulted Safety's FAULT level, b_start_pressed a
 * debounced press of the START button since the last tick.
 */
void
nav_sm_tick (nav_sm_t * p_sm, uint32_t now_ms, line_err_t const * p_line,
             uint32_t health, bool b_faulted, bool b_start_pressed)
{
    if ((NULL == p_sm) || (NULL == p_line))
    {
        /* Nothing to do without a state and a line record. */
    }
    else
    {
        nav_sm_tick_body(p_sm, now_ms, p_line, health, b_faulted,
                         b_start_pressed);
    }
}

/* The tick for valid arguments. */
static void
nav_sm_tick_body (nav_sm_t * p_sm, uint32_t now_ms,
                  line_err_t const * p_line, uint32_t health,
                  bool b_faulted, bool b_start_pressed)
{
    p_sm->now_ms = now_ms;

    if (b_faulted && (NAV_MODE_FAULT != p_sm->mode) &&
        (NAV_MODE_BOOT != p_sm->mode))
    {
        nav_sm_enter_fault(p_sm);
    }

    switch (p_sm->mode)
    {
        case NAV_MODE_BOOT:
            /* Initialisation complete: SELF-TEST (motors stay off). */
            nav_sm_set_mode(p_sm, NAV_MODE_SELF_TEST);
            nav_sm_stop(p_sm);
            nav_sm_scan(p_sm, SCAN_REQ_RUN_OVER, OBST_SIDE_NONE);
            p_sm->deadline_ms       = now_ms + CFG_NAV_SELFTEST_MS;
            p_sm->b_selftest_failed = false;
        break;

        case NAV_MODE_SELF_TEST:
            if (NAV_HEALTH_ALL == (health & NAV_HEALTH_ALL))
            {
                nav_sm_set_mode(p_sm, NAV_MODE_READY);
            }
            else if (((int32_t) (now_ms - p_sm->deadline_ms) >= 0) &&
                     !p_sm->b_selftest_failed)
            {
                /* Report once; Safety latches FAULT (report 5). */
                nav_act_t act = { 0 };

                act.type    = (uint8_t) NAV_ACT_SELF_TEST_FAIL;
                act.a_value = (int32_t) (health & NAV_HEALTH_ALL);
                nav_sm_push(p_sm, &act);
                p_sm->b_selftest_failed = true;
            }
            else
            {
                /* Keep waiting for every sensor to report healthy. */
            }
        break;

        case NAV_MODE_READY:
            if (b_start_pressed && !b_faulted)
            {
                nav_sm_start_run(p_sm);
            }
        break;

        case NAV_MODE_LINE_FOLLOWING:
        case NAV_MODE_HUMP_CROSSING:
        case NAV_MODE_LINE_SEARCH:
        case NAV_MODE_AT_JUNCTION:
        case NAV_MODE_AVOIDING:
            nav_sm_running_tick(p_sm, p_line);
        break;

        case NAV_MODE_SAFE_HOLD:
        case NAV_MODE_COMPLETE:
        case NAV_MODE_FAULT:
        default:
            /* Waiting for RESET, power off, or Safety's restart. */
        break;
    }
}

/* IF-01 events from the Line task. */
void
nav_sm_line_event (nav_sm_t * p_sm, line_evt_t const * p_evt)
{
    if ((NULL == p_sm) || (NULL == p_evt))
    {
        /* Nothing to do without a state and an event. */
    }
    else if (nav_sm_is_following(p_sm))
    {
        switch (line_evt_type(p_evt))
        {
            case LINE_EVT_JUNCTION:
                nav_sm_enter_junction(p_sm);
            break;

            case LINE_EVT_LOST:
                nav_sm_enter_search(p_sm);
            break;

            case LINE_EVT_CAPTURE_STARTED:
            case LINE_EVT_LETTER:
            case LINE_EVT_CAPTURE_FAILED:
                nav_sm_barcode_event(p_sm, p_evt);
            break;

            case LINE_EVT_FOUND:
            default:
                /* Nothing to do while following. */
            break;
        }
    }
    else if (LINE_EVT_CAPTURE_STARTED == line_evt_type(p_evt))
    {
        /* Barcode and junction events in the other modes are ignored
           (D-53): the junction crossbar also passes under the barcode
           sensor while turning.  A read that starts now stays ignored
           to its end, even if following has resumed by then. */
        p_sm->b_read_ignored = true;
    }
    else if ((LINE_EVT_LETTER == line_evt_type(p_evt)) ||
             (LINE_EVT_CAPTURE_FAILED == line_evt_type(p_evt)))
    {
        p_sm->b_read_ignored = false;   /* the ignored read has ended */
    }
    else
    {
        /* Junction, lost, found: each mode checks the line itself. */
    }
}

/* IF-03 events from the IMU task. */
void
nav_sm_imu_event (nav_sm_t * p_sm, imu_evt_t const * p_evt)
{
    if ((NULL != p_sm) && (NULL != p_evt))
    {
        nav_sm_imu_body(p_sm, p_evt);
    }
}

static void
nav_sm_imu_body (nav_sm_t * p_sm, imu_evt_t const * p_evt)
{
    uint8_t const type = imu_evt_type(p_evt);

    if (IMU_EVT_HEADING == type)
    {
        int32_t const heading = imu_evt_heading_cdeg(p_evt);

        /* Combine odometry with the compass on flat ground only (D-40). */
        if (!p_sm->b_fused_valid)
        {
            p_sm->fused_heading_cdeg = heading;
            p_sm->b_fused_valid      = true;
        }
        else if (imu_evt_level_ground(p_evt) &&
                 (NAV_MODE_HUMP_CROSSING != p_sm->mode))
        {
            int32_t const diff = fixmath_wrap_cdeg(heading -
                                                   p_sm->fused_heading_cdeg);

            p_sm->fused_heading_cdeg = fixmath_wrap_cdeg(
                p_sm->fused_heading_cdeg +
                fixmath_div_round(diff * CFG_NAV_COMPASS_WEIGHT_PM, 1000));
        }
        else
        {
            /* On a hump the tilt distorts the compass: odometry only. */
        }
    }
    else if ((IMU_EVT_HUMP_START == type) &&
             (NAV_MODE_LINE_FOLLOWING == p_sm->mode))
    {
        nav_sm_set_mode(p_sm, NAV_MODE_HUMP_CROSSING);

        if (0 != CFG_NAV_HUMP_FREEZE_LEVELS)
        {
            nav_sm_simple(p_sm, NAV_ACT_LINE_ADAPT, 0);     /* G7, D-23 */
        }
    }
    else if (IMU_EVT_HUMP_END == type)
    {
        /* Reports made while turning are dropped (report 6.4). */
        if (nav_sm_is_following(p_sm))
        {
            nav_sm_route(p_sm, ROUTE_HUMP, '\0', 0u, imu_evt_height_mm(p_evt),
                         imu_evt_peak_roll_cdeg(p_evt));
        }

        if (NAV_MODE_HUMP_CROSSING == p_sm->mode)
        {
            nav_sm_simple(p_sm, NAV_ACT_LINE_ADAPT, 1);
            nav_sm_set_mode(p_sm, NAV_MODE_LINE_FOLLOWING);
        }
    }
    else
    {
        /* Motion-class changes go to Telemetry from the IMU itself. */
    }
}

/* IF-05 events from Obstacle Scan (report 6.5, Fig 10). */
void
nav_sm_obstacle_event (nav_sm_t * p_sm, obstacle_evt_t const * p_evt)
{
    if ((NULL != p_sm) && (NULL != p_evt))
    {
        nav_sm_obstacle_body(p_sm, p_evt);
    }
}

static void
nav_sm_obstacle_body (nav_sm_t * p_sm, obstacle_evt_t const * p_evt)
{
    bool const    b_avoiding = (NAV_MODE_AVOIDING == p_sm->mode);
    uint8_t const type       = obstacle_evt_type(p_evt);

    if (OBST_EVT_NO_SAFE_ROUTE == type)
    {
        if (b_avoiding && (NAV_STEP_SCANNING == p_sm->step) &&
            (0 != CFG_NAV_NO_ROOM_FALLBACK))
        {
            /* The scan found the obstacle (its profile and width are
               already reported) but no side clear enough, even after
               reversing: with the fallback on, go round on the default
               side instead of stopping, like the no-room fallback below.
               Departs from FR11, as that fallback does. */
            nav_sm_fallback_bypass(p_sm);
        }
        else if (nav_sm_is_running(p_sm))
        {
            nav_sm_enter_safe_hold(p_sm, NAV_HOLD_NO_SAFE_ROUTE);
        }
        else
        {
            /* Not running: nothing to stop. */
        }
    }
    else if (OBST_EVT_AHEAD == type)
    {
        if (nav_sm_is_following(p_sm))
        {
            nav_sm_enter_avoiding(p_sm);
        }
        else if (!b_avoiding)
        {
            /* Not driving along the line: let Obstacle Scan carry on. */
            nav_sm_scan(p_sm, SCAN_REQ_RESUME, OBST_SIDE_NONE);
        }
        else if ((NAV_STEP_REJOINING == p_sm->step) &&
                 (p_sm->rejoin_tries < CFG_NAV_REJOIN_RETRIES))
        {
            /* Still in the way on the way back: turn parallel again and
               go further past it. */
            p_sm->rejoin_tries++;
            nav_sm_stop(p_sm);
            nav_sm_turn_toward(p_sm, p_sm->avoid_side, NAV_TURN_90_CDEG);
            nav_sm_set_step(p_sm, NAV_STEP_CLEAR_TURN);
        }
        else if (NAV_STEP_REJOINING == p_sm->step)
        {
            nav_sm_enter_safe_hold(p_sm, NAV_HOLD_NO_SAFE_ROUTE);
        }
        else
        {
            /* Already avoiding. */
        }
    }
    else if (b_avoiding && (NAV_STEP_SCANNING == p_sm->step))
    {
        if (OBST_EVT_PATH_CLEAR == type)
        {
            nav_sm_enter_following(p_sm);
        }
        else if (OBST_EVT_PLAN == type)
        {
            nav_sm_plan(p_sm, p_evt);
        }
        else if ((OBST_EVT_NO_ROOM == type) && !p_sm->b_reversed_once)
        {
            /* FR11: reverse 10 cm and scan once more. */
            nav_sm_move(p_sm, NAV_ACT_BACKWARD, CFG_NAV_REVERSE_MM);
            nav_sm_set_step(p_sm, NAV_STEP_REVERSING);
        }
        else if ((OBST_EVT_NO_ROOM == type) &&
                 (0 != CFG_NAV_NO_ROOM_FALLBACK))
        {
            /* No side looked clear twice: go round on the default side. */
            nav_sm_fallback_bypass(p_sm);
        }
        else if (OBST_EVT_NO_ROOM == type)
        {
            nav_sm_enter_safe_hold(p_sm, NAV_HOLD_NO_SAFE_ROUTE);
        }
        else
        {
            /* Other events do not apply while scanning. */
        }
    }
    else if (b_avoiding && (NAV_STEP_PASSING == p_sm->step) &&
             (OBST_EVT_BEHIND == type))
    {
        /* Side scan: obstacle behind.  Clear the rear of the car; if the
           scan said so before the car had even driven as far as the
           obstacle was ahead of it, drive the rest of that too. */
        int32_t const passed = p_sm->distance_mm - p_sm->avoid_pass_from_mm;
        int32_t const short_by = (p_sm->avoid_closest_mm > passed)
                                 ? (p_sm->avoid_closest_mm - passed) : 0;

        nav_sm_move(p_sm, NAV_ACT_FORWARD,
                    nav_sm_past_extra_mm(p_sm->avoid_side) + short_by);
        nav_sm_set_step(p_sm, NAV_STEP_PAST_EXTRA);
    }
    else
    {
        /* Not expected in this step: ignored. */
    }
}

/* IF-08 status from Motion: odometry every 20 ms and move done. */
void
nav_sm_motion_status (nav_sm_t * p_sm, motion_sts_t const * p_sts)
{
    if ((NULL != p_sm) && (NULL != p_sts))
    {
        nav_sm_motion_body(p_sm, p_sts);
    }
}

static void
nav_sm_motion_body (nav_sm_t * p_sm, motion_sts_t const * p_sts)
{
    uint8_t const type = motion_sts_type(p_sts);

    if (MOTION_STS_ODOMETRY == type)
    {
        int32_t const heading = motion_sts_heading_cdeg(p_sts);

        if (!p_sm->b_fused_valid)
        {
            /* No compass reading yet: start from odometry. */
            p_sm->fused_heading_cdeg = heading;
            p_sm->b_fused_valid      = true;
        }
        else if (p_sm->b_odo_valid)
        {
            int32_t const delta = fixmath_wrap_cdeg(heading -
                                                    p_sm->odo_heading_cdeg);

            p_sm->fused_heading_cdeg = fixmath_wrap_cdeg(
                p_sm->fused_heading_cdeg + delta);
        }

        p_sm->distance_mm      = motion_sts_distance_mm(p_sts);
        p_sm->odo_heading_cdeg = heading;
        p_sm->b_odo_valid      = true;
    }
    else if ((MOTION_STS_MOVE_DONE == type) &&
             (0u != p_sm->pending_move_id) &&
             (motion_sts_move_id(p_sts) == p_sm->pending_move_id))
    {
        p_sm->pending_move_id = 0u;
        p_sm->b_move_done     = true;
        nav_sm_move_finished(p_sm);
    }
    else
    {
        /* A move that was cancelled or replaced: ignore. */
    }
}

/* IF-10 commands from the Command task (FR6, NFR8). */
void
nav_sm_command (nav_sm_t * p_sm, nav_cmd_t const * p_cmd)
{
    if ((NULL != p_sm) && (NULL != p_cmd))
    {
        nav_act_t     act  = { 0 };
        uint8_t const type = nav_cmd_type(p_cmd);

        act.type    = (uint8_t) NAV_ACT_ACK;
        /* Reported back as is; at most CMD_SEQ_MAX, so it fits. */
        act.a_value = (int32_t) nav_cmd_seq(p_cmd);

        if (NAV_CMD_REMAP == type)
        {
            act.kind    = CMD_CODE_REMAP;
            act.letter  = nav_cmd_letter(p_cmd);
            act.action  = nav_cmd_action(p_cmd);
            /* letter_map_set() refuses no letter and an action out of
               range, so the cast cannot store an invalid action. */
            act.b_value = letter_map_set(&p_sm->map, act.letter,
                                         (letter_action_t) act.action)
                          ? 1 : 0;
        }
        else if (NAV_CMD_MAP_DEFAULT == type)
        {
            act.kind    = CMD_CODE_MAP_DEFAULT;
            letter_map_default(&p_sm->map);
            act.b_value = 1;
        }
        else
        {
            act.kind    = CMD_CODE_UNKNOWN;
            act.b_value = 0;
        }

        nav_sm_push(p_sm, &act);
    }
}

/* Safety cleared the fault: re-run SELF-TEST (report SD2). */
void
nav_sm_restart (nav_sm_t * p_sm)
{
    if ((NULL != p_sm) && (NAV_MODE_FAULT == p_sm->mode))
    {
        nav_sm_set_mode(p_sm, NAV_MODE_SELF_TEST);
        p_sm->deadline_ms       = p_sm->now_ms + CFG_NAV_SELFTEST_MS;
        p_sm->b_selftest_failed = false;

        /* SELF-TEST checks the line sensors, so the Line task senses
           again: it is idle if the fault came after COMPLETE (D-98). */
        nav_sm_simple(p_sm, NAV_ACT_LINE_RUN_OVER, 0);
    }
}

/* RESET pressed with no fault: SAFE HOLD -> READY (report 5). */
void
nav_sm_reset (nav_sm_t * p_sm)
{
    if ((NULL != p_sm) && (NAV_MODE_SAFE_HOLD == p_sm->mode))
    {
        nav_sm_set_mode(p_sm, NAV_MODE_READY);
    }
}

/* ------------------------------------------------------------------ */

static void
nav_sm_set_mode (nav_sm_t * p_sm, nav_mode_t mode)
{
    if (nav_sm_is_following(p_sm) && p_sm->b_speed_held &&
        (NAV_MODE_LINE_FOLLOWING != mode) &&
        (NAV_MODE_HUMP_CROSSING != mode))
    {
        /* A read is running (its speed hold is on) and the car stops
           following: its letter or failure is ignored when it comes,
           even if following has resumed by then (D-53). */
        nav_sm_release_hold(p_sm);
        p_sm->b_read_ignored = true;
    }

    p_sm->mode          = mode;
    p_sm->mode_since_ms = p_sm->now_ms;
    nav_sm_set_step(p_sm, NAV_STEP_NONE);
}

static void
nav_sm_set_step (nav_sm_t * p_sm, nav_step_t step)
{
    p_sm->step          = step;
    p_sm->step_since_ms = p_sm->now_ms;
}

static void
nav_sm_push (nav_sm_t * p_sm, nav_act_t const * p_act)
{
    if (p_sm->act_count < NAV_ACT_MAX)
    {
        p_sm->acts[p_sm->act_count] = *p_act;
        p_sm->act_count++;
    }
    else
    {
        p_sm->act_dropped++;
    }
}

static void
nav_sm_simple (nav_sm_t * p_sm, nav_act_type_t type, int32_t a_value)
{
    nav_act_t act = { 0 };

    act.type    = (uint8_t) type;
    act.a_value = a_value;
    nav_sm_push(p_sm, &act);
}

/* A distance move or turn with a new move ID; it replaces any running
   move, whose move-done is then ignored. */
static void
nav_sm_move (nav_sm_t * p_sm, nav_act_type_t type, int32_t a_value)
{
    nav_act_t act = { 0 };

    act.type    = (uint8_t) type;
    act.a_value = a_value;
    act.move_id = p_sm->next_move_id;

    p_sm->pending_move_id = p_sm->next_move_id;
    p_sm->b_move_done     = false;
    p_sm->next_move_id    = (uint8_t) (p_sm->next_move_id + 1u);

    if (0u == p_sm->next_move_id)
    {
        p_sm->next_move_id = 1u;    /* 0 means "no move" */
    }

    nav_sm_push(p_sm, &act);
}

static void
nav_sm_stop (nav_sm_t * p_sm)
{
    p_sm->pending_move_id = 0u;
    nav_sm_simple(p_sm, NAV_ACT_STOP, 0);
}

static void
nav_sm_scan (nav_sm_t * p_sm, uint8_t kind, uint8_t side)
{
    nav_act_t act = { 0 };

    act.type = (uint8_t) NAV_ACT_SCAN;
    act.kind = kind;
    act.side = side;
    nav_sm_push(p_sm, &act);
}

/* A route-log entry (FR13) at the current distance. */
static void
nav_sm_route (nav_sm_t * p_sm, uint8_t kind, char letter, uint8_t action,
              int32_t value, int32_t value2)
{
    nav_act_t act = { 0 };

    act.type    = (uint8_t) NAV_ACT_ROUTE;
    act.kind    = kind;
    act.letter  = letter;
    act.action  = action;
    act.a_value = p_sm->distance_mm;
    act.b_value = value;
    act.c_value = value2;
    nav_sm_push(p_sm, &act);
}

/* Release holdSpeed() if it was requested (report 6.3). */
static void
nav_sm_release_hold (nav_sm_t * p_sm)
{
    if (p_sm->b_speed_held)
    {
        p_sm->b_speed_held = false;
        nav_sm_simple(p_sm, NAV_ACT_HOLD_SPEED, 0);
    }
}

static void
nav_sm_turn_toward (nav_sm_t * p_sm, uint8_t side, int32_t cdeg)
{
    nav_sm_move(p_sm, (OBST_SIDE_LEFT == side) ? NAV_ACT_TURN_LEFT
                                               : NAV_ACT_TURN_RIGHT, cdeg);
}

/* READY -> RUNNING on START (FR16). */
static void
nav_sm_start_run (nav_sm_t * p_sm)
{
    p_sm->run_start_ms     = p_sm->now_ms;
    p_sm->b_command_held   = false;
    p_sm->held_letter      = '-';
    p_sm->held_action      = LETTER_ACTION_NONE;
    /* b_read_ignored is kept: a read that started before START (in
       READY) is ignored to its end, like one started while turning. */
    p_sm->b_reversed_once  = false;
    p_sm->hold_reason      = NAV_HOLD_NONE;
    p_sm->last_error_pm    = 0;
    p_sm->b_line_seq_known = false;     /* the stale check starts again */
    p_sm->b_line_stale     = false;

    nav_sm_simple(p_sm, NAV_ACT_LINE_RUN_OVER, 0);
    nav_sm_simple(p_sm, NAV_ACT_LINE_ADAPT, 1);
    nav_sm_scan(p_sm, SCAN_REQ_START, OBST_SIDE_NONE);
    nav_sm_route(p_sm, ROUTE_RUN_START, '\0', 0u, 0, 0);
    nav_sm_enter_following(p_sm);
}

/* LINE FOLLOWING / HUMP CROSSING tick: steer from the line error. */
static void
nav_sm_follow (nav_sm_t * p_sm)
{
    int32_t const base = (NAV_MODE_HUMP_CROSSING == p_sm->mode)
                         ? CFG_NAV_HUMP_MMPS : CFG_NAV_CRUISE_MMPS;
    int32_t speed = 0;
    int32_t diff  = 0;
    nav_act_t act = { 0 };

    /* While the line is briefly missing the last error is kept, so the
       car keeps turning towards where the line was (D-52). */
    steering_compute(&p_sm->steer, p_sm->last_error_pm, base, &speed,
                     &diff);

    /* The commanded speed stays inside NFR17's range (0.15-0.35 m/s,
       where line following must work), so the steering slow-down never
       commands a speed outside it.  Commanded only: the real speed is
       measured in SYS-17 (D-71). */
    speed = fixmath_clamp(speed, CFG_NAV_SPEED_MIN_MMPS,
                          CFG_NAV_SPEED_MAX_MMPS);

    act.type    = (uint8_t) NAV_ACT_DRIVE;
    act.a_value = speed;
    act.b_value = diff;
    nav_sm_push(p_sm, &act);

    if ((NAV_MODE_HUMP_CROSSING == p_sm->mode) &&
        ((p_sm->now_ms - p_sm->mode_since_ms) >= CFG_NAV_HUMP_MAX_MS))
    {
        nav_sm_simple(p_sm, NAV_ACT_LINE_ADAPT, 1);     /* D-38 */
        nav_sm_set_mode(p_sm, NAV_MODE_LINE_FOLLOWING);
    }
}

static void
nav_sm_enter_following (nav_sm_t * p_sm)
{
    steering_reset(&p_sm->steer);
    nav_sm_set_mode(p_sm, NAV_MODE_LINE_FOLLOWING);
}

/* LINE SEARCH: unexpected loss while following (FR18). */
static void
nav_sm_enter_search (nav_sm_t * p_sm)
{
    if (NAV_MODE_HUMP_CROSSING == p_sm->mode)
    {
        nav_sm_simple(p_sm, NAV_ACT_LINE_ADAPT, 1);
    }

    nav_sm_set_mode(p_sm, NAV_MODE_LINE_SEARCH);
    nav_sm_stop(p_sm);
    nav_sm_route(p_sm, ROUTE_LINE_SEARCH, '\0', 0u, 1, 0);
    p_sm->deadline_ms        = p_sm->now_ms + CFG_NAV_SEARCH_MS;
    p_sm->sweep_index        = 0u;
    p_sm->b_sweep_left_first = (p_sm->last_error_pm >= 0);
    nav_sm_set_step(p_sm, NAV_STEP_SWEEP);
}

/* AT JUNCTION (report 5, SD1): act on the held command or stop (FR5). */
static void
nav_sm_enter_junction (nav_sm_t * p_sm)
{
    if (NAV_MODE_HUMP_CROSSING == p_sm->mode)
    {
        nav_sm_simple(p_sm, NAV_ACT_LINE_ADAPT, 1);
    }

    nav_sm_set_mode(p_sm, NAV_MODE_AT_JUNCTION);

    if (!p_sm->b_command_held)
    {
        nav_sm_route(p_sm, ROUTE_NO_COMMAND, '\0', 0u, 0, 0);
        nav_sm_enter_safe_hold(p_sm, NAV_HOLD_NO_COMMAND);
    }
    else
    {
        /* Move the axle onto the junction, then turn there. */
        nav_sm_move(p_sm, NAV_ACT_FORWARD, CFG_NAV_SENSOR_TO_AXLE_MM);
        nav_sm_set_step(p_sm, NAV_STEP_TO_AXLE);
    }
}

/* AVOIDING OBSTACLE: stop first, then Obstacle Scan profiles it. */
static void
nav_sm_enter_avoiding (nav_sm_t * p_sm)
{
    if (NAV_MODE_HUMP_CROSSING == p_sm->mode)
    {
        nav_sm_simple(p_sm, NAV_ACT_LINE_ADAPT, 1);
    }

    nav_sm_set_mode(p_sm, NAV_MODE_AVOIDING);
    nav_sm_stop(p_sm);
    p_sm->avoid_side = OBST_SIDE_NONE;
    nav_sm_set_step(p_sm, NAV_STEP_STOPPING);
}

static void
nav_sm_enter_safe_hold (nav_sm_t * p_sm, nav_hold_t reason)
{
    p_sm->hold_reason = reason;
    nav_sm_set_mode(p_sm, NAV_MODE_SAFE_HOLD);
    nav_sm_stop(p_sm);
    nav_sm_simple(p_sm, NAV_ACT_LINE_ADAPT, 1);
    nav_sm_scan(p_sm, SCAN_REQ_RUN_OVER, OBST_SIDE_NONE);
    nav_sm_route(p_sm, ROUTE_SAFE_HOLD, '\0', 0u, (int32_t) reason, 0);
}

/* COMPLETE: no line after the 5 s search, the run is over (FR18). */
static void
nav_sm_enter_complete (nav_sm_t * p_sm)
{
    nav_sm_set_mode(p_sm, NAV_MODE_COMPLETE);
    nav_sm_stop(p_sm);
    nav_sm_route(p_sm, ROUTE_COMPLETE, '\0', 0u,
                 (int32_t) (p_sm->now_ms - p_sm->run_start_ms), 0);
    nav_sm_simple(p_sm, NAV_ACT_SUMMARY, 0);
    nav_sm_scan(p_sm, SCAN_REQ_RUN_OVER, OBST_SIDE_NONE);
    nav_sm_simple(p_sm, NAV_ACT_LINE_RUN_OVER, 1);
}

/* FAULT: Safety brakes the motors; Navigation stops commanding Motion
   (report SD2) until Safety restarts it. */
static void
nav_sm_enter_fault (nav_sm_t * p_sm)
{
    if (p_sm->b_speed_held)
    {
        p_sm->b_read_ignored = true;    /* a read cut by the fault (D-53) */
    }

    p_sm->b_speed_held    = false;
    p_sm->pending_move_id = 0u;
    nav_sm_set_mode(p_sm, NAV_MODE_FAULT);
    nav_sm_route(p_sm, ROUTE_FAULT, '\0', 0u, 0, 0);
    nav_sm_scan(p_sm, SCAN_REQ_RUN_OVER, OBST_SIDE_NONE);
    nav_sm_simple(p_sm, NAV_ACT_LINE_ADAPT, 1);
}

/*
 * Sweep on the spot (D-33): towards the side the line was last seen by
 * SWEEP, back across by 2 x SWEEP, then back to the start, repeated.
 */
static void
nav_sm_sweep (nav_sm_t * p_sm)
{
    uint32_t const phase  = p_sm->sweep_index % NAV_SWEEP_PHASES;
    bool           b_left = p_sm->b_sweep_left_first;
    int32_t        angle  = CFG_NAV_SWEEP_CDEG;

    if (1u == phase)
    {
        b_left = !b_left;
        angle  = NAV_HALF * CFG_NAV_SWEEP_CDEG;
    }

    nav_sm_move(p_sm, b_left ? NAV_ACT_TURN_LEFT : NAV_ACT_TURN_RIGHT,
                angle);
    p_sm->sweep_index++;
}

/* Tick for the RUNNING modes. */
static void
nav_sm_running_tick (nav_sm_t * p_sm, line_err_t const * p_line)
{
    bool const b_fresh = nav_sm_line_fresh(p_sm, p_line);
    bool const b_line  = (b_fresh && line_err_valid(p_line) &&
                          line_err_line_seen(p_line));
    uint32_t const acts_before = p_sm->act_count;

    if (b_line)
    {
        p_sm->last_error_pm = line_err_error_pm(p_line);
    }
    else if (nav_sm_is_following(p_sm))
    {
        /* Neither sensor on the line: let the held error fade. */
        p_sm->last_error_pm = (p_sm->last_error_pm *
                               CFG_NAV_LOST_DECAY_PM) / 1000;
    }
    else
    {
        /* Searching or avoiding: keep it (it picks the sweep side). */
    }

    if (nav_sm_is_following(p_sm))
    {
        nav_sm_follow(p_sm);
    }
    else if ((NAV_MODE_LINE_SEARCH == p_sm->mode) ||
             ((NAV_MODE_AT_JUNCTION == p_sm->mode) &&
              (NAV_STEP_FIND_LINE == p_sm->step)))
    {
        if (b_line)
        {
            if (NAV_MODE_LINE_SEARCH == p_sm->mode)
            {
                nav_sm_route(p_sm, ROUTE_LINE_SEARCH, '\0', 0u, 0, 0);
            }

            nav_sm_stop(p_sm);
            nav_sm_enter_following(p_sm);
        }
        else if ((int32_t) (p_sm->now_ms - p_sm->deadline_ms) >= 0)
        {
            nav_sm_stop(p_sm);

            if (NAV_MODE_LINE_SEARCH == p_sm->mode)
            {
                nav_sm_enter_complete(p_sm);
            }
            else
            {
                nav_sm_enter_safe_hold(p_sm, NAV_HOLD_LINE_NOT_FOUND);
            }
        }
        else if (0u == p_sm->pending_move_id)
        {
            nav_sm_sweep(p_sm);
        }
        else
        {
            /* Sweep move still running. */
        }
    }
    else if ((NAV_MODE_AVOIDING == p_sm->mode) &&
             (NAV_STEP_STOPPING == p_sm->step) &&
             ((p_sm->now_ms - p_sm->step_since_ms) >= CFG_NAV_STOP_SETTLE_MS))
    {
        nav_sm_scan(p_sm, SCAN_REQ_CAR_STOPPED, OBST_SIDE_NONE);
        nav_sm_set_step(p_sm, NAV_STEP_SCANNING);
    }
    else if ((NAV_MODE_AVOIDING == p_sm->mode) &&
             (NAV_STEP_ALIGN_TURN == p_sm->step) && b_line &&
             (fixmath_abs(fixmath_wrap_cdeg(p_sm->fused_heading_cdeg -
                                            p_sm->turn_start_cdeg)) >=
              CFG_NAV_ALIGN_MIN_CDEG))
    {
        /* Turned back far enough and a sensor is on the line: follow it
           from here, whatever way the line bends. */
        nav_sm_stop(p_sm);
        nav_sm_scan(p_sm, SCAN_REQ_LINE_FOUND, OBST_SIDE_NONE);
        nav_sm_enter_following(p_sm);
    }
    else if ((NAV_MODE_AVOIDING == p_sm->mode) &&
             (NAV_STEP_REJOINING == p_sm->step) && b_line)
    {
        /* Line found: put the axle on it, then turn along it. */
        nav_sm_move(p_sm, NAV_ACT_FORWARD, CFG_NAV_SENSOR_TO_AXLE_MM);
        nav_sm_set_step(p_sm, NAV_STEP_ALIGN_MOVE);
    }
    else
    {
        /* Waiting for a move to finish or for an event. */
    }

    /* The 100 ms update rule (report 6.2): keep a running move alive. */
    if ((0u != p_sm->pending_move_id) && (acts_before == p_sm->act_count))
    {
        nav_sm_simple(p_sm, NAV_ACT_KEEPALIVE, 0);
    }
}

/*
 * G19 (D-94): the Line task publishes a sample every 5 ms, and each one
 * (a failed read too) moves the record's sample count.  A count that
 * has not moved for CFG_NAV_LINE_STALE_MS means the Line task stopped
 * publishing: the record is not used as line seen, and a LINE_DATA
 * fault is reported once, so Safety brakes the car, as D-79 does for
 * failed reads.  Elapsed time since the count was first seen, not a
 * count of ticks: a late tick does not stretch the limit.  Returns
 * false while the record is stale.
 */
static bool
nav_sm_line_fresh (nav_sm_t * p_sm, line_err_t const * p_line)
{
    /* The count is an 8-bit field (IF-02): it fits uint8_t. */
    uint8_t const  seq     = (uint8_t) line_err_seq(p_line);
    uint32_t const age_ms  = p_sm->now_ms - p_sm->line_seq_ms;

    if (!p_sm->b_line_seq_known || (seq != p_sm->line_seq))
    {
        p_sm->b_line_seq_known = true;
        p_sm->b_line_stale     = false;
        p_sm->line_seq         = seq;
        p_sm->line_seq_ms      = p_sm->now_ms;
    }
    else if (!p_sm->b_line_stale &&
             (age_ms >= (uint32_t) CFG_NAV_LINE_STALE_MS))
    {
        nav_act_t act = { 0 };

        /* detail: how long the count had not moved, at most what the
           fault report holds */
        p_sm->b_line_stale = true;
        act.type    = (uint8_t) NAV_ACT_LINE_STALE;
        act.a_value = (int32_t) ((age_ms < FAULT_DETAIL_MAX)
                                 ? age_ms : FAULT_DETAIL_MAX);
        nav_sm_push(p_sm, &act);
    }
    else
    {
        /* Fresh, or stale and reported once already. */
    }

    return (!p_sm->b_line_stale);
}

/*
 * Extra travel once the side scan reports the obstacle behind (D-97).
 * The scan looks as far sideways as the servo goes, short of square by
 * "ahead" degrees, so it loses the obstacle early by about the passing
 * distance (gap + half the car) x tan(ahead); that is added to
 * CFG_NAV_PAST_EXTRA_MM.  Passing on the left, the scan looks right.
 */
static int32_t
nav_sm_past_extra_mm (uint8_t side)
{
    int32_t const servo_deg  = (OBST_SIDE_LEFT == side)
                               ? CFG_SCAN_SIDE_RIGHT_DEG
                               : CFG_SCAN_SIDE_LEFT_DEG;
    int32_t const ahead_cdeg = (NAV_SERVO_SQUARE_DEG -
                                fixmath_abs(servo_deg - NAV_SERVO_AHEAD_DEG))
                               * NAV_CDEG_PER_DEG;
    int32_t const dist_mm    = CFG_NAV_PASS_GAP_MM +
                               (CFG_CAR_WIDTH_MM / NAV_HALF);

    /* ahead is 0..45 degrees (build check): cos is above 0.7 and
       dist_mm x sin stays far below 2^31. */
    return (CFG_NAV_PAST_EXTRA_MM +
            fixmath_div_round(dist_mm * fixmath_sin_q15(ahead_cdeg),
                              fixmath_cos_q15(ahead_cdeg)));
}

static void
nav_sm_move_finished (nav_sm_t * p_sm)
{
    if (NAV_MODE_AT_JUNCTION == p_sm->mode)
    {
        nav_sm_junction_move_finished(p_sm);
    }
    else if (NAV_MODE_AVOIDING == p_sm->mode)
    {
        nav_sm_avoid_move_finished(p_sm);
    }
    else
    {
        /* LINE SEARCH: the next sweep move starts on the next tick. */
    }
}

/* SD1: axle on the junction -> turn -> clear the command -> find line. */
static void
nav_sm_junction_move_finished (nav_sm_t * p_sm)
{
    if (NAV_STEP_TO_AXLE == p_sm->step)
    {
        p_sm->turn_start_cdeg = p_sm->fused_heading_cdeg;
        nav_sm_set_step(p_sm, NAV_STEP_TURN);

        switch (p_sm->held_action)
        {
            case LETTER_ACTION_LEFT:
                p_sm->turn_command_cdeg = NAV_TURN_90_CDEG;
                nav_sm_move(p_sm, NAV_ACT_TURN_LEFT, NAV_TURN_90_CDEG);
            break;

            case LETTER_ACTION_RIGHT:
                p_sm->turn_command_cdeg = -NAV_TURN_90_CDEG;
                nav_sm_move(p_sm, NAV_ACT_TURN_RIGHT, NAV_TURN_90_CDEG);
            break;

            case LETTER_ACTION_UTURN:
                p_sm->turn_command_cdeg = NAV_TURN_180_CDEG;
                nav_sm_move(p_sm, (0 != CFG_NAV_UTURN_LEFT)
                                  ? NAV_ACT_TURN_LEFT : NAV_ACT_TURN_RIGHT,
                            NAV_TURN_180_CDEG);
            break;

            case LETTER_ACTION_STRAIGHT:
            case LETTER_ACTION_NONE:
            case LETTER_ACTION_COUNT:
            default:
                /* Straight on: no turn, finish at once. */
                p_sm->turn_command_cdeg = 0;
                nav_sm_junction_move_finished(p_sm);
            break;
        }
    }
    else if (NAV_STEP_TURN == p_sm->step)
    {
        int32_t const measured = fixmath_wrap_cdeg(p_sm->fused_heading_cdeg -
                                                   p_sm->turn_start_cdeg);

        nav_sm_route(p_sm, ROUTE_TURN, p_sm->held_letter,
                     (uint8_t) p_sm->held_action, p_sm->turn_command_cdeg,
                     measured);
        p_sm->b_command_held = false;
        p_sm->held_letter    = '-';
        p_sm->held_action    = LETTER_ACTION_NONE;
        p_sm->deadline_ms    = p_sm->now_ms + CFG_NAV_FIND_LINE_MS;
        p_sm->sweep_index    = 0u;
        nav_sm_set_step(p_sm, NAV_STEP_FIND_LINE);
    }
    else
    {
        /* FIND_LINE sweep move: the next one starts on the next tick. */
    }
}

/* Passing leg: far enough that the car's rear clears the obstacle. */
static int32_t
nav_sm_pass_mm (nav_sm_t const * p_sm)
{
    int32_t const depth = fixmath_clamp(p_sm->avoid_width_mm,
                                        CFG_NAV_DEPTH_MIN_MM,
                                        CFG_NAV_DEPTH_MAX_MM);

    return (fixmath_clamp(p_sm->avoid_closest_mm + depth +
                          CFG_CAR_LENGTH_MM + CFG_NAV_PASS_MARGIN_MM,
                          0, CFG_NAV_BYPASS_MAX_MM));
}

/* The rectangular bypass (D-36), one step per finished move. */
static void
nav_sm_avoid_move_finished (nav_sm_t * p_sm)
{
    uint8_t const side  = p_sm->avoid_side;
    uint8_t const other = (OBST_SIDE_LEFT == side) ? OBST_SIDE_RIGHT
                                                   : OBST_SIDE_LEFT;

    switch (p_sm->step)
    {
        case NAV_STEP_REVERSING:
            p_sm->b_reversed_once = true;
            nav_sm_scan(p_sm, SCAN_REQ_REVERSED, OBST_SIDE_NONE);
            nav_sm_set_step(p_sm, NAV_STEP_SCANNING);
        break;

        case NAV_STEP_TURN_OUT:
            nav_sm_move(p_sm, NAV_ACT_FORWARD, p_sm->avoid_lateral_mm);
            nav_sm_set_step(p_sm, NAV_STEP_LEG_OUT);
        break;

        case NAV_STEP_LEG_OUT:
            nav_sm_turn_toward(p_sm, other, NAV_TURN_90_CDEG);
            nav_sm_set_step(p_sm, NAV_STEP_TURN_PARALLEL);
        break;

        case NAV_STEP_TURN_PARALLEL:
            p_sm->avoid_pass_from_mm = p_sm->distance_mm;
            nav_sm_scan(p_sm, SCAN_REQ_BYPASS, side);
            nav_sm_move(p_sm, NAV_ACT_FORWARD, nav_sm_pass_mm(p_sm));
            nav_sm_set_step(p_sm, NAV_STEP_PASSING);
        break;

        case NAV_STEP_PASSING:
            /* Drove past it by dead reckoning (the side scan did not
               report it behind): turn back in; the scan watches ahead on
               the way back and sends the car further if it is still
               there. */
            nav_sm_turn_toward(p_sm, other, NAV_TURN_90_CDEG);
            nav_sm_set_step(p_sm, NAV_STEP_TURN_IN);
        break;

        case NAV_STEP_PAST_EXTRA:
            nav_sm_turn_toward(p_sm, other, NAV_TURN_90_CDEG);
            nav_sm_set_step(p_sm, NAV_STEP_TURN_IN);
        break;

        case NAV_STEP_TURN_IN:
            nav_sm_scan(p_sm, SCAN_REQ_REJOIN, OBST_SIDE_NONE);
            nav_sm_move(p_sm, NAV_ACT_FORWARD, CFG_NAV_REJOIN_MAX_MM);
            nav_sm_set_step(p_sm, NAV_STEP_REJOINING);
        break;

        case NAV_STEP_REJOINING:
            /* The whole rejoin leg without crossing the line. */
            if (0 != CFG_NAV_REJOIN_MISS_SEARCH)
            {
                /* The line bends here: sweep for it, watching ahead. */
                nav_sm_scan(p_sm, SCAN_REQ_RESUME, OBST_SIDE_NONE);
                nav_sm_enter_search(p_sm);
            }
            else
            {
                nav_sm_scan(p_sm, SCAN_REQ_LINE_MISSED, OBST_SIDE_NONE);
                nav_sm_enter_safe_hold(p_sm, NAV_HOLD_NO_SAFE_ROUTE);
            }
        break;

        case NAV_STEP_CLEAR_TURN:
            nav_sm_move(p_sm, NAV_ACT_FORWARD, CFG_NAV_REJOIN_MORE_MM);
            nav_sm_set_step(p_sm, NAV_STEP_CLEAR_MOVE);
        break;

        case NAV_STEP_CLEAR_MOVE:
            nav_sm_turn_toward(p_sm, other, NAV_TURN_90_CDEG);
            nav_sm_set_step(p_sm, NAV_STEP_TURN_IN);
        break;

        case NAV_STEP_ALIGN_MOVE:
            /* Turn back along the line until a sensor sees it (checked
               every tick), at most CFG_NAV_ALIGN_MAX_CDEG. */
            p_sm->turn_start_cdeg = p_sm->fused_heading_cdeg;
            nav_sm_turn_toward(p_sm, side, CFG_NAV_ALIGN_MAX_CDEG);
            nav_sm_set_step(p_sm, NAV_STEP_ALIGN_TURN);
        break;

        case NAV_STEP_ALIGN_TURN:
            nav_sm_scan(p_sm, SCAN_REQ_LINE_FOUND, OBST_SIDE_NONE);
            nav_sm_enter_following(p_sm);
        break;

        default:
            /* No move expected in this step. */
        break;
    }
}

/* Speed hold and held command from barcode events (report 6.3, SD1). */
static void
nav_sm_barcode_event (nav_sm_t * p_sm, line_evt_t const * p_evt)
{
    uint8_t const type   = line_evt_type(p_evt);
    char const    letter = line_evt_letter(p_evt);

    if (LINE_EVT_CAPTURE_STARTED == type)
    {
        p_sm->b_read_ignored = false;   /* a new read, started here */

        if (!p_sm->b_speed_held)
        {
            p_sm->b_speed_held = true;
            nav_sm_simple(p_sm, NAV_ACT_HOLD_SPEED, 1);
        }
    }
    else if (p_sm->b_read_ignored)
    {
        /* The end of a read that started while the car was not
           following (turning at a junction), or that the car stopped
           following during: ignored (D-53). */
        p_sm->b_read_ignored = false;
    }
    else if (LINE_EVT_LETTER == type)
    {
        letter_action_t const action = letter_map_get(&p_sm->map, letter);

        nav_sm_route(p_sm, ROUTE_BARCODE, letter, (uint8_t) action,
                     line_evt_reversed(p_evt) ? 1 : 0, 0);

        /* A letter without an action is reported but not held (D-54). */
        if ((LETTER_ACTION_NONE != action) &&
            ((!p_sm->b_command_held) || (0 != CFG_NAV_NEW_LETTER_REPLACES)))
        {
            p_sm->b_command_held = true;
            p_sm->held_letter    = letter;
            p_sm->held_action    = action;
        }

        nav_sm_release_hold(p_sm);
    }
    else
    {
        /* Capture failed: release the speed, keep any held command.
           Logged so the dashboard shows the sensor saw bars but the
           pattern did not decode. */
        nav_sm_route(p_sm, ROUTE_BC_FAIL, '\0', 0u, 0, 0);
        nav_sm_release_hold(p_sm);
    }
}

/* Fallback bypass (CFG_NAV_NO_ROOM_FALLBACK): no side was clear, so go
   round on the default side a fixed distance out; the passing leg and the
   watch on the way back still apply. */
static void
nav_sm_fallback_bypass (nav_sm_t * p_sm)
{
    p_sm->avoid_side       = (0 != CFG_NAV_FALLBACK_LEFT)
                             ? OBST_SIDE_LEFT : OBST_SIDE_RIGHT;
    p_sm->avoid_lateral_mm = CFG_NAV_FALLBACK_LATERAL_MM;
    p_sm->avoid_closest_mm = CFG_SCAN_DETECT_MM;
    p_sm->avoid_width_mm   = CFG_NAV_FALLBACK_DEPTH_MM;
    p_sm->rejoin_tries     = 0u;
    /* Logged with width 0: the width (if any) was in the profile event. */
    nav_sm_route(p_sm, ROUTE_OBSTACLE, '\0', p_sm->avoid_side, 0,
                 CFG_SCAN_DETECT_MM);
    nav_sm_turn_toward(p_sm, p_sm->avoid_side, NAV_TURN_90_CDEG);
    nav_sm_set_step(p_sm, NAV_STEP_TURN_OUT);
}

/* OBST_EVT_PLAN: work out the sideways leg, then turn out (FR11). */
static void
nav_sm_plan (nav_sm_t * p_sm, obstacle_evt_t const * p_evt)
{
    int32_t const half_car = CFG_CAR_WIDTH_MM / NAV_HALF;
    uint8_t const side     = obstacle_evt_side(p_evt);
    int32_t       lateral  = 0;
    /* The beam's spread makes each edge look further out than it is. */
    int32_t const spread   = fixmath_div_round(
                                 obstacle_evt_closest_mm(p_evt) *
                                 CFG_NAV_BEAM_SPREAD_PM, 1000);

    if (OBST_SIDE_LEFT == side)
    {
        lateral = obstacle_evt_edge_left_mm(p_evt) - spread;
    }
    else
    {
        lateral = -obstacle_evt_edge_right_mm(p_evt) - spread;
    }

    /* Car centre ends up gap + half a car beyond the obstacle's edge. */
    lateral = fixmath_clamp(lateral, 0, CFG_NAV_BYPASS_MAX_MM) +
              CFG_NAV_PASS_GAP_MM + half_car;

    p_sm->avoid_side       = side;
    p_sm->avoid_lateral_mm = lateral;
    p_sm->avoid_closest_mm = obstacle_evt_closest_mm(p_evt);
    p_sm->avoid_width_mm   = obstacle_evt_width_mm(p_evt);
    p_sm->rejoin_tries     = 0u;
    nav_sm_route(p_sm, ROUTE_OBSTACLE, '\0', side,
                 obstacle_evt_width_mm(p_evt), obstacle_evt_closest_mm(p_evt));
    nav_sm_turn_toward(p_sm, side, NAV_TURN_90_CDEG);
    nav_sm_set_step(p_sm, NAV_STEP_TURN_OUT);
}

static bool
nav_sm_is_following (nav_sm_t const * p_sm)
{
    return ((NAV_MODE_LINE_FOLLOWING == p_sm->mode) ||
            (NAV_MODE_HUMP_CROSSING == p_sm->mode));
}

static bool
nav_sm_is_running (nav_sm_t const * p_sm)
{
    return ((NAV_MODE_LINE_FOLLOWING == p_sm->mode) ||
            (NAV_MODE_HUMP_CROSSING == p_sm->mode) ||
            (NAV_MODE_LINE_SEARCH == p_sm->mode) ||
            (NAV_MODE_AT_JUNCTION == p_sm->mode) ||
            (NAV_MODE_AVOIDING == p_sm->mode));
}

/*** end of file ***/
