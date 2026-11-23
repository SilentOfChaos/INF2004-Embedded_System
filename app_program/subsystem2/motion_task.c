/*
 *  motion_task.c
 *  The Motion Task (report section 3, statechart 6.2 / Fig 7).
 *
 *  Every 20 ms, released by a cyclic handler (NFR3):
 *    - read the encoders, work out wheel speeds and odometry;
 *    - apply Navigation's commands (IF-07): drive at given speeds,
 *      moveForward/moveBackward a distance, turnLeft/turnRight an angle
 *      on the spot, stop, holdSpeed;
 *    - run the wheel-speed PID and set the motor PWM;
 *    - watch for a driven wheel whose encoder gives no counts and report
 *      a motor stall or an encoder fault, which Safety latches (D-105);
 *    - send odometry, and move-done when a move or turn ends (IF-08);
 *    - check in with Safety.
 *  States: IDLE, DRIVING, HOLDING SPEED, TURNING, BRAKED.  If no update
 *  arrives from Navigation for 100 ms while moving, the car stops.
 *  Safety's FAULT flag holds Motion in BRAKED; on Safety's reset request
 *  Motion clears its queue, command and PID state and acknowledges
 *  (IF-09) before Safety releases the brake.
 */

#include "../app.h"
#include "../common/app_mbf.h"
#include "../common/app_stats.h"
#include "../common/app_time.h"
#include "../common/fixmath.h"
#include "../navigation/navigation.h"
#include "../subsystem1/subsystem1.h"
#include "../subsystem4/subsystem4.h"
#include "drv_encoder.h"
#include "drv_motor.h"
#include "motion_ctl.h"
#include "motion_task.h"
#include "subsystem2.h"

/* Commands the queue holds: what it really held before the minimum-bit
   change (16 were asked for, with 8 bytes to spare each), kept so a
   burst overflows exactly as before (D-83, D-88). */
#define MOTION_CMD_DEPTH        20u
#define MOTION_STATUS_EVERY     (CFG_PERIOD_TELEMETRY_MS / CFG_PERIOD_MOTION_MS)
#define MOTION_HALF             2
#define MOTION_US_PER_MS        1000u

/* Kinds of distance move or turn in progress. */
typedef enum
{
    MOTION_MOVE_NONE = 0,
    MOTION_MOVE_FORWARD,
    MOTION_MOVE_BACKWARD,
    MOTION_MOVE_LEFT,
    MOTION_MOVE_RIGHT
} motion_move_t;

typedef struct
{
    motion_state_t state;
    motion_move_t  move;
    uint8_t        move_id;
    int32_t        move_counts;     /* counts to travel */
    int32_t        start_l;         /* counts when the move started */
    int32_t        start_r;
    int32_t        drive_speed;     /* DRIVE speed and difference */
    int32_t        drive_diff;
    int32_t        held_speed;      /* HOLDING SPEED forward speed */
    uint32_t       last_update_ms;
    motion_pid_t   pid_l;
    motion_pid_t   pid_r;
    int32_t        duty_l;
    int32_t        duty_r;
    int32_t        count_l;
    int32_t        count_r;
    int32_t        speed_l;
    int32_t        speed_r;
    uint32_t       read_us;         /* when the encoders were last read */
    motion_stall_t stall;           /* stall and encoder-fault watch */
} motion_ctx_t;

EXPORT ID mbf_motion_cmd = 0;
EXPORT ID flg_motion     = 0;
static ID g_cyc_motion   = 0;

static void motion_cyclic_handler(void * p_exinf);
static void motion_task(INT stacd, void * p_exinf);
static void motion_clear(motion_ctx_t * p_ctx);
static void motion_apply(motion_ctx_t * p_ctx, motion_cmd_t const * p_cmd);
static void motion_start_move(motion_ctx_t * p_ctx, motion_move_t move,
                              motion_cmd_t const * p_cmd);
static void motion_targets(motion_ctx_t * p_ctx, int32_t * p_left,
                           int32_t * p_right);
static void motion_finish_move(motion_ctx_t * p_ctx);
static int32_t motion_duty(motion_pid_t * p_pid, uint32_t dt_ms,
                           int32_t current_duty, int32_t target,
                           int32_t measured);
