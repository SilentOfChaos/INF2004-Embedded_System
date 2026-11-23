/*
 *  imu_msgs.c
 *  Pack and read the IMU events (IF-03) and fault reports (IF-04).  See
 *  imu_msgs.h for the layouts.
 */

#include <stddef.h>

#include "../app_config.h"
#include "../common/bitpack.h"
#include "imu_msgs.h"

#define IE_TYPE_SHIFT       0u
#define IE_TYPE_BITS        2u
#define IE_HEADING_SHIFT    2u      /* HEADING */
#define IE_HEADING_BITS     16u
#define IE_LEVEL_BIT        18u
#define IE_PEAK_SHIFT       2u      /* HUMP_END */
#define IE_PEAK_BITS        15u
#define IE_HEIGHT_SHIFT     17u
#define IE_HEIGHT_BITS      7u
#define IE_CLASS_SHIFT      2u      /* CLASS */
#define IE_CLASS_BITS       3u
#define IE_HALF_TURN_CDEG   18000

#define FE_CAUSE_SHIFT      0u
#define FE_CAUSE_BITS       4u
#define FE_SOURCE_SHIFT     4u
#define FE_SOURCE_BITS      3u
#define FE_DETAIL_SHIFT     7u
#define FE_DETAIL_BITS      13u
#define FE_CLEARED_BIT      20u

/* The layouts hold every value they must. */
BITPACK_ASSERT(BITPACK_BITS_FOR(IMU_EVT_CLASS - 1u) == IE_TYPE_BITS,
               ie_type_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR_SIGNED(IE_HALF_TURN_CDEG) == IE_HEADING_BITS,
               ie_heading_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(IE_HALF_TURN_CDEG) == IE_PEAK_BITS,
               ie_peak_bits);
BITPACK_ASSERT((IE_PEAK_SHIFT + IE_PEAK_BITS) == IE_HEIGHT_SHIFT,
               ie_layout);
BITPACK_ASSERT((IE_HEIGHT_SHIFT + IE_HEIGHT_BITS) <= 32u, ie_fits);
BITPACK_ASSERT(BITPACK_BITS_FOR(IMU_CLASS_COUNT - 1u) == IE_CLASS_BITS,
               ie_class_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(FAULT_CAUSE_MAX) == FE_CAUSE_BITS,
               fe_cause_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(FAULT_SRC_SAFETY - 1u) == FE_SOURCE_BITS,
               fe_source_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(FAULT_DETAIL_MAX) == FE_DETAIL_BITS,
               fe_detail_bits);
BITPACK_ASSERT((FE_CAUSE_SHIFT + FE_CAUSE_BITS) == FE_SOURCE_SHIFT,
               fe_layout_source);
BITPACK_ASSERT((FE_SOURCE_SHIFT + FE_SOURCE_BITS) == FE_DETAIL_SHIFT,
               fe_layout_detail);
BITPACK_ASSERT((FE_DETAIL_SHIFT + FE_DETAIL_BITS) == FE_CLEARED_BIT,
               fe_layout_cleared);
/* The fault mask has a bit for every cause and fits Safety's 16 bits. */
BITPACK_ASSERT(FAULT_MASK_BITS <= 16u, fault_mask_fits);
BITPACK_ASSERT(FAULT_MASK_ALL == (FAULT_MASK_LATCHING | FAULT_MASK_STATUS),
               fault_mask_table);

static void ie_set_type(uint32_t * p_word, uint8_t type);

/* ------------------------------------------------------------------ */
/*  IF-03 IMU events                                                   */
/* ------------------------------------------------------------------ */

/* Periodic heading.  Returns false if it was outside 16 bits. */
bool
imu_evt_heading (imu_evt_t * p_evt, int32_t heading_cdeg,
                 bool b_level_ground)
{
    bool b_ok = false;

    if (NULL != p_evt)
    {
        uint32_t word = 0u;

        ie_set_type(&word, IMU_EVT_HEADING);
        b_ok = bitpack_set_signed(&word, IE_HEADING_SHIFT, IE_HEADING_BITS,
                                  heading_cdeg);
        bitpack_set_flag(&word, IE_LEVEL_BIT, b_level_ground);
        p_evt->bits = word;
    }

    return (b_ok);
}

