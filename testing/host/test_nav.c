/*
 *  test_nav.c
 *  Host tests for Navigation's statechart (nav_sm.c): each test drives
 *  the pure state machine with events and 5 ms ticks and checks the
 *  modes and the actions it asks the Navigation task to carry out.
 *  Results: testing/navigation/TEST_LOG.md.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "navigation/nav_sm.h"
#include "subsystem3/line_keep.h"
#include "subsystem3/line_track.h"
#include "unit.h"

static nav_sm_t   g_sm;
static line_err_t g_line;
static uint32_t   g_now_ms;
static bool       g_b_line_stalled;     /* the Line task stopped (G19) */

/* ---------------------------------------------------------------- */

static void
nav_line (bool b_seen, int32_t error_pm)
{
    /* Navigation reads only valid, line seen and the error (IF-02). */
    (void) line_err_set_sample(&g_line, error_pm, 0u, 0u, b_seen);
}

/* The Line task publishes a sample every 5 ms, which moves the record's
   sample count: the same reading again, or another failed read. */
static void
nav_line_publish (void)
{
    if (line_err_valid(&g_line))
    {
        (void) line_err_set_sample(&g_line, line_err_error_pm(&g_line),
                                   line_err_raw_l(&g_line),
                                   line_err_raw_r(&g_line),
                                   line_err_line_seen(&g_line));
    }
    else
    {
        line_err_set_invalid(&g_line);
    }
}

static void
nav_tick_full (uint32_t health, bool b_faulted, bool b_start)
{
    g_now_ms += CFG_PERIOD_NAV_MS;

    if (!g_b_line_stalled)
    {
        nav_line_publish();
    }

    nav_sm_clear_actions(&g_sm);
    nav_sm_tick(&g_sm, g_now_ms, &g_line, health, b_faulted, b_start);
}

static void
nav_tick (void)
{
    nav_tick_full(NAV_HEALTH_ALL, false, false);
}

/* Ticks for the given time, stopping early when the mode changes. */
static void
nav_run_ms (uint32_t duration_ms)
{
    nav_mode_t const mode = g_sm.mode;

    for (uint32_t ms = 0u; (ms < duration_ms) && (mode == g_sm.mode);
         ms += CFG_PERIOD_NAV_MS)
    {
        nav_tick();
    }
}

static nav_act_t const *
nav_find (nav_act_type_t type, uint8_t kind)
{
    nav_act_t const * p_found = NULL;

    for (uint32_t idx = 0u; (idx < g_sm.act_count) && (NULL == p_found);
         idx++)
    {
        nav_act_t const * const p_act = &g_sm.acts[idx];

        if (((uint8_t) type == p_act->type) &&
            ((0u == kind) || (kind == p_act->kind)))
        {
            p_found = p_act;
        }
    }

    return (p_found);
}

static void
nav_line_event (uint8_t type, char letter)
{
    line_evt_t evt;

    (void) line_evt_pack(&evt, type, letter, false);
    nav_sm_clear_actions(&g_sm);
    g_sm.now_ms = g_now_ms;
    nav_sm_line_event(&g_sm, &evt);
}

static void
nav_obstacle (uint8_t type, uint8_t side)
{
    obstacle_evt_t evt;

    if (OBST_EVT_PLAN == type)
    {
        (void) obstacle_evt_plan(&evt, side, 300, 160, 80, -80);
    }
    else
    {
        (void) obstacle_evt_simple(&evt, type);
    }

    nav_sm_clear_actions(&g_sm);
    g_sm.now_ms = g_now_ms;
    nav_sm_obstacle_event(&g_sm, &evt);
}

/* Motion reports the running move as finished. */
static void
nav_move_done (void)
{
    motion_sts_t sts;

    motion_sts_move_done(&sts, g_sm.pending_move_id);
    nav_sm_clear_actions(&g_sm);
    g_sm.now_ms = g_now_ms;
    nav_sm_motion_status(&g_sm, &sts);
}

static void
nav_odometry (int32_t distance_mm, int32_t heading_cdeg)
{
    motion_sts_t sts;

    (void) motion_sts_odometry(&sts, distance_mm, heading_cdeg);
    nav_sm_clear_actions(&g_sm);
    nav_sm_motion_status(&g_sm, &sts);
}

/* Power-up to LINE FOLLOWING: BOOT, SELF-TEST, READY, START. */
static void
nav_start (void)
{
    (void) memset(&g_sm, 0, sizeof(g_sm));
    g_now_ms         = 0u;
    g_b_line_stalled = false;
    nav_sm_init(&g_sm, g_now_ms);
    nav_line(true, 0);
    nav_tick();                                 /* BOOT -> SELF-TEST */
    nav_tick();                                 /* healthy -> READY */
    nav_tick_full(NAV_HEALTH_ALL, false, true); /* START */
}

/* ---------------------------------------------------------------- */

static void
test_boot (void)
{
    UNIT_BEGIN("NAV-U01", "BOOT -> SELF-TEST -> READY -> LINE FOLLOWING");
    (void) memset(&g_sm, 0, sizeof(g_sm));
    g_now_ms = 0u;
    nav_sm_init(&g_sm, 0u);
    nav_line(true, 0);
    UNIT_EQ(g_sm.mode, NAV_MODE_BOOT);

    nav_tick_full(0u, false, false);
    UNIT_EQ(g_sm.mode, NAV_MODE_SELF_TEST);
    UNIT_CHECK(NULL != nav_find(NAV_ACT_STOP, 0u));
    UNIT_CHECK(NULL != nav_find(NAV_ACT_SCAN, SCAN_REQ_RUN_OVER));

    nav_tick_full(NAV_HEALTH_LINE | NAV_HEALTH_IMU, false, true);
    UNIT_EQ(g_sm.mode, NAV_MODE_SELF_TEST);     /* scan not healthy yet */
    nav_tick_full(NAV_HEALTH_ALL, false, true); /* START ignored here */
    UNIT_EQ(g_sm.mode, NAV_MODE_READY);
    nav_tick();
    UNIT_EQ(g_sm.mode, NAV_MODE_READY);         /* waits for START */

    nav_tick_full(NAV_HEALTH_ALL, false, true);
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);
    UNIT_CHECK(NULL != nav_find(NAV_ACT_ROUTE, ROUTE_RUN_START));
    UNIT_CHECK(NULL != nav_find(NAV_ACT_SCAN, SCAN_REQ_START));
    nav_tick();                                 /* steering from now on */
    UNIT_CHECK(NULL != nav_find(NAV_ACT_DRIVE, 0u));
    UNIT_END();
}

static void
test_selftest_fail (void)
{
    uint32_t reports = 0u;

    UNIT_BEGIN("NAV-U02", "SELF-TEST failure reported once after 3 s");
    (void) memset(&g_sm, 0, sizeof(g_sm));
    g_now_ms = 0u;
    nav_sm_init(&g_sm, 0u);
    nav_tick_full(0u, false, false);

    for (uint32_t ms = 0u; ms < (2u * CFG_NAV_SELFTEST_MS); ms += 5u)
    {
        nav_tick_full(NAV_HEALTH_LINE, false, false);
        reports += (NULL != nav_find(NAV_ACT_SELF_TEST_FAIL, 0u)) ? 1u : 0u;
    }

    UNIT_EQ(reports, 1);
    UNIT_EQ(g_sm.mode, NAV_MODE_SELF_TEST);
    UNIT_END();
}