static void motion_watch_stall(motion_ctx_t * p_ctx, int32_t delta_l,
                               int32_t delta_r, uint32_t dt_ms);
static void motion_send_odometry(motion_ctx_t const * p_ctx);
static void motion_send_status(motion_ctx_t const * p_ctx);
static ER motion_send(uint8_t type, uint8_t move_id, int32_t speed,
                      int32_t diff, int32_t value);

/* Create Motion's objects (usermain phase 1). */
ER
motion_task_init (void)
{
    T_CFLG cflg = { 0 };
    T_CMBF cmbf = { 0 };
    T_CCYC ccyc = { 0 };
    ER     ercd = E_OK;

    cflg.flgatr  = TA_TFIFO | TA_WMUL;
    cflg.iflgptn = 0u;
    flg_motion = tk_cre_flg(&cflg);

    cmbf.mbfatr = TA_TFIFO;
    cmbf.maxmsz = (INT) sizeof(motion_cmd_t);
    cmbf.bufsz  = (SZ) APP_MBF_BYTES(MOTION_CMD_DEPTH, sizeof(motion_cmd_t));
    mbf_motion_cmd = tk_cre_mbf(&cmbf);
    app_stats_queue("motion_cmd", mbf_motion_cmd);

    ccyc.cycatr = TA_HLNG;
    ccyc.cychdr = (FP) motion_cyclic_handler;  /* kernel calls it with exinf */
    ccyc.cyctim = CFG_PERIOD_MOTION_MS;
    ccyc.cycphs = CFG_PERIOD_MOTION_MS;
    g_cyc_motion = tk_cre_cyc(&ccyc);

    if (flg_motion < E_OK)
    {
        ercd = flg_motion;
    }
    else if (mbf_motion_cmd < E_OK)
    {
        ercd = mbf_motion_cmd;
    }
    else if (g_cyc_motion < E_OK)
    {
        ercd = g_cyc_motion;
    }
    else
    {
        ercd = E_OK;
    }

    return (ercd);
}

/* Start the Motion task on core 0 and its 20 ms cyclic handler. */
ER
motion_task_start (void)
{
    ER       ercd = E_OK;
    ID const tid  = app_start_task_stack((FP) motion_task, CFG_PRI_MOTION,
                                         APP_CORE0, CFG_STACK_DEFAULT,
                                         "motion_task");

    if (tid < E_OK)
    {
        ercd = tid;
    }
    else
    {
        ercd = tk_sta_cyc(g_cyc_motion);
    }

    return (ercd);
}

/* ---- The write-up's motion API (IF-07), called by Navigation ------- */

ER
motion_drive (int32_t speed_mmps, int32_t diff_mmps)
{
    return (motion_send(MOTION_CMD_DRIVE, 0u, speed_mmps, diff_mmps, 0));
}

ER
motion_move_forward (int32_t distance_mm, uint8_t move_id)
{
    return (motion_send(MOTION_CMD_FORWARD, move_id, 0, 0, distance_mm));
}

ER
motion_move_backward (int32_t distance_mm, uint8_t move_id)
{
    return (motion_send(MOTION_CMD_BACKWARD, move_id, 0, 0, distance_mm));
}

ER
motion_turn_left (int32_t angle_cdeg, uint8_t move_id)
{
    return (motion_send(MOTION_CMD_TURN_LEFT, move_id, 0, 0, angle_cdeg));
}

ER
motion_turn_right (int32_t angle_cdeg, uint8_t move_id)
{
    return (motion_send(MOTION_CMD_TURN_RIGHT, move_id, 0, 0, angle_cdeg));
}

ER
motion_stop (void)
{
    return (motion_send(MOTION_CMD_STOP, 0u, 0, 0, 0));
}

ER
motion_hold_speed (bool b_hold)
{
    return (motion_send(MOTION_CMD_HOLD_SPEED, 0u, 0, 0, b_hold ? 1 : 0));
}

ER
motion_keepalive (void)
{
    return (motion_send(MOTION_CMD_KEEPALIVE, 0u, 0, 0, 0));
}

