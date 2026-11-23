/*
 *  nav_task.c
 *  The Navigation task: every 5 ms and on an event (report section 3).
 *
 *  Each wake-up: drain the event queues into the statechart (nav_sm.c),
 *  then on the 5 ms tick read the line-error record, the START button,
 *  the health levels and Safety's FAULT level and step the statechart.
 *  The statechart's actions are carried out here: Motion commands
 *  (Navigation is the only task that commands Motion), scan requests,
 *  route-log entries, acknowledgements and line controls.
 */

#include "../common/app_gpio.h"
#include "../common/app_stats.h"
#include "../common/app_time.h"
#include "../common/gpio_irq.h"
#include "../subsystem1/subsystem1.h"
#include "../subsystem2/subsystem2.h"
#include "../subsystem3/subsystem3.h"
#include "../subsystem4/subsystem4.h"
#include "../subsystem5/subsystem5.h"
#include "nav_sm.h"
#include "nav_task.h"
#include "navigation.h"

#define NAV_STATUS_EVERY    (CFG_PERIOD_TELEMETRY_MS / CFG_PERIOD_NAV_MS)
#define NAV_DEBOUNCE_TICKS  (CFG_BUTTON_DEBOUNCE_MS / CFG_PERIOD_NAV_MS)

static ID g_cyc_nav = 0;

static void nav_cyclic_handler(void * p_exinf);
static void nav_task(INT stacd, void * p_exinf);
static void nav_drain_queues(nav_sm_t * p_sm);
static void nav_execute(nav_sm_t * p_sm);
static void nav_execute_one(nav_sm_t const * p_sm, nav_act_t const * p_act);
static void nav_send_route(nav_act_t const * p_act);
static void nav_send_ack(nav_act_t const * p_act);
static void nav_send_status(nav_sm_t const * p_sm);
static bool nav_start_pressed(uint32_t * p_stable_ticks, bool * p_pressed);

/* Create the 5 ms cyclic handler (the flag and queues are made in
   navigation.c). */
ER
nav_task_init (void)
{
    T_CCYC ccyc = { 0 };

    ccyc.cycatr = TA_HLNG;
    ccyc.cychdr = (FP) nav_cyclic_handler;     /* kernel calls it with exinf */
    ccyc.cyctim = CFG_PERIOD_NAV_MS;
    ccyc.cycphs = CFG_PERIOD_NAV_MS;
    g_cyc_nav = tk_cre_cyc(&ccyc);

    return ((g_cyc_nav < E_OK) ? g_cyc_nav : E_OK);
}

/* Start the task on core 0 and its cyclic handler. */
ER
nav_task_start (void)
{
    ER       ercd = E_OK;
    ID const tid  = app_start_task_stack((FP) nav_task, CFG_PRI_NAV,
                                         APP_CORE0, CFG_STACK_NAV,
                                         "nav_task");

    if (tid < E_OK)
    {
        ercd = tid;
    }
    else
    {
        ercd = tk_sta_cyc(g_cyc_nav);
    }

    return (ercd);
}

/* Cyclic handler, interrupt context: release Navigation. */
static void
nav_cyclic_handler (void * p_exinf)
{
    (void) p_exinf;
    (void) tk_set_flg(flg_nav, NAV_FLG_TICK);
}

/* Written by Navigation only, read by the IMU task (one word). */
static volatile bool g_nav_running = false;
static volatile bool g_nav_lift_watch = false;

bool
nav_is_running (void)
{
    return (g_nav_running);
}

bool
nav_lift_watch (void)
{
    return (g_nav_lift_watch);
}