static void
test_follow (void)
{
    nav_act_t const * p_drive = NULL;

    UNIT_BEGIN("NAV-U03", "following steers towards the line (D-52), "
                          "commanded speed inside NFR17's range (D-71)");
    nav_start();

    nav_line(true, 600);                    /* line under the left sensor */
    nav_tick();
    p_drive = nav_find(NAV_ACT_DRIVE, 0u);
    UNIT_CHECK(NULL != p_drive);
    UNIT_CHECK((NULL != p_drive) && (p_drive->b_value > 0));
    UNIT_CHECK((NULL != p_drive) && (p_drive->a_value < CFG_NAV_CRUISE_MMPS));

    /* Line briefly not seen: the last error is kept. */
    nav_line(false, 0);
    nav_tick();
    p_drive = nav_find(NAV_ACT_DRIVE, 0u);
    UNIT_CHECK((NULL != p_drive) && (p_drive->b_value > 0));

    /* Full error either way: slower, but never below 0.15 m/s. */
    nav_line(true, 1000);
    nav_tick();
    p_drive = nav_find(NAV_ACT_DRIVE, 0u);
    UNIT_CHECK((NULL != p_drive) &&
               (p_drive->a_value >= CFG_NAV_SPEED_MIN_MMPS) &&
               (p_drive->a_value <= CFG_NAV_SPEED_MAX_MMPS));
    nav_line(true, -1000);
    nav_tick();
    p_drive = nav_find(NAV_ACT_DRIVE, 0u);
    UNIT_CHECK((NULL != p_drive) && (p_drive->b_value < 0) &&
               (p_drive->a_value >= CFG_NAV_SPEED_MIN_MMPS));
    UNIT_END();
}

static void
test_junction_turn (void)
{
    nav_act_t const * p_act = NULL;

    UNIT_BEGIN("NAV-U04", "SD1: letter B held, junction, axle, turn left");
    nav_start();

    nav_line_event(LINE_EVT_CAPTURE_STARTED, '\0');
    p_act = nav_find(NAV_ACT_HOLD_SPEED, 0u);
    UNIT_CHECK((NULL != p_act) && (1 == p_act->a_value));
    nav_line_event(LINE_EVT_LETTER, 'B');
    p_act = nav_find(NAV_ACT_HOLD_SPEED, 0u);
    UNIT_CHECK((NULL != p_act) && (0 == p_act->a_value));
    p_act = nav_find(NAV_ACT_ROUTE, ROUTE_BARCODE);
    UNIT_CHECK((NULL != p_act) && ('B' == p_act->letter) &&
               (LETTER_ACTION_LEFT == p_act->action));
    UNIT_CHECK(g_sm.b_command_held);

    nav_line_event(LINE_EVT_JUNCTION, '\0');
    UNIT_EQ(g_sm.mode, NAV_MODE_AT_JUNCTION);
    p_act = nav_find(NAV_ACT_FORWARD, 0u);
    UNIT_CHECK((NULL != p_act) &&
               (CFG_NAV_SENSOR_TO_AXLE_MM == p_act->a_value));

    nav_tick();
    UNIT_CHECK(NULL != nav_find(NAV_ACT_KEEPALIVE, 0u));

    nav_move_done();
    p_act = nav_find(NAV_ACT_TURN_LEFT, 0u);
    UNIT_CHECK((NULL != p_act) && (9000 == p_act->a_value));

    nav_move_done();
    p_act = nav_find(NAV_ACT_ROUTE, ROUTE_TURN);
    UNIT_CHECK((NULL != p_act) && (9000 == p_act->b_value));
    UNIT_CHECK(!g_sm.b_command_held);
    UNIT_EQ(g_sm.step, NAV_STEP_FIND_LINE);

    nav_line(true, 0);
    nav_tick();
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);
    UNIT_END();
}

static void
test_junction_other (void)
{
    nav_act_t const * p_act = NULL;

    UNIT_BEGIN("NAV-U05", "right, U-turn and straight at a junction");
    nav_start();
    nav_line_event(LINE_EVT_LETTER, 'C');
    nav_line_event(LINE_EVT_JUNCTION, '\0');
    nav_move_done();
    p_act = nav_find(NAV_ACT_TURN_RIGHT, 0u);
    UNIT_CHECK((NULL != p_act) && (9000 == p_act->a_value));

    nav_start();
    nav_line_event(LINE_EVT_LETTER, 'D');
    nav_line_event(LINE_EVT_JUNCTION, '\0');
    nav_move_done();
    p_act = nav_find((0 != CFG_NAV_UTURN_LEFT) ? NAV_ACT_TURN_LEFT
                                               : NAV_ACT_TURN_RIGHT, 0u);
    UNIT_CHECK((NULL != p_act) && (18000 == p_act->a_value));

    nav_start();
    nav_line_event(LINE_EVT_LETTER, 'A');
    nav_line_event(LINE_EVT_JUNCTION, '\0');
    nav_move_done();                            /* axle on the junction */
    UNIT_CHECK(NULL == nav_find(NAV_ACT_TURN_LEFT, 0u));
    UNIT_CHECK(NULL == nav_find(NAV_ACT_TURN_RIGHT, 0u));
    UNIT_CHECK(NULL != nav_find(NAV_ACT_ROUTE, ROUTE_TURN));
    UNIT_EQ(g_sm.step, NAV_STEP_FIND_LINE);
    UNIT_END();
}

static void
test_no_command (void)
{
    nav_act_t const * p_act = NULL;

    UNIT_BEGIN("NAV-U06", "junction without a command: SAFE HOLD (FR5)");
    nav_start();
    nav_line_event(LINE_EVT_LETTER, 'E');       /* no action: not held */
    p_act = nav_find(NAV_ACT_ROUTE, ROUTE_BARCODE);
    UNIT_CHECK((NULL != p_act) && (LETTER_ACTION_NONE == p_act->action));
    UNIT_CHECK(!g_sm.b_command_held);

    nav_line_event(LINE_EVT_JUNCTION, '\0');
    UNIT_EQ(g_sm.mode, NAV_MODE_SAFE_HOLD);
    UNIT_EQ(g_sm.hold_reason, NAV_HOLD_NO_COMMAND);
    UNIT_CHECK(NULL != nav_find(NAV_ACT_ROUTE, ROUTE_NO_COMMAND));
    UNIT_CHECK(NULL != nav_find(NAV_ACT_ROUTE, ROUTE_SAFE_HOLD));
    UNIT_CHECK(NULL != nav_find(NAV_ACT_STOP, 0u));

    /* RESET with no fault: READY again. */
    nav_sm_clear_actions(&g_sm);
    nav_sm_reset(&g_sm);
    UNIT_EQ(g_sm.mode, NAV_MODE_READY);
    UNIT_END();
}

static void
test_search_complete (void)
{
    uint32_t sweeps = 0u;

    UNIT_BEGIN("NAV-U07", "line lost: 5 s sweep search, then COMPLETE");
    nav_start();
    nav_line(true, -300);
    nav_tick();
    nav_line(false, 0);
    nav_line_event(LINE_EVT_LOST, '\0');
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_SEARCH);
    UNIT_CHECK(NULL != nav_find(NAV_ACT_ROUTE, ROUTE_LINE_SEARCH));

    nav_tick();
    UNIT_CHECK(NULL != nav_find(NAV_ACT_TURN_RIGHT, 0u));   /* last seen */

    for (uint32_t ms = 0u; (ms < 6000u) &&
         (NAV_MODE_LINE_SEARCH == g_sm.mode); ms += 5u)
    {
        if (0u == (ms % 500u))
        {
            nav_move_done();
            nav_tick();
            sweeps += (NULL != nav_find(NAV_ACT_TURN_LEFT, 0u)) ? 1u : 0u;
            sweeps += (NULL != nav_find(NAV_ACT_TURN_RIGHT, 0u)) ? 1u : 0u;
        }
        else
        {
            nav_tick();
        }
    }

    UNIT_EQ(g_sm.mode, NAV_MODE_COMPLETE);
    UNIT_CHECK(sweeps >= 3u);
    UNIT_CHECK(NULL != nav_find(NAV_ACT_SUMMARY, 0u));
    UNIT_CHECK(NULL != nav_find(NAV_ACT_ROUTE, ROUTE_COMPLETE));
    UNIT_CHECK(NULL != nav_find(NAV_ACT_LINE_RUN_OVER, 0u));
    UNIT_CHECK((g_now_ms - g_sm.mode_since_ms) < 10u);
    UNIT_END();
}

