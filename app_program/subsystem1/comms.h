/*
 *  comms.h
 *  Private to subsystem 1: the objects and records shared by the Command
 *  task, the Telemetry task and the MQTT bridge (mqtt_bridge.c).  Other
 *  subsystems use subsystem1.h only.
 */

#ifndef COMMS_H
#define COMMS_H

#include "../app.h"
#include "cmd_parse.h"

/* g_flg_comms bits. */
#define COMMS_FLG_MQTT_UP       0x0001u /* MQTT session up (bridge) */
#define COMMS_FLG_TLM_TICK      0x0010u /* Telemetry 200 ms cyclic */
#define COMMS_FLG_TLM_URGENT    0x0020u /* urgent record in mbf_tlm */
#define COMMS_FLG_TLM_WAKE      (COMMS_FLG_TLM_TICK | COMMS_FLG_TLM_URGENT)

/* Topic number, the first byte of every mbf_publish record (IF-13). */
#define COMMS_TOPIC_TELEMETRY   0u
#define COMMS_TOPIC_EVENT       1u
#define COMMS_TOPIC_STATUS      2u
#define COMMS_TOPIC_ACK         3u
#define COMMS_TOPIC_COUNT       4u

/* An mbf_publish record: the topic number, then the JSON payload
   without its final NUL. */
#define COMMS_PUBLISH_MAX       (1u + CFG_MQTT_PAYLOAD_MAX)

/* An mbf_mqtt_cfg record (IF-15): comms_broker_t, packed (cmd_parse.h,
   D-91). */

IMPORT ID g_flg_comms;          /* bits above */
IMPORT ID g_sem_tlm_drop;       /* counts records tlm_post() dropped */

char const * comms_topic_name(uint8_t topic);
bool         comms_mqtt_up(void);

#endif /* COMMS_H */

/*** end of file ***/
