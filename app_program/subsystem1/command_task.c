/*
 *  command_task.c
 *  The Command Task (report section 3): priority 11, core 1.
 *
 *  Waits for MQTT commands from the bridge (IF-14) and acts on each one
 *  (cmd_parse.h lists the format):
 *    remap, default_map  -> Navigation (IF-10), which acknowledges once
 *                           the letter map has changed (FR6, NFR8)
 *    summary             -> Telemetry publishes the run summary (FR14)
 *    stop                -> Safety latches a fault and stops the car
 *                           (FR14, FR15)
 *    start, reset        -> same as the START (GP20) / RESET (GP21)
 *                           buttons, for the dashboard
 *    broker              -> new broker for the MQTT code (NFR14, D-16),
 *                           acknowledged first and applied
 *                           CFG_CMD_BROKER_DELAY_MS later, so the ack
 *                           still leaves on the old broker
 *    anything else       -> acknowledged with ok 0
 *  Every 100 ms it checks the Wi-Fi and MQTT link and reports a change
 *  to Telemetry, and to Safety as the Wi-Fi loss bit of the fault mask,
 *  a status bit that never stops the car (D-103).  The car never waits
 *  for this task (NFR10).
 */

#include "../common/app_stats.h"
#include "../common/app_time.h"
#include "../navigation/navigation.h"
#include "../subsystem4/subsystem4.h"
#include "cmd_parse.h"
#include "command_task.h"
#include "comms.h"
#include "subsystem1.h"

#if defined(TM_WIFI_CYW43) && TM_WIFI_CYW43
#include "cyw43_utk.h"
#endif

typedef struct
{
    bool           b_link_known;    /* a link state was reported */
    tlm_link_t     link;            /* last reported */
    bool           b_loss_known;    /* Safety has a Wi-Fi loss state */
    bool           b_loss_sent;     /* the one Safety has */
    bool           b_broker_due;    /* a broker change is waiting */
    uint32_t       broker_due_ms;
    comms_broker_t broker;
} command_ctx_t;

static void    command_task(INT stacd, void * p_exinf);
static void    command_handle(command_ctx_t * p_ctx, char const * p_text,
                              uint32_t len);
static void    command_ack(uint8_t code, uint32_t seq, bool b_ok);
static void    command_periodic(command_ctx_t * p_ctx, uint32_t now_ms);
static void    command_report_loss(command_ctx_t * p_ctx, bool b_lost);
static int32_t command_wifi_status(void);

ER
command_task_start (void)
{
    ID const tid = app_start_task_stack((FP) command_task, CFG_PRI_COMMAND,
                                        APP_CORE1, CFG_STACK_COMMAND,
                                        "command_task");

    return ((tid < E_OK) ? tid : E_OK);
}

static void
command_task (INT stacd, void * p_exinf)
{
    static command_ctx_t ctx;                       /* this task only */
    static char          text[CFG_MQTT_CMD_MAX];    /* this task only */
    uint32_t             next_check_ms = app_time_ms() +
                                         CFG_PERIOD_LINKCHECK_MS;

    (void) stacd;
    (void) p_exinf;

    app_stats_task_start(APP_STATS_COMMAND);

    for (;;)
    {
        uint32_t const now  = app_time_ms();
        int32_t  const wait = (int32_t) (next_check_ms - now);  /* wraps */
        INT            len  = 0;

        len = tk_rcv_mbf(mbf_cmd_in, text,
                         (wait > 0) ? (TMO) wait : TMO_POL);

        if (len > 0)
        {
            command_handle(&ctx, text, (uint32_t) len);
        }

        if ((int32_t) (app_time_ms() - next_check_ms) >= 0)
        {
            app_stats_release(APP_STATS_COMMAND);
            command_periodic(&ctx, app_time_ms());
            app_stats_work_end(APP_STATS_COMMAND);
            next_check_ms += CFG_PERIOD_LINKCHECK_MS;

            if ((int32_t) (app_time_ms() - next_check_ms) >= 0)
            {
                /* Late by a whole period: start counting again. */
                next_check_ms = app_time_ms() + CFG_PERIOD_LINKCHECK_MS;
            }
        }
    }
}

