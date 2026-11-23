/*
 *  tlm_msgs.c
 *  Pack and read the Navigation commands (IF-10) and the telemetry
 *  records (IF-12).  See tlm_msgs.h for the layouts.
 */

#include <stddef.h>

#include "../app_config.h"
#include "../common/bitpack.h"
#include "../navigation/nav_types.h"
#include "../subsystem2/motion_ctl.h"
#include "../subsystem3/letter_map.h"
#include "../subsystem3/line_msgs.h"
#include "../subsystem4/imu_msgs.h"
#include "../subsystem5/scan_logic.h"
#include "../subsystem5/scan_msgs.h"
#include "tlm_msgs.h"

#define NC_TYPE_SHIFT       0u      /* word 0 */
#define NC_TYPE_BITS        1u
#define NC_LETTER_SHIFT     1u
#define NC_LETTER_BITS      5u
#define NC_ACTION_SHIFT     6u
#define NC_ACTION_BITS      3u
#define NC_SEQ_SHIFT        0u      /* word 1 */
#define NC_SEQ_BITS         30u
#define NC_LETTERS          26u     /* 'A'..'Z' as 1..26, 0 = none */

/* The layout holds every value it must. */
BITPACK_ASSERT(BITPACK_BITS_FOR(NAV_CMD_MAP_DEFAULT - 1u) == NC_TYPE_BITS,
               nc_type_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(NC_LETTERS) == NC_LETTER_BITS,
               nc_letter_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(LETTER_ACTION_COUNT - 1u) == NC_ACTION_BITS,
               nc_action_bits);
BITPACK_ASSERT((NC_LETTER_SHIFT + NC_LETTER_BITS) == NC_ACTION_SHIFT,
               nc_layout);
BITPACK_ASSERT(BITPACK_BITS_FOR(CMD_SEQ_MAX) == NC_SEQ_BITS, nc_seq_bits);

/* IF-12 layouts (tlm_msgs.h).  A letter field is 5 bits everywhere. */
#define TM_TYPE_BITS        4u      /* head */
#define LETTER_BITS         5u

#define LN_RAW_BITS         12u     /* line, word 0 */
#define LN_RAW_L_SHIFT      0u
#define LN_RAW_R_SHIFT      12u
#define LN_LETTERS_SHIFT    24u
#define LN_LETTERS_BITS     8u
#define LN_ERROR_SHIFT      0u      /* word 1 */
#define LN_ERROR_BITS       11u
#define LN_LAST_SHIFT       11u

#define MO_SPEED_BITS       16u     /* motion, word 0 */
#define MO_SPEED_L_SHIFT    0u
#define MO_SPEED_R_SHIFT    16u
#define MO_DIST_SHIFT       0u      /* word 3 */

#define IM_RATE_SHIFT       0u      /* IMU, word 0 */
#define IM_RATE_BITS        16u
#define IM_CLASS_SHIFT      16u
#define IM_CLASS_BITS       3u
#define IM_HUMPS_SHIFT      19u
#define IM_HUMPS_BITS       8u
#define IM_HEIGHT_SHIFT     0u      /* word 1 */
#define IM_HEIGHT_BITS      7u

#define SC_RANGE_SHIFT      0u      /* scan */
#define SC_RANGE_BITS       12u
#define SC_OBST_SHIFT       12u
#define SC_OBST_BITS        16u

#define NV_MODE_SHIFT       0u      /* Navigation */
#define NV_MODE_BITS        4u
#define NV_HELD_SHIFT       4u
#define NV_HOLD_SHIFT       9u
#define NV_HOLD_BITS        2u

#define SF_FAULTED_BIT      0u      /* safety, word 0 */
#define SF_CAUSE_SHIFT      1u
#define SF_CAUSE_BITS       4u
#define SF_WDT_BIT          5u
#define SF_QFULL_SHIFT      6u
#define SF_QFULL_BITS       8u
#define SF_FAULTS_SHIFT     14u
#define SF_COUNT_BITS       16u
#define SF_MISSED_SHIFT     0u      /* word 1 */
#define SF_MASK_SHIFT       16u
#define SF_MASK_BITS        11u

#define RT_DIST_SHIFT       0u      /* route, word 0 */
#define RT_KIND_SHIFT       0u      /* word 1 */
#define RT_KIND_BITS        4u
#define RT_ACTION_BITS      3u
#define RT_ANGLE_BITS       16u
#define RT_BC_LETTER_SHIFT  4u      /* BARCODE */
#define RT_BC_ACTION_SHIFT  9u
#define RT_BC_REV_BIT       12u
#define RT_TURN_ACT_SHIFT   4u      /* TURN; measured: word 2 */
#define RT_TURN_CMD_SHIFT   7u
#define RT_TURN_MEAS_SHIFT  0u
#define RT_HUMP_H_SHIFT     4u      /* HUMP */
#define RT_HUMP_H_BITS      7u
#define RT_HUMP_PEAK_SHIFT  11u
#define RT_HUMP_PEAK_BITS   15u
#define RT_OB_SIDE_SHIFT    4u      /* OBSTACLE */
#define RT_OB_SIDE_BITS     2u
#define RT_OB_WIDTH_SHIFT   6u
#define RT_OB_WIDTH_BITS    11u
#define RT_OB_CLOSE_SHIFT   17u
#define RT_OB_CLOSE_BITS    9u
#define RT_SEARCH_BIT       4u      /* LINE_SEARCH */
#define RT_HOLD_SHIFT       4u      /* SAFE_HOLD */
#define RT_HOLD_BITS        2u

#define AK_SEQ_SHIFT        0u      /* ack, word 0 */
#define AK_SEQ_BITS         30u
#define AK_CODE_SHIFT       0u      /* word 1 */
#define AK_CODE_BITS        3u
#define AK_OK_BIT           3u

#define FT_CAUSE_SHIFT      0u      /* fault */
#define FT_CAUSE_BITS       4u
#define FT_SOURCE_SHIFT     4u
#define FT_SOURCE_BITS      3u
#define FT_DETAIL_SHIFT     7u
#define FT_DETAIL_BITS      13u
#define FT_LATCHED_BIT      20u

#define LS_ONLINE_BIT       0u      /* link */
#define LS_WIFI_SHIFT       1u
#define LS_WIFI_BITS        3u
#define LS_WIFI_MAX         3       /* CYW43_LINK_UP; BADAUTH is -3 */