void
imu_evt_hump_start (imu_evt_t * p_evt)
{
    if (NULL != p_evt)
    {
        uint32_t word = 0u;

        ie_set_type(&word, IMU_EVT_HUMP_START);
        p_evt->bits = word;
    }
}

/* Hump crossed: peak roll and height.  Returns false if one did not
   fit (the nearest value that fits is stored). */
bool
imu_evt_hump_end (imu_evt_t * p_evt, int32_t peak_roll_cdeg,
                  int32_t height_mm)
{
    bool b_ok = false;

    if (NULL != p_evt)
    {
        uint32_t word = 0u;

        ie_set_type(&word, IMU_EVT_HUMP_END);
        b_ok = (peak_roll_cdeg >= 0) &&
               bitpack_set(&word, IE_PEAK_SHIFT, IE_PEAK_BITS,
                           (uint32_t) peak_roll_cdeg);
        b_ok = (height_mm >= 0) &&
               bitpack_set(&word, IE_HEIGHT_SHIFT, IE_HEIGHT_BITS,
                           (uint32_t) height_mm) && b_ok;
        p_evt->bits = word;
    }

    return (b_ok);
}

/* Motion class changed. */
bool
imu_evt_class (imu_evt_t * p_evt, imu_class_t motion_class)
{
    bool b_ok = false;

    if (NULL != p_evt)
    {
        uint32_t word = 0u;

        ie_set_type(&word, IMU_EVT_CLASS);
        b_ok = bitpack_set(&word, IE_CLASS_SHIFT, IE_CLASS_BITS,
                           (uint32_t) motion_class);
        p_evt->bits = word;
    }

    return (b_ok);
}

/* IMU_EVT_xxx. */
uint8_t
imu_evt_type (imu_evt_t const * p_evt)
{
    /* A 2-bit field plus 1: 1..4. */
    return ((NULL != p_evt)
            ? (uint8_t) (bitpack_get(p_evt->bits, IE_TYPE_SHIFT,
                                     IE_TYPE_BITS) + 1u)
            : 0u);
}

int32_t
imu_evt_heading_cdeg (imu_evt_t const * p_evt)
{
    return ((IMU_EVT_HEADING == imu_evt_type(p_evt))
            ? bitpack_get_signed(p_evt->bits, IE_HEADING_SHIFT,
                                 IE_HEADING_BITS)
            : 0);
}

bool
imu_evt_level_ground (imu_evt_t const * p_evt)
{
    return ((IMU_EVT_HEADING == imu_evt_type(p_evt)) &&
            bitpack_get_flag(p_evt->bits, IE_LEVEL_BIT));
}

int32_t
imu_evt_peak_roll_cdeg (imu_evt_t const * p_evt)
{
    /* 15 bits: fits int32_t. */
    return ((IMU_EVT_HUMP_END == imu_evt_type(p_evt))
            ? (int32_t) bitpack_get(p_evt->bits, IE_PEAK_SHIFT, IE_PEAK_BITS)
            : 0);
}

int32_t
imu_evt_height_mm (imu_evt_t const * p_evt)
{
    /* 7 bits: fits int32_t. */
    return ((IMU_EVT_HUMP_END == imu_evt_type(p_evt))
            ? (int32_t) bitpack_get(p_evt->bits, IE_HEIGHT_SHIFT,
                                    IE_HEIGHT_BITS)
            : 0);
}

imu_class_t
imu_evt_motion_class (imu_evt_t const * p_evt)
{
    imu_class_t motion_class = IMU_CLASS_STATIONARY;

    if (IMU_EVT_CLASS == imu_evt_type(p_evt))
    {
        uint32_t const code = bitpack_get(p_evt->bits, IE_CLASS_SHIFT,
                                          IE_CLASS_BITS);

        if (code < (uint32_t) IMU_CLASS_COUNT)
        {
            motion_class = (imu_class_t) code;  /* range checked above */
        }
    }

    return (motion_class);
}

static void
ie_set_type (uint32_t * p_word, uint8_t type)
{
    /* Callers pass IMU_EVT_HEADING..IMU_EVT_CLASS: it fits. */
    (void) bitpack_set(p_word, IE_TYPE_SHIFT, IE_TYPE_BITS,
                       (uint32_t) type - 1u);
}