static void
test_search_found (void)
{
    UNIT_BEGIN("NAV-U08", "line found during the search: following again");
    nav_start();
    nav_line(false, 0);
    nav_line_event(LINE_EVT_LOST, '\0');
    nav_run_ms(1000u);
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_SEARCH);
    nav_line(true, 100);
    nav_tick();
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);
    UNIT_CHECK(NULL != nav_find(NAV_ACT_STOP, 0u));
    UNIT_END();
}

static void
test_find_line_timeout (void)
{
    UNIT_BEGIN("NAV-U09", "no line within 3 s of a turn: SAFE HOLD");
    nav_start();
    nav_line_event(LINE_EVT_LETTER, 'B');
    nav_line_event(LINE_EVT_JUNCTION, '\0');
    nav_move_done();
    nav_move_done();
    UNIT_EQ(g_sm.step, NAV_STEP_FIND_LINE);
    nav_line(false, 0);
    nav_run_ms(CFG_NAV_FIND_LINE_MS + 100u);
    UNIT_EQ(g_sm.mode, NAV_MODE_SAFE_HOLD);
    UNIT_EQ(g_sm.hold_reason, NAV_HOLD_LINE_NOT_FOUND);
    UNIT_END();
}

static void
test_events_ignored (void)
{
    imu_evt_t         hump;
    nav_act_t const * p_act = NULL;

    UNIT_BEGIN("NAV-U10", "barcode and junction events ignored while "
                          "turning; a read counts only if the car was "
                          "following from its start to its end (D-53)");
    nav_start();
    nav_line_event(LINE_EVT_LETTER, 'B');
    nav_line_event(LINE_EVT_JUNCTION, '\0');
    UNIT_EQ(g_sm.mode, NAV_MODE_AT_JUNCTION);

    nav_line_event(LINE_EVT_LETTER, 'C');
    UNIT_EQ(g_sm.act_count, 0);
    UNIT_EQ(g_sm.held_action, LETTER_ACTION_LEFT);
    nav_line_event(LINE_EVT_JUNCTION, '\0');
    UNIT_EQ(g_sm.act_count, 0);
    UNIT_EQ(g_sm.step, NAV_STEP_TO_AXLE);

    /* A read starts while turning; its letter arrives only after the
       line was found and following resumed: still ignored. */
    nav_line_event(LINE_EVT_CAPTURE_STARTED, '\0');
    UNIT_EQ(g_sm.act_count, 0);
    nav_move_done();                            /* axle on the junction */
    nav_move_done();                            /* turned: find the line */
    nav_line(true, 0);
    nav_tick();
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);
    nav_line_event(LINE_EVT_LETTER, 'D');
    UNIT_EQ(g_sm.act_count, 0);
    UNIT_CHECK(!g_sm.b_command_held);

    /* The next read counts again. */
    nav_line_event(LINE_EVT_CAPTURE_STARTED, '\0');
    UNIT_CHECK(g_sm.b_speed_held);
    nav_line_event(LINE_EVT_LETTER, 'C');
    UNIT_CHECK(g_sm.b_command_held);
    UNIT_EQ(g_sm.held_letter, 'C');

    /* A read counts only if the car was following from its start to its
       result; LINE FOLLOWING <-> HUMP CROSSING is still following. */
    nav_start();
    nav_line_event(LINE_EVT_CAPTURE_STARTED, '\0');
    imu_evt_hump_start(&hump);
    nav_sm_clear_actions(&g_sm);
    nav_sm_imu_event(&g_sm, &hump);
    UNIT_EQ(g_sm.mode, NAV_MODE_HUMP_CROSSING);
    UNIT_CHECK(g_sm.b_speed_held);
    nav_line_event(LINE_EVT_LETTER, 'B');
    UNIT_EQ(g_sm.held_letter, 'B');

    /* Line lost during a read: search, line found, following again; the
       read's letter is ignored and the next read counts. */
    nav_start();
    nav_line_event(LINE_EVT_CAPTURE_STARTED, '\0');
    nav_line_event(LINE_EVT_LOST, '\0');
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_SEARCH);
    UNIT_CHECK(!g_sm.b_speed_held);
    p_act = nav_find(NAV_ACT_HOLD_SPEED, 0u);
    UNIT_CHECK((NULL != p_act) && (0 == p_act->a_value));
    nav_line(true, 0);
    nav_tick();
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);
    nav_line_event(LINE_EVT_LETTER, 'B');
    UNIT_CHECK(!g_sm.b_command_held);
    nav_line_event(LINE_EVT_CAPTURE_STARTED, '\0');
    nav_line_event(LINE_EVT_LETTER, 'C');
    UNIT_EQ(g_sm.held_letter, 'C');

    /* A read still running at a junction: its result after the turn is
       ignored. */
    nav_start();
    nav_line_event(LINE_EVT_CAPTURE_STARTED, '\0');
    nav_line_event(LINE_EVT_LETTER, 'B');
    nav_line_event(LINE_EVT_CAPTURE_STARTED, '\0');
    nav_line_event(LINE_EVT_JUNCTION, '\0');
    UNIT_EQ(g_sm.mode, NAV_MODE_AT_JUNCTION);
    nav_move_done();                            /* axle on the junction */
    nav_move_done();                            /* turned: find the line */
    nav_line(true, 0);
    nav_tick();
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);
    UNIT_CHECK(!g_sm.b_command_held);
    nav_line_event(LINE_EVT_LETTER, 'C');
    UNIT_CHECK(!g_sm.b_command_held);

    /* A fault during a read, RESET, then a new run: the old read's
       letter is ignored. */
    nav_start();
    nav_line_event(LINE_EVT_CAPTURE_STARTED, '\0');
    nav_tick_full(NAV_HEALTH_ALL, true, false);
    UNIT_EQ(g_sm.mode, NAV_MODE_FAULT);
    nav_sm_clear_actions(&g_sm);
    nav_sm_restart(&g_sm);
    nav_tick();                                 /* SELF-TEST -> READY */
    nav_tick_full(NAV_HEALTH_ALL, false, true); /* START */
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);
    nav_line_event(LINE_EVT_LETTER, 'B');
    UNIT_CHECK(!g_sm.b_command_held);

    /* A read that starts in READY, before START: ignored to its end. */
    (void) memset(&g_sm, 0, sizeof(g_sm));
    g_now_ms = 0u;
    nav_sm_init(&g_sm, g_now_ms);
    nav_line(true, 0);
    nav_tick();                                 /* BOOT -> SELF-TEST */
    nav_tick();                                 /* healthy -> READY */
    UNIT_EQ(g_sm.mode, NAV_MODE_READY);
    nav_line_event(LINE_EVT_CAPTURE_STARTED, '\0');
    nav_tick_full(NAV_HEALTH_ALL, false, true); /* START */
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);
    nav_line_event(LINE_EVT_LETTER, 'B');
    UNIT_CHECK(!g_sm.b_command_held);
    nav_line_event(LINE_EVT_CAPTURE_STARTED, '\0');
    nav_line_event(LINE_EVT_LETTER, 'C');
    UNIT_EQ(g_sm.held_letter, 'C');
    UNIT_END();
}

