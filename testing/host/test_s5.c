/*
 *  test_s5.c
 *  Host tests for Buddy 5's scan geometry (scan_logic.c).
 *  Results: testing/subsystem5/TEST_LOG.md.
 *
 *  Scenes are built in floating point: a flat obstacle face 300 mm ahead
 *  and 200 mm wide, seen by the servo every 5 degrees, plus optional
 *  objects to the sides.
 */

#include <math.h>
#include <stdlib.h>

#include "app_config.h"
#include "subsystem5/scan_logic.h"
#include "subsystem5/scan_msgs.h"
#include "subsystem5/ultrasonic_echo.h"
#include "unit.h"

#define TEST_PI         3.14159265358979323846
#define TEST_FACE_MM    300.0       /* obstacle front, ahead of the sensor */
#define TEST_HALF_MM    100.0       /* half its width */

/* Range seen at a servo angle (90 = ahead): the face if the ray hits
   it, else nothing (maximum range, a valid reading). */
static int32_t
test_face_range (int32_t angle_deg)
{
    double const rel = (double) (angle_deg - 90) * TEST_PI / 180.0;
    double const lateral = TEST_FACE_MM * tan(rel);
    int32_t      range = CFG_US_MAX_MM;

    if (fabs(lateral) <= TEST_HALF_MM)
    {
        range = (int32_t) ((TEST_FACE_MM / cos(rel)) + 0.5);
    }

    return (range);
}

static void
test_scene (scan_set_t * p_set)
{
    scan_set_clear(p_set);

    for (int32_t angle = 60; angle <= 120; angle += 5)
    {
        scan_set_add(p_set, angle, test_face_range(angle), true);
    }
}

static void
test_range (void)
{
    bool b_valid = false;

    UNIT_BEGIN("S5-U01", "echo width to range, invalid and out of range");
    UNIT_EQ(scan_range_mm(1740u, &b_valid), 300);   /* 30 cm = 1740 us */
    UNIT_CHECK(b_valid);
    UNIT_EQ(scan_range_mm(58u, &b_valid), CFG_US_MAX_MM);
    UNIT_CHECK(!b_valid);                           /* 1 cm: malformed */
    UNIT_EQ(scan_range_mm(0u, &b_valid), CFG_US_MAX_MM);
    UNIT_CHECK(!b_valid);
    UNIT_EQ(scan_range_mm(30000u, &b_valid), CFG_US_MAX_MM);
    UNIT_CHECK(b_valid);                            /* nothing in range */
    UNIT_END();
}

static void
test_fine (void)
{
    scan_set_t set;
    int32_t    min_deg = 0;
    int32_t    max_deg = 0;
    int32_t    angles[12];

    UNIT_BEGIN("S5-U02", "coarse hits give the fine-scan angles");
    scan_set_clear(&set);
    scan_set_add(&set, 30, CFG_US_MAX_MM, true);
    scan_set_add(&set, 60, 450, true);
    scan_set_add(&set, 90, 300, true);
    scan_set_add(&set, 120, 450, false);            /* invalid: ignored */
    scan_set_add(&set, 150, 600, true);             /* too far */
    UNIT_CHECK(scan_hit_range(&set, &min_deg, &max_deg));
    UNIT_EQ(min_deg, 60);
    UNIT_EQ(max_deg, 90);
    UNIT_EQ(scan_fine_angles(60, 90, angles, 12u), 10);
    UNIT_EQ(angles[0], 55);
    UNIT_EQ(angles[1], 95);
    UNIT_EQ(angles[8], 35);
    UNIT_EQ(angles[9], 115);
    UNIT_EQ(scan_fine_angles(60, 90, angles, 4u), 4);
    UNIT_EQ(scan_fine_angles(CFG_SCAN_SERVO_MIN_DEG, CFG_SCAN_SERVO_MAX_DEG,
                             angles, 12u), 0);

    scan_set_clear(&set);
    scan_set_add(&set, 90, 700, true);
    UNIT_CHECK(!scan_hit_range(&set, &min_deg, &max_deg));
    UNIT_END();
}

