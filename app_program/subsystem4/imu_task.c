/*
 *  imu_task.c
 *  The IMU Task (report section 3, statechart 6.4 / Fig 9).
 *
 *  Every 10 ms on core 1 (accelerometer 100 Hz, magnetometer 75 Hz,
 *  NFR4):
 *    CALIBRATING  store the level offsets with the car at rest;
 *    MONITORING   tilt-compensated heading to Navigation, motion class
 *                 and turn rate (FR17), ON HUMP while the roll is above
 *                 the threshold, then the peak roll as a height (FR8);
 *    the car lifted or overturned during a run, or invalid data,
 *    latches a fault in Safety (the motors stop at once).  Collision
 *    detection was removed (requirement change, 19 Nov 2026); an
 *    over-range sample is now only left out.
 *  The IMU owns I2C1 (TEAM.md section 3).
 */

#include "../app.h"
#include "../common/app_stats.h"
#include "../common/app_time.h"
#include "../common/fixmath.h"
#include "../navigation/navigation.h"
#include "../subsystem1/subsystem1.h"
#include "drv_i2c1.h"
#include "drv_lsm303.h"
#include "imu_calc.h"
#include "imu_task.h"
#include "subsystem4.h"

#define IMU_FLG_TICK            0x0001u
#define IMU_STATUS_EVERY        (CFG_PERIOD_TELEMETRY_MS / CFG_PERIOD_IMU_MS)
#define IMU_RETRY_EVERY         10u     /* init retries every 100 ms */
#define IMU_MAG_OVERFLOW        (-4096) /* LSM303DLHC overflow code */
#define IMU_MS_PER_S            1000
#define IMU_CDEG_PER_DEG        100

typedef enum
{
    IMU_STATE_INIT = 0,         /* identify and configure the GY-511 */
    IMU_STATE_CALIBRATING,
    IMU_STATE_MONITORING
} imu_state_t;

typedef struct
{
    imu_state_t      state;
    uint32_t         cycle;
    int32_t          sum_x;             /* calibration sums, mg */
    int32_t          sum_y;
    int32_t          sum_z;
    uint32_t         cal_count;
    int32_t          roll0_cdeg;        /* resting tilt */
    int32_t          pitch0_cdeg;
    int32_t          fwd0_mg;           /* resting x and y readings */
    int32_t          lat0_mg;
    uint32_t         invalid_run;
    bool             b_ok;              /* data valid, not lifted now */
    bool             b_lift_sent;
    imu_lift_t       lift;
    imu_hump_t       hump;
    imu_classifier_t classifier;
    imu_class_t      motion_class;
    int32_t          roll_cdeg;         /* relative to the resting tilt */
    int32_t          pitch_cdeg;
    int32_t          heading_cdeg;
    bool             b_heading_valid;
    int32_t          rate_ref_cdeg;     /* last flat-ground heading */
    uint32_t         rate_ref_ms;
    bool             b_rate_ref;        /* rate_ref_xxx are valid */
    int32_t          rate_cdps;
    uint8_t          humps;
    int32_t          max_height_mm;
} imu_ctx_t;

static ID g_flg_imu = 0;
static ID g_cyc_imu = 0;

static void imu_cyclic_handler(void * p_exinf);
static void imu_task(INT stacd, void * p_exinf);
static void imu_try_init(imu_ctx_t * p_ctx);
static void imu_calibrate(imu_ctx_t * p_ctx);
static void imu_monitor(imu_ctx_t * p_ctx);
static imu_sample_t imu_read_sample(lsm303_accel_t * p_accel,
                                    uint32_t * p_total_mg);
static void imu_lifted(imu_ctx_t * p_ctx, imu_lift_evt_t event);
static void imu_set_ok(imu_ctx_t * p_ctx, bool b_ok);
static void imu_update_heading(imu_ctx_t * p_ctx);
static void imu_send_event(imu_ctx_t const * p_ctx, uint8_t type,
                           int32_t peak_cdeg, int32_t height_mm);
static void imu_send_status(imu_ctx_t const * p_ctx);

