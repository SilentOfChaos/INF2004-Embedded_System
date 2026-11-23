/*
 *  safety_task.c
 *  The Safety Task (report section 3, sections 4 and 6.4, SD2 / Fig 4).
 *
 *  Highest application priority (5), core 0, every 10 ms and at once on
 *  a fault event:
 *    - a fault (collision, STOP, invalid IMU or line data, failed
 *      SELF-TEST, full control queue, motor stall, encoder fault) is
 *      latched: the motor pins are forced LOW through the GPIO output
 *      override (A14, pending instructor agreement), the FAULT flag
 *      holds Motion in BRAKED, buzzer and LED raise the alarm (FR15,
 *      NFR6, NFR16);
 *    - Safety alone writes the fault mask (one bit per cause, imu_msgs.h)
 *      from the reports in its queue, so no two tasks ever write it; a
 *      status cause (no echo, Wi-Fi loss) only sets or clears its bit
 *      and never brakes the car (NFR10); a copy goes to Telemetry;
 *    - RESET (GP21) with the cause cleared: Motion is asked to clear its
 *      queue, command and PID state, and only after its acknowledgement
 *      is the override released and SELF-TEST re-run (SD2); a fault
 *      that arrives before the acknowledgement cancels the reset and
 *      becomes the latched cause (G20, D-95; safety_latch.h);
 *    - RESET with no fault: Navigation is told (SAFE HOLD -> READY);
 *    - the hardware watchdog is fed only while Line, Navigation, Motion,
 *      IMU and Obstacle Scan have all checked in within their deadlines.
 */

#include "../app.h"
#include "../common/app_gpio.h"
#include "../common/app_mbf.h"
#include "../common/app_stats.h"
#include "../common/app_time.h"
#include "../common/hw_regs.h"
#include "../navigation/navigation.h"
#include "../subsystem1/subsystem1.h"
#include "../subsystem2/subsystem2.h"
#include "drv_buzzer.h"
#include "drv_neopixel.h"
#include "drv_watchdog.h"
#include "safety_latch.h"
#include "safety_task.h"
#include "subsystem4.h"

#if defined(TM_WIFI_CYW43) && TM_WIFI_CYW43
#include "cyw43_utk.h"
#endif

/* Fault reports the queue holds: what it really held before the
   minimum-bit change (16 were asked for, with 8 bytes to spare each),
   kept so a burst overflows exactly as before (D-83, D-89). */
#define SAFETY_FAULT_DEPTH      21u
#define SAFETY_TASKS            5u
#define SAFETY_STATUS_EVERY     (CFG_PERIOD_TELEMETRY_MS / \
                                 CFG_PERIOD_SAFETY_MS)
#define SAFETY_DEBOUNCE_TICKS   (CFG_BUTTON_DEBOUNCE_MS / CFG_PERIOD_SAFETY_MS)
#define SAFETY_COUNT_MAX        0xFFFFu

typedef struct
{
    safety_latch_t latch;           /* the fault latch and RESET state */
    uint32_t ack_deadline_ms;
    bool     b_wdt_armed;
    uint32_t start_ms;
    uint32_t last_checkin_ms[SAFETY_TASKS];
    bool     b_late;
    uint16_t faults;
    uint16_t missed;
    uint8_t  queue_full;
    uint32_t ticks;
    bool     b_btn_pressed;
    uint32_t btn_stable;
    bool     b_alarm_on;
    uint32_t alarm_since_ms;
} safety_ctx_t;

EXPORT ID flg_safety = 0;
EXPORT ID mbf_fault  = 0;
static ID g_cyc_safety = 0;

static uint32_t const g_checkin_bits[SAFETY_TASKS] =
{
    SAFETY_CHECKIN_LINE, SAFETY_CHECKIN_NAV, SAFETY_CHECKIN_MOTION,
    SAFETY_CHECKIN_IMU, SAFETY_CHECKIN_SCAN
};

static uint32_t const g_checkin_ms[SAFETY_TASKS] =
{
    CFG_CHECKIN_LINE_MS, CFG_CHECKIN_NAV_MS, CFG_CHECKIN_MOTION_MS,
    CFG_CHECKIN_IMU_MS, CFG_CHECKIN_SCAN_MS
};

