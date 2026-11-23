/*
 *  scan_task.c
 *  The Obstacle Scan Task (report section 3, statechart 6.5 / Fig 10).
 *
 *  Every 65 ms on core 1; pings are kept at least CFG_US_MIN_PING_GAP_MS
 *  (60 ms) apart even if a release is late (NFR5):
 *    MONITORING   ping ahead; two readings <= 30 cm: tell Navigation,
 *                 which stops the car (STOPPING);
 *    COARSE SCAN  30, 60, 90, 120, 150 degrees once the car has stopped;
 *                 nothing close: path clear;
 *    FINE SCAN    5 degree steps out to the obstacle's edges;
 *    PLANNING     profile (FR10, also published as an event) and the side
 *                 with room (FR11); no room: reverse 10 cm once and scan
 *                 again, else NO SAFE ROUTE;
 *    BYPASSING    side scans while Navigation drives past; obstacle
 *                 behind (seen beside, then clear on valid readings):
 *                 REJOINING until Navigation finds the line;
 *    RUN OVER     ranging idle, still checking in (D-57: health pings).
 *  Three invalid readings in a row in a ranging state: NO SAFE ROUTE.
 *  The same three also set the no-echo bit of Safety's fault mask, a
 *  status bit that never brakes the car, until a valid reading clears
 *  it.  SELF-TEST asks only that the sensor answers its pings, with a
 *  valid range or not (D-104).
 *  The task owns the HC-SR04P and the servo (TEAM.md section 3).
 */

#include "../app.h"
#include "../common/app_mbf.h"
#include "../common/app_stats.h"
#include "../common/app_time.h"
#include "../common/bitpack.h"
#include "../navigation/navigation.h"
#include "../subsystem1/subsystem1.h"
#include "../subsystem4/subsystem4.h"
#include "drv_servo.h"
#include "drv_ultrasonic.h"
#include "scan_logic.h"
#include "scan_task.h"
#include "subsystem5.h"

#define SCAN_FLG_TICK           0x0001u
/* Requests the queue holds: what it really held before the minimum-bit
   change (8 were asked for, with 8 bytes to spare each), kept so a burst
   overflows exactly as before (D-83, D-90). */
#define SCAN_REQ_DEPTH          10u
#define SCAN_STATUS_EVERY       3u      /* 3 x 65 ms ~ 200 ms */
#define SCAN_STRAIGHT_DEG       90
#define SCAN_COARSE_COUNT       5u
#define SCAN_FINE_MAX           (2u * SCAN_FINE_PER_SIDE)
#define SCAN_US_PER_MS          1000u

/* The 65 ms release must leave room for NFR5's minimum ping gap. */
#if (CFG_PERIOD_SCAN_MS < CFG_US_MIN_PING_GAP_MS)
#error "CFG_PERIOD_SCAN_MS must not be shorter than CFG_US_MIN_PING_GAP_MS"
#endif

typedef enum
{
    SCAN_STATE_RUN_OVER = 0,
    SCAN_STATE_MONITORING,
    SCAN_STATE_STOPPING,
    SCAN_STATE_COARSE,
    SCAN_STATE_FINE,
    SCAN_STATE_WAIT_BYPASS,     /* PLANNING done, Navigation turning out */
    SCAN_STATE_BYPASSING,
    SCAN_STATE_REVERSING,
    SCAN_STATE_REJOINING
} scan_state_t;