ER
imu_task_init (void)
{
    T_CFLG cflg = { 0 };
    T_CCYC ccyc = { 0 };
    ER     ercd = E_OK;

    cflg.flgatr  = TA_TFIFO | TA_WMUL;
    cflg.iflgptn = 0u;
    g_flg_imu = tk_cre_flg(&cflg);

    ccyc.cycatr = TA_HLNG;
    ccyc.cychdr = (FP) imu_cyclic_handler;     /* kernel calls it with exinf */
    ccyc.cyctim = CFG_PERIOD_IMU_MS;
    ccyc.cycphs = CFG_PERIOD_IMU_MS;
    g_cyc_imu = tk_cre_cyc(&ccyc);

    if (g_flg_imu < E_OK)
    {
        ercd = g_flg_imu;
    }
    else if (g_cyc_imu < E_OK)
    {
        ercd = g_cyc_imu;
    }
    else
    {
        ercd = E_OK;
    }

    return (ercd);
}

ER
imu_task_start (void)
{
    ER       ercd = E_OK;
    ID const tid  = app_start_task_stack((FP) imu_task, CFG_PRI_IMU,
                                         APP_CORE1, CFG_STACK_DEFAULT,
                                         "imu_task");

    if (tid < E_OK)
    {
        ercd = tid;
    }
    else
    {
        ercd = tk_sta_cyc(g_cyc_imu);
    }

    return (ercd);
}

static void
imu_cyclic_handler (void * p_exinf)
{
    (void) p_exinf;
    (void) tk_set_flg(g_flg_imu, IMU_FLG_TICK);
}

static void
imu_task (INT stacd, void * p_exinf)
{
    static imu_ctx_t ctx;       /* kept off the task stack */

    (void) stacd;
    (void) p_exinf;

    app_stats_task_start(APP_STATS_IMU);
    ctx.state           = IMU_STATE_INIT;
    ctx.cycle           = 0u;
    ctx.invalid_run     = 0u;
    ctx.b_ok            = false;
    ctx.b_lift_sent     = false;
    imu_lift_init(&ctx.lift, 0, 0, 0);
    ctx.motion_class    = IMU_CLASS_STATIONARY;
    ctx.b_heading_valid = false;
    ctx.b_rate_ref      = false;
    ctx.rate_cdps       = 0;
    ctx.humps           = 0u;
    ctx.max_height_mm   = 0;
    imu_hump_init(&ctx.hump);
    imu_classifier_init(&ctx.classifier);
    i2c1_init(CFG_IMU_I2C_HZ);

    for (;;)
    {
        UINT flags = 0u;

        (void) tk_wai_flg(g_flg_imu, IMU_FLG_TICK, (TWF_ORW | TWF_BITCLR),
                          &flags, TMO_FEVR);
        app_stats_release(APP_STATS_IMU);
        safety_checkin(SAFETY_CHECKIN_IMU);
        ctx.cycle++;

        if (IMU_STATE_INIT == ctx.state)
        {
            if (0u == (ctx.cycle % IMU_RETRY_EVERY))
            {
                imu_try_init(&ctx);
            }
        }
        else if (IMU_STATE_CALIBRATING == ctx.state)
        {
            imu_calibrate(&ctx);
        }
        else
        {
            imu_monitor(&ctx);
        }

        if (0u == (ctx.cycle % IMU_STATUS_EVERY))
        {
            imu_send_status(&ctx);
        }

        app_stats_work_end(APP_STATS_IMU);
    }
}

/* Identify the GY-511 (magnetometer ID registers) and configure it. */
static void
imu_try_init (imu_ctx_t * p_ctx)
{
    if (!lsm303_check_id())
    {
        /* No answer: the bus may be stuck (sensor holding SDA after a
           reset).  Recover it and set I2C1 up again for the next try. */
        i2c1_init(CFG_IMU_I2C_HZ);
    }
    else if (lsm303_init())
    {
        p_ctx->state       = IMU_STATE_CALIBRATING;
        p_ctx->sum_x       = 0;
        p_ctx->sum_y       = 0;
        p_ctx->sum_z       = 0;
        p_ctx->cal_count   = 0u;
        p_ctx->invalid_run = 0u;
    }
}

/*
 * CALIBRATING: average the accelerometer at rest (report 6.4).  Invalid
 * samples are left out of the average and counted as in MONITORING:
 * CFG_IMU_INVALID_LIMIT in a row latch a fault in Safety ("invalid
 * sensor data in any IMU state", report 6.4, D-76), and the IMU starts
 * again from identification so the cause can clear.  A jolt (|a| at
 * least CFG_IMU_IMPACT_MG away from 1 g, or above the plausible range)
 * means the car is not at rest: the average starts again, without a
 * fault (G17 decided 16 Oct, D-81).
 */
