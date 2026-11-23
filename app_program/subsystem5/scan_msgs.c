/*
 *  scan_msgs.c
 *  Pack and read the obstacle events (IF-05) and scan requests (IF-06).
 *  See scan_msgs.h for the layouts.
 */

#include <stddef.h>

#include "../app_config.h"
#include "../common/bitpack.h"
#include "scan_msgs.h"

#define OE_TYPE_SHIFT       0u      /* word 0 */
#define OE_TYPE_BITS        3u
#define OE_SIDE_SHIFT       3u      /* PLAN */
#define OE_SIDE_BITS        2u
#define OE_CLOSEST_SHIFT    5u
#define OE_CLOSEST_BITS     BITPACK_BITS_FOR(CFG_SCAN_OBSTACLE_MM)
#define OE_WIDTH_SHIFT      14u
#define OE_WIDTH_BITS       BITPACK_BITS_FOR_SIGNED(2u * CFG_SCAN_OBSTACLE_MM)
#define OE_LEFT_SHIFT       0u      /* word 1 */
#define OE_RIGHT_SHIFT      10u
#define OE_EDGE_BITS        BITPACK_BITS_FOR_SIGNED(CFG_SCAN_OBSTACLE_MM)

#define SR_TYPE_SHIFT       0u
#define SR_TYPE_BITS        4u
#define SR_SIDE_SHIFT       4u
#define SR_SIDE_BITS        2u

/* The layouts hold every value they must (500 mm hits: 9, 11, 10 bits). */
BITPACK_ASSERT(BITPACK_BITS_FOR(OBST_EVT_NO_SAFE_ROUTE - 1u) == OE_TYPE_BITS,
               oe_type_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(OBST_SIDE_RIGHT) == OE_SIDE_BITS,
               oe_side_bits);
BITPACK_ASSERT((OE_CLOSEST_SHIFT + OE_CLOSEST_BITS) <= OE_WIDTH_SHIFT,
               oe_layout_1);
BITPACK_ASSERT((OE_WIDTH_SHIFT + OE_WIDTH_BITS) <= 32u, oe_layout_2);
BITPACK_ASSERT((OE_LEFT_SHIFT + OE_EDGE_BITS) <= OE_RIGHT_SHIFT,
               oe_layout_3);
BITPACK_ASSERT((OE_RIGHT_SHIFT + OE_EDGE_BITS) <= 32u, oe_layout_4);
BITPACK_ASSERT(BITPACK_BITS_FOR(SCAN_REQ_RESUME) == SR_TYPE_BITS,
               sr_type_bits);
BITPACK_ASSERT(BITPACK_BITS_FOR(OBST_SIDE_RIGHT) == SR_SIDE_BITS,
               sr_side_bits);
BITPACK_ASSERT((SR_SIDE_SHIFT + SR_SIDE_BITS) <= 8u, sr_fits_8_bits);

static bool oe_set_type(uint32_t * p_word, uint8_t type);

/* ------------------------------------------------------------------ */
/*  IF-05 obstacle events                                              */
/* ------------------------------------------------------------------ */

/* An event with no profile (every type but OBST_EVT_PLAN).  Returns
   false if the type did not fit (the nearest that fits is stored). */
bool
obstacle_evt_simple (obstacle_evt_t * p_evt, uint8_t type)
{
    bool b_ok = false;

    if (NULL != p_evt)
    {
        uint32_t word0 = 0u;

        b_ok = oe_set_type(&word0, type);
        p_evt->word[0] = word0;
        p_evt->word[1] = 0u;
    }

    return (b_ok);
}

/*
 * OBST_EVT_PLAN with the profile Navigation uses.  Returns false if a
 * value did not fit (the nearest value that fits is stored); a plan's
 * points are all hits, within CFG_SCAN_OBSTACLE_MM, so they fit.
 */
bool
obstacle_evt_plan (obstacle_evt_t * p_evt, uint8_t side, int32_t closest_mm,
                   int32_t width_mm, int32_t edge_left_mm,
                   int32_t edge_right_mm)
{
    bool b_ok = false;

    if (NULL != p_evt)
    {
        uint32_t word0 = 0u;
        uint32_t word1 = 0u;

        b_ok = oe_set_type(&word0, OBST_EVT_PLAN);
        b_ok = bitpack_set(&word0, OE_SIDE_SHIFT, OE_SIDE_BITS,
                           (uint32_t) side) && b_ok;
        b_ok = (closest_mm >= 0) &&
               bitpack_set(&word0, OE_CLOSEST_SHIFT, OE_CLOSEST_BITS,
                           (uint32_t) closest_mm) && b_ok;
        b_ok = bitpack_set_signed(&word0, OE_WIDTH_SHIFT, OE_WIDTH_BITS,
                                  width_mm) && b_ok;
        b_ok = bitpack_set_signed(&word1, OE_LEFT_SHIFT, OE_EDGE_BITS,
                                  edge_left_mm) && b_ok;
        b_ok = bitpack_set_signed(&word1, OE_RIGHT_SHIFT, OE_EDGE_BITS,
                                  edge_right_mm) && b_ok;
        p_evt->word[0] = word0;
        p_evt->word[1] = word1;
    }

    return (b_ok);
}