static void
test_profile (void)
{
    scan_set_t     set;
    scan_profile_t prof;

    UNIT_BEGIN("S5-U03", "profile of an obstacle ahead (FR10)");
    test_scene(&set);
    scan_profile(&set, &prof);
    UNIT_CHECK(prof.b_found);
    UNIT_EQ(prof.closest_mm, 300);
    UNIT_CHECK(abs(prof.distance_mm - 300) <= 2);
    UNIT_CHECK(abs(prof.offset_mm) <= 2);
    UNIT_CHECK((prof.edge_left_mm >= 75) && (prof.edge_left_mm <= 100));
    UNIT_CHECK((prof.edge_right_mm <= -75) && (prof.edge_right_mm >= -100));
    UNIT_EQ(prof.width_mm, prof.edge_left_mm - prof.edge_right_mm);
    UNIT_EQ(prof.clear_left_mm, CFG_US_MAX_MM);     /* open both sides */
    UNIT_EQ(prof.clear_right_mm, CFG_US_MAX_MM);
    UNIT_EQ(prof.side, OBST_SIDE_LEFT);             /* tie: left */

    scan_set_clear(&set);
    scan_set_add(&set, 90, CFG_US_MAX_MM, true);
    scan_profile(&set, &prof);
    UNIT_CHECK(!prof.b_found);
    UNIT_EQ(prof.side, OBST_SIDE_NONE);
    UNIT_END();
}

static void
test_side (void)
{
    scan_set_t     set;
    scan_profile_t prof;

    UNIT_BEGIN("S5-U04", "side with room for the car, or none (FR11)");

    /* Something 350 mm away at 130 degrees: 225 mm to the left. */
    test_scene(&set);
    scan_set_add(&set, 130, 350, true);
    scan_profile(&set, &prof);
    UNIT_CHECK(prof.clear_left_mm < (CFG_CAR_WIDTH_MM + CFG_NAV_PASS_GAP_MM));
    UNIT_EQ(prof.side, OBST_SIDE_RIGHT);

    /* And the same on the right: no way past. */
    scan_set_add(&set, 50, 350, true);
    scan_profile(&set, &prof);
    UNIT_EQ(prof.side, OBST_SIDE_NONE);

    /* A far object beside the path does not block (more than
       300 mm beyond the obstacle's front). */
    test_scene(&set);
    scan_set_add(&set, 130, 1500, true);
    scan_profile(&set, &prof);
    UNIT_EQ(prof.clear_left_mm, CFG_US_MAX_MM);
    UNIT_EQ(prof.side, OBST_SIDE_LEFT);
    UNIT_END();
}

static void
test_side_scan (void)
{
    scan_side_t side;
    int32_t const near = CFG_SCAN_SIDE_CLEAR_MM - 250;  /* beside us */
    int32_t const far  = CFG_SCAN_SIDE_CLEAR_MM + 100;  /* clear */
    bool          b_behind = false;

    UNIT_BEGIN("S5-U05", "side scan: obstacle behind only after it was "
                         "seen, then clear on valid readings (6.5)");

    /* Clear before the obstacle is level with the car: not behind. */
    scan_side_init(&side);
    for (uint32_t idx = 0u; idx < 5u; idx++)
    {
        b_behind = b_behind || scan_side_step(&side, true, far);
    }
    UNIT_CHECK(!b_behind);

    /* Seen beside, then invalid readings (missing echoes): never
       "behind", however many there are (D-74). */
    UNIT_CHECK(!scan_side_step(&side, true, near));
    for (uint32_t idx = 0u; idx < 10u; idx++)
    {
        b_behind = b_behind || scan_side_step(&side, false, CFG_US_MAX_MM);
    }
    UNIT_CHECK(!b_behind);

    /* Valid clear readings, interrupted by an invalid one: the count
       carries on over it; behind after CFG_SCAN_BEHIND_COUNT. */
    for (uint32_t idx = 1u; idx < CFG_SCAN_BEHIND_COUNT; idx++)
    {
        UNIT_CHECK(!scan_side_step(&side, true, far));
    }
    UNIT_CHECK(!scan_side_step(&side, false, 0));
    UNIT_CHECK(scan_side_step(&side, true, far));

    /* Seen again in between: the count starts again. */
    scan_side_init(&side);
    UNIT_CHECK(!scan_side_step(&side, true, near));
    UNIT_CHECK((1u == CFG_SCAN_BEHIND_COUNT) ||
               !scan_side_step(&side, true, far));
    UNIT_CHECK(!scan_side_step(&side, true, near));
    UNIT_EQ(side.clear_count, 0);
    UNIT_CHECK(!scan_side_step(NULL, true, far));
    UNIT_END();
}

