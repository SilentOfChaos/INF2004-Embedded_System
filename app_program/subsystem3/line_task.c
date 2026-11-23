/*
 *  line_task.c
 *  The Line Task (report section 3, statechart 6.3 / Fig 8).
 *
 *  Every 5 ms, released by a cyclic handler:
 *    - read both line sensors, run LINE TRACKING (line_track.c) and
 *      publish the latest line error under mtx_line_err (IF-02);
 *    - feed the Barcode ISR's edges to the BARCODE region (bc_capture.c);
 *    - send junction, line lost/found and barcode events to Navigation
 *      (IF-01);
 *    - check in with Safety for the watchdog;
 *    - every 200 ms, a status record for Telemetry (FR12).
 *  The two regions run in the same 5 ms step, so line tracking never
 *  pauses for a barcode (report 6.3).  In RUN OVER the sensing is idle
 *  but the task keeps checking in.
 */

#include "../app.h"
#include "../common/app_gpio.h"
#include "../common/app_mbf.h"
#include "../common/bitpack.h"
#include "../common/app_stats.h"
#include "../common/app_time.h"
#include "../navigation/navigation.h"
#include "../subsystem1/subsystem1.h"
#include "../subsystem4/subsystem4.h"
#include "bc_capture.h"
#include "drv_barcode.h"
#include "drv_line_adc.h"
#include "line_keep.h"
#include "line_task.h"
#include "line_track.h"
#include "subsystem3.h"

/* Event flag bits of flg_line. */
#define LINE_FLG_TICK           0x0001u /* cyclic handler */
#define LINE_FLG_EDGE_OVF       0x0002u /* Barcode ISR: queue was full */
#define LINE_FLG_ADAPT_OFF      0x0100u /* level: Navigation, HUMP CROSSING */
#define LINE_FLG_RUN_OVER       0x0200u /* level: Navigation, run over */

#define LINE_POLL_MAX           3u      /* barcode outputs per cycle */
#define LINE_US_PER_MS          1000u
#define LINE_EDGE_BUF_BYTES     APP_MBF_BYTES(BARCODE_EDGE_QUEUE, \
                                              sizeof(barcode_edge_t))

/* The status record (IF-12) shows "no letter yet" as line_keep does. */
BITPACK_ASSERT(LINE_KEEP_NO_LETTER == TLM_NO_LETTER, line_no_letter);

static ID g_flg_line   = 0;
static ID g_cyc_line   = 0;
static ID g_mtx_err    = 0;
static ID g_mbf_edges  = 0;

/* IF-02: the latest line record, two packed words (line_msgs.h).  One
   of the two shared variables the report justifies (section 4):
   Navigation only needs the newest value.  Written by the Line task and
   read by Navigation, both only while holding g_mtx_err, and always as
   whole words, so a reader never sees half of an update. */
static line_err_t g_line_err;

static void line_cyclic_handler(void * p_exinf);
static void line_task(INT stacd, void * p_exinf);
static void line_sample(line_track_t * p_track, bool b_adapt,
                        uint32_t elapsed_ms, uint32_t * p_raw_l,
                        uint32_t * p_raw_r, bool * p_in_range);
static void line_barcode_step(bc_capture_t * p_cap, line_keep_t * p_keep);
static void line_send_event(uint8_t type, char letter, bool b_reversed);
static void line_send_status(line_track_t const * p_track,
                             line_keep_t const * p_keep);

/* Create the Line task's kernel objects (usermain phase 1). */
ER
line_task_init (void)
{
    T_CFLG cflg = { 0 };
    T_CCYC ccyc = { 0 };
    T_CMTX cmtx = { 0 };
    T_CMBF cmbf = { 0 };
    ER     ercd = E_OK;

    cflg.flgatr  = TA_TFIFO | TA_WMUL;
    cflg.iflgptn = 0u;
    g_flg_line = tk_cre_flg(&cflg);

    ccyc.cycatr = TA_HLNG;
    ccyc.cychdr = (FP) line_cyclic_handler;    /* kernel calls it with exinf */
    ccyc.cyctim = CFG_PERIOD_LINE_MS;
    ccyc.cycphs = CFG_PERIOD_LINE_MS;
    g_cyc_line = tk_cre_cyc(&ccyc);

    /* Priority inheritance: Navigation (7) never blocks Line (6) long. */
    cmtx.mtxatr = TA_INHERIT;
    g_mtx_err = tk_cre_mtx(&cmtx);

    cmbf.mbfatr = TA_TFIFO;
    cmbf.bufsz  = (SZ) LINE_EDGE_BUF_BYTES;
    cmbf.maxmsz = (INT) sizeof(barcode_edge_t);
    g_mbf_edges = tk_cre_mbf(&cmbf);
    app_stats_queue("bc_edges", g_mbf_edges);

    if (g_flg_line < E_OK)
    {
        ercd = g_flg_line;
    }
    else if (g_cyc_line < E_OK)
    {
        ercd = g_cyc_line;
    }
    else if (g_mtx_err < E_OK)
    {
        ercd = g_mtx_err;
    }
    else if (g_mbf_edges < E_OK)
    {
        ercd = g_mbf_edges;
    }
    else
    {
        ercd = E_OK;
    }

    return (ercd);
}

