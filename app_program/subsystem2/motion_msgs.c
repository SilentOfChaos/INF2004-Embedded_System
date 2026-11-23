/*
 *  motion_msgs.c
 *  Pack and read Motion's commands (IF-07) and status (IF-08).  See
 *  motion_msgs.h for the layouts.
 */

#include <stddef.h>

#include "../common/bitpack.h"
#include "motion_ctl.h"
#include "motion_msgs.h"

#define MC_TYPE_SHIFT       0u
#define MC_TYPE_BITS        3u
#define MC_SPEED_SHIFT      3u      /* DRIVE */
#define MC_DIFF_SHIFT       14u
#define MC_SPEED_BITS       11u
#define MC_ID_SHIFT         3u      /* moves and turns */
#define MC_ID_BITS          8u
#define MC_VALUE_SHIFT      11u
#define MC_VALUE_BITS       MOTION_CMD_VALUE_BITS
#define MC_HOLD_BIT         3u      /* HOLD_SPEED */

#define MS_TYPE_SHIFT       0u
#define MS_TYPE_BITS        1u
#define MS_HEADING_SHIFT    1u      /* ODOMETRY */
#define MS_HEADING_BITS     16u
#define MS_ID_SHIFT         1u      /* MOVE_DONE */
#define MS_ID_BITS          8u
#define MS_HEADING_MAX      18000   /* cdeg */
#define MS_DISTANCE_SHIFT   0u      /* word 1 */
#define MS_DISTANCE_BITS    MOTION_DISTANCE_BITS

/* The layouts hold every value they must. */
BITPACK_ASSERT(BITPACK_BITS_FOR(MOTION_CMD_KEEPALIVE - 1u) == MC_TYPE_BITS,
               mc_type_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR_SIGNED(MOTION_CMD_SPEED_MAX) ==
               MC_SPEED_BITS, mc_speed_bits);
BITPACK_ASSERT((MC_DIFF_SHIFT + MC_SPEED_BITS) <= 32u, mc_drive_fits);
BITPACK_ASSERT(BITPACK_BITS_FOR(MOTION_CMD_VALUE_MAX) == MC_VALUE_BITS,
               mc_value_bits);
BITPACK_ASSERT((MC_VALUE_SHIFT + MC_VALUE_BITS) <= 32u, mc_move_fits);
BITPACK_ASSERT(BITPACK_BITS_FOR(MOTION_STS_MOVE_DONE - 1u) == MS_TYPE_BITS,
               ms_type_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR_SIGNED(MS_HEADING_MAX) == MS_HEADING_BITS,
               ms_heading_bits);

static bool mc_is_move(uint8_t type);

/* ------------------------------------------------------------------ */
/*  IF-07 commands                                                     */
/* ------------------------------------------------------------------ */

/*
 * Build one command; only the fields the type carries are kept.
 * Returns false if a kept value did not fit (the nearest value that
 * fits is stored): motion_send() limits speeds to +/-1000 mm/s and
 * refuses a command this returns false for, and nav_sm.c checks every
 * distance and angle it sends against MOTION_CMD_VALUE_MAX when
 * building.  A move or turn keeps the size of its value, which is all
 * Motion uses, so a negative value is its size, as it always was; a
 * size above MOTION_CMD_VALUE_MAX does not fit.
 */
bool
motion_cmd_pack (motion_cmd_t * p_cmd, uint8_t type, uint8_t move_id,
                 int32_t speed_mmps, int32_t diff_mmps, int32_t value)
{
    bool b_ok = false;

    if (NULL != p_cmd)
    {
        uint32_t      word      = 0u;
        bool const    b_type_ok = ((type >= MOTION_CMD_DRIVE) &&
                                   (type <= MOTION_CMD_KEEPALIVE));
        /* A type that is not a command is kept as a KEEPALIVE, which
           only refreshes the update time, and reported. */
        uint8_t const kept      = b_type_ok ? type : MOTION_CMD_KEEPALIVE;

        (void) bitpack_set(&word, MC_TYPE_SHIFT, MC_TYPE_BITS,
                           (uint32_t) kept - 1u);
        b_ok = b_type_ok;

        if (MOTION_CMD_DRIVE == kept)
        {
            b_ok = bitpack_set_signed(&word, MC_SPEED_SHIFT, MC_SPEED_BITS,
                                      speed_mmps) && b_ok;
            b_ok = bitpack_set_signed(&word, MC_DIFF_SHIFT, MC_SPEED_BITS,
                                      diff_mmps) && b_ok;
        }
        else if (mc_is_move(kept))
        {
            /* The size: 0u - value is exact for every negative int32. */
            uint32_t const size = (value < 0) ? (0u - (uint32_t) value)
                                              : (uint32_t) value;

            (void) bitpack_set(&word, MC_ID_SHIFT, MC_ID_BITS,
                               (uint32_t) move_id);
            b_ok = bitpack_set(&word, MC_VALUE_SHIFT, MC_VALUE_BITS, size) &&
                   b_ok;
        }
        else if (MOTION_CMD_HOLD_SPEED == kept)
        {
            b_ok = ((0 == value) || (1 == value)) && b_ok;
            bitpack_set_flag(&word, MC_HOLD_BIT, (0 != value));
        }
        else
        {
            /* STOP, KEEPALIVE: the type only. */
        }

        p_cmd->bits = word;
    }

    return (b_ok);
}

/* MOTION_CMD_xxx. */
uint8_t
motion_cmd_type (motion_cmd_t const * p_cmd)
{
    /* A 3-bit field plus 1: 1..8, fits uint8_t. */
    return ((NULL != p_cmd)
            ? (uint8_t) (bitpack_get(p_cmd->bits, MC_TYPE_SHIFT,
                                     MC_TYPE_BITS) + 1u)
            : 0u);
}