static void
test_unseen (void)
{
    scan_set_t     set;
    scan_profile_t prof;

    UNIT_BEGIN("S5-U06", "a side with no valid reading has no room; path "
                         "clear needs every coarse angle valid (D-80)");

    /* Obstacle ahead; the left side seen open, the right side only
       invalid readings: pass on the left, right clearance unknown. */
    scan_set_clear(&set);
    scan_set_add(&set, 30, CFG_US_MAX_MM, false);
    scan_set_add(&set, 60, CFG_US_MAX_MM, false);
    scan_set_add(&set, 85, 302, true);
    scan_set_add(&set, 90, 300, true);
    scan_set_add(&set, 95, 302, true);
    scan_set_add(&set, 120, CFG_US_MAX_MM, true);
    scan_set_add(&set, 150, CFG_US_MAX_MM, true);
    scan_profile(&set, &prof);
    UNIT_CHECK(prof.b_found);
    UNIT_EQ(prof.clear_right_mm, SCAN_CLEAR_UNKNOWN);
    UNIT_EQ(prof.clear_left_mm, CFG_US_MAX_MM);
    UNIT_EQ(prof.side, OBST_SIDE_LEFT);

    /* Neither side seen (the reviewer's case: 60 and 120 invalid around
       a 300 mm hit): no room either side, so reverse and rescan. */
    scan_set_clear(&set);
    scan_set_add(&set, 60, CFG_US_MAX_MM, false);
    scan_set_add(&set, 90, 300, true);
    scan_set_add(&set, 120, CFG_US_MAX_MM, false);
    scan_profile(&set, &prof);
    UNIT_CHECK(prof.b_found);
    UNIT_EQ(prof.clear_left_mm, SCAN_CLEAR_UNKNOWN);
    UNIT_EQ(prof.clear_right_mm, SCAN_CLEAR_UNKNOWN);
    UNIT_EQ(prof.side, OBST_SIDE_NONE);

    /* A side seen through a valid far reading counts as seen and open,
       as before (S5-U03). */
    test_scene(&set);
    scan_profile(&set, &prof);
    UNIT_EQ(prof.clear_left_mm, CFG_US_MAX_MM);
    UNIT_EQ(prof.clear_right_mm, CFG_US_MAX_MM);

    /* Coarse scan with no hit: clear only if all five readings are
       valid (scan_task.c sends PATH_CLEAR only then). */
    scan_set_clear(&set);
    UNIT_CHECK(!scan_all_valid(&set));              /* nothing read */
    scan_set_add(&set, 30, CFG_US_MAX_MM, true);
    scan_set_add(&set, 60, CFG_US_MAX_MM, true);
    scan_set_add(&set, 90, CFG_US_MAX_MM, true);
    scan_set_add(&set, 120, CFG_US_MAX_MM, true);
    scan_set_add(&set, 150, CFG_US_MAX_MM, true);
    UNIT_CHECK(scan_all_valid(&set));
    set.samples[2].b_valid = false;                 /* 90 degrees missed */
    UNIT_CHECK(!scan_all_valid(&set));
    UNIT_CHECK(!scan_all_valid(NULL));
    UNIT_END();
}

/* ---- Minimum-bit change (D-90): S5-U40 and up ---- */

/* xorshift32: the same sequence on every host. */
static uint32_t
test_rand (uint32_t * p_state)
{
    uint32_t x = *p_state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *p_state = x;

    return (x);
}