static void safety_cyclic_handler(void * p_exinf);
static void safety_task(INT stacd, void * p_exinf);
static void safety_pins_low(bool b_low);
static void safety_handle_report(safety_ctx_t * p_ctx,
                                 fault_evt_t const * p_evt);
static void safety_release(safety_ctx_t * p_ctx);
static void safety_tick(safety_ctx_t * p_ctx);
static void safety_watchdog(safety_ctx_t * p_ctx, uint32_t now);
static void safety_reset_button(safety_ctx_t * p_ctx);
static bool safety_cause_cleared(safety_ctx_t const * p_ctx);
static void safety_alarm(safety_ctx_t * p_ctx, uint32_t now);
static void safety_send_fault(safety_ctx_t const * p_ctx, bool b_latched);
static void safety_send_status(safety_ctx_t const * p_ctx);
static bool safety_radio_started(void);

ER
safety_task_init (void)
{
    T_CFLG cflg = { 0 };
    T_CMBF cmbf = { 0 };
    T_CCYC ccyc = { 0 };
    ER     ercd = E_OK;

    cflg.flgatr  = TA_TFIFO | TA_WMUL;
    cflg.iflgptn = 0u;
    flg_safety = tk_cre_flg(&cflg);

    cmbf.mbfatr = TA_TFIFO;
    cmbf.maxmsz = (INT) sizeof(fault_evt_t);
    cmbf.bufsz  = (SZ) APP_MBF_BYTES(SAFETY_FAULT_DEPTH,
                                     sizeof(fault_evt_t));
    mbf_fault = tk_cre_mbf(&cmbf);
    app_stats_queue("fault", mbf_fault);

    ccyc.cycatr = TA_HLNG;
    ccyc.cychdr = (FP) safety_cyclic_handler;  /* kernel calls it with exinf */
    ccyc.cyctim = CFG_PERIOD_SAFETY_MS;
    ccyc.cycphs = CFG_PERIOD_SAFETY_MS;
    g_cyc_safety = tk_cre_cyc(&ccyc);

    if (flg_safety < E_OK)
    {
        ercd = flg_safety;
    }
    else if (mbf_fault < E_OK)
    {
        ercd = mbf_fault;
    }
    else if (g_cyc_safety < E_OK)
    {
        ercd = g_cyc_safety;
    }
    else
    {
        ercd = E_OK;
    }

    return (ercd);
}

ER
safety_task_start (void)
{
    ER       ercd = E_OK;
    ID const tid  = app_start_task_stack((FP) safety_task, CFG_PRI_SAFETY,
                                         APP_CORE0, CFG_STACK_DEFAULT,
                                         "safety_task");

    if (tid < E_OK)
    {
        ercd = tid;
    }
    else
    {
        ercd = tk_sta_cyc(g_cyc_safety);
    }

    return (ercd);
}

/* ---- IF-04 / IF-09 functions used by the other tasks --------------- */

/* Queue a fault for Safety; if the queue is full, Safety still latches
   a fault from the FAULT_LOST bit, so no fault is ever lost. */
void
safety_report_fault (uint8_t cause, uint8_t source, uint16_t detail)
{
    fault_evt_t evt;

    /* Packed (D-89).  Every reporter's values fit: causes 1..11,
       sources 1..8, details at most FAULT_DETAIL_MAX. */
    (void) fault_evt_pack(&evt, cause, source, detail);

    if (E_OK == tk_snd_mbf(mbf_fault, &evt, (INT) sizeof(evt), TMO_POL))
    {
        (void) tk_set_flg(flg_safety, SAFETY_FLG_FAULT_EVT);
    }
    else
    {
        (void) tk_set_flg(flg_safety, SAFETY_FLG_FAULT_LOST);
    }
}

/*
 * Queue a status cause's start (b_on) or end for Safety.  Returns false
 * if the queue was full: the caller tries again later.  A full queue
 * never raises FAULT_LOST here, because a status cause must never latch
 * a fault (NFR10).
 */