/* IF-09: Safety asks Motion to clear everything before the release. */
void
motion_request_reset (void)
{
    (void) tk_set_flg(flg_motion, MOTION_FLG_RESET_REQ);
}

/* ------------------------------------------------------------------ */

static void
motion_cyclic_handler (void * p_exinf)
{
    (void) p_exinf;
    (void) tk_set_flg(flg_motion, MOTION_FLG_TICK);
}

static void
motion_task (INT stacd, void * p_exinf)
{
    motion_ctx_t ctx   = { 0 };     /* every field set before use */
    uint32_t     cycle = 0u;

    (void) stacd;
    (void) p_exinf;

    app_stats_task_start(APP_STATS_MOTION);
    motor_init();
    encoder_init();
    ctx.count_l = 0;        /* totals since boot (encoder_add_counts) */
    ctx.count_r = 0;
    motion_clear(&ctx);
    encoder_add_counts(&ctx.count_l, &ctx.count_r);
    ctx.read_us = app_time_us();
    ctx.state   = MOTION_STATE_IDLE;

    for (;;)
    {
        UINT         flags = 0u;
        motion_cmd_t cmd;

        (void) tk_wai_flg(flg_motion,
                          (MOTION_FLG_TICK | MOTION_FLG_RESET_REQ),
                          (TWF_ORW | TWF_BITCLR), &flags, TMO_FEVR);

        if (0u != (flags & MOTION_FLG_RESET_REQ))
        {
            /* Clear queue, current command and PID, then acknowledge. */
            while (tk_rcv_mbf(mbf_motion_cmd, &cmd, TMO_POL) > 0)
            {
                /* Discard queued commands. */
            }

            motion_clear(&ctx);
            ctx.state = MOTION_STATE_BRAKED;
            motor_set(0, 0);
            safety_ack_zero();
        }

        if (0u != (flags & MOTION_FLG_TICK))
        {
            int32_t const old_l = ctx.count_l;
            int32_t const old_r = ctx.count_r;
            int32_t       target_l = 0;
            int32_t       target_r = 0;
            uint32_t      dt_us = 0u;
            uint32_t      dt_ms = 0u;

            app_stats_release(APP_STATS_MOTION);
            safety_checkin(SAFETY_CHECKIN_MOTION);
            encoder_add_counts(&ctx.count_l, &ctx.count_r);

            /* The real time since the last read, not the nominal 20 ms:
               a late or missed release (the tick flag's bits merge)
               does not change the speed (24 Oct review, R-05). */
            dt_us       = app_time_us() - ctx.read_us;
            ctx.read_us = ctx.read_us + dt_us;
            dt_ms       = (dt_us + (MOTION_US_PER_MS / 2u)) /
                          MOTION_US_PER_MS;
            ctx.speed_l = motion_speed_mmps(ctx.count_l - old_l, dt_us);
            ctx.speed_r = motion_speed_mmps(ctx.count_r - old_r, dt_us);
            motion_watch_stall(&ctx, ctx.count_l - old_l,
                               ctx.count_r - old_r, dt_ms);

            if (safety_is_faulted())
            {
                /* BRAKED: Safety holds the pins low; commands dropped. */
                while (tk_rcv_mbf(mbf_motion_cmd, &cmd, TMO_POL) > 0)
                {
                    /* Discard: Navigation stops sending during FAULT. */
                }

                motion_clear(&ctx);
                ctx.state = MOTION_STATE_BRAKED;
            }
            else
            {
                if (MOTION_STATE_BRAKED == ctx.state)
                {
                    ctx.state = MOTION_STATE_IDLE;  /* fault released */
                }

                while (tk_rcv_mbf(mbf_motion_cmd, &cmd, TMO_POL) > 0)
                {
                    motion_apply(&ctx, &cmd);
                }

                motion_targets(&ctx, &target_l, &target_r);
            }

            ctx.duty_l = motion_duty(&ctx.pid_l, dt_ms, ctx.duty_l,
                                     target_l, ctx.speed_l);
            ctx.duty_r = motion_duty(&ctx.pid_r, dt_ms, ctx.duty_r,
                                     target_r, ctx.speed_r);
            motor_set(ctx.duty_l, ctx.duty_r);

            motion_send_odometry(&ctx);
            cycle++;

            if (0u == (cycle % MOTION_STATUS_EVERY))
            {
                motion_send_status(&ctx);
            }

            app_stats_work_end(APP_STATS_MOTION);
        }
    }
}

