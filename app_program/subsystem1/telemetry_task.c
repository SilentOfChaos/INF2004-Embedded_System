/*
 *  telemetry_task.c
 *  The Telemetry Task (report section 3): priority 12 (the lowest),
 *  core 1, every 200 ms, and at once for an urgent record.
 *
 *    - Drains mbf_tlm (IF-12).  Status records update the state; route
 *      entries, obstacle profiles (FR10), acks, faults, motion-class
 *      changes and link changes are published as soon as they arrive.
 *    - Every 200 ms one telemetry record (5 Hz), every second one
 *      heartbeat on the status topic (1 Hz), NFR9.
 *    - A summary request (FR14 on command, FR18 at the end of a run)
 *      publishes the run summary, then replays the route log (FR13) a
 *      few entries per tick so the publish queue does not overflow.
 *  Messages reach the MQTT code through mbf_publish (IF-13), only while
 *  the MQTT session is up; while it is down they are not queued and the
 *  route log keeps its entries for a later summary.  A message that does
 *  not fit or finds the queue full is dropped and counted (D-68), never
 *  sent truncated.  Nothing here is needed to drive the car (NFR10).
 */

#include "../common/app_stats.h"
#include "../common/app_time.h"
#include "../navigation/navigation.h"
#include "comms.h"
#include "subsystem1.h"
#include "telemetry_task.h"
#include "tlm_format.h"

/* Payload buffer: the JSON writer keeps one byte for its NUL. */
#define TLM_BUF_SIZE            (CFG_MQTT_PAYLOAD_MAX + 1u)

/* Heartbeat every CFG_HEARTBEAT_MS, counted in telemetry ticks. */
#if (CFG_HEARTBEAT_MS < CFG_PERIOD_TELEMETRY_MS) || \
    (0 != (CFG_HEARTBEAT_MS % CFG_PERIOD_TELEMETRY_MS))
#error "CFG_HEARTBEAT_MS must be a multiple of CFG_PERIOD_TELEMETRY_MS"
#endif
#define TLM_HEARTBEAT_EVERY     ((uint32_t) (CFG_HEARTBEAT_MS / \
                                             CFG_PERIOD_TELEMETRY_MS))

typedef struct
{
    tlm_state_t state;
    uint32_t    seq;                /* one number for every message */
    uint32_t    ticks;
    uint32_t    beats;
    bool        b_replaying;
    uint32_t    replay_next;        /* next log entry to publish again */
    bool        b_class_known;
    uint8_t     last_class;
    uint8_t     record[1u + TLM_BUF_SIZE];  /* topic, payload, NUL */
} tlm_ctx_t;

static ID g_cyc_tlm = 0;

static void tlm_cyclic_handler(void * p_exinf);
static void tlm_task(INT stacd, void * p_exinf);
static void tlm_receive(tlm_ctx_t * p_ctx, tlm_msg_t const * p_msg);
static void tlm_periodic(tlm_ctx_t * p_ctx);
static void tlm_replay(tlm_ctx_t * p_ctx);
static bool tlm_send(tlm_ctx_t * p_ctx, uint8_t topic, uint32_t len,
                     bool b_retry);
static void tlm_count_drops(tlm_ctx_t * p_ctx);

static inline char *
tlm_payload (tlm_ctx_t * p_ctx)
{
    return ((char *) &p_ctx->record[1]);
}

ER
telemetry_task_init (void)
{
    T_CCYC ccyc = { 0 };

    ccyc.cycatr = TA_HLNG;
    ccyc.cychdr = (FP) tlm_cyclic_handler;     /* kernel calls it with exinf */
    ccyc.cyctim = CFG_PERIOD_TELEMETRY_MS;
    ccyc.cycphs = CFG_PERIOD_TELEMETRY_MS;
    g_cyc_tlm = tk_cre_cyc(&ccyc);

    return ((g_cyc_tlm < E_OK) ? g_cyc_tlm : E_OK);
}

ER
telemetry_task_start (void)
{
    ER       ercd = E_OK;
    ID const tid  = app_start_task_stack((FP) tlm_task, CFG_PRI_TELEMETRY,
                                         APP_CORE1, CFG_STACK_TELEMETRY,
                                         "telemetry_task");

    if (tid < E_OK)
    {
        ercd = tid;
    }
    else
    {
        ercd = tk_sta_cyc(g_cyc_tlm);
    }

    return (ercd);
}

static void
tlm_cyclic_handler (void * p_exinf)
{
    (void) p_exinf;
    (void) tk_set_flg(g_flg_comms, COMMS_FLG_TLM_TICK);
}