static void
test_obstacle_events (void)
{
    obstacle_evt_t evt;
    scan_set_t     set;
    scan_profile_t prof;
    uint32_t       seed  = 0x5EED0501u;
    uint32_t       plans = 0u;
    bool           b_ok  = true;

    UNIT_BEGIN("S5-U40", "IF-05 obstacle event packed into two words: the "
                         "exact expected bits; every plan the profile code "
                         "makes from 20,000 random scans fits and "
                         "round-trips (D-90)");
    UNIT_EQ(sizeof(obstacle_evt_t), 8u);

    UNIT_CHECK(obstacle_evt_plan(&evt, OBST_SIDE_RIGHT, 500, -1000, 500,
                                 -500));
    UNIT_EQ(evt.word[0], 0x01063E92u);
    UNIT_EQ(evt.word[1], 0x000831F4u);
    UNIT_EQ(obstacle_evt_type(&evt), OBST_EVT_PLAN);
    UNIT_EQ(obstacle_evt_side(&evt), OBST_SIDE_RIGHT);
    UNIT_EQ(obstacle_evt_closest_mm(&evt), 500);
    UNIT_EQ(obstacle_evt_width_mm(&evt), -1000);
    UNIT_EQ(obstacle_evt_edge_left_mm(&evt), 500);
    UNIT_EQ(obstacle_evt_edge_right_mm(&evt), -500);

    UNIT_CHECK(obstacle_evt_simple(&evt, OBST_EVT_NO_SAFE_ROUTE));
    UNIT_EQ(evt.word[0], 0x00000005u);      /* the type alone */
    UNIT_EQ(evt.word[1], 0x00000000u);
    UNIT_EQ(obstacle_evt_type(&evt), OBST_EVT_NO_SAFE_ROUTE);
    UNIT_EQ(obstacle_evt_side(&evt), OBST_SIDE_NONE);   /* not carried */
    UNIT_CHECK(obstacle_evt_simple(&evt, OBST_EVT_AHEAD));
    UNIT_EQ(evt.word[0], 0x00000000u);
    UNIT_EQ(obstacle_evt_type(&evt), OBST_EVT_AHEAD);
    UNIT_EQ(obstacle_evt_closest_mm(&evt), 0);          /* not carried */

    /* Every type round-trips in bits 0-2 alone; a type that does not
       fit is kept at the nearest end and reported. */
    for (uint8_t type = OBST_EVT_AHEAD; type <= OBST_EVT_NO_SAFE_ROUTE;
         type++)
    {
        b_ok = b_ok && obstacle_evt_simple(&evt, type) &&
               (obstacle_evt_type(&evt) == type) &&
               (0u == (evt.word[0] & 0xFFFFFFF8u)) && (0u == evt.word[1]);
    }

    UNIT_CHECK(b_ok);
    UNIT_CHECK(!obstacle_evt_simple(&evt, 0u));
    UNIT_EQ(obstacle_evt_type(&evt), 1u);
    UNIT_CHECK(!obstacle_evt_simple(&evt, 9u));
    UNIT_EQ(obstacle_evt_type(&evt), 8u);
    UNIT_CHECK(!obstacle_evt_simple(NULL, OBST_EVT_AHEAD));

    /* Random scans: obstacles near and far, gaps, invalid readings.
       Whenever the profile code makes a plan, it must fit exactly.
       6 degree steps: 31 readings, within the 32 a scan set holds, so
       the far-left angles are used too. */
    for (uint32_t run = 0u; run < 20000u; run++)
    {
        uint32_t const near  = 100u + (test_rand(&seed) % 450u);
        int32_t const  from  = 30 + (int32_t) (test_rand(&seed) % 60u);
        int32_t const  to    = from + (int32_t) (test_rand(&seed) % 80u);

        scan_set_clear(&set);

        for (int32_t angle = 0; angle <= 180; angle += 6)
        {
            uint32_t const pick = test_rand(&seed) % 10u;
            int32_t        range = (int32_t) (test_rand(&seed) % 4001u);

            if ((angle >= from) && (angle <= to) && (pick < 8u))
            {
                range = (int32_t) near + (int32_t) (test_rand(&seed) % 120u);
            }

            scan_set_add(&set, angle, range, (0u != (pick % 7u)));
        }

        scan_profile(&set, &prof);

        if (prof.b_found && (OBST_SIDE_NONE != prof.side))
        {
            plans++;
            b_ok = b_ok &&
                   obstacle_evt_plan(&evt, prof.side, prof.closest_mm,
                                     prof.width_mm, prof.edge_left_mm,
                                     prof.edge_right_mm) &&
                   (obstacle_evt_side(&evt) == prof.side) &&
                   (obstacle_evt_closest_mm(&evt) == prof.closest_mm) &&
                   (obstacle_evt_width_mm(&evt) == prof.width_mm) &&
                   (obstacle_evt_edge_left_mm(&evt) == prof.edge_left_mm) &&
                   (obstacle_evt_edge_right_mm(&evt) == prof.edge_right_mm);
        }
    }

    UNIT_CHECK(b_ok);
    UNIT_CHECK(plans > 1000u);
    UNIT_CHECK(!obstacle_evt_plan(&evt, OBST_SIDE_LEFT, 600, 0, 0, 0));
    UNIT_EQ(obstacle_evt_closest_mm(&evt), 511);
    UNIT_EQ(obstacle_evt_type(NULL), 0u);
    UNIT_END();
}