bool
safety_report_status (uint8_t cause, uint8_t source, bool b_on)
{
    fault_evt_t evt;
    bool        b_sent = false;

    /* Packed (D-89).  Every reporter passes a status cause and a source
       1..8. */
    (void) fault_evt_pack_status(&evt, cause, source, b_on);

    if (E_OK == tk_snd_mbf(mbf_fault, &evt, (INT) sizeof(evt), TMO_POL))
    {
        (void) tk_set_flg(flg_safety, SAFETY_FLG_FAULT_EVT);
        b_sent = true;
    }

    return (b_sent);
}

bool
safety_is_faulted (void)
{
    T_RFLG rflg = { 0 };

    (void) tk_ref_flg(flg_safety, &rflg);

    return (0u != (rflg.flgptn & SAFETY_FLG_FAULTED));
}

void
safety_checkin (uint32_t checkin_bit)
{
    (void) tk_set_flg(flg_safety, checkin_bit);
}

void
safety_ack_zero (void)
{
    (void) tk_set_flg(flg_safety, SAFETY_FLG_ZERO_ACK);
}

void
safety_set_imu_ok (bool b_ok)
{
    if (b_ok)
    {
        (void) tk_set_flg(flg_safety, SAFETY_FLG_IMU_OK);
    }
    else
    {
        (void) tk_clr_flg(flg_safety, ~SAFETY_FLG_IMU_OK);
    }
}

/* ------------------------------------------------------------------ */

static void
safety_cyclic_handler (void * p_exinf)
{
    (void) p_exinf;
    (void) tk_set_flg(flg_safety, SAFETY_FLG_TICK);
}

static void
safety_task (INT stacd, void * p_exinf)
{
    static safety_ctx_t ctx;    /* kept off the task stack */

    (void) stacd;
    (void) p_exinf;

    app_stats_task_start(APP_STATS_SAFETY);
    safety_latch_init(&ctx.latch);
    ctx.start_ms = app_time_ms();

    for (uint32_t idx = 0u; idx < SAFETY_TASKS; idx++)
    {
        ctx.last_checkin_ms[idx] = ctx.start_ms;
    }

    if (watchdog_caused_reset())
    {
        tm_printf((UB *) "safety: last reset was a WATCHDOG timeout\n");
    }

    buzzer_init();
    (void) neopixel_init(CFG_PIN_STATUS_LED);
    neopixel_set(0u, CFG_LED_BRIGHTNESS, 0u);
    app_gpio_input(CFG_PIN_RESET, (0 != CFG_BUTTON_ACTIVE_LOW)
                                  ? APP_GPIO_PULL_UP : APP_GPIO_PULL_DOWN);

    for (;;)
    {
        UINT        flags = 0u;
        fault_evt_t evt;

        (void) tk_wai_flg(flg_safety, SAFETY_FLG_WAKE, (TWF_ORW | TWF_BITCLR),
                          &flags, TMO_FEVR);

        /* Faults first: the override goes on within this wake-up (NFR6). */
        while (tk_rcv_mbf(mbf_fault, &evt, TMO_POL) > 0)
        {
            safety_handle_report(&ctx, &evt);
        }

        if (0u != (flags & SAFETY_FLG_FAULT_LOST))
        {
            (void) fault_evt_pack(&evt, FAULT_CAUSE_LOST_RECORD,
                                  FAULT_SRC_SAFETY, 0u);
            safety_handle_report(&ctx, &evt);
        }

        /* Faults first, then the acknowledgement: a fault in the same
           wake-up cancels the reset before it can release (G20). */
        if ((0u != (flags & SAFETY_FLG_ZERO_ACK)) &&
            safety_latch_ack(&ctx.latch))
        {
            safety_release(&ctx);
        }

        if (0u != (flags & SAFETY_FLG_TICK))
        {
            app_stats_release(APP_STATS_SAFETY);
            safety_tick(&ctx);
            app_stats_work_end(APP_STATS_SAFETY);
        }
    }
}

