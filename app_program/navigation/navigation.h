/*
 *  navigation.h
 *  Navigation Task: the main controller (the briefing's Vehicle
 *  Controller), shared by the team.
 *    priority 7, core 0, every 5 ms and on an event (report section 3)
 *  It runs the system statechart (report section 5, Fig 5), steers from
 *  the line error with Buddy 3's steering code, holds the barcode
 *  command until the junction, searches for a lost line, ends the run,
 *  and is the only task that commands Motion.
 *
 *  Navigation owns the queues that carry events to it (template rule:
 *  the receiver creates the object).  Senders use nav_post().
 */

#ifndef NAVIGATION_H
#define NAVIGATION_H

#include "../app.h"
#include "nav_types.h"

/* Navigation's event flag bits. */
#define NAV_FLG_TICK            0x0001u /* cyclic handler, 5 ms */
#define NAV_FLG_EVENT           0x0002u /* something was posted */
#define NAV_FLG_RESTART         0x0004u /* Safety: fault cleared, re-run
                                           SELF-TEST (report SD2) */
#define NAV_FLG_RESET           0x0008u /* Safety: RESET pressed, no fault */
/* Level bit, not a wake-up: START was asked for over MQTT (the "start"
   command); read and cleared at the next 5 ms tick, like a button press. */
#define NAV_FLG_REMOTE_START    0x0010u
#define NAV_FLG_WAKE            (NAV_FLG_TICK | NAV_FLG_EVENT | \
                                 NAV_FLG_RESTART | NAV_FLG_RESET)
/* Health levels checked in SELF-TEST (set and cleared by their owners
   with nav_set_health). */
#define NAV_FLG_LINE_OK         NAV_HEALTH_LINE
#define NAV_FLG_IMU_OK          NAV_HEALTH_IMU
#define NAV_FLG_SCAN_OK         NAV_HEALTH_SCAN
#define NAV_FLG_HEALTH_ALL      NAV_HEALTH_ALL

IMPORT ID flg_nav;
IMPORT ID mbf_line_evt;         /* IF-01 line_evt_t      (Line) */
IMPORT ID mbf_imu_evt;          /* IF-03 imu_evt_t       (IMU) */
IMPORT ID mbf_obstacle;         /* IF-05 obstacle_evt_t  (Obstacle Scan) */
IMPORT ID mbf_motion_sts;       /* IF-08 motion_sts_t    (Motion) */
IMPORT ID mbf_nav_cmd;          /* IF-10 nav_cmd_t       (Command) */

/* Send an event to one of Navigation's queues without blocking and wake
   Navigation.  A full queue is a fault (report section 4): it is
   reported to Safety with the sender's FAULT_SRC_xxx code. */
ER nav_post(ID mbfid, void const * p_msg, INT size, uint8_t source);

/* Set or clear one of the NAV_FLG_xxx_OK health levels. */
void nav_set_health(UINT health_bit, bool b_ok);

/* True while a run is active (following, hump, search, junction,
   avoiding): the IMU only reports a lift during a run. */
bool nav_is_running(void);

/* True while a lift should end the run: during a run, and also in SAFE
   HOLD and COMPLETE (the car has stopped by itself; picking it up then
   is the end of the demo).  Not in READY, so the car can still be carried
   to the start line. */
bool nav_lift_watch(void);

/* Press START remotely (Command task, "start" command).  Acts only in
   READY with no fault latched, exactly like the GP20 button. */
void nav_request_start(void);

/* Text name of a mode, for the console and telemetry. */
char const * nav_mode_name(nav_mode_t mode);

#endif /* NAVIGATION_H */

/*** end of file ***/
