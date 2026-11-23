/*
 *  scan_msgs.h
 *  Message formats of subsystem 5 (Obstacle Scan): IF-05 and IF-06, each
 *  field packed into the bits its range needs (minimum-bit change, D-90).
 *  Build and read them with the functions below.
 *
 *  Pure C (no kernel headers), so host unit tests can use these types.
 */

#ifndef SCAN_MSGS_H
#define SCAN_MSGS_H

#include <stdbool.h>
#include <stdint.h>

/* ---------------------------------------------------------------------
 *  IF-05  Obstacle Scan -> Navigation: obstacle_evt_t in Navigation's
 *  mbf_obstacle.
 * --------------------------------------------------------------------- */
#define OBST_EVT_AHEAD          1u  /* two readings <= 30 cm: stop */
#define OBST_EVT_PATH_CLEAR     2u  /* coarse scan found nothing */
#define OBST_EVT_PLAN           3u  /* profile and the side to pass on */
#define OBST_EVT_NO_ROOM        4u  /* neither side: reverse 10 cm once */
#define OBST_EVT_BEHIND         5u  /* side scan: obstacle passed */
#define OBST_EVT_NO_SAFE_ROUTE  6u  /* give up: SAFE HOLD (3 invalid
                                       readings in a row, or still no
                                       room after reversing) */

#define OBST_SIDE_NONE          0u
#define OBST_SIDE_LEFT          1u
#define OBST_SIDE_RIGHT         2u

/*
 * Two words.  Only OBST_EVT_PLAN carries the profile, with what
 * Navigation reads; its points are all hits, so within
 * CFG_SCAN_OBSTACLE_MM (500 mm) of the sensor:
 *   word 0  bits  0-2   type - 1       OBST_EVT_xxx, 1..6
 *           PLAN
 *           bits  3-4   side           OBST_SIDE_xxx, 0..2
 *           bits  5-13  closest_mm     range to the closest point, 0..500
 *           bits 14-24  width_mm       left edge - right edge,
 *                                      -1000..1000
 *   word 1  PLAN
 *           bits  0-9   edge_left_mm   lateral position of each edge,
 *           bits 10-19  edge_right_mm  + = left of the car, -500..500
 * Unused bits are 0.  The full profile (position, centre, clearances)
 * goes to Telemetry in its own record (IF-12, D-72); the time stamp was
 * never read.  Why the Scan task gave up is not sent: Navigation holds
 * the car the same way for every reason, and nothing else reads it
 * (D-90).
 */
typedef struct
{
    uint32_t word[2];
} obstacle_evt_t;

bool    obstacle_evt_simple(obstacle_evt_t * p_evt, uint8_t type);
bool    obstacle_evt_plan(obstacle_evt_t * p_evt, uint8_t side,
                          int32_t closest_mm, int32_t width_mm,
                          int32_t edge_left_mm, int32_t edge_right_mm);
uint8_t obstacle_evt_type(obstacle_evt_t const * p_evt);
uint8_t obstacle_evt_side(obstacle_evt_t const * p_evt);
int32_t obstacle_evt_closest_mm(obstacle_evt_t const * p_evt);
int32_t obstacle_evt_width_mm(obstacle_evt_t const * p_evt);
int32_t obstacle_evt_edge_left_mm(obstacle_evt_t const * p_evt);
int32_t obstacle_evt_edge_right_mm(obstacle_evt_t const * p_evt);

/* ---------------------------------------------------------------------
 *  IF-06  Navigation -> Obstacle Scan: scan_req_t in mbf_scan_req.
 *  Use scan_request().
 * --------------------------------------------------------------------- */
#define SCAN_REQ_CAR_STOPPED    1u  /* car stopped: start COARSE SCAN */
#define SCAN_REQ_BYPASS         2u  /* driving past on `side`: side scans */
#define SCAN_REQ_REVERSED       3u  /* reversed 10 cm: scan once more */
#define SCAN_REQ_REJOIN         4u  /* turned back towards the line */
#define SCAN_REQ_LINE_FOUND     5u  /* rejoined: MONITORING again */
#define SCAN_REQ_LINE_MISSED    6u  /* no line within 30 cm */
#define SCAN_REQ_RUN_OVER       7u  /* RUN OVER: ranging idle */
#define SCAN_REQ_START          8u  /* new START: MONITORING */
#define SCAN_REQ_RESUME         9u  /* path clear: MONITORING again */

/*
 * One byte:
 *   bits 0-3  type   SCAN_REQ_xxx, 1..9
 *   bits 4-5  side   SCAN_REQ_BYPASS: OBST_SIDE_xxx, 0..2; else 0
 *   bits 6-7  0 (spare)
 * The time stamp was never read.
 */
typedef struct
{
    uint8_t bits;
} scan_req_t;

bool    scan_req_pack(scan_req_t * p_req, uint8_t type, uint8_t side);
uint8_t scan_req_type(scan_req_t const * p_req);
uint8_t scan_req_side(scan_req_t const * p_req);

#endif /* SCAN_MSGS_H */

/*** end of file ***/