/* Force GP8-GP11 low through the output override, or release it. */
static void
safety_pins_low (bool b_low)
{
#if CFG_SAFETY_PIN_OVERRIDE
    uint32_t const pins[] =
    {
        CFG_PIN_MOTOR_L_A, CFG_PIN_MOTOR_L_B,
        CFG_PIN_MOTOR_R_A, CFG_PIN_MOTOR_R_B
    };

    for (uint32_t idx = 0u; idx < (sizeof(pins) / sizeof(pins[0])); idx++)
    {
        if (b_low)
        {
            /* OUTOVER 00 -> 10 (drive low) and OEOVER -> 11 (enable) in
               one atomic write; Motion's PWM registers are untouched. */
            set_w(HW_GPIO_CTRL(pins[idx]), (HW_GPIO_CTRL_OUTOVER_LOW |
                                            HW_GPIO_CTRL_OEOVER_ENABLE));
        }
        else
        {
            clr_w(HW_GPIO_CTRL(pins[idx]), (HW_GPIO_CTRL_OUTOVER_MASK |
                                            HW_GPIO_CTRL_OEOVER_MASK));
        }
    }
#else
    (void) b_low;   /* A14 not agreed: braking through Motion only */
#endif
}

/* A report: a status cause only sets or clears its mask bit; a fault is
   latched, cancels a waiting reset (G20), or is counted if one is
   already latched (safety_latch.h). */
static void
safety_handle_report (safety_ctx_t * p_ctx, fault_evt_t const * p_evt)
{
    uint8_t const            cause     = fault_evt_cause(p_evt);
    uint8_t const            source    = fault_evt_source(p_evt);
    bool const               b_cleared = fault_evt_cleared(p_evt);
    safety_latch_out_t const out       = safety_latch_report(&p_ctx->latch,
                                             cause, source,
                                             fault_evt_detail(p_evt),
                                             b_cleared);

    if (SAFETY_LATCH_STATUS == out)
    {
        /* Never a fault: not counted, nothing braked (NFR10).  No
           console line from the highest-priority task: the mask goes
           to Telemetry every 200 ms, and the Command task prints each
           link change itself. */
    }
    else
    {
        if ((FAULT_CAUSE_QUEUE_FULL == cause) &&
            (p_ctx->queue_full < UINT8_MAX))
        {
            /* Stops at 255, the telemetry field's largest value (24 Oct
               review, R-16), like the other counts. */
            p_ctx->queue_full++;
        }

        if (p_ctx->faults < SAFETY_COUNT_MAX)
        {
            p_ctx->faults++;
        }
    }

    if (SAFETY_LATCH_LATCHED == out)
    {
        safety_pins_low(true);
        (void) tk_set_flg(flg_safety, SAFETY_FLG_FAULTED);
        p_ctx->b_alarm_on      = true;
        p_ctx->alarm_since_ms  = app_time_ms();
        buzzer_on(true);
        neopixel_set(CFG_LED_BRIGHTNESS, 0u, 0u);
        safety_send_fault(p_ctx, true);
        tm_printf((UB *) "safety: FAULT latched, cause %d from %d\n",
                  (INT) cause, (INT) source);
    }
    else if (SAFETY_LATCH_RESET_CANCELLED == out)
    {
        /* The override, the FAULT flag and the alarm are still on:
           report the new latched cause; RESET must be pressed again. */
        safety_send_fault(p_ctx, true);
        tm_printf((UB *) "safety: RESET cancelled by FAULT cause %d "
                  "from %d\n", (INT) cause, (INT) source);
    }
    else
    {
        /* Already latched: counted only.  Or a status bit. */
    }
}

/* Motion acknowledged the reset: release the brake, re-run SELF-TEST. */
static void
safety_release (safety_ctx_t * p_ctx)
{
    safety_pins_low(false);
    (void) tk_clr_flg(flg_safety, ~SAFETY_FLG_FAULTED);
    p_ctx->b_alarm_on = false;
    buzzer_on(false);
    neopixel_set(0u, CFG_LED_BRIGHTNESS, 0u);
    safety_send_fault(p_ctx, false);
    (void) tk_set_flg(flg_nav, NAV_FLG_RESTART);
    tm_printf((UB *) "safety: fault released after RESET\n");
}