#define OB_SIDE_SHIFT       0u      /* obstacle profile, word 0 */
#define OB_SIDE_BITS        2u
#define OB_REV_BIT          2u
#define OB_X_SHIFT          3u
#define OB_X_BITS           9u
#define OB_Y_SHIFT          12u
#define OB_Y_BITS           10u
#define OB_CLOSE_SHIFT      22u
#define OB_CLOSE_BITS       9u
#define OB_WIDTH_SHIFT      0u      /* word 1 */
#define OB_WIDTH_BITS       11u
#define OB_CLEAR_L_SHIFT    11u
#define OB_CLEAR_R_SHIFT    0u      /* word 2 */
#define OB_CLEAR_BITS       12u

/* Every value each record must hold fits its field. */
BITPACK_ASSERT(BITPACK_BITS_FOR(TLM_MSG_OBSTACLE - 1u) == TM_TYPE_BITS,
               tm_type_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(NC_LETTERS) == LETTER_BITS, tm_letter_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(4095u) == LN_RAW_BITS, ln_raw_bits);
BITPACK_ASSERT(LINE_ERR_ERROR_BITS == LN_ERROR_BITS, ln_error_bits);
BITPACK_ASSERT((LN_LAST_SHIFT + LETTER_BITS) <= 32u, ln_fits);
BITPACK_ASSERT(BITPACK_BITS_FOR_SIGNED(INT16_MAX) == MO_SPEED_BITS,
               mo_speed_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(IMU_CLASS_COUNT - 1u) == IM_CLASS_BITS,
               im_class_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR_SIGNED(INT16_MAX) == IM_RATE_BITS,
               im_rate_bits);
BITPACK_ASSERT((IM_HUMPS_SHIFT + IM_HUMPS_BITS) <= 32u, im_fits);
BITPACK_ASSERT(BITPACK_BITS_FOR(CFG_US_MAX_MM) == SC_RANGE_BITS,
               sc_range_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(NAV_MODE_COUNT - 1u) == NV_MODE_BITS,
               nv_mode_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(NAV_HOLD_COUNT - 1u) == NV_HOLD_BITS,
               nv_hold_bits);
BITPACK_ASSERT((NV_HELD_SHIFT + LETTER_BITS) == NV_HOLD_SHIFT, nv_layout);
BITPACK_ASSERT(BITPACK_BITS_FOR(FAULT_CAUSE_MAX) == SF_CAUSE_BITS,
               sf_cause_bits);
BITPACK_ASSERT((SF_CAUSE_SHIFT + SF_CAUSE_BITS) == SF_WDT_BIT, sf_layout);
BITPACK_ASSERT((SF_FAULTS_SHIFT + SF_COUNT_BITS) <= 32u, sf_fits);
BITPACK_ASSERT(FAULT_MASK_BITS == SF_MASK_BITS, sf_mask_bits);
BITPACK_ASSERT((SF_MISSED_SHIFT + SF_COUNT_BITS) == SF_MASK_SHIFT,
               sf_mask_layout);
BITPACK_ASSERT((SF_MASK_SHIFT + SF_MASK_BITS) <= 32u, sf_mask_fits);
BITPACK_ASSERT(BITPACK_BITS_FOR(ROUTE_BC_FAIL) == RT_KIND_BITS, rt_kind_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(LETTER_ACTION_COUNT - 1u) == RT_ACTION_BITS,
               rt_action_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR_SIGNED(18000) == RT_ANGLE_BITS,
               rt_angle_bits);
BITPACK_ASSERT((RT_TURN_CMD_SHIFT + RT_ANGLE_BITS) <= 32u, rt_turn_fits);
BITPACK_ASSERT(RT_HUMP_H_BITS == IM_HEIGHT_BITS, rt_hump_bits);
BITPACK_ASSERT((RT_HUMP_PEAK_SHIFT + RT_HUMP_PEAK_BITS) <= 32u,
               rt_hump_fits);
BITPACK_ASSERT(BITPACK_BITS_FOR(OBST_SIDE_RIGHT) == RT_OB_SIDE_BITS,
               rt_side_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR_SIGNED(2 * CFG_SCAN_OBSTACLE_MM) ==
               RT_OB_WIDTH_BITS, rt_width_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(CFG_SCAN_OBSTACLE_MM) == RT_OB_CLOSE_BITS,
               rt_close_bits);
BITPACK_ASSERT((RT_OB_CLOSE_SHIFT + RT_OB_CLOSE_BITS) <= 32u, rt_ob_fits);
BITPACK_ASSERT(BITPACK_BITS_FOR(CMD_SEQ_MAX) == AK_SEQ_BITS, ak_seq_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(CMD_CODE_RESET) == AK_CODE_BITS,
               ak_code_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(FAULT_CAUSE_MAX) == FT_CAUSE_BITS,
               ft_cause_bits);
BITPACK_ASSERT((FT_DETAIL_SHIFT + FT_DETAIL_BITS) == FT_LATCHED_BIT,
               ft_layout);
BITPACK_ASSERT(BITPACK_BITS_FOR(FAULT_SRC_SAFETY - 1u) == FT_SOURCE_BITS,
               ft_source_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(FAULT_DETAIL_MAX) == FT_DETAIL_BITS,
               ft_detail_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR_SIGNED(LS_WIFI_MAX) == LS_WIFI_BITS,
               ls_wifi_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(OBST_SIDE_RIGHT) == OB_SIDE_BITS,
               ob_side_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(CFG_SCAN_OBSTACLE_MM) == OB_X_BITS,
               ob_x_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR_SIGNED(CFG_SCAN_OBSTACLE_MM) == OB_Y_BITS,
               ob_y_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(CFG_SCAN_OBSTACLE_MM) == OB_CLOSE_BITS,
               ob_close_bits);
BITPACK_ASSERT((OB_CLOSE_SHIFT + OB_CLOSE_BITS) <= 32u, ob_fits);
BITPACK_ASSERT(BITPACK_BITS_FOR_SIGNED(2 * CFG_SCAN_OBSTACLE_MM) ==
               OB_WIDTH_BITS, ob_width_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(CFG_US_MAX_MM + 1) == OB_CLEAR_BITS,
               ob_clear_bits);