typedef struct
{
    scan_state_t   state;
    uint32_t       ticks;
    int32_t        servo_deg;
    uint32_t       settle;          /* pings to skip after a servo move */
    uint32_t       near_count;      /* readings in a row <= 30 cm */
    uint32_t       far_run;         /* open-space pings in a row, for the
                                       reported-range glitch filter */
    uint32_t       invalid_run;
    uint8_t        silent_run;      /* pings in a row not answered, up to
                                       CFG_SCAN_INVALID_LIMIT */
    bool           b_echo_known;    /* Safety has a no-echo state */
    bool           b_no_echo_sent;  /* the one Safety has */
    uint32_t       step;            /* index in the coarse / fine list */
    bool           b_retried;       /* this angle was invalid once (D-80) */
    int32_t        fine[SCAN_FINE_MAX];
    uint32_t       fine_count;
    bool           b_reversed_once;
    uint8_t        side;            /* side being passed on */
    scan_side_t    side_scan;       /* BYPASSING: beside, then clear */
    bool           b_rejoin_watch;  /* REJOINING: watching straight ahead
                                       for the obstacle still in the way */
    bool           b_pinged;        /* last_ping_us is valid */
    uint32_t       last_ping_us;    /* trigger time of the last ping */
    int32_t        last_range_mm;
    uint16_t       obstacles;
    scan_set_t     readings;
    scan_profile_t profile;
} scan_ctx_t;

EXPORT ID mbf_scan_req = 0;
static ID g_mbf_echo   = 0;
static ID g_flg_scan   = 0;
static ID g_cyc_scan   = 0;

static void scan_cyclic_handler(void * p_exinf);
static void scan_task(INT stacd, void * p_exinf);
static void scan_handle_request(scan_ctx_t * p_ctx, scan_req_t const * p_req);
static void scan_step(scan_ctx_t * p_ctx);
static void scan_point_servo(scan_ctx_t * p_ctx, int32_t angle_deg);
static bool scan_measure(int32_t * p_range_mm, uint32_t * p_ping_us,
                         bool * p_answered);
static void scan_reading(scan_ctx_t * p_ctx, bool b_valid, bool b_answered,
                         int32_t range);
static void scan_report_no_echo(scan_ctx_t * p_ctx, bool b_no_echo);
static void scan_coarse_done(scan_ctx_t * p_ctx);
static void scan_plan(scan_ctx_t * p_ctx);
static void scan_unconfirmed(scan_ctx_t * p_ctx);
static void scan_no_route(scan_ctx_t * p_ctx);
static void scan_send(scan_ctx_t const * p_ctx, uint8_t type);
static void scan_send_profile(scan_ctx_t const * p_ctx);
static void scan_send_status(scan_ctx_t const * p_ctx);

#define SCAN_COARSE_DEG(k)      (CFG_SCAN_COARSE_FIRST_DEG + \
                                 ((k) * CFG_SCAN_COARSE_STEP_DEG))

static int32_t const g_coarse_deg[SCAN_COARSE_COUNT] =
{
    SCAN_COARSE_DEG(0),
    SCAN_COARSE_DEG(1),
    SCAN_COARSE_DEG(2),
    SCAN_COARSE_DEG(3),
    CFG_SCAN_COARSE_LAST_DEG
};

/* A reading is kept at the angle asked for, so every coarse angle must
   be one the servo can reach (fine angles are kept within its range by
   scan_fine_angles()).  The telemetry profile relies on it: its forward
   distance is stored unsigned (tlm_msgs.c). */
#define SCAN_DEG_OK(deg)        (((deg) >= CFG_SCAN_SERVO_MIN_DEG) && \
                                 ((deg) <= CFG_SCAN_SERVO_MAX_DEG))
BITPACK_ASSERT(SCAN_DEG_OK(SCAN_COARSE_DEG(0)) &&
               SCAN_DEG_OK(SCAN_COARSE_DEG(1)) &&
               SCAN_DEG_OK(SCAN_COARSE_DEG(2)) &&
               SCAN_DEG_OK(SCAN_COARSE_DEG(3)) &&
               SCAN_DEG_OK(CFG_SCAN_COARSE_LAST_DEG),
               coarse_deg_reachable);

