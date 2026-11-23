/*
 *	mqtt_utk.h
 *	MQTT client wiring for micro T-Kernel (RP2040, NO_SYS=1)
 */

#ifndef MQTT_UTK_H
#define MQTT_UTK_H

/*
 * Call once, from cyw43_utk_task() the first time the link reaches
 * CYW43_LINK_UP (see cyw43_utk.c). Starts the MQTT connection to
 * MQTT_BROKER_IP:MQTT_BROKER_PORT (config/wifi_secrets.h) and, once
 * connected, a periodic heartbeat publish on MQTT_TOPIC. Safe to call more
 * than once -- a second call while already connecting/connected is a no-op.
 */
void mqtt_utk_start(void);

/*
 * IS19 extension (A11, pending instructor agreement; TEAM.md section 5,
 * IF-13 to IF-15).  Hooks the MQTT code calls, always from inside
 * cyw43_utk_task.  An application may define them; the weak defaults in
 * mqtt_utk.c keep the template's own behaviour.  Plain C types only, so
 * an application file can include this header without lwIP.
 *
 *   mqtt_utk_hook_next_publish  next message to publish: fills the topic
 *                               and payload, returns the payload length,
 *                               or -1 when there is nothing to send.
 *                               Must never block (use TMO_POL).
 *   mqtt_utk_hook_receive       one complete incoming message.
 *   mqtt_utk_hook_subscribe_topic  topic to subscribe to after connecting.
 *   mqtt_utk_hook_broker        1 = connect to a new broker (ip, port).
 *   mqtt_utk_hook_heartbeat     1 = keep the template's 5 s heartbeat.
 *   mqtt_utk_hook_link          MQTT session up (1) or down (0).
 */
int mqtt_utk_hook_next_publish(char *topic, int topic_size,
                               unsigned char *payload, int payload_size);
void mqtt_utk_hook_receive(const char *topic, const unsigned char *payload,
                           int len);
const char *mqtt_utk_hook_subscribe_topic(void);
int mqtt_utk_hook_broker(char *ip, int ip_size, int *port);
int mqtt_utk_hook_heartbeat(void);
void mqtt_utk_hook_link(int connected);

#endif /* MQTT_UTK_H */