BITPACK_ASSERT(-1 == SCAN_CLEAR_UNKNOWN, ob_clear_unknown);
/* x is never negative while every scan angle is within 0..180 degrees
   (the servo straight ahead at 90): cos >= 0 there.  Every angle a
   reading is kept at is within the servo's range: scan_task.c checks
   all five coarse angles when building, and scan_fine_angles() keeps
   the fine angles within it. */
BITPACK_ASSERT((CFG_SCAN_SERVO_MIN_DEG >= 0) &&
               (CFG_SCAN_SERVO_MAX_DEG <= 180), ob_x_not_negative);

static uint32_t tlm_letter_code(char letter, char none, bool * p_ok);
static char     tlm_letter_char(uint32_t code, char none);
static bool     tlm_set_clear(uint32_t * p_word, uint32_t shift,
                              int32_t clear_mm);
static int32_t  tlm_get_clear(uint32_t word, uint32_t shift);

/* ------------------------------------------------------------------ */
/*  IF-10 Navigation commands                                          */
/* ------------------------------------------------------------------ */

/*
 * Build one command; a REMAP carries its letter ('A'..'Z') and action,
 * a MAP_DEFAULT neither.  Returns false if a value was not valid:
 *   - a type that is not a command, or a REMAP whose letter or action
 *     is not valid, is stored as a REMAP of no letter, which Navigation
 *     refuses (ack ok 0) without changing the map;
 *   - a seq above CMD_SEQ_MAX is stored as is while it fits 30 bits,
 *     else as the largest value that fits.
 * cmd_parse() checks every field, so the Command task's commands always
 * pack exactly.
 */
bool
nav_cmd_pack (nav_cmd_t * p_cmd, uint8_t type, char letter, uint8_t action,
              uint32_t cmd_seq)
{
    bool b_ok = false;

    if (NULL != p_cmd)
    {
        uint32_t   word0     = 0u;
        uint32_t   word1     = 0u;
        bool const b_default = (NAV_CMD_MAP_DEFAULT == type);
        bool const b_remap   = (NAV_CMD_REMAP == type) &&
                               (letter >= 'A') && (letter <= 'Z') &&
                               (action < (uint8_t) LETTER_ACTION_COUNT);

        if (b_default)
        {
            (void) bitpack_set(&word0, NC_TYPE_SHIFT, NC_TYPE_BITS,
                               NAV_CMD_MAP_DEFAULT - 1u);
        }
        else
        {
            /* A REMAP; with no letter unless every field was valid. */
            (void) bitpack_set(&word0, NC_TYPE_SHIFT, NC_TYPE_BITS,
                               NAV_CMD_REMAP - 1u);

            if (b_remap)
            {
                /* 'A'..'Z' -> 1..26; action 0..4: both fit. */
                (void) bitpack_set(&word0, NC_LETTER_SHIFT, NC_LETTER_BITS,
                                   (uint32_t) (letter - 'A') + 1u);
                (void) bitpack_set(&word0, NC_ACTION_SHIFT, NC_ACTION_BITS,
                                   (uint32_t) action);
            }
        }

        b_ok = bitpack_set(&word1, NC_SEQ_SHIFT, NC_SEQ_BITS, cmd_seq) &&
               (cmd_seq <= CMD_SEQ_MAX);
        b_ok = b_ok && (b_default || b_remap);
        p_cmd->word[0] = word0;
        p_cmd->word[1] = word1;
    }

    return (b_ok);
}

/* NAV_CMD_xxx. */
uint8_t
nav_cmd_type (nav_cmd_t const * p_cmd)
{
    /* A 1-bit field plus 1: 1..2. */
    return ((NULL != p_cmd)
            ? (uint8_t) (bitpack_get(p_cmd->word[0], NC_TYPE_SHIFT,
                                     NC_TYPE_BITS) + 1u)
            : 0u);
}

/* A REMAP's letter 'A'..'Z', or '\0' for none. */
char
nav_cmd_letter (nav_cmd_t const * p_cmd)
{
    char letter = '\0';

    if (NAV_CMD_REMAP == nav_cmd_type(p_cmd))
    {
        uint32_t const code = bitpack_get(p_cmd->word[0], NC_LETTER_SHIFT,
                                          NC_LETTER_BITS);

        if ((code >= 1u) && (code <= NC_LETTERS))
        {
            letter = (char) ('A' + (int32_t) (code - 1u));  /* 'A'..'Z' */
        }
    }

    return (letter);
}

/* A REMAP's action (letter_action_t). */
uint8_t
nav_cmd_action (nav_cmd_t const * p_cmd)
{
    /* A 3-bit field: fits uint8_t. */
    return ((NAV_CMD_REMAP == nav_cmd_type(p_cmd))
            ? (uint8_t) bitpack_get(p_cmd->word[0], NC_ACTION_SHIFT,
                                    NC_ACTION_BITS)
            : (uint8_t) LETTER_ACTION_NONE);
}

/* The command's "seq", for the ack. */
uint32_t
nav_cmd_seq (nav_cmd_t const * p_cmd)
{
    return ((NULL != p_cmd)
            ? bitpack_get(p_cmd->word[1], NC_SEQ_SHIFT, NC_SEQ_BITS)
            : 0u);
}

/* ------------------------------------------------------------------ */
/*  IF-12 telemetry records                                            */
/* ------------------------------------------------------------------ */

/* An empty message of one type (TLM_MSG_LINE..TLM_MSG_OBSTACLE). */
void
tlm_msg_init (tlm_msg_t * p_msg, uint8_t type)
{
    if (NULL != p_msg)
    {
        tlm_msg_t const empty = { 0 };

        *p_msg = empty;
        /* A type outside 1..16 is kept as 16, which Telemetry ignores. */
        (void) bitpack_set(&p_msg->head, 0u, TM_TYPE_BITS,
                           (uint32_t) type - 1u);
    }
}

/* TLM_MSG_xxx. */
uint8_t
tlm_msg_type (tlm_msg_t const * p_msg)
{
    /* A 4-bit field plus 1: 1..16. */
    return ((NULL != p_msg)
            ? (uint8_t) (bitpack_get(p_msg->head, 0u, TM_TYPE_BITS) + 1u)
            : 0u);
}

/*
 * Each pack function below stores every value that fits its field and
 * returns false if one did not (the nearest value that fits is stored).
 * The producers' values are within range (tlm_msgs.h gives each one's
 * source), so their records pack exactly.
 */