ER
scan_task_init (void)
{
    T_CFLG cflg = { 0 };
    T_CMBF cmbf = { 0 };
    T_CCYC ccyc = { 0 };
    ER     ercd = E_OK;

    cflg.flgatr  = TA_TFIFO | TA_WMUL;
    cflg.iflgptn = 0u;
    g_flg_scan = tk_cre_flg(&cflg);

    cmbf.mbfatr = TA_TFIFO;
    cmbf.maxmsz = (INT) sizeof(scan_req_t);
    cmbf.bufsz  = (SZ) APP_MBF_BYTES(SCAN_REQ_DEPTH, sizeof(scan_req_t));
    mbf_scan_req = tk_cre_mbf(&cmbf);
    app_stats_queue("scan_req", mbf_scan_req);

    cmbf.maxmsz = (INT) sizeof(ultrasonic_echo_t);
    cmbf.bufsz  = (SZ) APP_MBF_BYTES(ULTRASONIC_ECHO_QUEUE,
                                     sizeof(ultrasonic_echo_t));
    g_mbf_echo = tk_cre_mbf(&cmbf);
    app_stats_queue("echo", g_mbf_echo);

    ccyc.cycatr = TA_HLNG;
    ccyc.cychdr = (FP) scan_cyclic_handler;    /* kernel calls it with exinf */
    ccyc.cyctim = CFG_PERIOD_SCAN_MS;
    ccyc.cycphs = CFG_PERIOD_SCAN_MS;
    g_cyc_scan = tk_cre_cyc(&ccyc);

    if (g_flg_scan < E_OK)
    {
        ercd = g_flg_scan;
    }
    else if ((mbf_scan_req < E_OK) || (g_mbf_echo < E_OK))
    {
        ercd = E_NOMEM;
    }
    else if (g_cyc_scan < E_OK)
    {
        ercd = g_cyc_scan;
    }
    else
    {
        ercd = E_OK;
    }

    return (ercd);
}

ER
scan_task_start (void)
{
    ER       ercd = E_OK;
    ID const tid  = app_start_task_stack((FP) scan_task, CFG_PRI_SCAN,
                                         APP_CORE1, CFG_STACK_DEFAULT,
                                         "scan_task");

    if (tid < E_OK)
    {
        ercd = tid;
    }
    else
    {
        ercd = tk_sta_cyc(g_cyc_scan);
    }

    return (ercd);
}

/* IF-06: a request from Navigation (a full queue is a fault). */
ER
scan_request (uint8_t type, uint8_t side)
{
    scan_req_t req;
    ER         ercd = E_OK;

    /* Navigation sends SCAN_REQ_xxx and OBST_SIDE_xxx only: they fit. */
    (void) scan_req_pack(&req, type, side);

    ercd = tk_snd_mbf(mbf_scan_req, &req, (INT) sizeof(req), TMO_POL);

    if (E_OK != ercd)
    {
        /* Object IDs are small positive numbers: they fit 16 bits. */
        safety_report_fault(FAULT_CAUSE_QUEUE_FULL, FAULT_SRC_NAV,
                            (uint16_t) mbf_scan_req);
    }

    return (ercd);
}

static void
scan_cyclic_handler (void * p_exinf)
{
    (void) p_exinf;
    (void) tk_set_flg(g_flg_scan, SCAN_FLG_TICK);
}

static void
scan_task (INT stacd, void * p_exinf)
{
    static scan_ctx_t ctx;      /* kept off the task stack */

    (void) stacd;
    (void) p_exinf;

    app_stats_task_start(APP_STATS_SCAN);
    ctx.state           = SCAN_STATE_RUN_OVER;  /* until START (D-57) */
    ctx.b_rejoin_watch  = false;
    ctx.servo_deg       = SCAN_STRAIGHT_DEG;
    ctx.last_range_mm   = CFG_US_MAX_MM;
    servo_init();
    CHECK(ultrasonic_init(g_mbf_echo), "ultrasonic_init");

    for (;;)
    {
        UINT       flags = 0u;
        scan_req_t req;

        (void) tk_wai_flg(g_flg_scan, SCAN_FLG_TICK, (TWF_ORW | TWF_BITCLR),
                          &flags, TMO_FEVR);
        app_stats_release(APP_STATS_SCAN);
        safety_checkin(SAFETY_CHECKIN_SCAN);
        ctx.ticks++;

        while (tk_rcv_mbf(mbf_scan_req, &req, TMO_POL) > 0)
        {
            scan_handle_request(&ctx, &req);
        }

        scan_step(&ctx);

        if (0u == (ctx.ticks % SCAN_STATUS_EVERY))
        {
            scan_send_status(&ctx);
        }

        app_stats_work_end(APP_STATS_SCAN);
    }
}