static void
imu_calibrate (imu_ctx_t * p_ctx)
{
    lsm303_accel_t     accel  = { 0, 0, 0 };
    uint32_t           total  = 0u;
    imu_sample_t const sample = imu_read_sample(&accel, &total);

    if (IMU_SAMPLE_OK == sample)
    {
        if (imu_calc_cal_jolt(total))
        {
            /* A jolt: the car is not at rest, so the average starts
               again, without a fault; the motors are off (G17, D-81). */
            p_ctx->sum_x     = 0;
            p_ctx->sum_y     = 0;
            p_ctx->sum_z     = 0;
            p_ctx->cal_count = 0u;
        }
        else
        {
            p_ctx->sum_x += accel.x_mg;
            p_ctx->sum_y += accel.y_mg;
            p_ctx->sum_z += accel.z_mg;
            p_ctx->cal_count++;
        }

        p_ctx->invalid_run = 0u;
    }
    else if (IMU_SAMPLE_OVERRANGE == sample)
    {
        /* Beyond the plausible range: a jolt too (D-81). */
        p_ctx->invalid_run = 0u;
        p_ctx->sum_x       = 0;
        p_ctx->sum_y       = 0;
        p_ctx->sum_z       = 0;
        p_ctx->cal_count   = 0u;
    }
    else
    {
        p_ctx->invalid_run++;

        if (CFG_IMU_INVALID_LIMIT == p_ctx->invalid_run)
        {
            imu_set_ok(p_ctx, false);
            nav_set_health(NAV_FLG_IMU_OK, false);
            safety_report_fault(FAULT_CAUSE_IMU_DATA, FAULT_SRC_IMU, 0u);
            p_ctx->state = IMU_STATE_INIT;  /* identify and start again */
        }
    }

    if (p_ctx->cal_count >= CFG_IMU_CAL_SAMPLES)
    {
        int32_t const count = (int32_t) p_ctx->cal_count;   /* 100 */
        int32_t const mean_x = p_ctx->sum_x / count;
        int32_t const mean_y = p_ctx->sum_y / count;
        int32_t const mean_z = p_ctx->sum_z / count;

        imu_calc_tilt(mean_x, mean_y, mean_z, &p_ctx->roll0_cdeg,
                      &p_ctx->pitch0_cdeg);
        p_ctx->fwd0_mg     = mean_x;
        p_ctx->lat0_mg     = mean_y;
        imu_lift_init(&p_ctx->lift, mean_x, mean_y, mean_z);
        p_ctx->invalid_run = 0u;
        p_ctx->state       = IMU_STATE_MONITORING;
        imu_set_ok(p_ctx, true);
        nav_set_health(NAV_FLG_IMU_OK, true);
    }
}