static void
test_new_letter (void)
{
    UNIT_BEGIN("NAV-U11", "a new letter replaces the held one (D-35); a "
                          "failed read keeps it");
    nav_start();
    nav_line_event(LINE_EVT_LETTER, 'B');
    nav_line_event(LINE_EVT_LETTER, 'C');
    UNIT_EQ(g_sm.held_letter, (0 != CFG_NAV_NEW_LETTER_REPLACES) ? 'C' : 'B');
    nav_line_event(LINE_EVT_CAPTURE_STARTED, '\0');
    nav_line_event(LINE_EVT_CAPTURE_FAILED, '\0');
    UNIT_CHECK(g_sm.b_command_held);
    UNIT_CHECK(!g_sm.b_speed_held);
    UNIT_END();
}

static void
test_remap (void)
{
    nav_cmd_t         cmd = { 0 };
    nav_act_t const * p_act = NULL;

    UNIT_BEGIN("NAV-U12", "remap and default map acknowledged (FR6, NFR8)");
    nav_start();

    UNIT_CHECK(nav_cmd_pack(&cmd, NAV_CMD_REMAP, 'E',
                            (uint8_t) LETTER_ACTION_RIGHT, 42u));
    nav_sm_clear_actions(&g_sm);
    nav_sm_command(&g_sm, &cmd);
    p_act = nav_find(NAV_ACT_ACK, CMD_CODE_REMAP);
    UNIT_CHECK((NULL != p_act) && (42 == p_act->a_value) &&
               (1 == p_act->b_value));
    nav_line_event(LINE_EVT_LETTER, 'E');
    UNIT_EQ(g_sm.held_action, LETTER_ACTION_RIGHT);

    UNIT_CHECK(!nav_cmd_pack(&cmd, NAV_CMD_REMAP, '7',  /* not a letter */
                             (uint8_t) LETTER_ACTION_RIGHT, 42u));
    nav_sm_clear_actions(&g_sm);
    nav_sm_command(&g_sm, &cmd);
    p_act = nav_find(NAV_ACT_ACK, CMD_CODE_REMAP);
    UNIT_CHECK((NULL != p_act) && (0 == p_act->b_value));

    UNIT_CHECK(nav_cmd_pack(&cmd, NAV_CMD_MAP_DEFAULT, '\0',
                            (uint8_t) LETTER_ACTION_NONE, 42u));
    nav_sm_clear_actions(&g_sm);
    nav_sm_command(&g_sm, &cmd);
    UNIT_CHECK(NULL != nav_find(NAV_ACT_ACK, CMD_CODE_MAP_DEFAULT));
    UNIT_EQ(letter_map_get(&g_sm.map, 'E'), LETTER_ACTION_NONE);
    UNIT_END();
}

static void
test_fault (void)
{
    UNIT_BEGIN("NAV-U13", "fault from any running mode; restart re-tests");
    nav_start();
    nav_line_event(LINE_EVT_CAPTURE_STARTED, '\0');
    nav_tick_full(NAV_HEALTH_ALL, true, false);
    UNIT_EQ(g_sm.mode, NAV_MODE_FAULT);
    UNIT_CHECK(NULL != nav_find(NAV_ACT_ROUTE, ROUTE_FAULT));
    UNIT_CHECK(NULL == nav_find(NAV_ACT_DRIVE, 0u));
    UNIT_CHECK(!g_sm.b_speed_held);

    nav_tick_full(NAV_HEALTH_ALL, true, true);  /* START does nothing */
    UNIT_EQ(g_sm.mode, NAV_MODE_FAULT);

    nav_sm_clear_actions(&g_sm);
    nav_sm_restart(&g_sm);
    UNIT_EQ(g_sm.mode, NAV_MODE_SELF_TEST);
    nav_tick();
    UNIT_EQ(g_sm.mode, NAV_MODE_READY);
    UNIT_END();
}

static void
test_hump (void)
{
    imu_evt_t         evt;
    nav_act_t const * p_act = NULL;

    UNIT_BEGIN("NAV-U14", "hump: slower, levels frozen, height logged");
    nav_start();
    imu_evt_hump_start(&evt);
    nav_sm_clear_actions(&g_sm);
    nav_sm_imu_event(&g_sm, &evt);
    UNIT_EQ(g_sm.mode, NAV_MODE_HUMP_CROSSING);
    p_act = nav_find(NAV_ACT_LINE_ADAPT, 0u);
    UNIT_CHECK((0 == CFG_NAV_HUMP_FREEZE_LEVELS) ? (NULL == p_act)
               : ((NULL != p_act) && (0 == p_act->a_value)));

    nav_line(true, 0);
    nav_tick();
    p_act = nav_find(NAV_ACT_DRIVE, 0u);
    UNIT_CHECK((NULL != p_act) && (CFG_NAV_HUMP_MMPS == p_act->a_value));

    /* A full error on the hump: the slow-down stops at 0.15 m/s (D-71). */
    nav_line(true, 1000);
    nav_tick();
    p_act = nav_find(NAV_ACT_DRIVE, 0u);
    UNIT_CHECK((NULL != p_act) &&
               (p_act->a_value >= CFG_NAV_SPEED_MIN_MMPS));
    nav_line(true, 0);

    (void) imu_evt_hump_end(&evt, 1000, 23);
    nav_sm_clear_actions(&g_sm);
    nav_sm_imu_event(&g_sm, &evt);
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);
    p_act = nav_find(NAV_ACT_ROUTE, ROUTE_HUMP);
    UNIT_CHECK((NULL != p_act) && (23 == p_act->b_value));
    p_act = nav_find(NAV_ACT_LINE_ADAPT, 0u);
    UNIT_CHECK((NULL != p_act) && (1 == p_act->a_value));
    UNIT_END();
}

/* The extra travel after "behind" (D-97), in floating point: the
   setting plus the side scan's look-ahead, (gap + half the car) x
   tan(degrees ahead of square) for the servo angle it looks at. */
static int32_t
test_past_extra_mm (int32_t servo_deg)
{
    double const ahead = (90.0 - fabs((double) servo_deg - 90.0)) *
                         (3.14159265358979323846 / 180.0);
    double const dist  = (double) (CFG_NAV_PASS_GAP_MM +
                                   (CFG_CAR_WIDTH_MM / 2));

    return (CFG_NAV_PAST_EXTRA_MM + (int32_t) ((dist * tan(ahead)) + 0.5));
}

/* From a stop for an obstacle to PASSING beside it on the given side. */
static void
nav_to_passing (uint8_t side)
{
    nav_start();
    nav_obstacle(OBST_EVT_AHEAD, OBST_SIDE_NONE);
    nav_run_ms(CFG_NAV_STOP_SETTLE_MS + 10u);
    nav_obstacle(OBST_EVT_PLAN, side);
    nav_move_done();                            /* turned out */
    nav_move_done();                            /* sideways leg */
    nav_move_done();                            /* turned parallel */
}

