/*
 *  scan_logic.h
 *  Buddy 5's scan geometry: turns the (servo angle, range) readings of
 *  the coarse and fine scans into the obstacle profile of FR10 (position,
 *  width, closest point, left and right clearance) and picks the side
 *  with more room for the car (FR11, report 6.5 PLANNING).
 *
 *  Angles in degrees, 90 = straight ahead, larger = left.  Positions in
 *  the car frame from the sensor: x forward, y left, mm.
 *
 *  Pure C, no kernel headers: tested on the host (testing/host/).
 */

#ifndef SCAN_LOGIC_H
#define SCAN_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#define SCAN_MAX_SAMPLES    32u
#define SCAN_FINE_PER_SIDE  5u      /* 5 steps of 5 degrees per side */
/* Clearance of a side with no valid reading beyond the obstacle: it was
   never seen, so it counts as no room (report 6.5, D-80). */
#define SCAN_CLEAR_UNKNOWN  (-1)

typedef struct
{
    int32_t angle_deg;
    int32_t range_mm;
    bool    b_valid;
} scan_sample_t;

typedef struct
{
    scan_sample_t samples[SCAN_MAX_SAMPLES];
    uint32_t      count;
} scan_set_t;

typedef struct
{
    bool    b_found;            /* an obstacle was seen */
    uint8_t side;               /* OBST_SIDE_xxx to pass on, or NONE */
    int32_t distance_mm;        /* forward distance to the closest point */
    int32_t offset_mm;          /* lateral centre */
    int32_t width_mm;
    int32_t closest_mm;         /* range to the closest point */
    int32_t edge_left_mm;       /* lateral position of each edge */
    int32_t edge_right_mm;
    int32_t clear_left_mm;      /* free space beyond each edge, or
                                   SCAN_CLEAR_UNKNOWN if never seen */
    int32_t clear_right_mm;
} scan_profile_t;

/* BYPASSING side scan (report 6.5): the obstacle must first be seen
   beside the car, then CFG_SCAN_BEHIND_COUNT valid readings must be
   clear with no near reading between them.  Invalid readings are
   skipped: they neither count nor reset the count (D-74). */
typedef struct
{
    bool     b_seen_beside;     /* the obstacle was seen beside the car */
    uint32_t clear_count;       /* clear valid readings since last seen */
} scan_side_t;

void     scan_side_init(scan_side_t * p_side);
bool     scan_side_step(scan_side_t * p_side, bool b_valid,
                        int32_t range_mm);

bool     scan_all_valid(scan_set_t const * p_set);
void     scan_set_clear(scan_set_t * p_set);
void     scan_set_add(scan_set_t * p_set, int32_t angle_deg,
                      int32_t range_mm, bool b_valid);
bool     scan_hit_range(scan_set_t const * p_set, int32_t * p_min_deg,
                        int32_t * p_max_deg);
uint32_t scan_fine_angles(int32_t min_hit_deg, int32_t max_hit_deg,
                          int32_t * p_angles, uint32_t max_angles);
void     scan_profile(scan_set_t const * p_set, scan_profile_t * p_out);
int32_t  scan_range_mm(uint32_t echo_width_us, bool * p_valid);

#endif /* SCAN_LOGIC_H */

/*** end of file ***/