/* MONITORING: one 10 ms step. */
static void
imu_monitor (imu_ctx_t * p_ctx)
{
    lsm303_accel_t     accel  = { 0, 0, 0 };
    uint32_t           total  = 0u;
    imu_sample_t const sample = imu_read_sample(&accel, &total);

    if (IMU_SAMPLE_BAD == sample)
    {
        p_ctx->invalid_run++;

        if (CFG_IMU_INVALID_LIMIT == p_ctx->invalid_run)
        {
            imu_set_ok(p_ctx, false);
            nav_set_health(NAV_FLG_IMU_OK, false);
            safety_report_fault(FAULT_CAUSE_IMU_DATA, FAULT_SRC_IMU, 0u);
        }
    }
    else if (IMU_SAMPLE_OVERRANGE == sample)
    {
        /* Above CFG_IMU_MAX_G_MG: a jolt.  Collision detection was
           removed (requirement change), so the sample is only left out
           of the tilt, hump, class and lift checks. */
        p_ctx->motion_class = IMU_CLASS_IMPACT;
    }
    else
    {
        int32_t roll  = 0;
        int32_t pitch = 0;
        int32_t const fwd = accel.x_mg - p_ctx->fwd0_mg;
        int32_t const lat = accel.y_mg - p_ctx->lat0_mg;
        imu_hump_evt_t hump_evt = IMU_HUMP_EVT_NONE;

        if (p_ctx->invalid_run >= CFG_IMU_INVALID_LIMIT)
        {
            nav_set_health(NAV_FLG_IMU_OK, true);   /* data back */
        }

        p_ctx->invalid_run = 0u;
        imu_calc_tilt(accel.x_mg, accel.y_mg, accel.z_mg, &roll, &pitch);
        p_ctx->roll_cdeg  = fixmath_wrap_cdeg(roll - p_ctx->roll0_cdeg);
        p_ctx->pitch_cdeg = fixmath_wrap_cdeg(pitch - p_ctx->pitch0_cdeg);

        hump_evt = imu_hump_step(&p_ctx->hump, p_ctx->roll_cdeg,
                                 CFG_PERIOD_IMU_MS);

        if (IMU_HUMP_EVT_STARTED == hump_evt)
        {
            imu_send_event(p_ctx, IMU_EVT_HUMP_START, 0, 0);
        }
        else if (IMU_HUMP_EVT_ENDED == hump_evt)
        {
            int32_t const height = imu_calc_hump_height(p_ctx->hump.peak_cdeg);

            p_ctx->humps++;
            p_ctx->max_height_mm = (height > p_ctx->max_height_mm)
                                   ? height : p_ctx->max_height_mm;
            imu_send_event(p_ctx, IMU_EVT_HUMP_END, p_ctx->hump.peak_cdeg,
                           height);
        }
        else
        {
            /* No hump change. */
        }

        imu_update_heading(p_ctx);
        /* |a| is at most 4000 mg x sqrt(3): fits int32. */
        p_ctx->motion_class = imu_classifier_step(&p_ctx->classifier, fwd, lat,
                                                  (int32_t) total,
                                                  p_ctx->rate_cdps,
                                                  p_ctx->roll_cdeg,
                                                  p_ctx->hump.b_on_hump);

        {
            imu_lift_evt_t const lift = imu_lift_step(&p_ctx->lift,
                                                      accel.x_mg, accel.y_mg,
                                                      accel.z_mg);

            if ((IMU_LIFT_NONE != lift) && nav_lift_watch())
            {
                imu_lifted(p_ctx, lift);
            }
            else if (imu_lift_clear(&p_ctx->lift))
            {
                p_ctx->b_lift_sent = false;
                imu_set_ok(p_ctx, true);
            }
            else
            {
                /* Building up, or out of a run: nothing to report. */
            }
        }

        if (0u == (p_ctx->cycle % CFG_IMU_HEADING_EVERY))
        {
            imu_send_event(p_ctx, IMU_EVT_HEADING, 0, 0);
        }
    }
}

/*
 * Read the accelerometer and sort the sample (imu_calc_check_sample):
 * OK, BAD (the read failed or |a| is implausibly low) or OVERRANGE.
 * *p_total_mg is |a| (0 when the read failed).
 */
static imu_sample_t
imu_read_sample (lsm303_accel_t * p_accel, uint32_t * p_total_mg)
{
    bool const b_read_ok = lsm303_read_accel(p_accel);

    *p_total_mg = b_read_ok ? imu_calc_norm(p_accel->x_mg, p_accel->y_mg,
                                            p_accel->z_mg)
                            : 0u;

    return (imu_calc_check_sample(b_read_ok, *p_total_mg));
}

/*
 * Lifted or overturned during a run: report one LIFTED fault to Safety
 * (detail 1 = lifted, 2 = overturned), which brakes the motors at once,
 * and keep the IMU's "cause cleared" off until the car is back upright
 * and still.
 */
static void
imu_lifted (imu_ctx_t * p_ctx, imu_lift_evt_t event)
{
    if (!p_ctx->b_lift_sent && !safety_is_faulted())
    {
        uint16_t const detail = (IMU_LIFT_OVERTURNED == event) ? 2u : 1u;

        safety_report_fault(FAULT_CAUSE_LIFTED, FAULT_SRC_IMU, detail);
        tm_printf((UB *) "imu: %s (up %d mg)\n",
                  (UB *) ((2u == detail) ? "overturned" : "lifted"),
                  (int) p_ctx->lift.last_up_mg);
    }

    p_ctx->b_lift_sent = true;
    imu_set_ok(p_ctx, false);
}

