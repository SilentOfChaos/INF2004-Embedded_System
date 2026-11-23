/*
 *  mqtt_bridge.c
 *  Subsystem 1's side of the template's MQTT code (A11, pending
 *  instructor agreement; TEAM.md section 5, docs/decisions.md D-14).
 *
 *  lib/liblwip/sysdepend/pico_rp2040/mqtt_utk.c calls the hooks below,
 *  always from inside cyw43_utk_task, which owns the radio, lwIP and
 *  MQTT.  They only move records through message buffers with TMO_POL,
 *  so they never block that task:
 *    IF-13  mbf_publish   Telemetry -> MQTT code (topic number, JSON)
 *    IF-14  mbf_cmd_in    MQTT code -> Command   (command text)
 *    IF-15  mbf_mqtt_cfg  Command -> MQTT code   (new broker, NFR14)
 *  The session state goes to g_flg_comms (COMMS_FLG_MQTT_UP).
 *
 *  Built into the image only with MQTT=1; without it the template's weak
 *  defaults stay and nothing is published.
 */

#include "../subsystem4/subsystem4.h"
#include "comms.h"
#include "subsystem1.h"

#if defined(TM_MQTT) && TM_MQTT

#include "mqtt_utk.h"

static bool bridge_text_equal(char const * p_left, char const * p_right);

/* Next message for the MQTT code to publish, or -1 when there is none. */
int
mqtt_utk_hook_next_publish (char * p_topic, int topic_size,
                            unsigned char * p_payload, int payload_size)
{
    static uint8_t g_record[COMMS_PUBLISH_MAX];     /* this task only */
    INT const      len    = tk_rcv_mbf(mbf_publish, g_record, TMO_POL);
    int            result = -1;

    if ((len > 1) && ((len - 1) <= payload_size) && (topic_size > 0))
    {
        char const * p_name = comms_topic_name(g_record[0]);
        int          idx    = 0;

        while ((idx < (topic_size - 1)) && ('\0' != p_name[idx]))
        {
            p_topic[idx] = p_name[idx];
            idx++;
        }

        p_topic[idx] = '\0';

        for (INT pos = 1; pos < len; pos++)
        {
            p_payload[pos - 1] = g_record[pos];
        }

        result = (int) (len - 1);
    }

    return (result);
}

/* One complete incoming message: commands go to the Command task. */
void
mqtt_utk_hook_receive (char const * p_topic, unsigned char const * p_payload,
                       int len)
{
    if (bridge_text_equal(p_topic, CFG_TOPIC_CMD) && (len > 0) &&
        (len <= (int) CFG_MQTT_CMD_MAX))
    {
        if (E_OK != tk_snd_mbf(mbf_cmd_in, p_payload, (INT) len, TMO_POL))
        {
            /* Command queue full: the sender gets no ack and can retry
               (D-68).  A STOP is never lost that way: it goes straight
               to Safety, as the Command task would send it (24 Oct
               review, R-04).  The parser is plain C and never blocks. */
            if (cmd_is_stop((char const *) p_payload, (uint32_t) len))
            {
                safety_report_fault(FAULT_CAUSE_STOP_CMD, FAULT_SRC_COMMAND,
                                    0u);
            }

            (void) tk_sig_sem(g_sem_tlm_drop, 1);
        }
    }
}

/* The car listens on the cmd topic only (report 6.1). */
char const *
mqtt_utk_hook_subscribe_topic (void)
{
    return (CFG_TOPIC_CMD);
}

/* 1 when the Command task has asked for a new broker (NFR14). */
int
mqtt_utk_hook_broker (char * p_ip, int ip_size, int * p_port)
{
    comms_broker_t broker;
    int            result = 0;

    if (((INT) sizeof(broker) == tk_rcv_mbf(mbf_mqtt_cfg, &broker, TMO_POL))
        && (ip_size >= (int) CMD_IP_MAX))
    {
        /* The address text exactly as the command gave it (D-91). */
        comms_broker_ip(&broker, p_ip, CMD_IP_MAX);
        *p_port = (int) comms_broker_port(&broker);
        result  = 1;
    }

    return (result);
}

/* IS19 publishes its own 1 Hz heartbeat on the status topic (NFR9). */
int
mqtt_utk_hook_heartbeat (void)
{
    return (0);
}

void
mqtt_utk_hook_link (int connected)
{
    if (0 != connected)
    {
        (void) tk_set_flg(g_flg_comms, COMMS_FLG_MQTT_UP);
    }
    else
    {
        (void) tk_clr_flg(g_flg_comms, ~COMMS_FLG_MQTT_UP);
    }
}

static bool
bridge_text_equal (char const * p_left, char const * p_right)
{
    uint32_t idx = 0u;

    while (('\0' != p_left[idx]) && (p_left[idx] == p_right[idx]))
    {
        idx++;
    }

    return (p_left[idx] == p_right[idx]);
}

#endif /* TM_MQTT */

/*** end of file ***/