/* Line status.  last_letter is 'A'..'Z' or TLM_NO_LETTER. */
bool
tlm_line_pack (tlm_line_t * p_rec, uint32_t raw_l, uint32_t raw_r,
               int32_t error_pm, uint32_t letters, char last_letter)
{
    bool b_ok = false;

    if (NULL != p_rec)
    {
        uint32_t word0 = 0u;
        uint32_t word1 = 0u;

        b_ok = bitpack_set(&word0, LN_RAW_L_SHIFT, LN_RAW_BITS, raw_l);
        b_ok = bitpack_set(&word0, LN_RAW_R_SHIFT, LN_RAW_BITS, raw_r) && b_ok;
        b_ok = bitpack_set(&word0, LN_LETTERS_SHIFT, LN_LETTERS_BITS,
                           letters) && b_ok;
        b_ok = bitpack_set_signed(&word1, LN_ERROR_SHIFT, LN_ERROR_BITS,
                                  error_pm) && b_ok;
        (void) bitpack_set(&word1, LN_LAST_SHIFT, LETTER_BITS,
                           tlm_letter_code(last_letter, TLM_NO_LETTER,
                                           &b_ok));
        p_rec->word[0] = word0;
        p_rec->word[1] = word1;
    }

    return (b_ok);
}

uint32_t
tlm_line_raw_l (tlm_line_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get(p_rec->word[0], LN_RAW_L_SHIFT, LN_RAW_BITS) : 0u);
}

uint32_t
tlm_line_raw_r (tlm_line_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get(p_rec->word[0], LN_RAW_R_SHIFT, LN_RAW_BITS) : 0u);
}

int32_t
tlm_line_error_pm (tlm_line_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get_signed(p_rec->word[1], LN_ERROR_SHIFT,
                                 LN_ERROR_BITS)
            : 0);
}

uint32_t
tlm_line_letters (tlm_line_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get(p_rec->word[0], LN_LETTERS_SHIFT, LN_LETTERS_BITS)
            : 0u);
}

char
tlm_line_last_letter (tlm_line_t const * p_rec)
{
    return ((NULL != p_rec)
            ? tlm_letter_char(bitpack_get(p_rec->word[1], LN_LAST_SHIFT,
                                          LETTER_BITS), TLM_NO_LETTER)
            : TLM_NO_LETTER);
}

/* Motion status. */
bool
tlm_motion_pack (tlm_motion_t * p_rec, int32_t speed_l_mmps,
                 int32_t speed_r_mmps, int32_t count_l, int32_t count_r,
                 int32_t distance_mm)
{
    bool b_ok = false;

    if (NULL != p_rec)
    {
        uint32_t word0 = 0u;
        uint32_t word3 = 0u;

        b_ok = bitpack_set_signed(&word0, MO_SPEED_L_SHIFT, MO_SPEED_BITS,
                                  speed_l_mmps);
        b_ok = bitpack_set_signed(&word0, MO_SPEED_R_SHIFT, MO_SPEED_BITS,
                                  speed_r_mmps) && b_ok;
        b_ok = bitpack_set_signed(&word3, MO_DIST_SHIFT, MOTION_DISTANCE_BITS,
                                  distance_mm) && b_ok;
        p_rec->word[0] = word0;
        p_rec->word[1] = (uint32_t) count_l;    /* two's complement */
        p_rec->word[2] = (uint32_t) count_r;
        p_rec->word[3] = word3;
    }

    return (b_ok);
}

int32_t
tlm_motion_speed_l (tlm_motion_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get_signed(p_rec->word[0], MO_SPEED_L_SHIFT,
                                 MO_SPEED_BITS)
            : 0);
}

int32_t
tlm_motion_speed_r (tlm_motion_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get_signed(p_rec->word[0], MO_SPEED_R_SHIFT,
                                 MO_SPEED_BITS)
            : 0);
}

int32_t
tlm_motion_count_l (tlm_motion_t const * p_rec)
{
    return ((NULL != p_rec) ? bitpack_word_signed(p_rec->word[1]) : 0);
}

int32_t
tlm_motion_count_r (tlm_motion_t const * p_rec)
{
    return ((NULL != p_rec) ? bitpack_word_signed(p_rec->word[2]) : 0);
}

int32_t
tlm_motion_distance_mm (tlm_motion_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get_signed(p_rec->word[3], MO_DIST_SHIFT,
                                 MOTION_DISTANCE_BITS)
            : 0);
}

/* IMU status.  A class code of 7 (no class) is kept and reported. */
bool
tlm_imu_pack (tlm_imu_t * p_rec, uint8_t motion_class, uint32_t humps,
              int32_t max_height_mm, int32_t turn_rate_dps)
{
    bool b_ok = false;

    if (NULL != p_rec)
    {
        uint32_t word0 = 0u;
        uint32_t word1 = 0u;

        b_ok = bitpack_set_signed(&word0, IM_RATE_SHIFT, IM_RATE_BITS,
                                  turn_rate_dps);
        b_ok = bitpack_set(&word0, IM_CLASS_SHIFT, IM_CLASS_BITS,
                           (uint32_t) motion_class) &&
               (motion_class < (uint8_t) IMU_CLASS_COUNT) && b_ok;
        b_ok = bitpack_set(&word0, IM_HUMPS_SHIFT, IM_HUMPS_BITS, humps) &&
               b_ok;
        b_ok = (max_height_mm >= 0) &&
               bitpack_set(&word1, IM_HEIGHT_SHIFT, IM_HEIGHT_BITS,
                           (uint32_t) max_height_mm) && b_ok;
        p_rec->word[0] = word0;
        p_rec->word[1] = word1;
    }

    return (b_ok);
}

uint8_t
tlm_imu_class (tlm_imu_t const * p_rec)
{
    /* A 3-bit field: fits uint8_t. */
    return ((NULL != p_rec)
            ? (uint8_t) bitpack_get(p_rec->word[0], IM_CLASS_SHIFT,
                                    IM_CLASS_BITS)
            : 0u);
}

uint32_t
tlm_imu_humps (tlm_imu_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get(p_rec->word[0], IM_HUMPS_SHIFT, IM_HUMPS_BITS) : 0u);
}