/* Drop any command, move and PID history; motors to brake. */
static void
motion_clear (motion_ctx_t * p_ctx)
{
    p_ctx->move           = MOTION_MOVE_NONE;
    p_ctx->move_id        = 0u;
    p_ctx->move_counts    = 0;
    p_ctx->start_l        = p_ctx->count_l;
    p_ctx->start_r        = p_ctx->count_r;
    p_ctx->drive_speed    = 0;
    p_ctx->drive_diff     = 0;
    p_ctx->held_speed     = 0;
    p_ctx->last_update_ms = app_time_ms();
    p_ctx->duty_l         = 0;
    p_ctx->duty_r         = 0;
    motion_pid_reset(&p_ctx->pid_l);
    motion_pid_reset(&p_ctx->pid_r);
    motion_stall_reset(&p_ctx->stall);

    if (MOTION_STATE_BRAKED != p_ctx->state)
    {
        p_ctx->state = MOTION_STATE_IDLE;
    }
}

/* One command from Navigation.  Every command counts as an update for
   the 100 ms limit. */
static void
motion_apply (motion_ctx_t * p_ctx, motion_cmd_t const * p_cmd)
{
    p_ctx->last_update_ms = app_time_ms();

    switch (motion_cmd_type(p_cmd))
    {
        case MOTION_CMD_DRIVE:
            p_ctx->move        = MOTION_MOVE_NONE;  /* replaces a move */
            p_ctx->drive_speed = motion_cmd_speed(p_cmd);
            p_ctx->drive_diff  = motion_cmd_diff(p_cmd);

            if (MOTION_STATE_HOLDING != p_ctx->state)
            {
                p_ctx->state = MOTION_STATE_DRIVING;
            }
        break;

        case MOTION_CMD_FORWARD:
            motion_start_move(p_ctx, MOTION_MOVE_FORWARD, p_cmd);
        break;

        case MOTION_CMD_BACKWARD:
            motion_start_move(p_ctx, MOTION_MOVE_BACKWARD, p_cmd);
        break;

        case MOTION_CMD_TURN_LEFT:
            motion_start_move(p_ctx, MOTION_MOVE_LEFT, p_cmd);
        break;

        case MOTION_CMD_TURN_RIGHT:
            motion_start_move(p_ctx, MOTION_MOVE_RIGHT, p_cmd);
        break;

        case MOTION_CMD_STOP:
            motion_clear(p_ctx);
        break;

        case MOTION_CMD_HOLD_SPEED:
            if ((0 != motion_cmd_value(p_cmd)) &&
                (MOTION_STATE_DRIVING == p_ctx->state) &&
                (MOTION_MOVE_NONE == p_ctx->move))
            {
                p_ctx->held_speed = p_ctx->drive_speed;
                p_ctx->state      = MOTION_STATE_HOLDING;
            }
            else if ((0 == motion_cmd_value(p_cmd)) &&
                     (MOTION_STATE_HOLDING == p_ctx->state))
            {
                p_ctx->state = MOTION_STATE_DRIVING;
            }
            else
            {
                /* Nothing to hold or release. */
            }
        break;

        case MOTION_CMD_KEEPALIVE:
        default:
            /* Only refreshes the update time. */
        break;
    }
}

/* Start a distance move or turn; it replaces whatever was running. */
static void
motion_start_move (motion_ctx_t * p_ctx, motion_move_t move,
                   motion_cmd_t const * p_cmd)
{
    bool const b_turn = ((MOTION_MOVE_LEFT == move) ||
                         (MOTION_MOVE_RIGHT == move));

    p_ctx->move        = move;
    p_ctx->move_id     = motion_cmd_move_id(p_cmd);
    p_ctx->start_l     = p_ctx->count_l;
    p_ctx->start_r     = p_ctx->count_r;
    p_ctx->move_counts = b_turn ? motion_turn_counts(motion_cmd_value(p_cmd))
                                : motion_mm_to_counts(
                                      fixmath_abs(motion_cmd_value(p_cmd)));
    p_ctx->state       = b_turn ? MOTION_STATE_TURNING
                                : MOTION_STATE_DRIVING;
}