/* Start the Line task and its 5 ms cyclic handler (usermain phase 2). */
ER
line_task_start (void)
{
    ER       ercd = E_OK;
    ID const tid  = app_start_task_stack((FP) line_task, CFG_PRI_LINE,
                                         APP_CORE0, CFG_STACK_DEFAULT,
                                         "line_task");

    if (tid < E_OK)
    {
        ercd = tid;
    }
    else
    {
        ercd = tk_sta_cyc(g_cyc_line);
    }

    return (ercd);
}

/* IF-02: copy the latest line-error record. */
ER
line_err_read (line_err_t * p_out)
{
    ER ercd = E_PAR;

    if (NULL != p_out)
    {
        ercd = tk_loc_mtx(g_mtx_err, TMO_FEVR);

        if (E_OK == ercd)
        {
            *p_out = g_line_err;
            ercd = tk_unl_mtx(g_mtx_err);
        }
    }

    return (ercd);
}

/* Navigation: freeze level adaptation during HUMP CROSSING (G7). */
void
line_set_adaptation (bool b_enable)
{
    if (b_enable)
    {
        (void) tk_clr_flg(g_flg_line, ~LINE_FLG_ADAPT_OFF);
    }
    else
    {
        (void) tk_set_flg(g_flg_line, LINE_FLG_ADAPT_OFF);
    }
}

/* Navigation: RUN OVER (sensing idle) or active again. */
void
line_set_run_over (bool b_run_over)
{
    if (b_run_over)
    {
        (void) tk_set_flg(g_flg_line, LINE_FLG_RUN_OVER);
    }
    else
    {
        (void) tk_clr_flg(g_flg_line, ~LINE_FLG_RUN_OVER);
    }
}

/* Cyclic handler, interrupt context: release the Line task. */
static void
line_cyclic_handler (void * p_exinf)
{
    (void) p_exinf;
    (void) tk_set_flg(g_flg_line, LINE_FLG_TICK);
}