static void
test_obstacle_bypass (void)
{
    nav_act_t const * p_act = NULL;

    UNIT_BEGIN("NAV-U15", "obstacle: stop, scan, go round on the left, "
                          "rejoin (FR9-FR11); after the side scan's "
                          "'behind' the extra travel includes its "
                          "look-ahead, on either side (D-97)");
    nav_start();
    nav_obstacle(OBST_EVT_AHEAD, OBST_SIDE_NONE);
    UNIT_EQ(g_sm.mode, NAV_MODE_AVOIDING);
    UNIT_CHECK(NULL != nav_find(NAV_ACT_STOP, 0u));

    nav_run_ms(CFG_NAV_STOP_SETTLE_MS + 10u);
    UNIT_EQ(g_sm.step, NAV_STEP_SCANNING);

    nav_obstacle(OBST_EVT_PLAN, OBST_SIDE_LEFT);
    UNIT_CHECK(NULL != nav_find(NAV_ACT_ROUTE, ROUTE_OBSTACLE));
    UNIT_CHECK(NULL != nav_find(NAV_ACT_TURN_LEFT, 0u));
    nav_move_done();
    p_act = nav_find(NAV_ACT_FORWARD, 0u);      /* sideways leg */
    UNIT_CHECK((NULL != p_act) &&
               ((((80 > ((300 * CFG_NAV_BEAM_SPREAD_PM) / 1000))
                  ? (80 - ((300 * CFG_NAV_BEAM_SPREAD_PM) / 1000)) : 0) +
                 CFG_NAV_PASS_GAP_MM + (CFG_CAR_WIDTH_MM / 2)) ==
                p_act->a_value));
    nav_move_done();
    UNIT_CHECK(NULL != nav_find(NAV_ACT_TURN_RIGHT, 0u));   /* parallel */
    nav_move_done();
    UNIT_CHECK(NULL != nav_find(NAV_ACT_SCAN, SCAN_REQ_BYPASS));
    UNIT_EQ(g_sm.step, NAV_STEP_PASSING);
    p_act = nav_find(NAV_ACT_FORWARD, 0u);      /* range + depth + car + margin */
    UNIT_CHECK((NULL != p_act) &&
               ((300 + 160 + CFG_CAR_LENGTH_MM + CFG_NAV_PASS_MARGIN_MM) ==
                p_act->a_value));

    nav_odometry(g_sm.distance_mm + 400, g_sm.odo_heading_cdeg);  /* passed it */
    nav_obstacle(OBST_EVT_BEHIND, OBST_SIDE_NONE);
    p_act = nav_find(NAV_ACT_FORWARD, 0u);      /* passing left: looks right */
    UNIT_CHECK((NULL != p_act) &&
               (abs(test_past_extra_mm(CFG_SCAN_SIDE_RIGHT_DEG) -
                    p_act->a_value) <= 1));
    nav_move_done();
    UNIT_CHECK(NULL != nav_find(NAV_ACT_TURN_RIGHT, 0u));   /* back in */
    nav_move_done();
    UNIT_CHECK(NULL != nav_find(NAV_ACT_SCAN, SCAN_REQ_REJOIN));
    UNIT_EQ(g_sm.step, NAV_STEP_REJOINING);

    nav_line(true, 0);
    nav_tick();
    UNIT_EQ(g_sm.step, NAV_STEP_ALIGN_MOVE);
    nav_move_done();
    p_act = nav_find(NAV_ACT_TURN_LEFT, 0u);                /* along it */
    UNIT_CHECK((NULL != p_act) && (CFG_NAV_ALIGN_MAX_CDEG == p_act->a_value));
    /* Line seen after too small a turn: keep turning. */
    g_sm.fused_heading_cdeg = g_sm.turn_start_cdeg + 2000;
    nav_line(true, 0);
    nav_tick();
    UNIT_EQ(g_sm.step, NAV_STEP_ALIGN_TURN);
    /* Turned enough and the line is seen: stop and follow. */
    g_sm.fused_heading_cdeg = g_sm.turn_start_cdeg + CFG_NAV_ALIGN_MIN_CDEG;
    nav_tick();
    UNIT_CHECK(NULL != nav_find(NAV_ACT_STOP, 0u));
    UNIT_CHECK(NULL != nav_find(NAV_ACT_SCAN, SCAN_REQ_LINE_FOUND));
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);

    /* Rejoin blocked: the obstacle is still ahead on the way back. */
    nav_to_passing(OBST_SIDE_LEFT);
    nav_odometry(g_sm.distance_mm + 400, g_sm.odo_heading_cdeg);  /* passed it */
    nav_obstacle(OBST_EVT_BEHIND, OBST_SIDE_NONE);
    nav_move_done();                                        /* past extra */
    nav_move_done();                                        /* turned in */
    UNIT_EQ(g_sm.step, NAV_STEP_REJOINING);
    nav_obstacle(OBST_EVT_AHEAD, OBST_SIDE_NONE);
    UNIT_CHECK(NULL != nav_find(NAV_ACT_STOP, 0u));
    UNIT_CHECK(NULL != nav_find(NAV_ACT_TURN_LEFT, 0u));    /* parallel */
    UNIT_EQ(g_sm.step, NAV_STEP_CLEAR_TURN);
    nav_move_done();
    p_act = nav_find(NAV_ACT_FORWARD, 0u);
    UNIT_CHECK((NULL != p_act) && (CFG_NAV_REJOIN_MORE_MM == p_act->a_value));
    nav_move_done();
    UNIT_CHECK(NULL != nav_find(NAV_ACT_TURN_RIGHT, 0u));   /* in again */
    nav_move_done();
    UNIT_EQ(g_sm.step, NAV_STEP_REJOINING);
    for (uint32_t tries = 1u; tries < CFG_NAV_REJOIN_RETRIES; tries++)
    {
        nav_obstacle(OBST_EVT_AHEAD, OBST_SIDE_NONE);
        nav_move_done();
        nav_move_done();
        nav_move_done();
    }
    UNIT_EQ(g_sm.step, NAV_STEP_REJOINING);
    nav_obstacle(OBST_EVT_AHEAD, OBST_SIDE_NONE);           /* one too many */
    UNIT_EQ(g_sm.mode, NAV_MODE_SAFE_HOLD);

    /* Side scan never reports it behind: past by distance, turn in. */
    nav_to_passing(OBST_SIDE_LEFT);
    nav_move_done();                                        /* leg done */
    UNIT_CHECK(NULL != nav_find(NAV_ACT_TURN_RIGHT, 0u));   /* back in */
    UNIT_EQ(g_sm.step, NAV_STEP_TURN_IN);
    nav_move_done();
    UNIT_CHECK(NULL != nav_find(NAV_ACT_SCAN, SCAN_REQ_REJOIN));
    UNIT_EQ(g_sm.step, NAV_STEP_REJOINING);

    /* Passing on the right: the scan looks left. */
    nav_to_passing(OBST_SIDE_RIGHT);
    UNIT_EQ(g_sm.step, NAV_STEP_PASSING);
    nav_odometry(g_sm.distance_mm + 400, g_sm.odo_heading_cdeg);  /* passed it */
    nav_obstacle(OBST_EVT_BEHIND, OBST_SIDE_NONE);
    p_act = nav_find(NAV_ACT_FORWARD, 0u);
    UNIT_CHECK((NULL != p_act) &&
               (abs(test_past_extra_mm(CFG_SCAN_SIDE_LEFT_DEG) -
                    p_act->a_value) <= 1));
    UNIT_END();
}

