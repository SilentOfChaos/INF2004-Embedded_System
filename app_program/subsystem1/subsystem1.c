/*
 *  subsystem1.c
 *  Subsystem 1 (Buddy 1): creates its objects and starts its tasks.
 *
 *  Files in this subsystem:
 *    command_task.c    Command Task: MQTT commands, link check
 *    telemetry_task.c  Telemetry Task: 5 Hz telemetry, 1 Hz heartbeat,
 *                      events, route log and run summary
 *    mqtt_bridge.c     hooks for the template's MQTT code (A11)
 *    cmd_parse.c       command parser (pure logic)
 *    tlm_format.c      telemetry messages (pure logic)
 *    tlm_json.c        bounded JSON writer (pure logic)
 *    tlm_msgs.c        packed message records (pure logic)
 */

#include "../common/app_mbf.h"
#include "../common/app_stats.h"
#include "command_task.h"
#include "comms.h"
#include "subsystem1.h"
#include "telemetry_task.h"

/* Queue depths (messages).  Telemetry drains mbf_tlm every 200 ms: six
   status records per tick plus bursts of events.  The MQTT code drains
   mbf_publish every 10 ms.  [DECISION D-59]  The record queues hold
   exactly their packed records (common/app_mbf.h, D-83): mbf_tlm the
   27 it really held before (24 were asked for, with 8 bytes of margin
   per message; D-92), mbf_mqtt_cfg 2 (D-91).  mbf_publish and
   mbf_cmd_in carry MQTT text, which the minimum-bit change does not
   touch: they keep their bytes, 8 bytes of margin per message. */
#define S1_DEPTH_TLM            27u
#define S1_DEPTH_PUBLISH        8u
#define S1_DEPTH_CMD_IN         4u
#define S1_DEPTH_MQTT_CFG       2u
#define S1_MBF_MARGIN           8u      /* bytes per message, see above */
#define S1_MARGIN_BYTES(depth, size)                                      \
    ((depth) * ((size) + S1_MBF_MARGIN))
#define S1_DROP_COUNT_MAX       0x7FFF  /* drop counter saturates here */

EXPORT ID mbf_tlm        = 0;
EXPORT ID mbf_publish    = 0;
EXPORT ID mbf_cmd_in     = 0;
EXPORT ID mbf_mqtt_cfg   = 0;
EXPORT ID g_flg_comms    = 0;
EXPORT ID g_sem_tlm_drop = 0;

static char const * const g_topic_names[COMMS_TOPIC_COUNT] =
{
    CFG_TOPIC_TELEMETRY, CFG_TOPIC_EVENT, CFG_TOPIC_STATUS, CFG_TOPIC_ACK
};

static ID subsystem1_make_mbf(uint32_t msg_size, uint32_t buf_size);

EXPORT ER
subsystem1_init (void)
{
    T_CFLG cflg = { 0 };
    T_CSEM csem = { 0 };
    ER     ercd = E_OK;

    cflg.flgatr  = TA_TFIFO | TA_WMUL;
    cflg.iflgptn = 0u;
    g_flg_comms = tk_cre_flg(&cflg);

    csem.sematr  = TA_TFIFO | TA_FIRST;
    csem.isemcnt = 0;
    csem.maxsem  = S1_DROP_COUNT_MAX;
    g_sem_tlm_drop = tk_cre_sem(&csem);

    mbf_tlm      = subsystem1_make_mbf((uint32_t) sizeof(tlm_msg_t),
                                       APP_MBF_BYTES(S1_DEPTH_TLM,
                                                     sizeof(tlm_msg_t)));
    mbf_publish  = subsystem1_make_mbf(COMMS_PUBLISH_MAX,
                                       S1_MARGIN_BYTES(S1_DEPTH_PUBLISH,
                                                       COMMS_PUBLISH_MAX));
    mbf_cmd_in   = subsystem1_make_mbf(CFG_MQTT_CMD_MAX,
                                       S1_MARGIN_BYTES(S1_DEPTH_CMD_IN,
                                                       CFG_MQTT_CMD_MAX));
    mbf_mqtt_cfg = subsystem1_make_mbf((uint32_t) sizeof(comms_broker_t),
                                       APP_MBF_BYTES(S1_DEPTH_MQTT_CFG,
                                                     sizeof(comms_broker_t)));
    app_stats_queue("tlm", mbf_tlm);
    app_stats_queue("publish", mbf_publish);
    app_stats_queue("cmd_in", mbf_cmd_in);
    app_stats_queue("mqtt_cfg", mbf_mqtt_cfg);

    if (g_flg_comms < E_OK)
    {
        ercd = g_flg_comms;
    }
    else if (g_sem_tlm_drop < E_OK)
    {
        ercd = g_sem_tlm_drop;
    }
    else if ((mbf_tlm < E_OK) || (mbf_publish < E_OK) ||
             (mbf_cmd_in < E_OK) || (mbf_mqtt_cfg < E_OK))
    {
        ercd = E_NOMEM;
    }
    else
    {
        ercd = telemetry_task_init();
    }

    return (ercd);
}

EXPORT ER
subsystem1_start (void)
{
    ER ercd = telemetry_task_start();

    if (E_OK == ercd)
    {
        ercd = command_task_start();
    }

    return (ercd);
}

/*
 * IF-12: post a record to Telemetry.  Never blocks.  A full queue drops
 * the record and counts it (telemetry is not needed to drive, NFR10;
 * D-68).  Called from tasks on both cores: only kernel objects are used.
 */
void
tlm_post (tlm_msg_t * p_msg, bool b_urgent)
{
    if ((NULL != p_msg) &&
        (E_OK == tk_snd_mbf(mbf_tlm, p_msg, (INT) sizeof(*p_msg), TMO_POL)))
    {
        if (b_urgent)
        {
            (void) tk_set_flg(g_flg_comms, COMMS_FLG_TLM_URGENT);
        }
    }
    else
    {
        (void) tk_sig_sem(g_sem_tlm_drop, 1);
    }
}

/* Topic of an mbf_publish record. */
char const *
comms_topic_name (uint8_t topic)
{
    return ((topic < COMMS_TOPIC_COUNT) ? g_topic_names[topic]
                                        : CFG_TOPIC_EVENT);
}

/* True while the MQTT session is up (set by the bridge). */
bool
comms_mqtt_up (void)
{
    T_RFLG rflg = { 0 };

    return ((E_OK == tk_ref_flg(g_flg_comms, &rflg)) &&
            (0u != (rflg.flgptn & COMMS_FLG_MQTT_UP)));
}

static ID
subsystem1_make_mbf (uint32_t msg_size, uint32_t buf_size)
{
    T_CMBF cmbf = { 0 };

    cmbf.mbfatr = TA_TFIFO;
    cmbf.maxmsz = (INT) msg_size;
    cmbf.bufsz  = (SZ) buf_size;

    return (tk_cre_mbf(&cmbf));
}

/*** end of file ***/