/* IF-06 requests move the statechart on (Fig 10). */
static void
scan_handle_request (scan_ctx_t * p_ctx, scan_req_t const * p_req)
{
    switch (scan_req_type(p_req))
    {
        case SCAN_REQ_START:
            p_ctx->b_reversed_once = false;
            /* Fall through - START also resumes monitoring. */
        case SCAN_REQ_RESUME:
        case SCAN_REQ_LINE_FOUND:
            p_ctx->state      = SCAN_STATE_MONITORING;
            p_ctx->near_count = 0u;
            scan_point_servo(p_ctx, SCAN_STRAIGHT_DEG);
        break;

        case SCAN_REQ_CAR_STOPPED:
        case SCAN_REQ_REVERSED:
            if (SCAN_REQ_REVERSED == scan_req_type(p_req))
            {
                p_ctx->b_reversed_once = true;
            }

            scan_set_clear(&p_ctx->readings);
            p_ctx->state     = SCAN_STATE_COARSE;
            p_ctx->step      = 0u;
            p_ctx->b_retried = false;
            scan_point_servo(p_ctx, g_coarse_deg[0]);
        break;

        case SCAN_REQ_BYPASS:
            p_ctx->state = SCAN_STATE_BYPASSING;
            p_ctx->side  = scan_req_side(p_req);
            scan_side_init(&p_ctx->side_scan);
            /* Passing on the left means the obstacle is on the right. */
            scan_point_servo(p_ctx, (OBST_SIDE_LEFT == scan_req_side(p_req))
                                    ? CFG_SCAN_SIDE_RIGHT_DEG
                                    : CFG_SCAN_SIDE_LEFT_DEG);
        break;

        case SCAN_REQ_REJOIN:
            p_ctx->state          = SCAN_STATE_REJOINING;
            p_ctx->b_rejoin_watch = true;
            p_ctx->near_count     = 0u;
            scan_point_servo(p_ctx, SCAN_STRAIGHT_DEG);
        break;

        case SCAN_REQ_LINE_MISSED:
        case SCAN_REQ_RUN_OVER:
        default:
            p_ctx->state = SCAN_STATE_RUN_OVER;
            scan_point_servo(p_ctx, SCAN_STRAIGHT_DEG);
        break;
    }
}

/* One 65 ms step: at most one ping, never sooner than NFR5 allows. */
static void
scan_step (scan_ctx_t * p_ctx)
{
    bool const b_ranging = ((SCAN_STATE_MONITORING == p_ctx->state) ||
                            (SCAN_STATE_COARSE == p_ctx->state) ||
                            (SCAN_STATE_FINE == p_ctx->state) ||
                            (SCAN_STATE_BYPASSING == p_ctx->state) ||
                            ((SCAN_STATE_REJOINING == p_ctx->state) &&
                             p_ctx->b_rejoin_watch) ||
                            (SCAN_STATE_RUN_OVER == p_ctx->state));
    /* A release that came late leaves the next one closer than 65 ms
       to it: that ping waits a period (D-75). */
    bool const b_gap_ok = !p_ctx->b_pinged ||
                          ((app_time_us() - p_ctx->last_ping_us) >=
                           (CFG_US_MIN_PING_GAP_MS * SCAN_US_PER_MS));

    if (p_ctx->settle > 0u)
    {
        p_ctx->settle--;        /* servo still moving: no ping */
    }
    else if (b_ranging && b_gap_ok)
    {
        int32_t    range      = CFG_US_MAX_MM;
        bool       b_answered = false;
        bool const b_valid    = scan_measure(&range, &p_ctx->last_ping_us,
                                             &b_answered);

        p_ctx->b_pinged = true;
        scan_reading(p_ctx, b_valid, b_answered, range);
    }
    else
    {
        /* STOPPING, WAIT_BYPASS, REVERSING, REJOINING: wait for
           Navigation; or the last ping was too recent. */
    }
}