/* The Line task. */
static void
line_task (INT stacd, void * p_exinf)
{
    line_track_t track;
    bc_capture_t capture;
    line_keep_t  keep;      /* kept between cycles, packed (D-87) */
    uint32_t     tick_us = 0u;  /* when the last 5 ms release ran */

    (void) stacd;
    (void) p_exinf;

    app_stats_task_start(APP_STATS_LINE);

    /* LOAD LEVELS, then ACTIVE with both regions. */
    line_track_init(&track);
    line_keep_init(&keep);
    CHECK(line_adc_open(), "line_adc_open");
    CHECK(barcode_init(g_mbf_edges, g_flg_line, LINE_FLG_EDGE_OVF),
          "barcode_init");
    bc_capture_init(&capture, barcode_is_dark(), app_time_us());
    tick_us = app_time_us();

#if (CFG_PIN_TIMING_PROBE >= 0)
    app_gpio_output((uint32_t) CFG_PIN_TIMING_PROBE, false);
#endif

    for (;;)
    {
        UINT flags = 0u;
        bool b_in_range = true;

        (void) tk_wai_flg(g_flg_line, (LINE_FLG_TICK | LINE_FLG_EDGE_OVF),
                          (TWF_ORW | TWF_BITCLR), &flags, TMO_FEVR);

        if (0u != (flags & LINE_FLG_EDGE_OVF))
        {
            /* The ISR dropped an edge, newer than every queued edge:
               marked now, and again after the next drain (D-78). */
            bc_capture_mark_lost(&capture);
            line_keep_set_edges_dropped(&keep, true);
        }

        if (0u != (flags & LINE_FLG_TICK))
        {
            T_RFLG         rflg = { 0 };
            bool           b_run_over = false;
            bool           b_adapt = true;
            uint32_t const now_us = app_time_us();
            /* The real time since the last release, not the nominal
               5 ms, so a late or missed release (the tick flag's bits
               merge) does not stretch the 300 ms "line lost" time (24 Oct
               review, R-05); it is only compared with that time. */
            uint32_t       elapsed_ms = (now_us - tick_us +
                                         (LINE_US_PER_MS / 2u)) /
                                        LINE_US_PER_MS;

            tick_us    = now_us;
            elapsed_ms = (elapsed_ms < (uint32_t) CFG_LINE_LOST_MS)
                         ? elapsed_ms : (uint32_t) CFG_LINE_LOST_MS;

#if (CFG_PIN_TIMING_PROBE >= 0)
            /* A12 timing probe: one toggle per 5 ms release, not on a
               wake-up for a full edge queue alone (S3-U33). */
            out_w(HW_SIO_GPIO_OUT_XOR, (1u << CFG_PIN_TIMING_PROBE));
#endif
            app_stats_release(APP_STATS_LINE);
            safety_checkin(SAFETY_CHECKIN_LINE);
            (void) tk_ref_flg(g_flg_line, &rflg);
            b_run_over = (0u != (rflg.flgptn & LINE_FLG_RUN_OVER));
            b_adapt    = (0u == (rflg.flgptn & LINE_FLG_ADAPT_OFF));

            if (b_run_over)
            {
                /* RUN OVER: sensing idle, edges discarded.  A run of
                   failed reads does not span the idle time: its count
                   starts again, so D-79 can report when sensing resumes
                   even if its one report came just before (D-96).  No
                   reading is not a good sample: the health drops and
                   comes back only after a whole window of new good
                   reads (D-98). */
                barcode_edge_t edge;

                b_in_range = false;
                (void) line_track_read_result(&track, true);

                while (tk_rcv_mbf(g_mbf_edges, &edge, TMO_POL) > 0)
                {
                    /* Drop the edge. */
                }

                bc_capture_init(&capture, barcode_is_dark(),
                                app_time_us());
            }
            else
            {
                /* A failed read can leave either reading as it was. */
                uint32_t raw_l = line_keep_raw_l(&keep);
                uint32_t raw_r = line_keep_raw_r(&keep);

                line_sample(&track, b_adapt, elapsed_ms, &raw_l, &raw_r,
                            &b_in_range);
                (void) line_keep_set_raw(&keep, raw_l, raw_r);  /* 12-bit */
                line_barcode_step(&capture, &keep);
            }

            /* The queue has been emptied. */
            line_keep_set_edges_dropped(&keep, false);

            if (line_keep_note_sample(&keep, b_in_range))
            {
                /* The first bad sample of this window: the health drops
                   now, so SELF-TEST cannot pass on the last window while
                   reads fail (D-96); it comes back after a whole good
                   window, whose good reads also re-arm D-79's count. */
                nav_set_health(NAV_FLG_LINE_OK, false);
            }

            if (line_keep_next_cycle(&keep))
            {
                nav_set_health(NAV_FLG_LINE_OK, line_keep_window_ok(&keep));
                line_keep_set_window_ok(&keep, true);
                line_send_status(&track, &keep);
            }

            app_stats_work_end(APP_STATS_LINE);
        }
    }
}

/*
 * LINE TRACKING step: read the sensors, update the shared record and
 * send any junction or line lost/found event.
 */