/* The 10 ms work. */
static void
safety_tick (safety_ctx_t * p_ctx)
{
    uint32_t const now = app_time_ms();

    p_ctx->ticks++;

    if (p_ctx->latch.b_latched)
    {
        safety_pins_low(true);  /* keep the override in place */
    }

    if (p_ctx->latch.b_reset_pending &&
        ((int32_t) (now - p_ctx->ack_deadline_ms) > 0))
    {
        safety_latch_ack_timeout(&p_ctx->latch);
        tm_printf((UB *) "safety: no acknowledgement from Motion\n");
    }

    safety_watchdog(p_ctx, now);
    safety_reset_button(p_ctx);
    safety_alarm(p_ctx, now);

    if (0u == (p_ctx->ticks % SAFETY_STATUS_EVERY))
    {
        safety_send_status(p_ctx);
    }
}

/* Check-in deadlines, arming and feeding the watchdog (report sec. 3). */
static void
safety_watchdog (safety_ctx_t * p_ctx, uint32_t now)
{
    UINT pattern = 0u;
    bool b_all_in = true;

    /* Read and clear the check-in bits in one atomic call. */
    if (E_OK == tk_wai_flg(flg_safety, SAFETY_CHECKIN_ALL,
                           (TWF_ORW | TWF_BITCLR), &pattern, TMO_POL))
    {
        for (uint32_t idx = 0u; idx < SAFETY_TASKS; idx++)
        {
            if (0u != (pattern & g_checkin_bits[idx]))
            {
                p_ctx->last_checkin_ms[idx] = now;
            }
        }
    }

    for (uint32_t idx = 0u; idx < SAFETY_TASKS; idx++)
    {
        if ((now - p_ctx->last_checkin_ms[idx]) > g_checkin_ms[idx])
        {
            b_all_in = false;
        }
    }

    if (!b_all_in && !p_ctx->b_late && (p_ctx->missed < SAFETY_COUNT_MAX))
    {
        p_ctx->missed++;
    }

    p_ctx->b_late = !b_all_in;

    if ((0 != CFG_WDT_ENABLE) && !p_ctx->b_wdt_armed &&
        (safety_radio_started() ||
         ((now - p_ctx->start_ms) >= CFG_WDT_ARM_MAX_WAIT_MS)))
    {
        watchdog_arm(CFG_WDT_TIMEOUT_MS);
        p_ctx->b_wdt_armed = true;
    }

    if (p_ctx->b_wdt_armed && b_all_in)
    {
        watchdog_feed();
    }
}

/* Press RESET remotely (dashboard "reset" command). */
void
safety_request_reset (void)
{
    (void) tk_set_flg(flg_safety, SAFETY_FLG_REMOTE_RESET);
}

/* RESET button (GP21, owned by Safety), debounced, acts once per press. */
static void
safety_reset_button (safety_ctx_t * p_ctx)
{
    bool const b_level  = app_gpio_read(CFG_PIN_RESET);
    bool const b_active = (0 != CFG_BUTTON_ACTIVE_LOW) ? !b_level : b_level;
    bool       b_press  = false;

    if (b_active != p_ctx->b_btn_pressed)
    {
        p_ctx->btn_stable++;

        if (p_ctx->btn_stable >= SAFETY_DEBOUNCE_TICKS)
        {
            p_ctx->b_btn_pressed = b_active;
            p_ctx->btn_stable    = 0u;
            b_press              = b_active;
        }
    }
    else
    {
        p_ctx->btn_stable = 0u;
    }

    /* RESET asked for over MQTT (dashboard): one press, like GP21. */
    {
        T_RFLG rflg = { 0 };

        (void) tk_ref_flg(flg_safety, &rflg);

        if (0u != (rflg.flgptn & SAFETY_FLG_REMOTE_RESET))
        {
            (void) tk_clr_flg(flg_safety, ~SAFETY_FLG_REMOTE_RESET);
            b_press = true;
        }
    }

    if (b_press && p_ctx->latch.b_latched && !p_ctx->latch.b_reset_pending)
    {
        if (safety_latch_reset(&p_ctx->latch, safety_cause_cleared(p_ctx)))
        {
            p_ctx->ack_deadline_ms = app_time_ms() +
                                     CFG_SAFETY_ACK_TIMEOUT_MS;
            motion_request_reset();
        }
        else
        {
            tm_printf((UB *) "safety: RESET refused, cause still present\n");
        }
    }
    else if (b_press && !p_ctx->latch.b_latched)
    {
        (void) tk_set_flg(flg_nav, NAV_FLG_RESET);
    }
    else
    {
        /* No press, or a reset already in progress. */
    }
}

