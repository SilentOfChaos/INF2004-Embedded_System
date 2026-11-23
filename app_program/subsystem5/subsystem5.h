/*
 *  subsystem5.h
 *  Subsystem 5 (Buddy 5, Wong Qian Hui): ultrasonic scanning and
 *  obstacle avoidance.
 *    Obstacle Scan Task  priority 10, core 1, every 65 ms (report sec. 3)
 *    Echo ISR            GP5, core 0
 *  Navigation drives the car around the obstacle (report 6.5).
 *
 *  Public interface of subsystem 5.
 */

#ifndef SUBSYSTEM5_H
#define SUBSYSTEM5_H

#include "../app.h"
#include "scan_msgs.h"

IMPORT ID mbf_scan_req;

/* Send a request to Obstacle Scan (never blocks; a full queue is
   reported to Safety as a fault). */
ER scan_request(uint8_t type, uint8_t side);

#endif /* SUBSYSTEM5_H */

/*** end of file ***/