int32_t
tlm_imu_max_height_mm (tlm_imu_t const * p_rec)
{
    /* 7 bits: fits int32_t. */
    return ((NULL != p_rec)
            ? (int32_t) bitpack_get(p_rec->word[1], IM_HEIGHT_SHIFT,
                                    IM_HEIGHT_BITS)
            : 0);
}

int32_t
tlm_imu_rate_dps (tlm_imu_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get_signed(p_rec->word[0], IM_RATE_SHIFT, IM_RATE_BITS)
            : 0);
}

/* Scan status. */
bool
tlm_scan_pack (tlm_scan_t * p_rec, int32_t range_mm, uint32_t obstacles)
{
    bool b_ok = false;

    if (NULL != p_rec)
    {
        uint32_t word = 0u;

        b_ok = (range_mm >= 0) &&
               bitpack_set(&word, SC_RANGE_SHIFT, SC_RANGE_BITS,
                           (uint32_t) range_mm);
        b_ok = bitpack_set(&word, SC_OBST_SHIFT, SC_OBST_BITS, obstacles) &&
               b_ok;
        p_rec->word[0] = word;
    }

    return (b_ok);
}

int32_t
tlm_scan_range_mm (tlm_scan_t const * p_rec)
{
    /* 12 bits: fits int32_t. */
    return ((NULL != p_rec)
            ? (int32_t) bitpack_get(p_rec->word[0], SC_RANGE_SHIFT,
                                    SC_RANGE_BITS)
            : 0);
}

uint32_t
tlm_scan_obstacles (tlm_scan_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get(p_rec->word[0], SC_OBST_SHIFT, SC_OBST_BITS) : 0u);
}

/* Navigation status.  held_letter is 'A'..'Z' or TLM_NO_LETTER. */
bool
tlm_nav_pack (tlm_nav_t * p_rec, uint8_t mode, char held_letter,
              uint8_t hold_reason)
{
    bool b_ok = false;

    if (NULL != p_rec)
    {
        uint32_t word = 0u;

        b_ok = bitpack_set(&word, NV_MODE_SHIFT, NV_MODE_BITS,
                           (uint32_t) mode);
        b_ok = bitpack_set(&word, NV_HOLD_SHIFT, NV_HOLD_BITS,
                           (uint32_t) hold_reason) && b_ok;
        (void) bitpack_set(&word, NV_HELD_SHIFT, LETTER_BITS,
                           tlm_letter_code(held_letter, TLM_NO_LETTER,
                                           &b_ok));
        p_rec->word[0] = word;
    }

    return (b_ok);
}

uint8_t
tlm_nav_mode (tlm_nav_t const * p_rec)
{
    /* A 4-bit field: fits uint8_t. */
    return ((NULL != p_rec)
            ? (uint8_t) bitpack_get(p_rec->word[0], NV_MODE_SHIFT,
                                    NV_MODE_BITS)
            : 0u);
}

char
tlm_nav_held_letter (tlm_nav_t const * p_rec)
{
    return ((NULL != p_rec)
            ? tlm_letter_char(bitpack_get(p_rec->word[0], NV_HELD_SHIFT,
                                          LETTER_BITS), TLM_NO_LETTER)
            : TLM_NO_LETTER);
}

uint8_t
tlm_nav_hold_reason (tlm_nav_t const * p_rec)
{
    /* A 2-bit field: fits uint8_t. */
    return ((NULL != p_rec)
            ? (uint8_t) bitpack_get(p_rec->word[0], NV_HOLD_SHIFT,
                                    NV_HOLD_BITS)
            : 0u);
}

/* Safety status. */
bool
tlm_safety_pack (tlm_safety_t * p_rec, bool b_faulted, uint8_t cause,
                 bool b_wdt_armed, uint32_t queue_full, uint32_t faults,
                 uint32_t missed, uint32_t mask)
{
    bool b_ok = false;

    if (NULL != p_rec)
    {
        uint32_t word0 = 0u;
        uint32_t word1 = 0u;

        bitpack_set_flag(&word0, SF_FAULTED_BIT, b_faulted);
        bitpack_set_flag(&word0, SF_WDT_BIT, b_wdt_armed);
        b_ok = bitpack_set(&word0, SF_CAUSE_SHIFT, SF_CAUSE_BITS,
                           (uint32_t) cause);
        b_ok = bitpack_set(&word0, SF_QFULL_SHIFT, SF_QFULL_BITS,
                           queue_full) && b_ok;
        b_ok = bitpack_set(&word0, SF_FAULTS_SHIFT, SF_COUNT_BITS, faults) &&
               b_ok;
        b_ok = bitpack_set(&word1, SF_MISSED_SHIFT, SF_COUNT_BITS, missed) &&
               b_ok;
        b_ok = bitpack_set(&word1, SF_MASK_SHIFT, SF_MASK_BITS, mask) && b_ok;
        p_rec->word[0] = word0;
        p_rec->word[1] = word1;
    }

    return (b_ok);
}

bool
tlm_safety_faulted (tlm_safety_t const * p_rec)
{
    return ((NULL != p_rec) &&
            bitpack_get_flag(p_rec->word[0], SF_FAULTED_BIT));
}

uint8_t
tlm_safety_cause (tlm_safety_t const * p_rec)
{
    /* A 4-bit field: fits uint8_t. */
    return ((NULL != p_rec)
            ? (uint8_t) bitpack_get(p_rec->word[0], SF_CAUSE_SHIFT,
                                    SF_CAUSE_BITS)
            : 0u);
}

bool
tlm_safety_wdt_armed (tlm_safety_t const * p_rec)
{
    return ((NULL != p_rec) && bitpack_get_flag(p_rec->word[0], SF_WDT_BIT));
}

uint32_t
tlm_safety_queue_full (tlm_safety_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get(p_rec->word[0], SF_QFULL_SHIFT, SF_QFULL_BITS)
            : 0u);
}

uint32_t
tlm_safety_faults (tlm_safety_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get(p_rec->word[0], SF_FAULTS_SHIFT, SF_COUNT_BITS)
            : 0u);
}

uint32_t
tlm_safety_missed (tlm_safety_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get(p_rec->word[1], SF_MISSED_SHIFT, SF_COUNT_BITS)
            : 0u);
}

/* The fault mask: bit (cause - 1), FAULT_MASK_BITS bits. */
uint32_t
tlm_safety_mask (tlm_safety_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get(p_rec->word[1], SF_MASK_SHIFT, SF_MASK_BITS)
            : 0u);
}