/* OBST_EVT_xxx. */
uint8_t
obstacle_evt_type (obstacle_evt_t const * p_evt)
{
    /* A 3-bit field plus 1: 1..8. */
    return ((NULL != p_evt)
            ? (uint8_t) (bitpack_get(p_evt->word[0], OE_TYPE_SHIFT,
                                     OE_TYPE_BITS) + 1u)
            : 0u);
}

uint8_t
obstacle_evt_side (obstacle_evt_t const * p_evt)
{
    /* A 2-bit field: fits uint8_t. */
    return ((OBST_EVT_PLAN == obstacle_evt_type(p_evt))
            ? (uint8_t) bitpack_get(p_evt->word[0], OE_SIDE_SHIFT,
                                    OE_SIDE_BITS)
            : OBST_SIDE_NONE);
}

int32_t
obstacle_evt_closest_mm (obstacle_evt_t const * p_evt)
{
    /* 9 bits: fits int32_t. */
    return ((OBST_EVT_PLAN == obstacle_evt_type(p_evt))
            ? (int32_t) bitpack_get(p_evt->word[0], OE_CLOSEST_SHIFT,
                                    OE_CLOSEST_BITS)
            : 0);
}

int32_t
obstacle_evt_width_mm (obstacle_evt_t const * p_evt)
{
    return ((OBST_EVT_PLAN == obstacle_evt_type(p_evt))
            ? bitpack_get_signed(p_evt->word[0], OE_WIDTH_SHIFT,
                                 OE_WIDTH_BITS)
            : 0);
}

int32_t
obstacle_evt_edge_left_mm (obstacle_evt_t const * p_evt)
{
    return ((OBST_EVT_PLAN == obstacle_evt_type(p_evt))
            ? bitpack_get_signed(p_evt->word[1], OE_LEFT_SHIFT, OE_EDGE_BITS)
            : 0);
}

int32_t
obstacle_evt_edge_right_mm (obstacle_evt_t const * p_evt)
{
    return ((OBST_EVT_PLAN == obstacle_evt_type(p_evt))
            ? bitpack_get_signed(p_evt->word[1], OE_RIGHT_SHIFT,
                                 OE_EDGE_BITS)
            : 0);
}

/* The type is stored as type - 1, so types 1..8 fit.  Returns false
   for any other (0 is stored as 1, above 8 as 8). */
static bool
oe_set_type (uint32_t * p_word, uint8_t type)
{
    bool const b_zero = (0u == type);

    return (bitpack_set(p_word, OE_TYPE_SHIFT, OE_TYPE_BITS,
                        b_zero ? 0u : ((uint32_t) type - 1u)) &&
            !b_zero);
}

/* ------------------------------------------------------------------ */
/*  IF-06 scan requests                                                */
/* ------------------------------------------------------------------ */

/* One request; side only for SCAN_REQ_BYPASS.  Returns false if a value
   did not fit. */
bool
scan_req_pack (scan_req_t * p_req, uint8_t type, uint8_t side)
{
    bool b_ok = false;

    if (NULL != p_req)
    {
        uint32_t word = 0u;

        b_ok = bitpack_set(&word, SR_TYPE_SHIFT, SR_TYPE_BITS,
                           (uint32_t) type);

        if (SCAN_REQ_BYPASS == type)
        {
            b_ok = bitpack_set(&word, SR_SIDE_SHIFT, SR_SIDE_BITS,
                               (uint32_t) side) && b_ok;
        }

        p_req->bits = (uint8_t) word;   /* 6 bits used */
    }

    return (b_ok);
}

/* SCAN_REQ_xxx. */
uint8_t
scan_req_type (scan_req_t const * p_req)
{
    return ((NULL != p_req)
            ? (uint8_t) bitpack_get((uint32_t) p_req->bits, SR_TYPE_SHIFT,
                                    SR_TYPE_BITS)
            : 0u);
}

uint8_t
scan_req_side (scan_req_t const * p_req)
{
    return ((NULL != p_req)
            ? (uint8_t) bitpack_get((uint32_t) p_req->bits, SR_SIDE_SHIFT,
                                    SR_SIDE_BITS)
            : OBST_SIDE_NONE);
}

/*** end of file ***/