/* Move the servo; the next CFG_SCAN_SETTLE_PINGS pings are skipped. */
static void
scan_point_servo (scan_ctx_t * p_ctx, int32_t angle_deg)
{
    if (angle_deg != p_ctx->servo_deg)
    {
        p_ctx->servo_deg = angle_deg;
        p_ctx->settle    = CFG_SCAN_SETTLE_PINGS;
        servo_set_angle(angle_deg);
    }
}

/*
 * One ping, waiting up to CFG_US_ECHO_TIMEOUT_MS for the echo.  Stale
 * echoes from an earlier ping are dropped first and any echo that rose
 * before this trigger is ignored.  The trigger time goes to *p_ping_us.
 * *p_answered: the sensor answered this ping, with a valid range or
 * not: an echo rose after the trigger, or Echo was low before the
 * trigger and is still high when the wait ends (a pulse longer than
 * the wait, which open space may give: risk R4).  Returns false for an
 * invalid reading (missing, malformed or timed out).
 */
static bool
scan_measure (int32_t * p_range_mm, uint32_t * p_ping_us, bool * p_answered)
{
    ultrasonic_echo_t echo;
    bool              b_valid = false;
    bool              b_idle  = false;
    uint32_t          trigger_us = 0u;
    INT               len = 0;

    while (tk_rcv_mbf(g_mbf_echo, &echo, TMO_POL) > 0)
    {
        /* Drop echoes left from an earlier ping. */
    }

    /* Resynchronise the ISR's edge state to the line before this ping so
       a lost edge from an earlier pulse (e.g. during a jolt) cannot keep
       pairing falling edges against a stale rise time and freeze the
       range until a reflash. */
    ultrasonic_ping_reset();

    b_idle     = !ultrasonic_echo_high();
    trigger_us = ultrasonic_trigger();
    *p_ping_us = trigger_us;
    len = tk_rcv_mbf(g_mbf_echo, &echo, CFG_US_ECHO_TIMEOUT_MS);

    if ((len > 0) && ((int32_t) (echo.rise_us - trigger_us) >= 0))
    {
        *p_answered = true;
        *p_range_mm = scan_range_mm(ultrasonic_echo_width_us(&echo),
                                    &b_valid);
    }
    else
    {
        *p_answered = b_idle && ultrasonic_echo_high();
    }

    return (b_valid);
}