static void
tlm_task (INT stacd, void * p_exinf)
{
    static tlm_ctx_t ctx;       /* about 1.5 KB: kept off the task stack */

    (void) stacd;
    (void) p_exinf;

    app_stats_task_start(APP_STATS_TELEMETRY);
    tlm_state_init(&ctx.state);

    for (;;)
    {
        UINT      flags = 0u;
        tlm_msg_t msg;
        INT       len   = 0;

        (void) tk_wai_flg(g_flg_comms, COMMS_FLG_TLM_WAKE,
                          (TWF_ORW | TWF_BITCLR), &flags, TMO_FEVR);

        len = tk_rcv_mbf(mbf_tlm, &msg, TMO_POL);

        while (len > 0)
        {
            if ((INT) sizeof(msg) == len)
            {
                tlm_receive(&ctx, &msg);
            }

            len = tk_rcv_mbf(mbf_tlm, &msg, TMO_POL);
        }

        if (0u != (flags & COMMS_FLG_TLM_TICK))
        {
            app_stats_release(APP_STATS_TELEMETRY);
            tlm_periodic(&ctx);
            app_stats_work_end(APP_STATS_TELEMETRY);

            /* Measurement builds only: sample queues, print the report. */
            app_stats_report_tick();
        }
    }
}

/* One IF-12 record (packed, D-92).  Only the records Telemetry prints
   with a time carry one (u.ev.time_ms). */
static void
tlm_receive (tlm_ctx_t * p_ctx, tlm_msg_t const * p_msg)
{
    tlm_state_t * const p_state = &p_ctx->state;
    uint32_t            len     = 0u;
    int32_t             index   = 0;

    switch (tlm_msg_type(p_msg))
    {
        case TLM_MSG_LINE:
            p_state->line = p_msg->u.line;
        break;

        case TLM_MSG_MOTION:
            p_state->motion = p_msg->u.motion;
        break;

        case TLM_MSG_IMU:
        {
            tlm_imu_t const * const p_imu = &p_msg->u.ev.u.imu;
            uint8_t const           cls   = tlm_imu_class(p_imu);

            p_state->imu = *p_imu;

            /* FR17 motion events: the class changed since last time. */
            if (p_ctx->b_class_known && (p_ctx->last_class != cls))
            {
                len = tlm_format_motion_event(cls, tlm_imu_rate_dps(p_imu),
                                              p_ctx->seq,
                                              p_msg->u.ev.time_ms,
                                              tlm_payload(p_ctx),
                                              TLM_BUF_SIZE);
                (void) tlm_send(p_ctx, COMMS_TOPIC_EVENT, len, false);
            }

            p_ctx->b_class_known = true;
            p_ctx->last_class    = cls;
        }
        break;

        case TLM_MSG_SCAN:
            p_state->scan = p_msg->u.scan;
        break;

        case TLM_MSG_NAV:
            p_state->nav         = p_msg->u.nav;
            p_state->p_mode_name = nav_mode_name((nav_mode_t)
                                                 tlm_nav_mode(&p_msg->u.nav));
        break;

        case TLM_MSG_SAFETY:
            p_state->safety = p_msg->u.safety;
        break;

        case TLM_MSG_ROUTE:
        {
            tlm_route_t const * const p_route = &p_msg->u.ev.u.route;

            index = tlm_state_route(p_state, p_route, p_msg->u.ev.time_ms);

            if (ROUTE_RUN_START == tlm_route_kind(p_route))
            {
                p_ctx->b_replaying = false;     /* the old log is gone */
            }

            len = tlm_format_route(p_route, p_ctx->seq, p_msg->u.ev.time_ms,
                                   index, false, tlm_payload(p_ctx),
                                   TLM_BUF_SIZE);
            (void) tlm_send(p_ctx, COMMS_TOPIC_EVENT, len, false);
        }
        break;

        case TLM_MSG_ACK:
            len = tlm_format_ack(&p_msg->u.ev.u.ack, p_ctx->seq,
                                 p_msg->u.ev.time_ms, tlm_payload(p_ctx),
                                 TLM_BUF_SIZE);
            (void) tlm_send(p_ctx, COMMS_TOPIC_ACK, len, false);
        break;

        case TLM_MSG_SUMMARY:
            len = tlm_format_summary(p_state, p_ctx->seq, p_msg->u.ev.time_ms,
                                     tlm_payload(p_ctx), TLM_BUF_SIZE);
            (void) tlm_send(p_ctx, COMMS_TOPIC_EVENT, len, false);
            p_ctx->b_replaying = true;
            p_ctx->replay_next = 0u;
        break;

        case TLM_MSG_FAULT:
            len = tlm_format_fault(&p_msg->u.ev.u.fault, p_ctx->seq,
                                   p_msg->u.ev.time_ms, tlm_payload(p_ctx),
                                   TLM_BUF_SIZE);
            (void) tlm_send(p_ctx, COMMS_TOPIC_EVENT, len, false);
        break;

        case TLM_MSG_LINK:
            p_state->link = p_msg->u.ev.u.link;
            len = tlm_format_link(&p_msg->u.ev.u.link, p_ctx->seq,
                                  p_msg->u.ev.time_ms, tlm_payload(p_ctx),
                                  TLM_BUF_SIZE);
            (void) tlm_send(p_ctx, COMMS_TOPIC_EVENT, len, false);
        break;

        case TLM_MSG_OBSTACLE:
            /* FR10: the profile Obstacle Scan measured in PLANNING. */
            len = tlm_format_obstacle(&p_msg->u.ev.u.obstacle, p_ctx->seq,
                                      p_msg->u.ev.time_ms,
                                      tlm_payload(p_ctx), TLM_BUF_SIZE);
            (void) tlm_send(p_ctx, COMMS_TOPIC_EVENT, len, false);
        break;

        default:
            /* Unknown record: ignored. */
        break;
    }
}

