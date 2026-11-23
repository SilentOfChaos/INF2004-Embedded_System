/*
 *  drv_ultrasonic.h
 *  HC-SR04P ultrasonic sensor: Trig on GP4, Echo on GP5 (Grove 3, report
 *  section 2).  The Echo ISR (core 0) only time-stamps the pulse and
 *  passes {rise time, width} to the Obstacle Scan task (core 1) through
 *  the message buffer mbf_echo: data crossing cores goes through a
 *  kernel object (report section 3).  The record, ultrasonic_echo_t,
 *  is packed (minimum-bit change, D-90): see ultrasonic_echo.h.
 */

#ifndef DRV_ULTRASONIC_H
#define DRV_ULTRASONIC_H

#include "../app.h"
#include "ultrasonic_echo.h"

/* Echoes the buffer holds: what it really held before the minimum-bit
   change (8 were asked for, with 8 bytes to spare each), kept (D-83). */
#define ULTRASONIC_ECHO_QUEUE   10u

ER       ultrasonic_init(ID mbf_echo);
void     ultrasonic_ping_reset(void);
uint32_t ultrasonic_trigger(void);
bool     ultrasonic_echo_high(void);

#endif /* DRV_ULTRASONIC_H */

/*** end of file ***/