/* Use one reading in the current state. */
static void
scan_reading (scan_ctx_t * p_ctx, bool b_valid, bool b_answered,
              int32_t range)
{
    p_ctx->invalid_run = b_valid ? 0u : (p_ctx->invalid_run + 1u);

    if (b_answered)
    {
        p_ctx->silent_run = 0u;
    }
    else if (p_ctx->silent_run < CFG_SCAN_INVALID_LIMIT)
    {
        p_ctx->silent_run++;
    }
    else
    {
        /* Kept at the limit. */
    }

    /* SELF-TEST health: the sensor answers, a valid range or not, so a
       working sensor facing open space passes and an unplugged one does
       not (PostTri1 Version C, risk R4; D-104). */
    nav_set_health(NAV_FLG_SCAN_OK,
                   p_ctx->silent_run < CFG_SCAN_INVALID_LIMIT);
    scan_report_no_echo(p_ctx,
                        p_ctx->invalid_run >= CFG_SCAN_INVALID_LIMIT);

    /* Reported range.  A reading counts as "open space" when the sensor
       answered but saw nothing within range: either an invalid/timed-out
       ping with the line still high (b_answered, !b_valid) or a valid
       reading already clamped to the maximum.  A real close reading is
       reported at once and clears the run; open space is reported only
       after CFG_SCAN_FAR_CONFIRM in a row, so a single stray miss (a
       weak or off-angle echo off a real object) does not flicker the
       range to 4000.  A ping that is not answered at all is left to the
       silent_run health check and does not touch the reported range. */
    if (b_valid && (range < CFG_US_MAX_MM))
    {
        p_ctx->far_run       = 0u;
        p_ctx->last_range_mm = range;
    }
    else if (b_valid || b_answered)
    {
        if (p_ctx->far_run < CFG_SCAN_FAR_CONFIRM)
        {
            p_ctx->far_run++;
        }

        if (p_ctx->far_run >= CFG_SCAN_FAR_CONFIRM)
        {
            p_ctx->last_range_mm = CFG_US_MAX_MM;
        }
    }
    else
    {
        /* No answer: keep the last range; silent_run tracks the sensor. */
    }

    if (SCAN_STATE_RUN_OVER == p_ctx->state)
    {
        /* Health ping only. */
    }
    else if (p_ctx->invalid_run >= CFG_SCAN_INVALID_LIMIT)
    {
        scan_no_route(p_ctx);
    }
    else if (SCAN_STATE_MONITORING == p_ctx->state)
    {
        p_ctx->near_count = (b_valid && (range <= CFG_SCAN_DETECT_MM))
                            ? (p_ctx->near_count + 1u) : 0u;

        if (p_ctx->near_count >= CFG_SCAN_DETECT_COUNT)
        {
            p_ctx->profile.closest_mm  = range;
            p_ctx->profile.distance_mm = range;
            scan_send(p_ctx, OBST_EVT_AHEAD);
            p_ctx->state = SCAN_STATE_STOPPING;
        }
    }
    else if (((SCAN_STATE_COARSE == p_ctx->state) ||
              (SCAN_STATE_FINE == p_ctx->state)) &&
             !b_valid && !p_ctx->b_retried)
    {
        /* An invalid reading in a scan is repeated once at the same
           angle (D-80); it still counts towards the three in a row. */
        p_ctx->b_retried = true;
    }
    else if (SCAN_STATE_COARSE == p_ctx->state)
    {
        p_ctx->b_retried = false;
        scan_set_add(&p_ctx->readings, p_ctx->servo_deg, range, b_valid);
        p_ctx->step++;

        if (p_ctx->step < SCAN_COARSE_COUNT)
        {
            scan_point_servo(p_ctx, g_coarse_deg[p_ctx->step]);
        }
        else
        {
            scan_coarse_done(p_ctx);
        }
    }
    else if (SCAN_STATE_FINE == p_ctx->state)
    {
        p_ctx->b_retried = false;
        scan_set_add(&p_ctx->readings, p_ctx->servo_deg, range, b_valid);
        p_ctx->step++;

        if (p_ctx->step < p_ctx->fine_count)
        {
            scan_point_servo(p_ctx, p_ctx->fine[p_ctx->step]);
        }
        else
        {
            scan_plan(p_ctx);
        }
    }
    else if (SCAN_STATE_REJOINING == p_ctx->state)
    {
        /* Driving back to the line: is the obstacle still ahead? */
        p_ctx->near_count = (b_valid && (range <= CFG_SCAN_REJOIN_BLOCK_MM))
                            ? (p_ctx->near_count + 1u) : 0u;

        if (p_ctx->b_rejoin_watch &&
            (p_ctx->near_count >= CFG_SCAN_DETECT_COUNT))
        {
            scan_send(p_ctx, OBST_EVT_AHEAD);
            p_ctx->b_rejoin_watch = false;  /* re-armed by the next REJOIN */
            p_ctx->near_count     = 0u;
        }
    }
    else if (scan_side_step(&p_ctx->side_scan, b_valid, range))
    {
        /* BYPASSING: seen beside the car, then clear on valid readings
           only; an invalid reading never confirms it (6.5, D-74). */
        scan_send(p_ctx, OBST_EVT_BEHIND);
        p_ctx->state          = SCAN_STATE_REJOINING;
        p_ctx->b_rejoin_watch = false;  /* not until Navigation turns in */
        scan_point_servo(p_ctx, SCAN_STRAIGHT_DEG);
    }
    else
    {
        /* BYPASSING: obstacle not behind yet. */
    }
}