/*
 * Route-log entry.  Only the values the kind carries are kept (tlm_msgs.h
 * lists them; letter is a BARCODE's 'A'..'Z', action is a letter_action_t
 * or, for OBSTACLE, the side); the others read 0.
 */
bool
tlm_route_pack (tlm_route_t * p_rec, uint8_t kind, char letter,
                uint8_t action, int32_t distance_mm, int32_t value,
                int32_t value2)
{
    bool b_ok = false;

    if (NULL != p_rec)
    {
        uint32_t word0 = 0u;
        uint32_t word1 = 0u;
        uint32_t word2 = 0u;

        b_ok = bitpack_set_signed(&word0, RT_DIST_SHIFT, MOTION_DISTANCE_BITS,
                                  distance_mm);
        b_ok = bitpack_set(&word1, RT_KIND_SHIFT, RT_KIND_BITS,
                           (uint32_t) kind) && b_ok;

        switch (kind)
        {
            case ROUTE_BARCODE:
                (void) bitpack_set(&word1, RT_BC_LETTER_SHIFT, LETTER_BITS,
                                   tlm_letter_code(letter, '\0', &b_ok));
                b_ok = bitpack_set(&word1, RT_BC_ACTION_SHIFT, RT_ACTION_BITS,
                                   (uint32_t) action) && b_ok;
                b_ok = ((0 == value) || (1 == value)) && b_ok;
                bitpack_set_flag(&word1, RT_BC_REV_BIT, (0 != value));
            break;

            case ROUTE_TURN:
                b_ok = bitpack_set(&word1, RT_TURN_ACT_SHIFT, RT_ACTION_BITS,
                                   (uint32_t) action) && b_ok;
                b_ok = bitpack_set_signed(&word1, RT_TURN_CMD_SHIFT,
                                          RT_ANGLE_BITS, value) && b_ok;
                b_ok = bitpack_set_signed(&word2, RT_TURN_MEAS_SHIFT,
                                          RT_ANGLE_BITS, value2) && b_ok;
            break;

            case ROUTE_HUMP:
                b_ok = (value >= 0) &&
                       bitpack_set(&word1, RT_HUMP_H_SHIFT, RT_HUMP_H_BITS,
                                   (uint32_t) value) && b_ok;
                b_ok = (value2 >= 0) &&
                       bitpack_set(&word1, RT_HUMP_PEAK_SHIFT,
                                   RT_HUMP_PEAK_BITS, (uint32_t) value2) &&
                       b_ok;
            break;

            case ROUTE_OBSTACLE:
                b_ok = bitpack_set(&word1, RT_OB_SIDE_SHIFT, RT_OB_SIDE_BITS,
                                   (uint32_t) action) && b_ok;
                b_ok = bitpack_set_signed(&word1, RT_OB_WIDTH_SHIFT,
                                          RT_OB_WIDTH_BITS, value) && b_ok;
                b_ok = (value2 >= 0) &&
                       bitpack_set(&word1, RT_OB_CLOSE_SHIFT,
                                   RT_OB_CLOSE_BITS, (uint32_t) value2) &&
                       b_ok;
            break;

            case ROUTE_LINE_SEARCH:
                b_ok = ((0 == value) || (1 == value)) && b_ok;
                bitpack_set_flag(&word1, RT_SEARCH_BIT, (0 != value));
            break;

            case ROUTE_SAFE_HOLD:
                b_ok = (value >= 0) &&
                       bitpack_set(&word1, RT_HOLD_SHIFT, RT_HOLD_BITS,
                                   (uint32_t) value) && b_ok;
            break;

            case ROUTE_COMPLETE:
                word2 = (uint32_t) value;       /* all 32 bits */
            break;

            default:
                /* RUN_START, NO_COMMAND, FAULT: no values. */
            break;
        }

        p_rec->word[0] = word0;
        p_rec->word[1] = word1;
        p_rec->word[2] = word2;
    }

    return (b_ok);
}

/* ROUTE_xxx. */
uint8_t
tlm_route_kind (tlm_route_t const * p_rec)
{
    /* A 4-bit field: fits uint8_t. */
    return ((NULL != p_rec)
            ? (uint8_t) bitpack_get(p_rec->word[1], RT_KIND_SHIFT,
                                    RT_KIND_BITS)
            : 0u);
}

/* A BARCODE's letter, else '\0'. */
char
tlm_route_letter (tlm_route_t const * p_rec)
{
    return ((ROUTE_BARCODE == tlm_route_kind(p_rec))
            ? tlm_letter_char(bitpack_get(p_rec->word[1], RT_BC_LETTER_SHIFT,
                                          LETTER_BITS), '\0')
            : '\0');
}

/* A BARCODE's or TURN's action, an OBSTACLE's side, else 0. */
uint8_t
tlm_route_action (tlm_route_t const * p_rec)
{
    uint8_t const kind   = tlm_route_kind(p_rec);
    uint32_t      action = 0u;

    if (ROUTE_BARCODE == kind)
    {
        action = bitpack_get(p_rec->word[1], RT_BC_ACTION_SHIFT,
                             RT_ACTION_BITS);
    }
    else if (ROUTE_TURN == kind)
    {
        action = bitpack_get(p_rec->word[1], RT_TURN_ACT_SHIFT,
                             RT_ACTION_BITS);
    }
    else if (ROUTE_OBSTACLE == kind)
    {
        action = bitpack_get(p_rec->word[1], RT_OB_SIDE_SHIFT,
                             RT_OB_SIDE_BITS);
    }
    else
    {
        /* No action. */
    }

    return ((uint8_t) action);          /* at most 3 bits */
}

int32_t
tlm_route_distance_mm (tlm_route_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get_signed(p_rec->word[0], RT_DIST_SHIFT,
                                 MOTION_DISTANCE_BITS)
            : 0);
}

