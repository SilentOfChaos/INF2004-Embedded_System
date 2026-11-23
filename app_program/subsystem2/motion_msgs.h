/*
 *  motion_msgs.h
 *  Message formats of subsystem 2 (Motion): IF-07 and IF-08, each field
 *  packed into the bits its range needs (minimum-bit change, D-88).
 *  Build and read them with the functions below.
 *
 *  Pure C (no kernel headers), so host unit tests can use these types.
 */

#ifndef MOTION_MSGS_H
#define MOTION_MSGS_H

#include <stdbool.h>
#include <stdint.h>

/* ---------------------------------------------------------------------
 *  IF-07  Navigation -> Motion: motion_cmd_t in mbf_motion_cmd.
 *  Use the motion_xxx() functions below; they fill and send it.
 * --------------------------------------------------------------------- */
#define MOTION_CMD_DRIVE        1u  /* follow speeds: speed +/- diff */
#define MOTION_CMD_FORWARD      2u  /* moveForward(value mm) */
#define MOTION_CMD_BACKWARD     3u  /* moveBackward(value mm) */
#define MOTION_CMD_TURN_LEFT    4u  /* turnLeft(value cdeg), pivot */
#define MOTION_CMD_TURN_RIGHT   5u  /* turnRight(value cdeg), pivot */
#define MOTION_CMD_STOP         6u  /* stop() */
#define MOTION_CMD_HOLD_SPEED   7u  /* holdSpeed(): value 1 hold, 0 release */
#define MOTION_CMD_KEEPALIVE    8u  /* resets the 100 ms update limit */

/*
 * One word; the fields after the type depend on the type:
 *   bits  0-2   type - 1     MOTION_CMD_xxx, 1..8
 *   DRIVE
 *   bits  3-13  speed_mmps   forward speed, -1000..1000
 *   bits 14-24  diff_mmps    left = speed - diff, right = speed + diff,
 *                            -1000..1000
 *   FORWARD, BACKWARD, TURN_LEFT, TURN_RIGHT
 *   bits  3-10  move_id      echoed in MOTION_STS_MOVE_DONE, 0..255
 *   bits 11-25  value        distance mm or angle cdeg, its size only
 *                            (the type gives the direction), 0..32767
 *   HOLD_SPEED
 *   bit   3     value        1 hold, 0 release
 *   STOP, KEEPALIVE: the type only.  Unused bits are 0.
 * Reading a field the type does not carry gives 0, as before.
 *
 * A move or turn keeps only the size of its value: Motion has always
 * used the size alone, so a negative value means its size, as before.
 * Navigation's largest value is a U-turn, 18000 cdeg, which needs 15
 * bits; nav_sm.c checks every distance and angle it sends against
 * MOTION_CMD_VALUE_MAX when building, and Motion's API refuses a larger
 * one rather than send it shortened (D-88).
 */
#define MOTION_CMD_SPEED_MAX    1000    /* mm/s, both speed and diff */
#define MOTION_CMD_VALUE_BITS   15u
#define MOTION_CMD_VALUE_MAX    32767   /* 2^15 - 1 */

typedef struct
{
    uint32_t bits;
} motion_cmd_t;

bool    motion_cmd_pack(motion_cmd_t * p_cmd, uint8_t type, uint8_t move_id,
                        int32_t speed_mmps, int32_t diff_mmps, int32_t value);
uint8_t motion_cmd_type(motion_cmd_t const * p_cmd);
uint8_t motion_cmd_move_id(motion_cmd_t const * p_cmd);
int32_t motion_cmd_speed(motion_cmd_t const * p_cmd);
int32_t motion_cmd_diff(motion_cmd_t const * p_cmd);
int32_t motion_cmd_value(motion_cmd_t const * p_cmd);

/* ---------------------------------------------------------------------
 *  IF-08  Motion -> Navigation: motion_sts_t in Navigation's
 *  mbf_motion_sts.  Odometry every 20 ms, MOVE_DONE when a distance move
 *  or turn finishes.
 * --------------------------------------------------------------------- */
#define MOTION_STS_ODOMETRY     1u
#define MOTION_STS_MOVE_DONE    2u

typedef enum
{
    MOTION_STATE_IDLE = 0,      /* motors off, speeds zero */
    MOTION_STATE_DRIVING,       /* wheel-speed PID every 20 ms */
    MOTION_STATE_HOLDING,       /* forward speed fixed, steering on */
    MOTION_STATE_TURNING,       /* turn counted on the encoders */
    MOTION_STATE_BRAKED         /* Safety FAULT flag set */
} motion_state_t;

/*
 * Two words; Navigation reads only these fields:
 *   word 0  bit   0     type - 1      MOTION_STS_xxx, 1..2
 *           ODOMETRY
 *           bits  1-16  heading_cdeg  odometry heading, (-18000, 18000]
 *           MOVE_DONE
 *           bits  1-8   move_id       the finished move
 *   word 1  ODOMETRY
 *           bits  0-30  distance_mm   signed distance since boot (average
 *                                     of both wheels).  A run has no set
 *                                     length (the track runs endlessly),
 *                                     so the range is what the 32-bit
 *                                     counters can give: below 2^30 mm in
 *                                     size (MOTION_DISTANCE_BITS)
 * Unused bits are 0.  Measured speeds, counts and the state go to
 * Telemetry in Motion's status record (IF-12), not here.
 */
typedef struct
{
    uint32_t word[2];
} motion_sts_t;

bool    motion_sts_odometry(motion_sts_t * p_sts, int32_t distance_mm,
                            int32_t heading_cdeg);
void    motion_sts_move_done(motion_sts_t * p_sts, uint8_t move_id);
uint8_t motion_sts_type(motion_sts_t const * p_sts);
uint8_t motion_sts_move_id(motion_sts_t const * p_sts);
int32_t motion_sts_distance_mm(motion_sts_t const * p_sts);
int32_t motion_sts_heading_cdeg(motion_sts_t const * p_sts);

#endif /* MOTION_MSGS_H */

/*** end of file ***/