static void
test_obstacle_no_room (void)
{
    nav_act_t const * p_act = NULL;

    UNIT_BEGIN("NAV-U16", "no room: reverse 10 cm once, then SAFE HOLD");
    nav_start();
    nav_obstacle(OBST_EVT_AHEAD, OBST_SIDE_NONE);
    nav_run_ms(CFG_NAV_STOP_SETTLE_MS + 10u);
    nav_obstacle(OBST_EVT_NO_ROOM, OBST_SIDE_NONE);
    p_act = nav_find(NAV_ACT_BACKWARD, 0u);
    UNIT_CHECK((NULL != p_act) && (CFG_NAV_REVERSE_MM == p_act->a_value));
    nav_move_done();
    UNIT_CHECK(NULL != nav_find(NAV_ACT_SCAN, SCAN_REQ_REVERSED));
    nav_obstacle(OBST_EVT_NO_ROOM, OBST_SIDE_NONE);
#if (0 != CFG_NAV_NO_ROOM_FALLBACK)
    /* Fallback: round on the default side, a fixed leg out. */
    UNIT_EQ(g_sm.mode, NAV_MODE_AVOIDING);
    UNIT_EQ(g_sm.step, NAV_STEP_TURN_OUT);
    UNIT_CHECK(NULL != nav_find((0 != CFG_NAV_FALLBACK_LEFT)
                                ? NAV_ACT_TURN_LEFT : NAV_ACT_TURN_RIGHT, 0u));
    nav_move_done();
    p_act = nav_find(NAV_ACT_FORWARD, 0u);
    UNIT_CHECK((NULL != p_act) &&
               (CFG_NAV_FALLBACK_LATERAL_MM == p_act->a_value));
#else
    UNIT_EQ(g_sm.mode, NAV_MODE_SAFE_HOLD);
    UNIT_EQ(g_sm.hold_reason, NAV_HOLD_NO_SAFE_ROUTE);
#endif

    /* What the scan really sends after reversing once is NO SAFE ROUTE
       (scan_plan / scan_unconfirmed): with the fallback on, that goes
       round too; a NO SAFE ROUTE outside the scan step still holds. */
    nav_start();
    nav_obstacle(OBST_EVT_AHEAD, OBST_SIDE_NONE);
    nav_run_ms(CFG_NAV_STOP_SETTLE_MS + 10u);
    nav_obstacle(OBST_EVT_NO_ROOM, OBST_SIDE_NONE);
    nav_move_done();
    nav_obstacle(OBST_EVT_NO_SAFE_ROUTE, OBST_SIDE_NONE);
#if (0 != CFG_NAV_NO_ROOM_FALLBACK)
    UNIT_EQ(g_sm.mode, NAV_MODE_AVOIDING);
    UNIT_EQ(g_sm.step, NAV_STEP_TURN_OUT);
    UNIT_CHECK(NULL != nav_find(NAV_ACT_ROUTE, ROUTE_OBSTACLE));
#else
    UNIT_EQ(g_sm.mode, NAV_MODE_SAFE_HOLD);
#endif
    nav_start();
    nav_obstacle(OBST_EVT_NO_SAFE_ROUTE, OBST_SIDE_NONE);   /* following */
    UNIT_EQ(g_sm.mode, NAV_MODE_SAFE_HOLD);

    /* Path clear after the scan: back to following. */
    nav_start();
    nav_obstacle(OBST_EVT_AHEAD, OBST_SIDE_NONE);
    nav_run_ms(CFG_NAV_STOP_SETTLE_MS + 10u);
    nav_obstacle(OBST_EVT_PATH_CLEAR, OBST_SIDE_NONE);
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);
    UNIT_END();
}

static void
test_keepalive (void)
{
    uint32_t keepalives = 0u;

    UNIT_BEGIN("NAV-U17", "a running move is kept alive every tick "
                          "(report 6.2)");
    nav_start();
    nav_line_event(LINE_EVT_LETTER, 'B');
    nav_line_event(LINE_EVT_JUNCTION, '\0');

    for (uint32_t idx = 0u; idx < 40u; idx++)
    {
        nav_tick();
        keepalives += (NULL != nav_find(NAV_ACT_KEEPALIVE, 0u)) ? 1u : 0u;
    }

    UNIT_EQ(keepalives, 40);
    nav_move_done();
    nav_move_done();
    nav_line(true, 0);
    nav_tick();
    UNIT_CHECK(NULL == nav_find(NAV_ACT_KEEPALIVE, 0u));
    UNIT_END();
}

static void
test_heading (void)
{
    imu_evt_t evt;

    UNIT_BEGIN("NAV-U18", "fused heading: odometry, nudged by the compass");
    nav_start();
    nav_odometry(0, 1000);
    UNIT_EQ(g_sm.fused_heading_cdeg, 1000);     /* starts from odometry */
    nav_odometry(10, 1500);
    UNIT_EQ(g_sm.fused_heading_cdeg, 1500);

    (void) imu_evt_heading(&evt, 2500, true);
    nav_sm_clear_actions(&g_sm);
    nav_sm_imu_event(&g_sm, &evt);
    UNIT_EQ(g_sm.fused_heading_cdeg,
            1500 + ((1000 * CFG_NAV_COMPASS_WEIGHT_PM) / 1000));

    /* Across +/-180 degrees the shorter way is taken. */
    nav_start();
    nav_odometry(0, 17900);
    (void) imu_evt_heading(&evt, -17900, true);
    nav_sm_clear_actions(&g_sm);
    nav_sm_imu_event(&g_sm, &evt);
    UNIT_CHECK(g_sm.fused_heading_cdeg >= 17900);
    UNIT_END();
}

static void
test_actions_bounded (void)
{
    UNIT_BEGIN("NAV-U19", "the action list never overflows in one step");

    /* The busiest steps: SAFE HOLD while the speed is held ... */
    nav_start();
    nav_line_event(LINE_EVT_CAPTURE_STARTED, '\0');
    nav_line_event(LINE_EVT_LETTER, 'E');
    nav_line_event(LINE_EVT_CAPTURE_STARTED, '\0');
    nav_line_event(LINE_EVT_JUNCTION, '\0');
    UNIT_EQ(g_sm.mode, NAV_MODE_SAFE_HOLD);
    UNIT_EQ(g_sm.act_dropped, 0);

    /* ... the end of a search, and a fault on a hump. */
    nav_start();
    nav_line(false, 0);
    nav_line_event(LINE_EVT_LOST, '\0');
    nav_run_ms(CFG_NAV_SEARCH_MS + 100u);
    UNIT_EQ(g_sm.mode, NAV_MODE_COMPLETE);
    UNIT_EQ(g_sm.act_dropped, 0);

    nav_start();
    {
        imu_evt_t evt;

        imu_evt_hump_start(&evt);
        nav_sm_imu_event(&g_sm, &evt);
    }
    nav_tick_full(NAV_HEALTH_ALL, true, false);
    UNIT_EQ(g_sm.mode, NAV_MODE_FAULT);
    UNIT_EQ(g_sm.act_dropped, 0);
    UNIT_END();
}

/* ---- A line record that stops changing (G19, D-94) ---- */

/* How many actions of a type the last step asked for. */
static uint32_t
nav_count (nav_act_type_t type)
{
    uint32_t count = 0u;

    for (uint32_t idx = 0u; idx < g_sm.act_count; idx++)
    {
        count += ((uint8_t) type == g_sm.acts[idx].type) ? 1u : 0u;
    }

    return (count);
}

/* Ticks with the Line task stopped; returns the stale-line reports. */
static uint32_t
nav_stalled_ticks (uint32_t ticks)
{
    uint32_t reports = 0u;

    g_b_line_stalled = true;

    for (uint32_t tick = 0u; tick < ticks; tick++)
    {
        nav_tick();
        reports += nav_count(NAV_ACT_LINE_STALE);
    }

    return (reports);
}

/* One tick gap_ms (at least 1) after the last, with the Line task
   stopped; returns the stale-line reports. */
static uint32_t
nav_stalled_tick_after (uint32_t gap_ms)
{
    g_b_line_stalled = true;
    g_now_ms         = (g_now_ms + gap_ms) - CFG_PERIOD_NAV_MS;
    nav_tick();                         /* adds one period */

    return (nav_count(NAV_ACT_LINE_STALE));
}