/* "RESET, cause cleared" (report 6.4): IMU causes need healthy data.
   Every cause since the latch is in the mask, so an IMU fault counted
   after another one is checked too. */
static bool
safety_cause_cleared (safety_ctx_t const * p_ctx)
{
    uint16_t const imu_bits  = (uint16_t) (FAULT_BIT(FAULT_CAUSE_LIFTED) |
                                           FAULT_BIT(FAULT_CAUSE_IMU_DATA));
    bool           b_cleared = true;

    if (0u != (p_ctx->latch.mask & imu_bits))
    {
        T_RFLG rflg = { 0 };

        (void) tk_ref_flg(flg_safety, &rflg);
        b_cleared = (0u != (rflg.flgptn & SAFETY_FLG_IMU_OK));
    }

    return (b_cleared);
}

/* Alarm while latched: buzzer and red LED blink together. */
static void
safety_alarm (safety_ctx_t * p_ctx, uint32_t now)
{
    if (p_ctx->latch.b_latched)
    {
        uint32_t const period = p_ctx->b_alarm_on ? CFG_ALARM_ON_MS
                                                  : CFG_ALARM_OFF_MS;

        if ((now - p_ctx->alarm_since_ms) >= period)
        {
            p_ctx->b_alarm_on     = !p_ctx->b_alarm_on;
            p_ctx->alarm_since_ms = now;
            buzzer_on(p_ctx->b_alarm_on);
            neopixel_set(p_ctx->b_alarm_on ? CFG_LED_BRIGHTNESS : 0u, 0u, 0u);
        }
    }
}

/* IF-12: fault latched or released, for telemetry and the route log
   (packed, D-92).  Always fits: the cause, source and detail are those
   of a packed fault report (IF-04), sent only once one is latched. */
static void
safety_send_fault (safety_ctx_t const * p_ctx, bool b_latched)
{
    tlm_msg_t msg;

    tlm_msg_init(&msg, TLM_MSG_FAULT);
    msg.u.ev.time_ms = app_time_ms();
    (void) tlm_fault_pack(&msg.u.ev.u.fault, p_ctx->latch.cause,
                          p_ctx->latch.source, p_ctx->latch.detail,
                          b_latched);
    tlm_post(&msg, true);
}

/* IF-12: Safety status every 200 ms with a copy of the fault mask
   (queue-full count: report sec. 4; packed, D-92).  Always fits: the
   counters are 8 and 16 bits here, the mask FAULT_MASK_BITS. */
static void
safety_send_status (safety_ctx_t const * p_ctx)
{
    tlm_msg_t msg;

    tlm_msg_init(&msg, TLM_MSG_SAFETY);
    (void) tlm_safety_pack(&msg.u.safety, p_ctx->latch.b_latched,
                           p_ctx->latch.b_latched ? p_ctx->latch.cause : 0u,
                           p_ctx->b_wdt_armed, (uint32_t) p_ctx->queue_full,
                           (uint32_t) p_ctx->faults,
                           (uint32_t) p_ctx->missed,
                           (uint32_t) p_ctx->latch.mask);
    tlm_post(&msg, false);
}

/* True once the radio driver has finished its start-up (A1). */
static bool
safety_radio_started (void)
{
    bool b_started = true;

#if defined(TM_WIFI_CYW43) && TM_WIFI_CYW43
    T_CYW43_UTK_STATUS status;

    cyw43_utk_get_status(&status);
    b_started = (0u != status.init_complete);
#endif

    return (b_started);
}

/*** end of file ***/