/* Wheel speed targets for this cycle, and move completion. */
static void
motion_targets (motion_ctx_t * p_ctx, int32_t * p_left, int32_t * p_right)
{
    int32_t const moved_l = fixmath_abs(p_ctx->count_l - p_ctx->start_l);
    int32_t const moved_r = fixmath_abs(p_ctx->count_r - p_ctx->start_r);
    int32_t const progress = (moved_l + moved_r) / MOTION_HALF;
    int32_t const slow_counts = motion_mm_to_counts(CFG_MOTION_SLOWDOWN_MM);
    bool const    b_moving = (MOTION_STATE_IDLE != p_ctx->state);

    *p_left  = 0;
    *p_right = 0;

    if (b_moving && ((app_time_ms() - p_ctx->last_update_ms) >
                     CFG_MOTION_UPDATE_TIMEOUT_MS))
    {
        motion_clear(p_ctx);    /* 100 ms without an update: stop */
    }
    else if (MOTION_MOVE_NONE != p_ctx->move)
    {
        bool const b_turn = ((MOTION_MOVE_LEFT == p_ctx->move) ||
                             (MOTION_MOVE_RIGHT == p_ctx->move));
        int32_t speed = b_turn ? CFG_MOTION_TURN_MMPS : CFG_MOTION_MOVE_MMPS;

        if (progress >= p_ctx->move_counts)
        {
            motion_finish_move(p_ctx);
        }
        else
        {
            if ((p_ctx->move_counts - progress) < slow_counts)
            {
                speed = speed / MOTION_HALF;
            }

            *p_left  = ((MOTION_MOVE_BACKWARD == p_ctx->move) ||
                        (MOTION_MOVE_LEFT == p_ctx->move)) ? -speed : speed;
            *p_right = ((MOTION_MOVE_BACKWARD == p_ctx->move) ||
                        (MOTION_MOVE_RIGHT == p_ctx->move)) ? -speed : speed;
        }
    }
    else if ((MOTION_STATE_DRIVING == p_ctx->state) ||
             (MOTION_STATE_HOLDING == p_ctx->state))
    {
        int32_t const base = (MOTION_STATE_HOLDING == p_ctx->state)
                             ? p_ctx->held_speed : p_ctx->drive_speed;

        *p_left  = base - p_ctx->drive_diff;
        *p_right = base + p_ctx->drive_diff;
    }
    else
    {
        /* IDLE: motors off, speeds zero. */
    }
}

/* A move or turn reached its target: move-done with its ID.  A distance
   move returns to IDLE; a turn returns to DRIVING at zero speed until
   Navigation sends speeds (report 6.2). */
static void
motion_finish_move (motion_ctx_t * p_ctx)
{
    motion_sts_t sts;
    bool const   b_turn = ((MOTION_MOVE_LEFT == p_ctx->move) ||
                           (MOTION_MOVE_RIGHT == p_ctx->move));

    motion_sts_move_done(&sts, p_ctx->move_id);

    p_ctx->move        = MOTION_MOVE_NONE;
    p_ctx->drive_speed = 0;
    p_ctx->drive_diff  = 0;
    p_ctx->state       = b_turn ? MOTION_STATE_DRIVING : MOTION_STATE_IDLE;

    (void) nav_post(mbf_motion_sts, &sts, (INT) sizeof(sts),
                    FAULT_SRC_MOTION);
}

/* PID over the real time step, made safe to drive (never against the
   target, start-up duty only from standstill), then the soft-start
   ramp. */
static int32_t
motion_duty (motion_pid_t * p_pid, uint32_t dt_ms, int32_t current_duty,
             int32_t target, int32_t measured)
{
    int32_t const duty = motion_output_duty(
                             motion_pid_step(p_pid, target, measured, dt_ms),
                             target, measured);

    return (motion_ramp(current_duty, duty));
}