static void
test_scan_requests (void)
{
    scan_req_t req;
    bool       b_ok = true;

    UNIT_BEGIN("S5-U41", "IF-06 scan request packed into one byte: the "
                         "exact expected bits; every type and side "
                         "round-trips (D-90)");
    UNIT_EQ(sizeof(scan_req_t), 1u);
    UNIT_CHECK(scan_req_pack(&req, SCAN_REQ_BYPASS, OBST_SIDE_LEFT));
    UNIT_EQ(req.bits, 0x12u);
    UNIT_CHECK(scan_req_pack(&req, SCAN_REQ_RESUME, OBST_SIDE_LEFT));
    UNIT_EQ(req.bits, 0x09u);           /* side kept only for BYPASS */

    for (uint8_t type = SCAN_REQ_CAR_STOPPED; type <= SCAN_REQ_RESUME;
         type++)
    {
        for (uint8_t side = OBST_SIDE_NONE; side <= OBST_SIDE_RIGHT; side++)
        {
            b_ok = b_ok && scan_req_pack(&req, type, side) &&
                   (scan_req_type(&req) == type) &&
                   (scan_req_side(&req) ==
                    ((SCAN_REQ_BYPASS == type) ? side : OBST_SIDE_NONE));
        }
    }

    UNIT_CHECK(b_ok);
    UNIT_CHECK(!scan_req_pack(&req, 16u, 0u));
    UNIT_EQ(scan_req_type(&req), 15u);
    UNIT_END();
}

static void
test_echo_width (void)
{
    ultrasonic_echo_t echo;
    bool              b_ok = true;

    UNIT_BEGIN("S5-U42", "echo record: two words with the exact expected "
                         "bits; the width in 15 bits, kept at most 32767 "
                         "us by the ISR's own packing: every width up to "
                         "200 ms gives the same range and validity as the "
                         "full 32-bit width (D-90)");
    UNIT_EQ(sizeof(ultrasonic_echo_t), 8u);
    UNIT_EQ(US_ECHO_FULL_RANGE_US, 23200u);
    UNIT_EQ(US_ECHO_WIDTH_BITS, 15u);

    echo = ultrasonic_echo_pack(0xFFFFFFFFu, 23200u);
    UNIT_EQ(echo.rise_us, 0xFFFFFFFFu);
    UNIT_EQ(echo.bits, 0x00005AA0u);        /* 23200 */
    echo = ultrasonic_echo_pack(7u, 67000u);     /* a no-target pulse */
    UNIT_EQ(echo.rise_us, 7u);
    UNIT_EQ(echo.bits, 0x00007FFFu);        /* kept at 32767, not cut */

    for (uint32_t width = 0u; width <= 200000u; width++)
    {
        bool           b_full = false;
        bool           b_kept = false;
        uint32_t       kept   = 0u;
        int32_t        r_full = 0;
        int32_t        r_kept = 0;

        echo   = ultrasonic_echo_pack(width * 3u, width);
        kept   = ultrasonic_echo_width_us(&echo);
        r_full = scan_range_mm(width, &b_full);
        r_kept = scan_range_mm(kept, &b_kept);
        b_ok   = b_ok && (r_full == r_kept) && (b_full == b_kept) &&
                 (kept == ((width < 32767u) ? width : 32767u)) &&
                 (echo.rise_us == (width * 3u)) &&
                 (0u == (echo.bits & 0xFFFF8000u));
    }

    UNIT_CHECK(b_ok);
    UNIT_END();
}

void
test_s5 (void)
{
    test_range();
    test_fine();
    test_profile();
    test_side();
    test_side_scan();
    test_unseen();
    test_obstacle_events();
    test_scan_requests();
    test_echo_width();
}

/*** end of file ***/