uint8_t
motion_cmd_move_id (motion_cmd_t const * p_cmd)
{
    uint8_t move_id = 0u;

    if (mc_is_move(motion_cmd_type(p_cmd)))
    {
        /* An 8-bit field: fits uint8_t. */
        move_id = (uint8_t) bitpack_get(p_cmd->bits, MC_ID_SHIFT,
                                        MC_ID_BITS);
    }

    return (move_id);
}

int32_t
motion_cmd_speed (motion_cmd_t const * p_cmd)
{
    return ((MOTION_CMD_DRIVE == motion_cmd_type(p_cmd))
            ? bitpack_get_signed(p_cmd->bits, MC_SPEED_SHIFT, MC_SPEED_BITS)
            : 0);
}

int32_t
motion_cmd_diff (motion_cmd_t const * p_cmd)
{
    return ((MOTION_CMD_DRIVE == motion_cmd_type(p_cmd))
            ? bitpack_get_signed(p_cmd->bits, MC_DIFF_SHIFT, MC_SPEED_BITS)
            : 0);
}

/* Distance or angle of a move or turn (0..32767); 1 or 0 for
   HOLD_SPEED. */
int32_t
motion_cmd_value (motion_cmd_t const * p_cmd)
{
    uint8_t const type  = motion_cmd_type(p_cmd);
    int32_t       value = 0;

    if (mc_is_move(type))
    {
        /* 15 bits: fits int32_t. */
        value = (int32_t) bitpack_get(p_cmd->bits, MC_VALUE_SHIFT,
                                      MC_VALUE_BITS);
    }
    else if (MOTION_CMD_HOLD_SPEED == type)
    {
        value = bitpack_get_flag(p_cmd->bits, MC_HOLD_BIT) ? 1 : 0;
    }
    else
    {
        /* No value. */
    }

    return (value);
}

/* ------------------------------------------------------------------ */
/*  IF-08 status                                                       */
/* ------------------------------------------------------------------ */

/* Odometry: distance since boot and heading.  Returns false if one did
   not fit (the nearest value that fits is stored): motion_heading_cdeg()
   gives (-18000, 18000] and a distance made from the counters is below
   2^30 mm in size. */
bool
motion_sts_odometry (motion_sts_t * p_sts, int32_t distance_mm,
                     int32_t heading_cdeg)
{
    bool b_ok = false;

    if (NULL != p_sts)
    {
        uint32_t word0 = 0u;
        uint32_t word1 = 0u;

        (void) bitpack_set(&word0, MS_TYPE_SHIFT, MS_TYPE_BITS,
                           MOTION_STS_ODOMETRY - 1u);
        b_ok = bitpack_set_signed(&word0, MS_HEADING_SHIFT, MS_HEADING_BITS,
                                  heading_cdeg);
        b_ok = bitpack_set_signed(&word1, MS_DISTANCE_SHIFT,
                                  MS_DISTANCE_BITS, distance_mm) && b_ok;
        p_sts->word[0] = word0;
        p_sts->word[1] = word1;
    }

    return (b_ok);
}

/* A distance move or turn finished. */
void
motion_sts_move_done (motion_sts_t * p_sts, uint8_t move_id)
{
    if (NULL != p_sts)
    {
        uint32_t word0 = 0u;

        (void) bitpack_set(&word0, MS_TYPE_SHIFT, MS_TYPE_BITS,
                           MOTION_STS_MOVE_DONE - 1u);
        (void) bitpack_set(&word0, MS_ID_SHIFT, MS_ID_BITS,
                           (uint32_t) move_id);
        p_sts->word[0] = word0;
        p_sts->word[1] = 0u;
    }
}

/* MOTION_STS_xxx. */
uint8_t
motion_sts_type (motion_sts_t const * p_sts)
{
    /* A 1-bit field plus 1: 1..2. */
    return ((NULL != p_sts)
            ? (uint8_t) (bitpack_get(p_sts->word[0], MS_TYPE_SHIFT,
                                     MS_TYPE_BITS) + 1u)
            : 0u);
}

uint8_t
motion_sts_move_id (motion_sts_t const * p_sts)
{
    /* An 8-bit field: fits uint8_t. */
    return ((MOTION_STS_MOVE_DONE == motion_sts_type(p_sts))
            ? (uint8_t) bitpack_get(p_sts->word[0], MS_ID_SHIFT, MS_ID_BITS)
            : 0u);
}

int32_t
motion_sts_distance_mm (motion_sts_t const * p_sts)
{
    int32_t distance = 0;

    if (MOTION_STS_ODOMETRY == motion_sts_type(p_sts))
    {
        distance = bitpack_get_signed(p_sts->word[1], MS_DISTANCE_SHIFT,
                                      MS_DISTANCE_BITS);
    }

    return (distance);
}

int32_t
motion_sts_heading_cdeg (motion_sts_t const * p_sts)
{
    return ((MOTION_STS_ODOMETRY == motion_sts_type(p_sts))
            ? bitpack_get_signed(p_sts->word[0], MS_HEADING_SHIFT,
                                 MS_HEADING_BITS)
            : 0);
}

static bool
mc_is_move (uint8_t type)
{
    return ((MOTION_CMD_FORWARD == type) || (MOTION_CMD_BACKWARD == type) ||
            (MOTION_CMD_TURN_LEFT == type) || (MOTION_CMD_TURN_RIGHT == type));
}

/*** end of file ***/