static void
line_sample (line_track_t * p_track, bool b_adapt, uint32_t elapsed_ms,
             uint32_t * p_raw_l, uint32_t * p_raw_r, bool * p_in_range)
{
    line_track_evt_t event = LINE_TRACK_EVT_NONE;
    ER const         ercd  = line_adc_read(p_raw_l, p_raw_r);

    if (E_OK != ercd)
    {
        /* A failed read (G15, D-79): the record is marked invalid, so
           Navigation never steers on it as if it were new, and
           CFG_LINE_READ_FAIL_LIMIT failures in a row latch a fault. */
        *p_in_range = false;

        if (E_OK == tk_loc_mtx(g_mtx_err, TMO_FEVR))
        {
            line_err_set_invalid(&g_line_err);
            (void) tk_unl_mtx(g_mtx_err);
        }

        if (line_track_read_result(p_track, false))
        {
            /* detail: the kernel's main error code, made positive
               (for example 57 for E_IO). */
            safety_report_fault(FAULT_CAUSE_LINE_DATA, FAULT_SRC_LINE,
                                (uint16_t) (-MERCD(ercd)));
        }
    }
    else
    {
        (void) line_track_read_result(p_track, true);

        *p_in_range = ((*p_raw_l >= CFG_LINE_SELFTEST_MIN) &&
                       (*p_raw_l <= CFG_LINE_SELFTEST_MAX) &&
                       (*p_raw_r >= CFG_LINE_SELFTEST_MIN) &&
                       (*p_raw_r <= CFG_LINE_SELFTEST_MAX));

        event = line_track_sample(p_track, *p_raw_l, *p_raw_r, elapsed_ms,
                                  b_adapt);

        if (E_OK == tk_loc_mtx(g_mtx_err, TMO_FEVR))
        {
            /* Always fits: the error is -1000..1000 (line_track.c) and
               the readings are 12-bit (drv_line_adc.c). */
            (void) line_err_set_sample(&g_line_err,
                                       line_track_error_pm(p_track),
                                       *p_raw_l, *p_raw_r,
                                       line_track_line_seen(p_track));
            (void) tk_unl_mtx(g_mtx_err);
        }
    }

    if (LINE_TRACK_EVT_JUNCTION == event)
    {
        line_send_event(LINE_EVT_JUNCTION, '\0', false);
    }
    else if (LINE_TRACK_EVT_LOST == event)
    {
        line_send_event(LINE_EVT_LOST, '\0', false);
    }
    else if (LINE_TRACK_EVT_FOUND == event)
    {
        line_send_event(LINE_EVT_FOUND, '\0', false);
    }
    else
    {
        /* No line event this sample. */
    }
}

/*
 * BARCODE step: take the queued edges, then report what happened.  If
 * the ISR dropped an edge since the last step, the dropped edge came
 * after the queued ones, so a capture they started has lost an edge
 * inside its symbol too (D-78).
 */
static void
line_barcode_step (bc_capture_t * p_cap, line_keep_t * p_keep)
{
    barcode_edge_t edge;

    while (tk_rcv_mbf(g_mbf_edges, &edge, TMO_POL) > 0)
    {
        /* The clock is read after the edge left the queue, so the edge
           is never newer than it (barcode_edge.h). */
        bc_capture_edge(p_cap, barcode_edge_time(&edge, app_time_us()),
                        barcode_edge_dark(&edge));
    }

    if (line_keep_edges_dropped(p_keep))
    {
        bc_capture_mark_lost(p_cap);
    }

    for (uint32_t poll = 0u; poll < LINE_POLL_MAX; poll++)
    {
        bc_out_t const out = bc_capture_poll(p_cap, app_time_us());

        if (BC_OUT_STARTED == out.type)
        {
            line_send_event(LINE_EVT_CAPTURE_STARTED, '\0', false);
        }
        else if (BC_OUT_LETTER == out.type)
        {
            line_keep_count_letter(p_keep, out.result.letter);
            line_send_event(LINE_EVT_LETTER, out.result.letter,
                            out.result.b_reversed);
        }
        else if (BC_OUT_FAILED == out.type)
        {
            line_send_event(LINE_EVT_CAPTURE_FAILED, '\0', false);
        }
        else
        {
            break;  /* nothing more this cycle */
        }
    }
}

/* IF-01: one packed event to Navigation (a full queue is a fault). */
static void
line_send_event (uint8_t type, char letter, bool b_reversed)
{
    line_evt_t event;

    /* Always fits: the types are LINE_EVT_xxx and a letter is 'A'..'Z'
       (bc_decode) or '\0'. */
    (void) line_evt_pack(&event, type, letter, b_reversed);

    (void) nav_post(mbf_line_evt, &event, (INT) sizeof(event),
                    FAULT_SRC_LINE);
}

/* IF-12: status record for Telemetry every 200 ms (packed, D-92). */
static void
line_send_status (line_track_t const * p_track, line_keep_t const * p_keep)
{
    tlm_msg_t msg;

    /* Always fits: the readings are 12-bit, the error is +/-1000, the
       letter count is modulo 256 and the letter 'A'..'Z' or '-'. */
    tlm_msg_init(&msg, TLM_MSG_LINE);
    (void) tlm_line_pack(&msg.u.line, line_keep_raw_l(p_keep),
                         line_keep_raw_r(p_keep),
                         line_track_error_pm(p_track),
                         line_keep_letters(p_keep),
                         line_keep_last_letter(p_keep));
    tlm_post(&msg, false);
}

/*** end of file ***/