/* ------------------------------------------------------------------ */
/*  IF-04 fault reports                                                */
/* ------------------------------------------------------------------ */

/*
 * Build one fault report.  Returns false if a value did not fit: the
 * nearest value that fits is stored, and a source outside 1..8 is kept
 * as FAULT_SRC_SAFETY.
 */
bool
fault_evt_pack (fault_evt_t * p_evt, uint8_t cause, uint8_t source,
                uint16_t detail)
{
    bool b_ok = false;

    if (NULL != p_evt)
    {
        uint32_t      word        = 0u;
        bool const    b_source_ok = ((source >= FAULT_SRC_LINE) &&
                                     (source <= FAULT_SRC_SAFETY));
        uint8_t const kept_source = b_source_ok ? source : FAULT_SRC_SAFETY;

        b_ok = bitpack_set(&word, FE_CAUSE_SHIFT, FE_CAUSE_BITS,
                           (uint32_t) cause);
        (void) bitpack_set(&word, FE_SOURCE_SHIFT, FE_SOURCE_BITS,
                           (uint32_t) kept_source - 1u);
        b_ok = bitpack_set(&word, FE_DETAIL_SHIFT, FE_DETAIL_BITS,
                           (uint32_t) detail) && b_ok && b_source_ok;
        p_evt->bits = word;
    }

    return (b_ok);
}

/*
 * A status cause started (b_on) or ended.  Returns false if the cause
 * is not a status cause (it is packed all the same, and Safety latches
 * it like any latching fault) or the source is not 1..8.
 */
bool
fault_evt_pack_status (fault_evt_t * p_evt, uint8_t cause, uint8_t source,
                       bool b_on)
{
    bool b_ok = fault_evt_pack(p_evt, cause, source, 0u);

    if (NULL != p_evt)
    {
        bitpack_set_flag(&p_evt->bits, FE_CLEARED_BIT, !b_on);
        b_ok = b_ok && !fault_cause_latches(cause);
    }

    return (b_ok);
}

/* FAULT_CAUSE_xxx. */
uint8_t
fault_evt_cause (fault_evt_t const * p_evt)
{
    /* A 4-bit field: fits uint8_t. */
    return ((NULL != p_evt)
            ? (uint8_t) bitpack_get(p_evt->bits, FE_CAUSE_SHIFT,
                                    FE_CAUSE_BITS)
            : 0u);
}

/* FAULT_SRC_xxx. */
uint8_t
fault_evt_source (fault_evt_t const * p_evt)
{
    /* A 3-bit field plus 1: 1..8. */
    return ((NULL != p_evt)
            ? (uint8_t) (bitpack_get(p_evt->bits, FE_SOURCE_SHIFT,
                                     FE_SOURCE_BITS) + 1u)
            : 0u);
}

uint16_t
fault_evt_detail (fault_evt_t const * p_evt)
{
    /* A 13-bit field: fits uint16_t. */
    return ((NULL != p_evt)
            ? (uint16_t) bitpack_get(p_evt->bits, FE_DETAIL_SHIFT,
                                     FE_DETAIL_BITS)
            : 0u);
}

/* A status cause's report: true if the condition has ended. */
bool
fault_evt_cleared (fault_evt_t const * p_evt)
{
    return ((NULL != p_evt) &&
            bitpack_get_flag(p_evt->bits, FE_CLEARED_BIT));
}

/* The cause's bit in the fault mask; 0 for a cause outside 1..11. */
uint16_t
fault_cause_bit (uint8_t cause)
{
    /* At most bit 10 (BITPACK_ASSERT above): fits uint16_t. */
    return (((cause >= FAULT_CAUSE_LIFTED) && (cause <= FAULT_CAUSE_MAX))
            ? (uint16_t) FAULT_BIT(cause)
            : 0u);
}

/*
 * The fault table's latch flag.  False only for a status cause; any
 * other value, even one outside the table, latches (fail safe).
 */
bool
fault_cause_latches (uint8_t cause)
{
    return (0u == (fault_cause_bit(cause) & FAULT_MASK_STATUS));
}

/*** end of file ***/
