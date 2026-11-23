/*
 *  subsystem1.h
 *  Subsystem 1 (Buddy 1, Ang Hung Ling): Wi-Fi / MQTT communication and
 *  telemetry.
 *    Command Task    priority 11, core 1, on a message, link check
 *                    every 100 ms                    (report section 3)
 *    Telemetry Task  priority 12, core 1, every 200 ms
 *  The Wi-Fi, lwIP and MQTT calls stay inside the kernel's
 *  cyw43_utk_task (TEAM.md section 3); this subsystem only exchanges
 *  message buffers with the MQTT code (A11, pending instructor
 *  agreement).
 *
 *  Public interface of subsystem 1.
 */

#ifndef SUBSYSTEM1_H
#define SUBSYSTEM1_H

#include "../app.h"
#include "tlm_msgs.h"

/* ---------------------------------------------------------------------
 *  IF-13/14/15: message buffers shared with the MQTT code in
 *  cyw43_utk_task (A11).  Only subsystem 1 and that task use them.
 * --------------------------------------------------------------------- */
IMPORT ID mbf_tlm;              /* IF-12 */
IMPORT ID mbf_publish;          /* IF-13 Telemetry -> MQTT code */
IMPORT ID mbf_cmd_in;           /* IF-14 MQTT code -> Command */
IMPORT ID mbf_mqtt_cfg;         /* IF-15 Command -> MQTT code (broker) */

/* Post a record to Telemetry.  b_urgent wakes Telemetry at once (acks,
   summary requests); otherwise it is handled at the next 200 ms tick. */
void tlm_post(tlm_msg_t * p_msg, bool b_urgent);

#endif /* SUBSYSTEM1_H */

/*** end of file ***/