/* A new sample, seen by the next tick. */
static void
nav_fresh_sample (void)
{
    g_b_line_stalled = false;
    nav_line(true, 300);
    nav_tick();
}

static void
test_line_stale (void)
{
    nav_act_t const * p_act   = NULL;
    uint32_t          reports = 0u;
    uint32_t const    limit_ticks = CFG_NAV_LINE_STALE_MS / CFG_PERIOD_NAV_MS;

    UNIT_BEGIN("NAV-U40", "a line record whose sample count has not moved "
                          "for 20 ms while running is no longer used and "
                          "a LINE_DATA fault is reported once, at the "
                          "first tick 20 ms on even when ticks come late; "
                          "a moving count, failed reads included, or "
                          "ticks close together never trip it; not "
                          "checked outside a run (D-94)");

    /* Following on a live record, then on failed reads (their count
       still moves; D-79 handles them): no report over 2 s. */
    nav_start();
    nav_line(true, 300);

    for (uint32_t tick = 0u; tick < 200u; tick++)
    {
        nav_tick();
        reports += nav_count(NAV_ACT_LINE_STALE);
    }

    line_err_set_invalid(&g_line);

    for (uint32_t tick = 0u; tick < 200u; tick++)
    {
        nav_tick();
        reports += nav_count(NAV_ACT_LINE_STALE);
    }

    UNIT_EQ(reports, 0u);
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);

    /* The Line task stops on a "line seen" sample: ticks 5, 10 and
       15 ms on still use it, the one 20 ms on reports the fault, once,
       with the time as its detail. */
    nav_fresh_sample();
    UNIT_EQ(nav_stalled_ticks(limit_ticks - 1u), 0u);
    UNIT_EQ(nav_stalled_ticks(1u), 1u);
    p_act = nav_find(NAV_ACT_LINE_STALE, 0u);
    UNIT_CHECK((NULL != p_act) && (CFG_NAV_LINE_STALE_MS == p_act->a_value));
    UNIT_EQ(nav_stalled_ticks(100u), 0u);

    /* The old "line seen" no longer ends a line search. */
    nav_line_event(LINE_EVT_LOST, '\0');
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_SEARCH);
    UNIT_EQ(nav_stalled_ticks(20u), 0u);
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_SEARCH);

    /* A new sample: used at once, and the check starts again. */
    g_b_line_stalled = false;
    nav_line(true, 0);
    nav_tick();
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);
    UNIT_EQ(nav_stalled_ticks(limit_ticks), 1u);

    /* Navigation held up: the first tick after the count stopped comes
       25 ms later.  Reported at that tick, not three ticks later. */
    nav_fresh_sample();
    UNIT_EQ(nav_stalled_tick_after(25u), 1u);
    p_act = nav_find(NAV_ACT_LINE_STALE, 0u);
    UNIT_CHECK((NULL != p_act) && (25 == p_act->a_value));

    /* Ticks close together (a late wake-up, then the next period):
       four ticks within 8 ms without a new sample are not 20 ms. */
    nav_fresh_sample();
    UNIT_EQ(nav_stalled_tick_after(5u), 0u);
    UNIT_EQ(nav_stalled_tick_after(1u), 0u);
    UNIT_EQ(nav_stalled_tick_after(1u), 0u);
    UNIT_EQ(nav_stalled_tick_after(1u), 0u);
    nav_fresh_sample();
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);

    /* Outside a run (READY) a still record is not checked. */
    nav_start();
    g_sm.mode = NAV_MODE_READY;
    UNIT_EQ(nav_stalled_ticks(100u), 0u);
    UNIT_END();
}

/* ---- Line read failures across RESET (D-96): NAV-U41 ---- */

/* The Line task's work per 5 ms sample that matters here (line_task.c):
   the record, D-79's failed-read count, the published health and RUN
   OVER (idle), which follows Navigation's own actions. */
static line_track_t g_model_track;
static line_keep_t  g_model_keep;
static bool         g_b_model_health;   /* NAV_FLG_LINE_OK as published */
static bool         g_b_model_run_over; /* RUN OVER: sensing idle */
static bool         g_b_model_seen;     /* good reads see the line */
static bool         g_b_adc_fails;
static uint32_t     g_model_samples;
static uint32_t     g_model_reads;      /* ADC reads, good or failed */
static uint32_t     g_model_reports;    /* LINE_DATA reports (D-79) */

static void
nav_model_init (void)
{
    (void) memset(&g_sm, 0, sizeof(g_sm));
    g_now_ms         = 0u;
    g_b_line_stalled = true;            /* the model writes the record */
    line_track_init(&g_model_track);
    line_keep_init(&g_model_keep);
    g_b_model_health   = false;
    g_b_model_run_over = false;
    g_b_model_seen     = true;
    g_b_adc_fails      = false;
    g_model_samples    = 0u;
    g_model_reads      = 0u;
    g_model_reports    = 0u;
    nav_sm_init(&g_sm, g_now_ms);
}

/* Carry out the actions the Line task acts on (nav_task.c). */
static void
nav_model_actions (void)
{
    for (uint32_t idx = 0u; idx < g_sm.act_count; idx++)
    {
        if ((uint8_t) NAV_ACT_LINE_RUN_OVER == g_sm.acts[idx].type)
        {
            g_b_model_run_over = (0 != g_sm.acts[idx].a_value);
        }
    }
}

/* Safety released the latch: Navigation restarts into SELF-TEST. */
static void
nav_model_restart (void)
{
    nav_sm_clear_actions(&g_sm);
    nav_sm_restart(&g_sm);
    nav_model_actions();
}

static void
nav_model_line_sample (void)
{
    bool b_good = false;                /* a good sample for the window */

    if (g_b_model_run_over)
    {
        /* Idle: no reading; the count starts again (D-96) and the
           cycle is not a good sample (D-98). */
        (void) line_track_read_result(&g_model_track, true);
    }
    else if (g_b_adc_fails)
    {
        g_model_reads++;
        line_err_set_invalid(&g_line);

        if (line_track_read_result(&g_model_track, false))
        {
            g_model_reports++;
        }
    }
    else
    {
        g_model_reads++;
        b_good = true;
        (void) line_track_read_result(&g_model_track, true);
        (void) line_err_set_sample(&g_line, 0, 2000u, 2000u,
                                   g_b_model_seen);
    }

    if (line_keep_note_sample(&g_model_keep, b_good))
    {
        g_b_model_health = false;                   /* at once (D-96) */
    }

    if (line_keep_next_cycle(&g_model_keep))
    {
        g_b_model_health = line_keep_window_ok(&g_model_keep);
        line_keep_set_window_ok(&g_model_keep, true);
    }

    g_model_samples++;
}

/* One 5 ms step: a Line sample, then a Navigation tick that sees the
   published health; b_faulted is Safety's latch. */
static void
nav_model_tick (bool b_faulted, bool b_start)
{
    uint32_t const others = NAV_HEALTH_ALL & ~((uint32_t) NAV_HEALTH_LINE);

    nav_model_line_sample();
    nav_tick_full(others | (g_b_model_health ? NAV_HEALTH_LINE : 0u),
                  b_faulted, b_start);
    nav_model_actions();
}

/* Steps until the mode is reached (or max_ticks pass); the steps. */
static uint32_t
nav_model_until (nav_mode_t mode, bool b_faulted, uint32_t max_ticks)
{
    uint32_t ticks = 0u;

    while ((mode != g_sm.mode) && (ticks < max_ticks))
    {
        nav_model_tick(b_faulted, false);
        ticks++;
    }

    return (ticks);
}