/* Tell Safety whether the IMU's fault causes are clear (only changes). */
static void
imu_set_ok (imu_ctx_t * p_ctx, bool b_ok)
{
    if (b_ok != p_ctx->b_ok)
    {
        p_ctx->b_ok = b_ok;
        safety_set_imu_ok(b_ok);
    }
}

/*
 * New magnetometer sample: the tilt-compensated heading, and the turn
 * rate "taken from the compass on flat ground" (report 6.4).  On a hump
 * the tilt distorts the compass, so the rate keeps its last value and
 * the next one is measured from the first heading after the hump (D-77).
 */
static void
imu_update_heading (imu_ctx_t * p_ctx)
{
    lsm303_mag_t mag   = { 0, 0, 0 };
    bool         b_new = false;

    if (lsm303_read_mag(&mag, &b_new) && b_new &&
        (IMU_MAG_OVERFLOW != mag.x_raw) && (IMU_MAG_OVERFLOW != mag.y_raw) &&
        (IMU_MAG_OVERFLOW != mag.z_raw))
    {
        uint32_t const now = app_time_ms();
        int32_t const heading = imu_calc_heading(
            mag.x_raw, mag.y_raw, mag.z_raw,
            p_ctx->roll_cdeg + p_ctx->roll0_cdeg,
            p_ctx->pitch_cdeg + p_ctx->pitch0_cdeg);

        if (p_ctx->hump.b_on_hump)
        {
            p_ctx->b_rate_ref = false;      /* tilted: no rate */
        }
        else
        {
            if (p_ctx->b_rate_ref)
            {
                int32_t const dt = (int32_t) (now - p_ctx->rate_ref_ms);

                if (dt > 0)
                {
                    int32_t const change = fixmath_wrap_cdeg(
                        heading - p_ctx->rate_ref_cdeg);
                    int32_t const rate = fixmath_div_round(
                        change * IMU_MS_PER_S, dt);

                    p_ctx->rate_cdps += (rate - p_ctx->rate_cdps) /
                                        CFG_IMU_RATE_SMOOTH;
                }
            }

            p_ctx->rate_ref_cdeg = heading;
            p_ctx->rate_ref_ms   = now;
            p_ctx->b_rate_ref    = true;
        }

        p_ctx->heading_cdeg    = heading;
        p_ctx->b_heading_valid = true;
    }
}

/* IF-03: one event to Navigation (a full queue is a fault). */
static void
imu_send_event (imu_ctx_t const * p_ctx, uint8_t type, int32_t peak_cdeg,
                int32_t height_mm)
{
    imu_evt_t evt;

    /* Packed (D-89) with what Navigation reads.  Every value fits: the
       heading is (-18000, 18000], the peak |roll| 0..18000 and the
       height at most the table's 65 mm. */
    if (IMU_EVT_HEADING == type)
    {
        (void) imu_evt_heading(&evt, p_ctx->heading_cdeg,
                               !p_ctx->hump.b_on_hump);
    }
    else if (IMU_EVT_HUMP_END == type)
    {
        (void) imu_evt_hump_end(&evt, peak_cdeg, height_mm);
    }
    else
    {
        imu_evt_hump_start(&evt);
    }

    if ((IMU_EVT_HEADING != type) || p_ctx->b_heading_valid)
    {
        (void) nav_post(mbf_imu_evt, &evt, (INT) sizeof(evt), FAULT_SRC_IMU);
    }
}

/* IF-12: IMU status for Telemetry every 200 ms (FR8, FR17; packed,
   D-92).  Its time goes with it: Telemetry prints it on a motion event.
   The rate is cut to 16 bits as before; the class is 0..6, the hump
   count modulo 256 and the highest hump at most 65 mm (the table). */
static void
imu_send_status (imu_ctx_t const * p_ctx)
{
    tlm_msg_t msg;

    tlm_msg_init(&msg, TLM_MSG_IMU);
    msg.u.ev.time_ms = app_time_ms();
    (void) tlm_imu_pack(&msg.u.ev.u.imu, (uint8_t) p_ctx->motion_class,
                        (uint32_t) p_ctx->humps, p_ctx->max_height_mm,
                        (int16_t) (p_ctx->rate_cdps / IMU_CDEG_PER_DEG));
    tlm_post(&msg, false);
}

/*** end of file ***/