/* Every 200 ms: telemetry (5 Hz), heartbeat (1 Hz), route-log replay. */
static void
tlm_periodic (tlm_ctx_t * p_ctx)
{
    uint32_t const now = app_time_ms();
    uint32_t       len = 0u;

    p_ctx->ticks++;
    tlm_count_drops(p_ctx);

    len = tlm_format_telemetry(&p_ctx->state, p_ctx->seq, now,
                               tlm_payload(p_ctx), TLM_BUF_SIZE);
    (void) tlm_send(p_ctx, COMMS_TOPIC_TELEMETRY, len, false);

    if (0u == (p_ctx->ticks % TLM_HEARTBEAT_EVERY))
    {
        p_ctx->beats++;
        len = tlm_format_heartbeat(&p_ctx->state, p_ctx->seq, now,
                                   p_ctx->beats, tlm_payload(p_ctx),
                                   TLM_BUF_SIZE);
        (void) tlm_send(p_ctx, COMMS_TOPIC_STATUS, len, false);
    }

    if (p_ctx->b_replaying)
    {
        tlm_replay(p_ctx);
    }
}

/* FR13: publish the route log again, a few entries per tick. */
static void
tlm_replay (tlm_ctx_t * p_ctx)
{
    uint32_t sent = 0u;

    while (p_ctx->b_replaying && (sent < CFG_TLM_REPLAY_PER_TICK))
    {
        if (p_ctx->replay_next >= p_ctx->state.log_count)
        {
            p_ctx->b_replaying = false;
        }
        else if (!comms_mqtt_up())
        {
            break;              /* paused until the session is back */
        }
        else
        {
            tlm_log_t const * const p_log =
                &p_ctx->state.log[p_ctx->replay_next];
            uint32_t const len =
                tlm_format_route(&p_log->entry, p_ctx->seq, p_log->time_ms,
                                 (int32_t) p_ctx->replay_next, true,
                                 tlm_payload(p_ctx), TLM_BUF_SIZE);

            /* A full queue is not a loss here: the entry is tried again
               on the next tick. */
            if (!tlm_send(p_ctx, COMMS_TOPIC_EVENT, len, true))
            {
                break;
            }

            p_ctx->replay_next++;
            sent++;
        }
    }
}

/*
 * Queue the message in p_ctx->record for the MQTT code.  Returns false
 * only when b_retry is set and the queue was full: the caller sends the
 * same message again later.  Every other message uses up a sequence
 * number, sent or not, so a gap tells the receiver messages were lost.
 */
static bool
tlm_send (tlm_ctx_t * p_ctx, uint8_t topic, uint32_t len, bool b_retry)
{
    bool b_done = true;

    if (0u == len)
    {
        p_ctx->state.dropped++;         /* did not fit (D-49) */
    }
    else if (comms_mqtt_up())
    {
        p_ctx->record[0] = topic;

        /* len <= CFG_MQTT_PAYLOAD_MAX, so the record size fits INT. */
        if (E_OK != tk_snd_mbf(mbf_publish, p_ctx->record, (INT) (1u + len),
                               TMO_POL))
        {
            if (b_retry)
            {
                b_done = false;
            }
            else
            {
                p_ctx->state.dropped++;
            }
        }
    }
    else
    {
        /* MQTT session down: nothing is queued (NFR10). */
    }

    if (b_done)
    {
        if ((0 != CFG_TLM_ECHO_CONSOLE) && (0u != len))
        {
            tm_printf((UB *) "tlm [%s] %s\n",
                      (UB *) comms_topic_name(topic),
                      (UB *) tlm_payload(p_ctx));
        }

        p_ctx->seq++;
    }

    return (b_done);
}

/* Add the records tlm_post() and the bridge could not queue. */
static void
tlm_count_drops (tlm_ctx_t * p_ctx)
{
    T_RSEM rsem = { 0 };

    if ((E_OK == tk_ref_sem(g_sem_tlm_drop, &rsem)) && (rsem.semcnt > 0))
    {
        if (E_OK == tk_wai_sem(g_sem_tlm_drop, rsem.semcnt, TMO_POL))
        {
            p_ctx->state.dropped += (uint32_t) rsem.semcnt;
        }
    }
}

/*** end of file ***/