static void
nav_task (INT stacd, void * p_exinf)
{
    static nav_sm_t sm;         /* large: kept off the task stack */
    uint32_t        ticks          = 0u;
    uint32_t        stable_ticks   = 0u;
    bool            b_btn_pressed  = false;
    nav_mode_t      last_mode      = NAV_MODE_COUNT;

    (void) stacd;
    (void) p_exinf;

    app_stats_task_start(APP_STATS_NAV);

    /* All GPIO interrupts run on core 0 (report section 3): this task is
       pinned to core 0, so it enables the shared dispatcher here. */
    CHECK(gpio_irq_enable_core0(), "gpio_irq_enable_core0");
    app_gpio_input(CFG_PIN_START, (0 != CFG_BUTTON_ACTIVE_LOW)
                                  ? APP_GPIO_PULL_UP : APP_GPIO_PULL_DOWN);
    nav_sm_init(&sm, app_time_ms());

    for (;;)
    {
        UINT flags = 0u;

        (void) tk_wai_flg(flg_nav, NAV_FLG_WAKE, (TWF_ORW | TWF_BITCLR),
                          &flags, TMO_FEVR);
        sm.now_ms = app_time_ms();

        if (0u != (flags & NAV_FLG_RESTART))
        {
            nav_sm_restart(&sm);
            nav_execute(&sm);
        }

        if (0u != (flags & NAV_FLG_RESET))
        {
            nav_sm_reset(&sm);
            nav_execute(&sm);
        }

        nav_drain_queues(&sm);

        if (0u != (flags & NAV_FLG_TICK))
        {
            line_err_t line;
            T_RFLG     rflg   = { 0 };
            bool       b_start = nav_start_pressed(&stable_ticks,
                                                   &b_btn_pressed);

            line_err_init(&line);   /* no line if the read fails */
            app_stats_release(APP_STATS_NAV);
            safety_checkin(SAFETY_CHECKIN_NAV);
            (void) line_err_read(&line);
            (void) tk_ref_flg(flg_nav, &rflg);

            /* START asked for over MQTT: one press, like the button. */
            if (0u != (rflg.flgptn & NAV_FLG_REMOTE_START))
            {
                (void) tk_clr_flg(flg_nav, ~NAV_FLG_REMOTE_START);
                b_start = true;
            }

            nav_sm_tick(&sm, app_time_ms(), &line,
                        (uint32_t) (rflg.flgptn & NAV_FLG_HEALTH_ALL),
                        safety_is_faulted(), b_start);
            nav_execute(&sm);

            ticks++;

            if (0u == (ticks % NAV_STATUS_EVERY))
            {
                nav_send_status(&sm);
            }

            app_stats_work_end(APP_STATS_NAV);
        }

        g_nav_running = ((NAV_MODE_LINE_FOLLOWING == sm.mode) ||
                         (NAV_MODE_HUMP_CROSSING == sm.mode) ||
                         (NAV_MODE_LINE_SEARCH == sm.mode) ||
                         (NAV_MODE_AT_JUNCTION == sm.mode) ||
                         (NAV_MODE_AVOIDING == sm.mode));
        g_nav_lift_watch = (g_nav_running ||
                            (NAV_MODE_SAFE_HOLD == sm.mode) ||
                            (NAV_MODE_COMPLETE == sm.mode));

        if (sm.mode != last_mode)
        {
            last_mode = sm.mode;
            tm_printf((UB *) "nav: %s\n", (UB *) nav_mode_name(sm.mode));
        }
    }
}

/* Feed every queued event to the statechart, Motion status first so a
   move-done is seen before events that depend on it. */
static void
nav_drain_queues (nav_sm_t * p_sm)
{
    motion_sts_t   sts;
    line_evt_t     line_evt;
    imu_evt_t      imu_evt;
    obstacle_evt_t obst_evt;
    nav_cmd_t      cmd;

    while (tk_rcv_mbf(mbf_motion_sts, &sts, TMO_POL) > 0)
    {
        nav_sm_motion_status(p_sm, &sts);
        nav_execute(p_sm);
    }

    while (tk_rcv_mbf(mbf_line_evt, &line_evt, TMO_POL) > 0)
    {
        nav_sm_line_event(p_sm, &line_evt);
        nav_execute(p_sm);
    }

    while (tk_rcv_mbf(mbf_imu_evt, &imu_evt, TMO_POL) > 0)
    {
        nav_sm_imu_event(p_sm, &imu_evt);
        nav_execute(p_sm);
    }

    while (tk_rcv_mbf(mbf_obstacle, &obst_evt, TMO_POL) > 0)
    {
        nav_sm_obstacle_event(p_sm, &obst_evt);
        nav_execute(p_sm);
    }

    while (tk_rcv_mbf(mbf_nav_cmd, &cmd, TMO_POL) > 0)
    {
        nav_sm_command(p_sm, &cmd);
        nav_execute(p_sm);
    }
}

/* Carry out and forget the statechart's actions. */
static void
nav_execute (nav_sm_t * p_sm)
{
    for (uint32_t idx = 0u; idx < p_sm->act_count; idx++)
    {
        nav_execute_one(p_sm, &p_sm->acts[idx]);
    }

    nav_sm_clear_actions(p_sm);
}