static void
test_line_fail_recovery (void)
{
    uint32_t const window   = LINE_STATUS_EVERY;
    uint32_t const selftest = CFG_NAV_SELFTEST_MS / CFG_PERIOD_NAV_MS;
    uint32_t       ticks    = 0u;
    uint32_t       fails    = 0u;

    UNIT_BEGIN("NAV-U41", "line reads failing across RESET: SELF-TEST "
                          "never passes on the health from before the "
                          "failure, however soon RESET comes; after the "
                          "reads recover, a whole good window lets it "
                          "pass, and a new run of failures in READY is "
                          "reported again and blocks START (D-79, D-96)");

    /* Power-up on good readings: READY after the first whole window. */
    nav_model_init();
    ticks = nav_model_until(NAV_MODE_READY, false, 2u * window);
    UNIT_EQ(g_sm.mode, NAV_MODE_READY);
    UNIT_CHECK(ticks >= window);
    nav_model_tick(false, true);                    /* START */
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);

    /* Following until a window has just been published, then the ADC
       fails for good: health drops at the first failure, the fourth
       is reported once (D-79) and Safety latches. */
    while (0u != (g_model_samples % window))
    {
        nav_model_tick(false, false);
    }

    g_b_adc_fails = true;
    nav_model_tick(false, false);
    UNIT_CHECK(!g_b_model_health);

    while ((0u == g_model_reports) && (fails < 10u))
    {
        nav_model_tick(false, false);
        fails++;
    }

    UNIT_EQ(g_model_reports, 1u);
    nav_model_tick(true, false);
    UNIT_EQ(g_sm.mode, NAV_MODE_FAULT);

    /* RESET at once, long before the window's end (Motion acknowledged,
       latch released): SELF-TEST waits, and fails at its deadline; the
       count does not report again. */
    nav_model_restart();
    UNIT_EQ(g_sm.mode, NAV_MODE_SELF_TEST);
    fails = 0u;

    for (uint32_t tick = 0u; tick <= selftest; tick++)
    {
        nav_model_tick(false, false);
        fails += (NULL != nav_find(NAV_ACT_SELF_TEST_FAIL, 0u)) ? 1u : 0u;
        UNIT_CHECK(NAV_MODE_SELF_TEST == g_sm.mode);
    }

    UNIT_EQ(fails, 1u);
    UNIT_EQ(g_model_reports, 1u);

    /* Safety latches the SELF-TEST fault.  The reads recover; RESET:
       READY only after a whole good window. */
    nav_model_tick(true, false);
    UNIT_EQ(g_sm.mode, NAV_MODE_FAULT);
    g_b_adc_fails = false;
    nav_model_restart();
    ticks = nav_model_until(NAV_MODE_READY, false, 3u * window);
    UNIT_EQ(g_sm.mode, NAV_MODE_READY);
    UNIT_CHECK(ticks >= window);

    /* Failing again in READY: reported again after four failures (the
       good window cleared the count), so START is refused. */
    g_b_adc_fails = true;

    for (uint32_t tick = 0u; tick < (uint32_t) CFG_LINE_READ_FAIL_LIMIT;
         tick++)
    {
        nav_model_tick(false, false);
    }

    UNIT_EQ(g_model_reports, 2u);
    nav_model_tick(true, true);                     /* START, latched */
    UNIT_EQ(g_sm.mode, NAV_MODE_FAULT);
    g_b_line_stalled = false;
    UNIT_END();
}

/* ---- SELF-TEST after COMPLETE (D-98): NAV-U42 ---- */

/* Following with the model until the line is lost for good: COMPLETE,
   the Line task idle (RUN OVER). */
static void
nav_model_to_complete (void)
{
    uint32_t const search = CFG_NAV_SEARCH_MS / CFG_PERIOD_NAV_MS;

    g_b_model_seen = false;
    nav_line_event(LINE_EVT_LOST, '\0');
    nav_model_actions();
    (void) nav_model_until(NAV_MODE_COMPLETE, false, 2u * search);
}

static void
test_line_after_complete (void)
{
    uint32_t const window   = LINE_STATUS_EVERY;
    uint32_t const selftest = CFG_NAV_SELFTEST_MS / CFG_PERIOD_NAV_MS;
    uint32_t       reads    = 0u;
    uint32_t       ticks    = 0u;
    uint32_t       fails    = 0u;

    UNIT_BEGIN("NAV-U42", "after COMPLETE (Line idle), a fault and "
                          "RESET: SELF-TEST wakes the Line task and "
                          "passes only after a whole window of new good "
                          "reads, never on the health from idle cycles; "
                          "with the reads failing it does not pass and "
                          "the failures are reported (D-96, D-98)");

    /* A run that ends in COMPLETE: the Line task goes idle and its
       health drops while it is. */
    nav_model_init();
    (void) nav_model_until(NAV_MODE_READY, false, 2u * window);
    nav_model_tick(false, true);                    /* START */
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);
    nav_model_to_complete();
    UNIT_EQ(g_sm.mode, NAV_MODE_COMPLETE);
    UNIT_CHECK(g_b_model_run_over);

    for (uint32_t tick = 0u; tick < (2u * window); tick++)
    {
        nav_model_tick(false, false);
    }

    UNIT_CHECK(!g_b_model_health);

    /* A fault (a STOP, say), then RESET with good sensors: the Line
       task reads again, and READY only after a whole window of them. */
    nav_model_tick(true, false);
    UNIT_EQ(g_sm.mode, NAV_MODE_FAULT);
    nav_model_restart();
    UNIT_EQ(g_sm.mode, NAV_MODE_SELF_TEST);
    UNIT_CHECK(!g_b_model_run_over);
    reads = g_model_reads;
    g_b_model_seen = true;
    ticks = nav_model_until(NAV_MODE_READY, false, 3u * window);
    UNIT_EQ(g_sm.mode, NAV_MODE_READY);
    UNIT_CHECK((g_model_reads - reads) >= window);
    UNIT_CHECK(ticks >= window);

    /* Again to COMPLETE; this time the sensor fails while idle. */
    nav_model_tick(false, true);                    /* START */
    UNIT_EQ(g_sm.mode, NAV_MODE_LINE_FOLLOWING);
    nav_model_to_complete();
    UNIT_EQ(g_sm.mode, NAV_MODE_COMPLETE);
    g_b_adc_fails = true;
    nav_model_tick(true, false);
    UNIT_EQ(g_sm.mode, NAV_MODE_FAULT);
    nav_model_restart();
    reads = g_model_reports;

    for (uint32_t tick = 0u; tick <= selftest; tick++)
    {
        nav_model_tick(false, false);
        fails += (NULL != nav_find(NAV_ACT_SELF_TEST_FAIL, 0u)) ? 1u : 0u;
        UNIT_CHECK(NAV_MODE_SELF_TEST == g_sm.mode);
    }

    UNIT_EQ(fails, 1u);
    UNIT_EQ(g_model_reports - reads, 1u);           /* D-79 reported */
    g_b_line_stalled = false;
    UNIT_END();
}

void
test_nav (void)
{
    test_boot();
    test_selftest_fail();
    test_follow();
    test_junction_turn();
    test_junction_other();
    test_no_command();
    test_search_complete();
    test_search_found();
    test_find_line_timeout();
    test_events_ignored();
    test_new_letter();
    test_remap();
    test_fault();
    test_hump();
    test_obstacle_bypass();
    test_obstacle_no_room();
    test_keepalive();
    test_heading();
    test_actions_bounded();
    test_line_stale();
    test_line_fail_recovery();
    test_line_after_complete();
}

/*** end of file ***/