/*
 * A driven wheel whose encoder gives no counts for CFG_MOTION_STALL_MS:
 * report a motor stall or an encoder fault once, and Safety latches it
 * (24 Oct review, R-01; D-105).  The duties are the ones driven while
 * these counts were made; the detail is the silent wheels (1 left,
 * 2 right).
 */
static void
motion_watch_stall (motion_ctx_t * p_ctx, int32_t delta_l, int32_t delta_r,
                    uint32_t dt_ms)
{
    uint8_t                  wheels = 0u;
    motion_stall_out_t const out    = motion_stall_step(&p_ctx->stall,
                                          p_ctx->duty_l, p_ctx->duty_r,
                                          delta_l, delta_r, dt_ms,
                                          &wheels);

    if (MOTION_STALL_MOTOR == out)
    {
        safety_report_fault(FAULT_CAUSE_MOTOR_STALL, FAULT_SRC_MOTION,
                            (uint16_t) wheels);
    }
    else if (MOTION_STALL_ENCODER == out)
    {
        safety_report_fault(FAULT_CAUSE_ENCODER, FAULT_SRC_MOTION,
                            (uint16_t) wheels);
    }
    else
    {
        /* Nothing new to report. */
    }
}

/* IF-08: odometry every 20 ms. */
static void
motion_send_odometry (motion_ctx_t const * p_ctx)
{
    motion_sts_t sts;

    /* The heading is (-18000, 18000] (motion_heading_cdeg): it fits. */
    (void) motion_sts_odometry(&sts,
                               motion_counts_to_mm(
                                   (p_ctx->count_l / MOTION_HALF) +
                                   (p_ctx->count_r / MOTION_HALF)),
                               motion_heading_cdeg(p_ctx->count_l,
                                                   p_ctx->count_r));

    (void) nav_post(mbf_motion_sts, &sts, (INT) sizeof(sts),
                    FAULT_SRC_MOTION);
}

/* IF-12: Motion status for Telemetry every 200 ms (FR12; packed,
   D-92).  Always fits: the speeds are limited to 16 bits here and the
   distance is made from the counters (MOTION_DISTANCE_BITS). */
static void
motion_send_status (motion_ctx_t const * p_ctx)
{
    tlm_msg_t msg;

    tlm_msg_init(&msg, TLM_MSG_MOTION);
    (void) tlm_motion_pack(&msg.u.motion,
                           fixmath_clamp(p_ctx->speed_l, INT16_MIN,
                                         INT16_MAX),
                           fixmath_clamp(p_ctx->speed_r, INT16_MIN,
                                         INT16_MAX),
                           p_ctx->count_l, p_ctx->count_r,
                           motion_counts_to_mm(
                               (p_ctx->count_l / MOTION_HALF) +
                               (p_ctx->count_r / MOTION_HALF)));
    tlm_post(&msg, false);
}

/* Fill and queue one command.  A full queue is a fault (report 4). */
static ER
motion_send (uint8_t type, uint8_t move_id, int32_t speed, int32_t diff,
             int32_t value)
{
    motion_cmd_t cmd;
    ER           ercd = E_OK;

    /* Speeds are limited to +/-1000 mm/s here.  A distance or angle is
       kept as its size in 15 bits (D-88): a larger one is refused with
       E_PAR and nothing is queued, rather than sent shortened.
       Navigation's values are checked against that when building
       (nav_sm.c), so it never sends one. */
    if (!motion_cmd_pack(&cmd, type, move_id,
                         fixmath_clamp(speed, -MOTION_CMD_SPEED_MAX,
                                       MOTION_CMD_SPEED_MAX),
                         fixmath_clamp(diff, -MOTION_CMD_SPEED_MAX,
                                       MOTION_CMD_SPEED_MAX),
                         value))
    {
        ercd = E_PAR;
    }
    else
    {
        ercd = tk_snd_mbf(mbf_motion_cmd, &cmd, (INT) sizeof(cmd), TMO_POL);

        if (E_OK != ercd)
        {
            /* Object IDs are small positive numbers: they fit 16 bits. */
            safety_report_fault(FAULT_CAUSE_QUEUE_FULL, FAULT_SRC_NAV,
                                (uint16_t) mbf_motion_cmd);
        }
    }

    return (ercd);
}

/*** end of file ***/
