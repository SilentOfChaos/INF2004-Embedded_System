/*
 *  subsystem2.h
 *  Subsystem 2 (Buddy 2, Dominic Ng Jun Jie): motion control.
 *    Motion Task  priority 9, core 0, every 20 ms   (report section 3)
 *    Encoder ISR  GP0/GP1 and GP16/GP17, core 0
 *
 *  Public interface of subsystem 2.  Only Navigation commands Motion
 *  (report section 4); Safety uses motion_request_reset().
 */

#ifndef SUBSYSTEM2_H
#define SUBSYSTEM2_H

#include "../app.h"
#include "motion_msgs.h"

/* Motion's own objects (owned by subsystem 2). */
#define MOTION_FLG_TICK         0x0001u     /* cyclic handler, 20 ms */
#define MOTION_FLG_RESET_REQ    0x0002u     /* IF-09: Safety asks for zero */

IMPORT ID mbf_motion_cmd;
IMPORT ID flg_motion;

/* The write-up's motion API (report 6.2).  Called by Navigation only.
   Each returns E_OK, or an error if the command queue was full (which
   is also reported to Safety as a fault).  A distance (mm) or angle
   (cdeg) is used as its size, 0..MOTION_CMD_VALUE_MAX (32767); a larger
   one returns E_PAR and nothing is sent (D-88). */
ER motion_drive(int32_t speed_mmps, int32_t diff_mmps);
ER motion_move_forward(int32_t distance_mm, uint8_t move_id);
ER motion_move_backward(int32_t distance_mm, uint8_t move_id);
ER motion_turn_left(int32_t angle_cdeg, uint8_t move_id);
ER motion_turn_right(int32_t angle_cdeg, uint8_t move_id);
ER motion_stop(void);
ER motion_hold_speed(bool b_hold);
ER motion_keepalive(void);

/* IF-09: Safety asks Motion to clear its queue, command and PID state;
   Motion answers with safety_ack_zero(). */
void motion_request_reset(void);

#endif /* SUBSYSTEM2_H */

/*** end of file ***/