static void
nav_execute_one (nav_sm_t const * p_sm, nav_act_t const * p_act)
{
    switch (p_act->type)
    {
        case NAV_ACT_DRIVE:
            (void) motion_drive(p_act->a_value, p_act->b_value);
        break;

        case NAV_ACT_FORWARD:
            (void) motion_move_forward(p_act->a_value, p_act->move_id);
        break;

        case NAV_ACT_BACKWARD:
            (void) motion_move_backward(p_act->a_value, p_act->move_id);
        break;

        case NAV_ACT_TURN_LEFT:
            (void) motion_turn_left(p_act->a_value, p_act->move_id);
        break;

        case NAV_ACT_TURN_RIGHT:
            (void) motion_turn_right(p_act->a_value, p_act->move_id);
        break;

        case NAV_ACT_STOP:
            (void) motion_stop();
        break;

        case NAV_ACT_HOLD_SPEED:
            (void) motion_hold_speed(0 != p_act->a_value);
        break;

        case NAV_ACT_KEEPALIVE:
            (void) motion_keepalive();
        break;

        case NAV_ACT_SCAN:
            (void) scan_request(p_act->kind, p_act->side);
        break;

        case NAV_ACT_ROUTE:
            nav_send_route(p_act);
        break;

        case NAV_ACT_ACK:
            nav_send_ack(p_act);
        break;

        case NAV_ACT_SUMMARY:
        {
            tlm_msg_t msg;

            tlm_msg_init(&msg, TLM_MSG_SUMMARY);
            msg.u.ev.time_ms = p_sm->now_ms;
            tlm_post(&msg, true);
        }
        break;

        case NAV_ACT_SELF_TEST_FAIL:
            /* The missing health bits fit in 16 bits (0x0700 at most). */
            safety_report_fault(FAULT_CAUSE_SELF_TEST, FAULT_SRC_NAV,
                                (uint16_t) p_act->a_value);
        break;

        case NAV_ACT_LINE_STALE:
            /* The line record stopped changing (G19, D-94); the time
               is at most FAULT_DETAIL_MAX (nav_sm.c). */
            safety_report_fault(FAULT_CAUSE_LINE_DATA, FAULT_SRC_NAV,
                                (uint16_t) p_act->a_value);
        break;

        case NAV_ACT_LINE_ADAPT:
            line_set_adaptation(0 != p_act->a_value);
        break;

        case NAV_ACT_LINE_RUN_OVER:
            line_set_run_over(0 != p_act->a_value);
        break;

        default:
            /* Unknown action: nothing to do. */
        break;
    }
}

/* FR13: one route-log entry to Telemetry (packed, D-92).  Always fits:
   the distance comes from Motion's odometry (MOTION_DISTANCE_BITS) and
   each kind's values from the packed events (tlm_msgs.h). */
static void
nav_send_route (nav_act_t const * p_act)
{
    tlm_msg_t msg;

    tlm_msg_init(&msg, TLM_MSG_ROUTE);
    msg.u.ev.time_ms = app_time_ms();
    (void) tlm_route_pack(&msg.u.ev.u.route, p_act->kind, p_act->letter,
                          p_act->action, p_act->a_value, p_act->b_value,
                          p_act->c_value);
    tlm_post(&msg, true);
}

/* Answer a remap on the ack topic (NFR8: effective when acknowledged).
   The seq round-trips: it came from a packed command (30 bits). */
static void
nav_send_ack (nav_act_t const * p_act)
{
    tlm_msg_t msg;

    tlm_msg_init(&msg, TLM_MSG_ACK);
    msg.u.ev.time_ms = app_time_ms();
    (void) tlm_ack_pack(&msg.u.ev.u.ack, (uint32_t) p_act->a_value,
                        p_act->kind, (0 != p_act->b_value));
    tlm_post(&msg, true);
}

/* IF-12: Navigation's status every 200 ms (FR12: robot state; packed,
   D-92).  Always fits: mode 0..10, a held letter 'A'..'Z' or '-', and
   a SAFE HOLD reason 0..3. */
static void
nav_send_status (nav_sm_t const * p_sm)
{
    tlm_msg_t msg;

    tlm_msg_init(&msg, TLM_MSG_NAV);
    (void) tlm_nav_pack(&msg.u.nav, (uint8_t) p_sm->mode,
                        p_sm->b_command_held ? p_sm->held_letter
                                             : TLM_NO_LETTER,
                        (uint8_t) p_sm->hold_reason);
    tlm_post(&msg, false);
}

/*
 * START button (GP20, owned by Navigation): true once per press, after
 * the level has been stable for the debounce time.
 */
static bool
nav_start_pressed (uint32_t * p_stable_ticks, bool * p_pressed)
{
    bool const b_level  = app_gpio_read(CFG_PIN_START);
    bool const b_active = (0 != CFG_BUTTON_ACTIVE_LOW) ? !b_level : b_level;
    bool       b_edge   = false;

    if (b_active != *p_pressed)
    {
        (*p_stable_ticks)++;

        if (*p_stable_ticks >= NAV_DEBOUNCE_TICKS)
        {
            *p_pressed     = b_active;
            *p_stable_ticks = 0u;
            b_edge         = b_active;
        }
    }
    else
    {
        *p_stable_ticks = 0u;
    }

    return (b_edge);
}

/*** end of file ***/