/*
 * Coarse scan finished: fine scan around the hits; with no hit, path
 * clear only if all five angles gave a valid reading (D-80), otherwise
 * the space ahead was not all seen and is treated like no room (FR11).
 */
static void
scan_coarse_done (scan_ctx_t * p_ctx)
{
    int32_t min_hit = 0;
    int32_t max_hit = 0;

    if (!scan_hit_range(&p_ctx->readings, &min_hit, &max_hit))
    {
        if (scan_all_valid(&p_ctx->readings))
        {
            scan_send(p_ctx, OBST_EVT_PATH_CLEAR);
            p_ctx->state      = SCAN_STATE_MONITORING;
            p_ctx->near_count = 0u;
            scan_point_servo(p_ctx, SCAN_STRAIGHT_DEG);
        }
        else
        {
            scan_unconfirmed(p_ctx);
        }
    }
    else
    {
        p_ctx->fine_count = scan_fine_angles(min_hit, max_hit, p_ctx->fine,
                                             SCAN_FINE_MAX);

        if (0u == p_ctx->fine_count)
        {
            scan_plan(p_ctx);
        }
        else
        {
            p_ctx->state = SCAN_STATE_FINE;
            p_ctx->step  = 0u;
            scan_point_servo(p_ctx, p_ctx->fine[0]);
        }
    }
}

/* PLANNING: profile, then a side, or reverse once, or no route. */
static void
scan_plan (scan_ctx_t * p_ctx)
{
    scan_profile(&p_ctx->readings, &p_ctx->profile);
    scan_point_servo(p_ctx, SCAN_STRAIGHT_DEG);

    if (!p_ctx->profile.b_found)
    {
        scan_send(p_ctx, OBST_EVT_PATH_CLEAR);
        p_ctx->state = SCAN_STATE_MONITORING;
    }
    else
    {
        scan_send_profile(p_ctx);       /* FR10: report the profile */

        if (OBST_SIDE_NONE != p_ctx->profile.side)
        {
            p_ctx->obstacles++;
            scan_send(p_ctx, OBST_EVT_PLAN);
            p_ctx->state = SCAN_STATE_WAIT_BYPASS;
        }
        else if (!p_ctx->b_reversed_once)
        {
            scan_send(p_ctx, OBST_EVT_NO_ROOM);
            p_ctx->state = SCAN_STATE_REVERSING;
        }
        else
        {
            scan_no_route(p_ctx);
        }
    }
}

/*
 * Nothing close was found but not every coarse angle gave a valid
 * reading: the path is not confirmed clear (report 6.5, D-80).  As with
 * no room (FR11): reverse 10 cm once and scan again, then NO SAFE ROUTE.
 */
static void
scan_unconfirmed (scan_ctx_t * p_ctx)
{
    p_ctx->profile.side = OBST_SIDE_NONE;
    scan_point_servo(p_ctx, SCAN_STRAIGHT_DEG);

    if (!p_ctx->b_reversed_once)
    {
        scan_send(p_ctx, OBST_EVT_NO_ROOM);
        p_ctx->state = SCAN_STATE_REVERSING;
    }
    else
    {
        scan_no_route(p_ctx);
    }
}