/* The kind's first value as Navigation passed it (tlm_msgs.h), else 0. */
int32_t
tlm_route_value (tlm_route_t const * p_rec)
{
    int32_t value = 0;

    switch (tlm_route_kind(p_rec))
    {
        case ROUTE_BARCODE:
            value = bitpack_get_flag(p_rec->word[1], RT_BC_REV_BIT) ? 1 : 0;
        break;

        case ROUTE_TURN:
            value = bitpack_get_signed(p_rec->word[1], RT_TURN_CMD_SHIFT,
                                       RT_ANGLE_BITS);
        break;

        case ROUTE_HUMP:
            /* 7 bits: fits int32_t. */
            value = (int32_t) bitpack_get(p_rec->word[1], RT_HUMP_H_SHIFT,
                                          RT_HUMP_H_BITS);
        break;

        case ROUTE_OBSTACLE:
            value = bitpack_get_signed(p_rec->word[1], RT_OB_WIDTH_SHIFT,
                                       RT_OB_WIDTH_BITS);
        break;

        case ROUTE_LINE_SEARCH:
            value = bitpack_get_flag(p_rec->word[1], RT_SEARCH_BIT) ? 1 : 0;
        break;

        case ROUTE_SAFE_HOLD:
            /* 2 bits: fits int32_t. */
            value = (int32_t) bitpack_get(p_rec->word[1], RT_HOLD_SHIFT,
                                          RT_HOLD_BITS);
        break;

        case ROUTE_COMPLETE:
            value = bitpack_word_signed(p_rec->word[2]);
        break;

        default:
            /* No value. */
        break;
    }

    return (value);
}

/* The kind's second value as Navigation passed it, else 0. */
int32_t
tlm_route_value2 (tlm_route_t const * p_rec)
{
    int32_t value = 0;

    switch (tlm_route_kind(p_rec))
    {
        case ROUTE_TURN:
            value = bitpack_get_signed(p_rec->word[2], RT_TURN_MEAS_SHIFT,
                                       RT_ANGLE_BITS);
        break;

        case ROUTE_HUMP:
            /* 15 bits: fits int32_t. */
            value = (int32_t) bitpack_get(p_rec->word[1], RT_HUMP_PEAK_SHIFT,
                                          RT_HUMP_PEAK_BITS);
        break;

        case ROUTE_OBSTACLE:
            /* 9 bits: fits int32_t. */
            value = (int32_t) bitpack_get(p_rec->word[1], RT_OB_CLOSE_SHIFT,
                                          RT_OB_CLOSE_BITS);
        break;

        default:
            /* No second value. */
        break;
    }

    return (value);
}

/* Ack. */
bool
tlm_ack_pack (tlm_ack_t * p_rec, uint32_t cmd_seq, uint8_t cmd_code,
              bool b_cmd_ok)
{
    bool b_ok = false;

    if (NULL != p_rec)
    {
        uint32_t word0 = 0u;
        uint32_t word1 = 0u;

        b_ok = bitpack_set(&word0, AK_SEQ_SHIFT, AK_SEQ_BITS, cmd_seq);
        b_ok = bitpack_set(&word1, AK_CODE_SHIFT, AK_CODE_BITS,
                           (uint32_t) cmd_code) && b_ok;
        bitpack_set_flag(&word1, AK_OK_BIT, b_cmd_ok);
        p_rec->word[0] = word0;
        p_rec->word[1] = word1;
    }

    return (b_ok);
}

uint32_t
tlm_ack_seq (tlm_ack_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get(p_rec->word[0], AK_SEQ_SHIFT, AK_SEQ_BITS) : 0u);
}

uint8_t
tlm_ack_code (tlm_ack_t const * p_rec)
{
    /* A 3-bit field: fits uint8_t. */
    return ((NULL != p_rec)
            ? (uint8_t) bitpack_get(p_rec->word[1], AK_CODE_SHIFT,
                                    AK_CODE_BITS)
            : 0u);
}

bool
tlm_ack_ok (tlm_ack_t const * p_rec)
{
    return ((NULL != p_rec) && bitpack_get_flag(p_rec->word[1], AK_OK_BIT));
}

/* Fault latched or released.  A source outside 1..8 is kept as
   FAULT_SRC_SAFETY and reported, as in fault_evt_pack(). */
bool
tlm_fault_pack (tlm_fault_t * p_rec, uint8_t cause, uint8_t source,
                uint16_t detail, bool b_latched)
{
    bool b_ok = false;

    if (NULL != p_rec)
    {
        uint32_t      word        = 0u;
        bool const    b_source_ok = ((source >= FAULT_SRC_LINE) &&
                                     (source <= FAULT_SRC_SAFETY));
        uint8_t const kept_source = b_source_ok ? source : FAULT_SRC_SAFETY;

        b_ok = bitpack_set(&word, FT_CAUSE_SHIFT, FT_CAUSE_BITS,
                           (uint32_t) cause);
        (void) bitpack_set(&word, FT_SOURCE_SHIFT, FT_SOURCE_BITS,
                           (uint32_t) kept_source - 1u);
        b_ok = bitpack_set(&word, FT_DETAIL_SHIFT, FT_DETAIL_BITS,
                           (uint32_t) detail) && b_ok && b_source_ok;
        bitpack_set_flag(&word, FT_LATCHED_BIT, b_latched);
        p_rec->word[0] = word;
    }

    return (b_ok);
}

uint8_t
tlm_fault_cause (tlm_fault_t const * p_rec)
{
    /* A 4-bit field: fits uint8_t. */
    return ((NULL != p_rec)
            ? (uint8_t) bitpack_get(p_rec->word[0], FT_CAUSE_SHIFT,
                                    FT_CAUSE_BITS)
            : 0u);
}

uint8_t
tlm_fault_source (tlm_fault_t const * p_rec)
{
    /* A 3-bit field plus 1: 1..8. */
    return ((NULL != p_rec)
            ? (uint8_t) (bitpack_get(p_rec->word[0], FT_SOURCE_SHIFT,
                                     FT_SOURCE_BITS) + 1u)
            : 0u);
}

uint16_t
tlm_fault_detail (tlm_fault_t const * p_rec)
{
    /* A 13-bit field: fits uint16_t. */
    return ((NULL != p_rec)
            ? (uint16_t) bitpack_get(p_rec->word[0], FT_DETAIL_SHIFT,
                                     FT_DETAIL_BITS)
            : 0u);
}

bool
tlm_fault_latched (tlm_fault_t const * p_rec)
{
    return ((NULL != p_rec) &&
            bitpack_get_flag(p_rec->word[0], FT_LATCHED_BIT));
}