/* One command from the cmd topic. */
static void
command_handle (command_ctx_t * p_ctx, char const * p_text, uint32_t len)
{
    cmd_t     cmd;
    nav_cmd_t nav = { 0 };
    tlm_msg_t msg;

    (void) cmd_parse(p_text, len, &cmd);

    switch (cmd.code)
    {
        case CMD_CODE_REMAP:
        case CMD_CODE_MAP_DEFAULT:
            /* cmd_parse() checked every field: it packs exactly (D-91). */
            (void) nav_cmd_pack(&nav, (CMD_CODE_REMAP == cmd.code)
                                      ? NAV_CMD_REMAP : NAV_CMD_MAP_DEFAULT,
                                cmd.letter, cmd.action, cmd.seq);

            /* Navigation acknowledges; a full queue is a fault (D-68). */
            if (E_OK != nav_post(mbf_nav_cmd, &nav, (INT) sizeof(nav),
                                 FAULT_SRC_COMMAND))
            {
                command_ack(cmd.code, cmd.seq, false);
            }
        break;

        case CMD_CODE_SUMMARY:
            command_ack(cmd.code, cmd.seq, true);
            tlm_msg_init(&msg, TLM_MSG_SUMMARY);
            msg.u.ev.time_ms = app_time_ms();
            tlm_post(&msg, true);
        break;

        case CMD_CODE_STOP:
            safety_report_fault(FAULT_CAUSE_STOP_CMD, FAULT_SRC_COMMAND, 0u);
            command_ack(cmd.code, cmd.seq, true);
        break;

        case CMD_CODE_START:
            /* Dashboard START: acts like GP20 (only in READY, no fault). */
            nav_request_start();
            command_ack(cmd.code, cmd.seq, true);
            tm_printf((UB *) "command: start\n");
        break;

        case CMD_CODE_RESET:
            /* Dashboard RESET: acts like GP21 (clears a fault whose cause
               is gone, or SAFE HOLD / COMPLETE -> READY). */
            safety_request_reset();
            command_ack(cmd.code, cmd.seq, true);
            tm_printf((UB *) "command: reset\n");
        break;

        case CMD_CODE_BROKER:
            /* cmd_parse() checked the address (digits and dots, at most
               15 characters): it packs exactly (D-91). */
            (void) comms_broker_pack(&p_ctx->broker, cmd.ip, cmd.port);
            p_ctx->b_broker_due  = true;
            p_ctx->broker_due_ms = app_time_ms() + CFG_CMD_BROKER_DELAY_MS;
            command_ack(cmd.code, cmd.seq, true);
            tm_printf((UB *) "command: broker %s:%d in %d ms\n",
                      (UB *) cmd.ip, (INT) cmd.port,
                      (INT) CFG_CMD_BROKER_DELAY_MS);
        break;

        default:
            command_ack(CMD_CODE_UNKNOWN, cmd.seq, false);
        break;
    }
}

/* Answer on the ack topic (report 6.1; packed, D-92).  Always fits: a
   seq is at most CMD_SEQ_MAX (cmd_parse()) and a code 0..5. */
static void
command_ack (uint8_t code, uint32_t seq, bool b_ok)
{
    tlm_msg_t msg;

    tlm_msg_init(&msg, TLM_MSG_ACK);
    msg.u.ev.time_ms = app_time_ms();
    (void) tlm_ack_pack(&msg.u.ev.u.ack, seq, code, b_ok);
    tlm_post(&msg, true);
}

/* Every 100 ms: link check, and a broker change once it is due. */
static void
command_periodic (command_ctx_t * p_ctx, uint32_t now_ms)
{
    tlm_link_t link;

    /* Always fits: CYW43 link states are -3..3 (packed, D-92). */
    (void) tlm_link_pack(&link, comms_mqtt_up(), command_wifi_status());

    if (!p_ctx->b_link_known ||
        (tlm_link_online(&link) != tlm_link_online(&p_ctx->link)) ||
        (tlm_link_wifi(&link) != tlm_link_wifi(&p_ctx->link)))
    {
        tlm_msg_t msg;

        p_ctx->b_link_known = true;
        p_ctx->link         = link;
        tlm_msg_init(&msg, TLM_MSG_LINK);
        msg.u.ev.time_ms    = now_ms;
        msg.u.ev.u.link     = link;
        tlm_post(&msg, true);
        tm_printf((UB *) "command: wifi %d mqtt %d\n",
                  (INT) tlm_link_wifi(&link),
                  tlm_link_online(&link) ? (INT) 1 : (INT) 0);
    }

    command_report_loss(p_ctx, !tlm_link_online(&link));

    if (p_ctx->b_broker_due && ((int32_t) (now_ms - p_ctx->broker_due_ms)
                                >= 0))
    {
        p_ctx->b_broker_due = false;

        if (E_OK != tk_snd_mbf(mbf_mqtt_cfg, &p_ctx->broker,
                               (INT) sizeof(p_ctx->broker), TMO_POL))
        {
            tm_printf((UB *) "command: broker change lost\n");
        }
    }
}

/*
 * Wi-Fi loss (no MQTT link: Wi-Fi down or the broker unreachable) is a
 * status bit in Safety's fault mask, never a fault (NFR10, D-103).
 * Sent when it changes; if Safety's queue was full, again at the next
 * check.
 */
static void
command_report_loss (command_ctx_t * p_ctx, bool b_lost)
{
    if ((!p_ctx->b_loss_known || (b_lost != p_ctx->b_loss_sent)) &&
        safety_report_status(FAULT_CAUSE_WIFI_LOSS, FAULT_SRC_COMMAND,
                             b_lost))
    {
        p_ctx->b_loss_known = true;
        p_ctx->b_loss_sent  = b_lost;
    }
}

/* CYW43 link status (cyw43_utk.h); 0 in a build without Wi-Fi. */
static int32_t
command_wifi_status (void)
{
    int32_t status = 0;

#if defined(TM_WIFI_CYW43) && TM_WIFI_CYW43
    T_CYW43_UTK_STATUS radio = { 0 };

    cyw43_utk_get_status(&radio);
    status = radio.link_status;
#endif

    return (status);
}

/*** end of file ***/