/*
 * No echo (CFG_SCAN_INVALID_LIMIT invalid readings in a row) is a status
 * bit in Safety's fault mask, never a fault: in a run Navigation goes to
 * SAFE HOLD on NO SAFE ROUTE instead (D-104).  Sent when it changes; if
 * Safety's queue was full, again with the next reading.
 */
static void
scan_report_no_echo (scan_ctx_t * p_ctx, bool b_no_echo)
{
    if ((!p_ctx->b_echo_known || (b_no_echo != p_ctx->b_no_echo_sent)) &&
        safety_report_status(FAULT_CAUSE_NO_ECHO, FAULT_SRC_SCAN, b_no_echo))
    {
        p_ctx->b_echo_known   = true;
        p_ctx->b_no_echo_sent = b_no_echo;
    }
}

/* NO SAFE ROUTE: report it; Navigation holds the car (SAFE HOLD). */
static void
scan_no_route (scan_ctx_t * p_ctx)
{
    scan_send(p_ctx, OBST_EVT_NO_SAFE_ROUTE);
    p_ctx->state = SCAN_STATE_RUN_OVER;
    scan_point_servo(p_ctx, SCAN_STRAIGHT_DEG);
}

/* IF-05: one event to Navigation with the current profile. */
static void
scan_send (scan_ctx_t const * p_ctx, uint8_t type)
{
    obstacle_evt_t               evt;
    scan_profile_t const * const p_prof = &p_ctx->profile;

    /* Packed (D-90).  Types are OBST_EVT_xxx, and a plan's points are
       all hits, within CFG_SCAN_OBSTACLE_MM of the sensor: everything
       fits. */
    if (OBST_EVT_PLAN == type)
    {
        (void) obstacle_evt_plan(&evt, p_prof->side, p_prof->closest_mm,
                                 p_prof->width_mm, p_prof->edge_left_mm,
                                 p_prof->edge_right_mm);
    }
    else
    {
        (void) obstacle_evt_simple(&evt, type);
    }

    (void) nav_post(mbf_obstacle, &evt, (INT) sizeof(evt), FAULT_SRC_SCAN);
}

/*
 * IF-12: the obstacle profile for Telemetry (FR10: position, width,
 * closest point, left and right clearance; D-72).  Sent at once: it is
 * an event, not a status record.
 */
static void
scan_send_profile (scan_ctx_t const * p_ctx)
{
    tlm_msg_t                    msg;
    scan_profile_t const * const p_prof = &p_ctx->profile;

    /* Packed (D-92).  Always fits: a profile is only sent when it found
       an obstacle, so every point is a hit within CFG_SCAN_OBSTACLE_MM
       (S1-U44 checks it on random scans). */
    tlm_msg_init(&msg, TLM_MSG_OBSTACLE);
    msg.u.ev.time_ms = app_time_ms();
    (void) tlm_obstacle_pack(&msg.u.ev.u.obstacle, p_prof->side,
                             p_ctx->b_reversed_once, p_prof->distance_mm,
                             p_prof->offset_mm, p_prof->width_mm,
                             p_prof->closest_mm, p_prof->clear_left_mm,
                             p_prof->clear_right_mm);
    tlm_post(&msg, true);
}

/* IF-12: scan status for Telemetry about every 200 ms (packed, D-92).
   Always fits: a range is 0..4000 and the obstacle count 16 bits. */
static void
scan_send_status (scan_ctx_t const * p_ctx)
{
    tlm_msg_t msg;

    tlm_msg_init(&msg, TLM_MSG_SCAN);
    (void) tlm_scan_pack(&msg.u.scan, p_ctx->last_range_mm,
                         (uint32_t) p_ctx->obstacles);
    tlm_post(&msg, false);
}

/*** end of file ***/