/* MQTT link state. */
bool
tlm_link_pack (tlm_link_t * p_rec, bool b_online, int32_t wifi_status)
{
    bool b_ok = false;

    if (NULL != p_rec)
    {
        uint32_t word = 0u;

        bitpack_set_flag(&word, LS_ONLINE_BIT, b_online);
        b_ok = bitpack_set_signed(&word, LS_WIFI_SHIFT, LS_WIFI_BITS,
                                  wifi_status);
        p_rec->word[0] = word;
    }

    return (b_ok);
}

bool
tlm_link_online (tlm_link_t const * p_rec)
{
    return ((NULL != p_rec) &&
            bitpack_get_flag(p_rec->word[0], LS_ONLINE_BIT));
}

int32_t
tlm_link_wifi (tlm_link_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get_signed(p_rec->word[0], LS_WIFI_SHIFT, LS_WIFI_BITS)
            : 0);
}

/* Obstacle profile (FR10). */
bool
tlm_obstacle_pack (tlm_obstacle_t * p_rec, uint8_t side, bool b_reversed,
                   int32_t distance_mm, int32_t offset_mm, int32_t width_mm,
                   int32_t closest_mm, int32_t clear_left_mm,
                   int32_t clear_right_mm)
{
    bool b_ok = false;

    if (NULL != p_rec)
    {
        uint32_t word0 = 0u;
        uint32_t word1 = 0u;
        uint32_t word2 = 0u;

        b_ok = bitpack_set(&word0, OB_SIDE_SHIFT, OB_SIDE_BITS,
                           (uint32_t) side);
        bitpack_set_flag(&word0, OB_REV_BIT, b_reversed);
        b_ok = (distance_mm >= 0) &&
               bitpack_set(&word0, OB_X_SHIFT, OB_X_BITS,
                           (uint32_t) distance_mm) && b_ok;
        b_ok = bitpack_set_signed(&word0, OB_Y_SHIFT, OB_Y_BITS, offset_mm) &&
               b_ok;
        b_ok = (closest_mm >= 0) &&
               bitpack_set(&word0, OB_CLOSE_SHIFT, OB_CLOSE_BITS,
                           (uint32_t) closest_mm) && b_ok;
        b_ok = bitpack_set_signed(&word1, OB_WIDTH_SHIFT, OB_WIDTH_BITS,
                                  width_mm) && b_ok;
        b_ok = tlm_set_clear(&word1, OB_CLEAR_L_SHIFT, clear_left_mm) && b_ok;
        b_ok = tlm_set_clear(&word2, OB_CLEAR_R_SHIFT, clear_right_mm) &&
               b_ok;
        p_rec->word[0] = word0;
        p_rec->word[1] = word1;
        p_rec->word[2] = word2;
    }

    return (b_ok);
}

uint8_t
tlm_obstacle_side (tlm_obstacle_t const * p_rec)
{
    /* A 2-bit field: fits uint8_t. */
    return ((NULL != p_rec)
            ? (uint8_t) bitpack_get(p_rec->word[0], OB_SIDE_SHIFT,
                                    OB_SIDE_BITS)
            : 0u);
}

bool
tlm_obstacle_reversed (tlm_obstacle_t const * p_rec)
{
    return ((NULL != p_rec) && bitpack_get_flag(p_rec->word[0], OB_REV_BIT));
}

int32_t
tlm_obstacle_distance_mm (tlm_obstacle_t const * p_rec)
{
    /* 9 bits: fits int32_t. */
    return ((NULL != p_rec)
            ? (int32_t) bitpack_get(p_rec->word[0], OB_X_SHIFT, OB_X_BITS)
            : 0);
}

int32_t
tlm_obstacle_offset_mm (tlm_obstacle_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get_signed(p_rec->word[0], OB_Y_SHIFT, OB_Y_BITS) : 0);
}

int32_t
tlm_obstacle_width_mm (tlm_obstacle_t const * p_rec)
{
    return ((NULL != p_rec)
            ? bitpack_get_signed(p_rec->word[1], OB_WIDTH_SHIFT,
                                 OB_WIDTH_BITS)
            : 0);
}

int32_t
tlm_obstacle_closest_mm (tlm_obstacle_t const * p_rec)
{
    /* 9 bits: fits int32_t. */
    return ((NULL != p_rec)
            ? (int32_t) bitpack_get(p_rec->word[0], OB_CLOSE_SHIFT,
                                    OB_CLOSE_BITS)
            : 0);
}

int32_t
tlm_obstacle_clear_left_mm (tlm_obstacle_t const * p_rec)
{
    return ((NULL != p_rec)
            ? tlm_get_clear(p_rec->word[1], OB_CLEAR_L_SHIFT) : 0);
}

int32_t
tlm_obstacle_clear_right_mm (tlm_obstacle_t const * p_rec)
{
    return ((NULL != p_rec)
            ? tlm_get_clear(p_rec->word[2], OB_CLEAR_R_SHIFT) : 0);
}

/* 'A'..'Z' -> 1..26, none -> 0; anything else -> 0 and *p_ok false. */
static uint32_t
tlm_letter_code (char letter, char none, bool * p_ok)
{
    uint32_t code = 0u;

    if ((letter >= 'A') && (letter <= 'Z'))
    {
        code = (uint32_t) (letter - 'A') + 1u;      /* 1..26 */
    }
    else if (letter != none)
    {
        *p_ok = false;
    }
    else
    {
        /* None: 0. */
    }

    return (code);
}

static char
tlm_letter_char (uint32_t code, char none)
{
    return (((code >= 1u) && (code <= NC_LETTERS))
            ? (char) ('A' + (int32_t) (code - 1u))  /* 'A'..'Z' */
            : none);
}

/* A clearance, -1 (never seen) .. 4000, stored + 1. */
static bool
tlm_set_clear (uint32_t * p_word, uint32_t shift, int32_t clear_mm)
{
    /* Below -1: the nearest, -1 (code 0).  No overflow at INT32_MAX:
       the + 1 is done unsigned and bitpack_set() then reports it. */
    uint32_t const code = (clear_mm < 0) ? 0u : ((uint32_t) clear_mm + 1u);

    return (bitpack_set(p_word, shift, OB_CLEAR_BITS, code) &&
            (clear_mm >= SCAN_CLEAR_UNKNOWN));
}

static int32_t
tlm_get_clear (uint32_t word, uint32_t shift)
{
    /* 12 bits: fits int32_t. */
    return ((int32_t) bitpack_get(word, shift, OB_CLEAR_BITS) - 1);
}

/*** end of file ***/
