/*
 *  test_s4.c
 *  Host tests for Buddy 4's IMU maths (imu_calc.c), message words
 *  (imu_msgs.c) and Safety's fault latch and fault mask
 *  (safety_latch.c).
 *  Results: testing/subsystem4/TEST_LOG.md.
 *
 *  The heading test builds the accelerometer and magnetometer readings
 *  of a car turned, pitched and rolled by known angles (a real rotation
 *  matrix, in floating point), then checks that the fixed-point
 *  tilt-compensated heading gives back the turn angle.
 */

#include <math.h>
#include <stdlib.h>

#include "app_config.h"
#include "subsystem4/imu_calc.h"
#include "subsystem4/imu_msgs.h"
#include "subsystem4/safety_latch.h"
#include "unit.h"

#define TEST_PI         3.14159265358979323846
#define TEST_RAD(deg)   ((double) (deg) * TEST_PI / 180.0)

static int32_t
test_round (double value)
{
    return ((int32_t) ((value >= 0.0) ? (value + 0.5) : (value - 0.5)));
}

/*
 * World frame: x north, y west, z up.  Car frame: x forward, y left,
 * z up.  Body-to-world rotation R = Rz(yaw) Ry(pitch) Rx(roll); a world
 * vector seen from the car is R^T v.
 */
static void
test_to_body (double yaw, double pitch, double roll, double const * p_world,
              int32_t * p_body)
{
    double const cy = cos(yaw);
    double const sy = sin(yaw);
    double const cp = cos(pitch);
    double const sp = sin(pitch);
    double const cr = cos(roll);
    double const sr = sin(roll);
    double const rot[3][3] =
    {
        { cy * cp, (cy * sp * sr) - (sy * cr), (cy * sp * cr) + (sy * sr) },
        { sy * cp, (sy * sp * sr) + (cy * cr), (sy * sp * cr) - (cy * sr) },
        { -sp,     cp * sr,                    cp * cr }
    };

    for (uint32_t row = 0u; row < 3u; row++)
    {
        double sum = 0.0;

        for (uint32_t col = 0u; col < 3u; col++)
        {
            sum += rot[col][row] * p_world[col];    /* R transposed */
        }

        p_body[row] = test_round(sum);
    }
}

static void
test_tilt (void)
{
    int32_t roll  = 0;
    int32_t pitch = 0;

    UNIT_BEGIN("S4-U01", "roll and pitch from gravity");
    imu_calc_tilt(0, 0, 1000, &roll, &pitch);
    UNIT_EQ(roll, 0);
    UNIT_EQ(pitch, 0);
    imu_calc_tilt(0, 500, 866, &roll, &pitch);      /* left side up 30 */
    UNIT_CHECK(abs(roll - 3000) <= 5);
    UNIT_CHECK(abs(pitch) <= 5);
    imu_calc_tilt(0, -500, 866, &roll, &pitch);
    UNIT_CHECK(abs(roll + 3000) <= 5);
    imu_calc_tilt(500, 0, 866, &roll, &pitch);
    UNIT_CHECK(abs(roll) <= 5);
    UNIT_CHECK(abs(pitch + 3000) <= 5);
    UNIT_END();
}

static void
test_heading (void)
{
    double const gravity[3] = { 0.0, 0.0, 1000.0 };
    double const field[3]   = { 450.0, 0.0, -300.0 };   /* north, dipping */
    int32_t const pitches[3] = { -15, 0, 10 };
    int32_t const rolls[3]   = { -20, 0, 15 };
    int32_t       worst = 0;

    UNIT_BEGIN("S4-U02", "tilt-compensated heading on rotated vectors");

    for (int32_t yaw = -170; yaw <= 180; yaw += 10)
    {
        for (uint32_t ip = 0u; ip < 3u; ip++)
        {
            for (uint32_t ir = 0u; ir < 3u; ir++)
            {
                int32_t acc[3];
                int32_t mag[3];
                int32_t roll    = 0;
                int32_t pitch   = 0;
                int32_t heading = 0;
                int32_t error   = 0;

                test_to_body(TEST_RAD(yaw), TEST_RAD(pitches[ip]),
                             TEST_RAD(rolls[ir]), gravity, acc);
                test_to_body(TEST_RAD(yaw), TEST_RAD(pitches[ip]),
                             TEST_RAD(rolls[ir]), field, mag);
                imu_calc_tilt(acc[0], acc[1], acc[2], &roll, &pitch);
                heading = imu_calc_heading(mag[0], mag[1], mag[2], roll,
                                           pitch);
                error = abs(heading - (yaw * 100));
                error = (error > 18000) ? (36000 - error) : error;
                worst = (error > worst) ? error : worst;
            }
        }
    }

    UNIT_CHECK(worst <= 50);        /* 0.5 degree */
    UNIT_END();
}

static void
test_norm_height (void)
{
    UNIT_BEGIN("S4-U03", "vector length and hump height table");
    UNIT_EQ(imu_calc_norm(0, 0, 1000), 1000);
    UNIT_EQ(imu_calc_norm(3000, 4000, 0), 5000);
    UNIT_EQ(imu_calc_norm(-3000, -4000, 0), 5000);
    UNIT_EQ(imu_calc_norm(10000, 0, 0), 4000);      /* clamped to 4 g */
    UNIT_EQ(imu_calc_hump_height(0), 0);
    UNIT_EQ(imu_calc_hump_height(200), 5);
    UNIT_EQ(imu_calc_hump_height(1000), 23);
    UNIT_EQ(imu_calc_hump_height(750), 17);
    UNIT_EQ(imu_calc_hump_height(-1000), 23);       /* either side */
    UNIT_EQ(imu_calc_hump_height(9000), 65);        /* table end */
    UNIT_END();
}

static void
test_hump (void)
{
    imu_hump_t     hump;
    imu_hump_evt_t evt = IMU_HUMP_EVT_NONE;
    uint32_t       started_at = 0u;

    UNIT_BEGIN("S4-U04", "hump: on after 100 ms above, off when level");
    imu_hump_init(&hump);

    /* A 50 ms jolt is not a hump. */
    for (uint32_t ms = 0u; ms < 50u; ms += 10u)
    {
        evt = imu_hump_step(&hump, 500, 10u);
    }

    (void) imu_hump_step(&hump, 0, 10u);
    UNIT_EQ(evt, IMU_HUMP_EVT_NONE);
    UNIT_CHECK(!hump.b_on_hump);

    for (uint32_t ms = 10u; (ms <= 200u) && (0u == started_at); ms += 10u)
    {
        if (IMU_HUMP_EVT_STARTED == imu_hump_step(&hump, 400, 10u))
        {
            started_at = ms;
        }
    }

    UNIT_EQ(started_at, CFG_IMU_HUMP_MIN_MS);
    (void) imu_hump_step(&hump, 1200, 10u);
    (void) imu_hump_step(&hump, -900, 10u);         /* other side */
    UNIT_EQ(imu_hump_step(&hump, 200, 10u), IMU_HUMP_EVT_NONE);
    UNIT_EQ(imu_hump_step(&hump, 100, 10u), IMU_HUMP_EVT_ENDED);
    UNIT_EQ(hump.peak_cdeg, 1200);
    UNIT_END();
}

static imu_class_t
test_classify (imu_classifier_t * p_cls, uint32_t samples, int32_t forward,
               int32_t total_a, int32_t total_b, int32_t rate,
               int32_t roll_start, int32_t roll_step, bool b_hump)
{
    imu_class_t result = IMU_CLASS_STEADY;

    for (uint32_t idx = 0u; idx < samples; idx++)
    {
        int32_t const total = (0u == (idx % 2u)) ? total_a : total_b;

        result = imu_classifier_step(p_cls, forward, 0, total, rate,
                                     roll_start + ((int32_t) idx * roll_step),
                                     b_hump);
    }

    return (result);
}

static void
test_classes (void)
{
    imu_classifier_t cls;

    UNIT_BEGIN("S4-U05", "motion classes (FR17)");
    imu_classifier_init(&cls);
    UNIT_EQ(test_classify(&cls, 20u, 0, 1000, 1000, 0, 0, 0, false),
            IMU_CLASS_STATIONARY);
    UNIT_EQ(test_classify(&cls, 20u, 0, 950, 1050, 0, 0, 0, false),
            IMU_CLASS_STEADY);
    UNIT_EQ(test_classify(&cls, 20u, 120, 1000, 1010, 0, 0, 0, false),
            IMU_CLASS_ACCELERATING);
    UNIT_EQ(test_classify(&cls, 20u, -120, 1000, 1010, 0, 0, 0, false),
            IMU_CLASS_ACCELERATING);
    UNIT_EQ(test_classify(&cls, 20u, 0, 1000, 1010, 2500, 0, 0, false),
            IMU_CLASS_TURNING);
    UNIT_EQ(test_classify(&cls, 20u, 0, 1000, 1010, 0, 300, 50, true),
            IMU_CLASS_CLIMBING);
    UNIT_EQ(test_classify(&cls, 20u, 0, 1000, 1010, 0, 1300, -50, true),
            IMU_CLASS_DESCENDING);
    UNIT_EQ(test_classify(&cls, 1u, 1100, 1000, 1000, 0, 0, 0, false),
            IMU_CLASS_IMPACT);
    UNIT_END();
}

static void
test_samples (void)
{
    UNIT_BEGIN("S4-U06", "sample check: bad data, plausible, overrange; "
                         "calibration jolts (D-81)");
    /* A failed read, or |a| below the plausible range, is invalid data
       (5 in a row latch IMU_DATA, D-42). */
    UNIT_EQ(imu_calc_check_sample(false, 1000u), IMU_SAMPLE_BAD);
    UNIT_EQ(imu_calc_check_sample(true, (uint32_t) CFG_IMU_MIN_G_MG - 1u),
            IMU_SAMPLE_BAD);
    UNIT_EQ(imu_calc_check_sample(true, (uint32_t) CFG_IMU_MIN_G_MG),
            IMU_SAMPLE_OK);
    UNIT_EQ(imu_calc_check_sample(true, (uint32_t) CFG_IMU_MAX_G_MG),
            IMU_SAMPLE_OK);

    /* Above the plausible range: a collision while monitoring. */
    UNIT_EQ(imu_calc_check_sample(true, (uint32_t) CFG_IMU_MAX_G_MG + 1u),
            IMU_SAMPLE_OVERRANGE);
    UNIT_EQ(imu_calc_check_sample(true,
                                  imu_calc_norm(4000, 4000, 4000)),
            IMU_SAMPLE_OVERRANGE);            /* every axis saturated */

    /* Calibrating: |a| at least CFG_IMU_IMPACT_MG away from 1 g is a
       jolt that restarts the average; normal noise is not. */
    UNIT_CHECK(!imu_calc_cal_jolt(1000u));
    UNIT_CHECK(!imu_calc_cal_jolt(1000u + (uint32_t) CFG_IMU_STILL_MG));
    UNIT_CHECK(!imu_calc_cal_jolt(1000u + (uint32_t) CFG_IMU_IMPACT_MG - 1u));
    UNIT_CHECK(imu_calc_cal_jolt(1000u + (uint32_t) CFG_IMU_IMPACT_MG));
    UNIT_CHECK(imu_calc_cal_jolt((uint32_t) CFG_IMU_MAX_G_MG + 1u));
    UNIT_END();
}

/* ---- RESET handshake (G20, D-95): S4-U07 ---- */

/* The latch's three states and the six things that can happen to it. */
typedef enum
{
    TEST_LS_FREE = 0,       /* nothing latched */
    TEST_LS_LATCHED,        /* latched, no reset waiting */
    TEST_LS_WAITING         /* latched, RESET waiting for Motion */
} test_ls_t;

typedef enum
{
    TEST_LE_FAULT = 0,      /* a fault report */
    TEST_LE_STATUS,         /* a status report (Wi-Fi loss): R-02 */
    TEST_LE_RESET,          /* RESET pressed, cause cleared */
    TEST_LE_RESET_HELD,     /* RESET pressed, cause still present */
    TEST_LE_ACK,            /* Motion acknowledged zeroing its state */
    TEST_LE_TIMEOUT         /* no acknowledgement in time */
} test_le_t;

#define TEST_LE_NO_RESULT   (-1)    /* the call returns nothing */

typedef struct
{
    test_ls_t from;
    test_le_t event;
    int32_t   result;       /* safety_latch_out_t, or 1/0 for true/false */
    test_ls_t to;
    bool      b_new_cause;  /* the fault became the latched cause */
} test_latch_row_t;

/* Every state against every event: what the call returns, the next
   state and whether the latched cause changes. */
static test_latch_row_t const g_latch_rows[] =
{
    { TEST_LS_FREE,    TEST_LE_FAULT,      SAFETY_LATCH_LATCHED,
      TEST_LS_LATCHED, true  },
    { TEST_LS_FREE,    TEST_LE_STATUS,     SAFETY_LATCH_STATUS,
      TEST_LS_FREE,    false },
    { TEST_LS_FREE,    TEST_LE_RESET,      0,
      TEST_LS_FREE,    false },
    { TEST_LS_FREE,    TEST_LE_RESET_HELD, 0,
      TEST_LS_FREE,    false },
    { TEST_LS_FREE,    TEST_LE_ACK,        0,
      TEST_LS_FREE,    false },
    { TEST_LS_FREE,    TEST_LE_TIMEOUT,    TEST_LE_NO_RESULT,
      TEST_LS_FREE,    false },

    { TEST_LS_LATCHED, TEST_LE_FAULT,      SAFETY_LATCH_COUNTED,
      TEST_LS_LATCHED, false },
    { TEST_LS_LATCHED, TEST_LE_STATUS,     SAFETY_LATCH_STATUS,
      TEST_LS_LATCHED, false },
    { TEST_LS_LATCHED, TEST_LE_RESET,      1,
      TEST_LS_WAITING, false },
    { TEST_LS_LATCHED, TEST_LE_RESET_HELD, 0,
      TEST_LS_LATCHED, false },
    { TEST_LS_LATCHED, TEST_LE_ACK,        0,
      TEST_LS_LATCHED, false },
    { TEST_LS_LATCHED, TEST_LE_TIMEOUT,    TEST_LE_NO_RESULT,
      TEST_LS_LATCHED, false },

    /* G20: a fault while RESET waits cancels it and becomes the cause. */
    { TEST_LS_WAITING, TEST_LE_FAULT,      SAFETY_LATCH_RESET_CANCELLED,
      TEST_LS_LATCHED, true  },
    /* A status report never cancels a waiting reset (R-02). */
    { TEST_LS_WAITING, TEST_LE_STATUS,     SAFETY_LATCH_STATUS,
      TEST_LS_WAITING, false },
    { TEST_LS_WAITING, TEST_LE_RESET,      0,
      TEST_LS_WAITING, false },
    { TEST_LS_WAITING, TEST_LE_RESET_HELD, 0,
      TEST_LS_WAITING, false },
    { TEST_LS_WAITING, TEST_LE_ACK,        1,
      TEST_LS_FREE,    false },
    { TEST_LS_WAITING, TEST_LE_TIMEOUT,    TEST_LE_NO_RESULT,
      TEST_LS_LATCHED, false }
};

/* One row: build its starting state, apply its event, check the result. */
static bool
test_latch_row (test_latch_row_t const * p_row)
{
    safety_latch_t latch;
    int32_t        result = TEST_LE_NO_RESULT;
    bool           b_ok   = true;

    safety_latch_init(&latch);

    if (TEST_LS_FREE != p_row->from)
    {
        (void) safety_latch_report(&latch, FAULT_CAUSE_LIFTED,
                                   FAULT_SRC_IMU, 100u, false);
    }

    if (TEST_LS_WAITING == p_row->from)
    {
        (void) safety_latch_reset(&latch, true);
    }

    switch (p_row->event)
    {
        case TEST_LE_FAULT:
            result = (int32_t) safety_latch_report(&latch,
                                                   FAULT_CAUSE_LINE_DATA,
                                                   FAULT_SRC_NAV, 4u, false);
            break;

        case TEST_LE_STATUS:
            result = (int32_t) safety_latch_report(&latch,
                                                   FAULT_CAUSE_WIFI_LOSS,
                                                   FAULT_SRC_COMMAND, 0u,
                                                   false);
            break;

        case TEST_LE_RESET:
            result = safety_latch_reset(&latch, true) ? 1 : 0;
            break;

        case TEST_LE_RESET_HELD:
            result = safety_latch_reset(&latch, false) ? 1 : 0;
            break;

        case TEST_LE_ACK:
            result = safety_latch_ack(&latch) ? 1 : 0;
            break;

        default:
            safety_latch_ack_timeout(&latch);
            break;
    }

    b_ok = (result == p_row->result) &&
           (latch.b_latched == (TEST_LS_FREE != p_row->to)) &&
           (latch.b_reset_pending == (TEST_LS_WAITING == p_row->to));

    if (p_row->b_new_cause)
    {
        b_ok = b_ok && (FAULT_CAUSE_LINE_DATA == latch.cause) &&
               (FAULT_SRC_NAV == latch.source) && (4u == latch.detail);
    }
    else if (TEST_LS_FREE != p_row->to)
    {
        b_ok = b_ok && (FAULT_CAUSE_LIFTED == latch.cause) &&
               (FAULT_SRC_IMU == latch.source) && (100u == latch.detail);
    }
    else
    {
        /* Nothing latched: the cause fields are not used. */
    }

    return (b_ok);
}

static void
test_safety_latch (void)
{
    safety_latch_t latch;
    uint32_t const rows = sizeof(g_latch_rows) / sizeof(g_latch_rows[0]);

    UNIT_BEGIN("S4-U07", "Safety latch: every state against every event; "
                         "a fault while RESET waits for Motion cancels "
                         "the reset and becomes the latched cause, so "
                         "Motion's acknowledgement then releases nothing "
                         "(G20, D-95); a status report changes no state "
                         "(R-02)");
    UNIT_EQ(rows, 18u);

    for (uint32_t idx = 0u; idx < rows; idx++)
    {
        if (!test_latch_row(&g_latch_rows[idx]))
        {
            UNIT_EQ(idx, rows);     /* names the failing row */
        }
    }

    /* The G20 story.  A collision latches; RESET once the IMU is
       healthy again; a line fault arrives before Motion acknowledges
       (the task handles faults before the acknowledgement, so this also
       covers both in one wake-up). */
    safety_latch_init(&latch);
    UNIT_EQ(safety_latch_report(&latch, FAULT_CAUSE_LIFTED, FAULT_SRC_IMU,
                                6928u, false), SAFETY_LATCH_LATCHED);
    UNIT_CHECK(safety_latch_reset(&latch, true));
    UNIT_EQ(safety_latch_report(&latch, FAULT_CAUSE_LINE_DATA, FAULT_SRC_NAV,
                                4u, false), SAFETY_LATCH_RESET_CANCELLED);
    UNIT_CHECK(!safety_latch_ack(&latch));
    UNIT_CHECK(latch.b_latched);
    UNIT_EQ(latch.cause, FAULT_CAUSE_LINE_DATA);

    /* More faults are counted only; RESET again; now it releases. */
    UNIT_EQ(safety_latch_report(&latch, FAULT_CAUSE_LIFTED, FAULT_SRC_IMU,
                                1u, false), SAFETY_LATCH_COUNTED);
    UNIT_EQ(latch.cause, FAULT_CAUSE_LINE_DATA);
    UNIT_CHECK(safety_latch_reset(&latch, true));
    UNIT_CHECK(safety_latch_ack(&latch));
    UNIT_CHECK(!latch.b_latched && !latch.b_reset_pending);
    UNIT_CHECK(!safety_latch_ack(&latch));  /* a second acknowledgement */

    /* No pointer: nothing happens. */
    safety_latch_init(NULL);
    safety_latch_ack_timeout(NULL);
    UNIT_EQ(safety_latch_report(NULL, FAULT_CAUSE_LIFTED, FAULT_SRC_IMU,
                                0u, false), SAFETY_LATCH_COUNTED);
    UNIT_EQ(safety_latch_report(NULL, FAULT_CAUSE_WIFI_LOSS,
                                FAULT_SRC_COMMAND, 0u, false),
            SAFETY_LATCH_COUNTED);
    UNIT_CHECK(!safety_latch_reset(NULL, true));
    UNIT_CHECK(!safety_latch_ack(NULL));
    UNIT_END();
}

/* ---- Fault mask (Change Requirement 1; 24 Oct review, R-02): S4-U08 */

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

/* The mask as the fault table defines it, kept by hand beside the
   latch: the status bits now on, and every latching cause reported
   since the last release. */
typedef struct
{
    uint16_t status;
    uint16_t latching;
    bool     b_latched;
    bool     b_waiting;
} test_mask_model_t;

/* One random step on both the latch and the model; false if they
   disagree afterwards. */
static bool
test_mask_step (safety_latch_t * p_latch, test_mask_model_t * p_model,
                uint32_t * p_seed)
{
    uint32_t const pick  = test_rand(p_seed) % 8u;
    uint8_t const  cause = (uint8_t) (test_rand(p_seed) % 16u);
    bool const     b_off = (0u != (test_rand(p_seed) & 1u));

    if (pick <= 3u)
    {
        /* A report of any 4-bit cause, latching or status. */
        uint16_t const bit = (cause >= 1u) && (cause <= 11u)
                             ? (uint16_t) (1u << (cause - 1u)) : 0u;

        (void) safety_latch_report(p_latch, cause, FAULT_SRC_SAFETY, 0u,
                                   b_off);

        if ((10u == cause) || (11u == cause))
        {
            p_model->status = b_off ? (uint16_t) (p_model->status & ~bit)
                                    : (uint16_t) (p_model->status | bit);
        }
        else
        {
            p_model->latching |= bit;

            if (!p_model->b_latched || p_model->b_waiting)
            {
                p_model->b_latched = true;
                p_model->b_waiting = false;
            }
        }
    }
    else if (pick <= 5u)
    {
        if (safety_latch_reset(p_latch, !b_off))
        {
            p_model->b_waiting = true;
        }
    }
    else if (6u == pick)
    {
        if (safety_latch_ack(p_latch))
        {
            p_model->b_latched = false;
            p_model->b_waiting = false;
            p_model->latching  = 0u;
        }
    }
    else
    {
        safety_latch_ack_timeout(p_latch);
        p_model->b_waiting = false;
    }

    return ((p_latch->mask ==
             (uint16_t) (p_model->status | p_model->latching)) &&
            (p_latch->b_latched == p_model->b_latched) &&
            (p_latch->b_reset_pending == p_model->b_waiting));
}

static void
test_fault_mask (void)
{
    safety_latch_t    latch;
    test_mask_model_t model = { 0u, 0u, false, false };
    uint32_t          seed  = 0x5EED0408u;
    bool              b_ok  = true;

    UNIT_BEGIN("S4-U08", "fault mask: one bit per cause (11); a status "
                         "cause (no echo, Wi-Fi loss) sets and clears its "
                         "bit without latching or cancelling a waiting "
                         "reset; every latching cause sets its bit, the "
                         "counted ones too; the release clears only the "
                         "latching bits; 100,000 random steps agree with "
                         "the fault table (24 Oct review, R-02)");

    /* The fault table: 9 latching causes and 2 status causes. */
    UNIT_EQ(FAULT_MASK_BITS, 11u);
    UNIT_EQ(FAULT_MASK_ALL, 0x7FFu);
    UNIT_EQ(FAULT_MASK_STATUS, 0x600u);
    UNIT_EQ(FAULT_MASK_LATCHING, 0x1FFu);

    for (uint8_t cause = 1u; cause <= FAULT_CAUSE_MAX; cause++)
    {
        b_ok = b_ok && (fault_cause_bit(cause) == (1u << (cause - 1u))) &&
               (fault_cause_latches(cause) ==
                (cause <= FAULT_CAUSE_ENCODER));
    }

    UNIT_CHECK(b_ok);
    UNIT_EQ(fault_cause_bit(0u), 0u);
    UNIT_EQ(fault_cause_bit(12u), 0u);
    UNIT_CHECK(fault_cause_latches(0u));     /* outside the table: */
    UNIT_CHECK(fault_cause_latches(15u));    /* latched (fail safe) */

    /* Wi-Fi loss and no echo: their bits only. */
    safety_latch_init(&latch);
    UNIT_EQ(latch.mask, 0u);
    UNIT_EQ(safety_latch_report(&latch, FAULT_CAUSE_WIFI_LOSS,
                                FAULT_SRC_COMMAND, 0u, false),
            SAFETY_LATCH_STATUS);
    UNIT_EQ(safety_latch_report(&latch, FAULT_CAUSE_NO_ECHO, FAULT_SRC_SCAN,
                                0u, false), SAFETY_LATCH_STATUS);
    UNIT_CHECK(!latch.b_latched);
    UNIT_EQ(latch.mask, 0x600u);
    UNIT_EQ(safety_latch_report(&latch, FAULT_CAUSE_WIFI_LOSS,
                                FAULT_SRC_COMMAND, 0u, true),
            SAFETY_LATCH_STATUS);
    UNIT_EQ(latch.mask, 0x200u);
    UNIT_CHECK(!latch.b_latched);

    /* The first fault latches, later ones are counted, and every one
       sets its bit; a latching cause ignores b_cleared. */
    UNIT_EQ(safety_latch_report(&latch, FAULT_CAUSE_STOP_CMD,
                                FAULT_SRC_COMMAND, 0u, false),
            SAFETY_LATCH_LATCHED);
    UNIT_EQ(safety_latch_report(&latch, FAULT_CAUSE_MOTOR_STALL,
                                FAULT_SRC_MOTION, 3u, false),
            SAFETY_LATCH_COUNTED);
    UNIT_EQ(safety_latch_report(&latch, FAULT_CAUSE_LIFTED, FAULT_SRC_IMU,
                                5000u, true), SAFETY_LATCH_COUNTED);
    UNIT_EQ(latch.mask, 0x200u | FAULT_BIT(FAULT_CAUSE_STOP_CMD) |
                        FAULT_BIT(FAULT_CAUSE_MOTOR_STALL) |
                        FAULT_BIT(FAULT_CAUSE_LIFTED));
    UNIT_EQ(latch.cause, FAULT_CAUSE_STOP_CMD);

    /* Wi-Fi drops while RESET waits for Motion: the reset goes on, and
       the release clears the latching bits only. */
    UNIT_CHECK(safety_latch_reset(&latch, true));
    UNIT_EQ(safety_latch_report(&latch, FAULT_CAUSE_WIFI_LOSS,
                                FAULT_SRC_COMMAND, 0u, false),
            SAFETY_LATCH_STATUS);
    UNIT_CHECK(latch.b_reset_pending);
    UNIT_CHECK(safety_latch_ack(&latch));
    UNIT_CHECK(!latch.b_latched);
    UNIT_EQ(latch.mask, 0x600u);

    /* A cause outside the table latches and sets no bit. */
    UNIT_EQ(safety_latch_report(&latch, 0u, FAULT_SRC_SAFETY, 0u, false),
            SAFETY_LATCH_LATCHED);
    UNIT_EQ(safety_latch_report(&latch, 13u, FAULT_SRC_SAFETY, 0u, true),
            SAFETY_LATCH_COUNTED);
    UNIT_EQ(latch.mask, 0x600u);

    /* Random reports (every 4-bit cause, set or cleared), RESET presses,
       acknowledgements and time-outs, against the model. */
    safety_latch_init(&latch);
    b_ok = true;

    for (uint32_t step = 0u; step < 100000u; step++)
    {
        b_ok = test_mask_step(&latch, &model, &seed) && b_ok;
    }

    UNIT_CHECK(b_ok);
    UNIT_END();
}

/* ---- Minimum-bit change (D-89): S4-U40 and up ---- */

static void
test_imu_events (void)
{
    imu_evt_t evt;
    bool      b_ok = true;

    UNIT_BEGIN("S4-U40", "IF-03 IMU event packed into one word: the exact "
                         "expected bits; every heading, peak roll and "
                         "table height round-trips (D-89)");
    UNIT_EQ(sizeof(imu_evt_t), 4u);

    UNIT_CHECK(imu_evt_heading(&evt, -17999, true));
    UNIT_EQ(evt.bits, 0x0006E6C4u);
    UNIT_EQ(imu_evt_type(&evt), IMU_EVT_HEADING);
    UNIT_EQ(imu_evt_heading_cdeg(&evt), -17999);
    UNIT_CHECK(imu_evt_level_ground(&evt));
    UNIT_EQ(imu_evt_height_mm(&evt), 0);    /* not carried by HEADING */

    UNIT_CHECK(imu_evt_hump_end(&evt, 18000, 65));
    UNIT_EQ(evt.bits, 0x00831942u);
    UNIT_EQ(imu_evt_peak_roll_cdeg(&evt), 18000);
    UNIT_EQ(imu_evt_height_mm(&evt), 65);
    UNIT_CHECK(!imu_evt_level_ground(&evt));

    imu_evt_hump_start(&evt);
    UNIT_EQ(evt.bits, 0x00000001u);
    UNIT_CHECK(imu_evt_class(&evt, IMU_CLASS_STEADY));
    UNIT_EQ(evt.bits, 0x0000001Bu);
    UNIT_EQ(imu_evt_motion_class(&evt), IMU_CLASS_STEADY);

    for (int32_t heading = -17999; heading <= 18000; heading++)
    {
        b_ok = b_ok && imu_evt_heading(&evt, heading, (0 != (heading % 2))) &&
               (imu_evt_heading_cdeg(&evt) == heading) &&
               (imu_evt_level_ground(&evt) == (0 != (heading % 2)));
    }

    /* Every peak roll, with the height the calibration table gives for
       it: the table never gives more than the 7-bit field holds. */
    for (int32_t peak = 0; peak <= 18000; peak++)
    {
        int32_t const height = imu_calc_hump_height(peak);

        b_ok = b_ok && (height >= 0) && (height <= 127) &&
               imu_evt_hump_end(&evt, peak, height) &&
               (imu_evt_peak_roll_cdeg(&evt) == peak) &&
               (imu_evt_height_mm(&evt) == height);
    }

    UNIT_CHECK(b_ok);
    UNIT_CHECK(!imu_evt_hump_end(&evt, -1, 200));
    UNIT_EQ(imu_evt_peak_roll_cdeg(&evt), 0);
    UNIT_EQ(imu_evt_height_mm(&evt), 127);
    UNIT_EQ(imu_evt_type(NULL), 0u);
    UNIT_END();
}

static void
test_fault_events (void)
{
    fault_evt_t evt;
    bool        b_ok = true;

    UNIT_BEGIN("S4-U41", "IF-04 fault report packed into one word: the "
                         "exact expected bits; every cause (11, in 4 "
                         "bits), source and detail up to 8191 "
                         "round-trips; a status report carries its "
                         "cleared flag (D-89, R-02)");
    UNIT_EQ(sizeof(fault_evt_t), 4u);

    UNIT_CHECK(fault_evt_pack(&evt, FAULT_CAUSE_LIFTED, FAULT_SRC_IMU,
                              6928u));
    UNIT_EQ(evt.bits, 0x000D8821u);
    UNIT_CHECK(fault_evt_pack(&evt, FAULT_CAUSE_MAX, FAULT_SRC_SAFETY,
                              8191u));
    UNIT_EQ(evt.bits, 0x000FFFFBu);
    UNIT_CHECK(!fault_evt_cleared(&evt));

    for (uint8_t cause = FAULT_CAUSE_LIFTED; cause <= FAULT_CAUSE_MAX;
         cause++)
    {
        for (uint8_t source = FAULT_SRC_LINE; source <= FAULT_SRC_SAFETY;
             source++)
        {
            for (uint32_t detail = 0u; detail <= 8191u; detail += 7u)
            {
                b_ok = b_ok &&
                       fault_evt_pack(&evt, cause, source, (uint16_t) detail) &&
                       (fault_evt_cause(&evt) == cause) &&
                       (fault_evt_source(&evt) == source) &&
                       (fault_evt_detail(&evt) == detail) &&
                       !fault_evt_cleared(&evt);
            }
        }
    }

    UNIT_CHECK(b_ok);

    /* Status reports: the cleared flag is bit 20, the detail 0. */
    UNIT_CHECK(fault_evt_pack_status(&evt, FAULT_CAUSE_NO_ECHO,
                                     FAULT_SRC_SCAN, false));
    UNIT_EQ(evt.bits, 0x0010004Au);
    UNIT_CHECK(fault_evt_cleared(&evt));
    UNIT_CHECK(fault_evt_pack_status(&evt, FAULT_CAUSE_WIFI_LOSS,
                                     FAULT_SRC_COMMAND, true));
    UNIT_EQ(evt.bits, 0x0000005Bu);
    UNIT_CHECK(!fault_evt_cleared(&evt));
    UNIT_EQ(fault_evt_cause(&evt), FAULT_CAUSE_WIFI_LOSS);
    UNIT_EQ(fault_evt_source(&evt), FAULT_SRC_COMMAND);
    /* A latching cause is not a status: reported, packed all the same. */
    UNIT_CHECK(!fault_evt_pack_status(&evt, FAULT_CAUSE_LIFTED,
                                      FAULT_SRC_IMU, false));
    UNIT_EQ(fault_evt_cause(&evt), FAULT_CAUSE_LIFTED);
    UNIT_CHECK(!fault_evt_pack_status(NULL, FAULT_CAUSE_NO_ECHO,
                                      FAULT_SRC_SCAN, true));
    UNIT_CHECK(!fault_evt_cleared(NULL));

    /* Values that cannot occur: reported, the nearest value kept. */
    UNIT_CHECK(!fault_evt_pack(&evt, FAULT_CAUSE_LIFTED, FAULT_SRC_IMU,
                               9000u));
    UNIT_EQ(fault_evt_detail(&evt), 8191u);
    UNIT_CHECK(!fault_evt_pack(&evt, FAULT_CAUSE_LIFTED, 0u, 1u));
    UNIT_EQ(fault_evt_source(&evt), FAULT_SRC_SAFETY);
    UNIT_CHECK(!fault_evt_pack(&evt, 16u, FAULT_SRC_IMU, 1u));
    UNIT_EQ(fault_evt_cause(&evt), 15u);
    UNIT_EQ(fault_evt_cause(NULL), 0u);
    UNIT_END();
}

/* Lift / overturn detector (replaces collision detection). */
static void
test_lift (void)
{
    imu_lift_t     lift;
    imu_lift_evt_t evt = IMU_LIFT_NONE;
    uint32_t       idx = 0u;

    UNIT_BEGIN("S4-U09", "lift: upward acceleration held for N samples, "
                         "overturn past 60 degrees held for N samples; "
                         "short jolts, a 30-degree hump and flat driving "
                         "never trip it");
    imu_lift_init(&lift, 0, 0, 1000);

    /* Flat, vibration within +/-200 mg up: nothing. */
    for (idx = 0u; idx < 50u; idx++)
    {
        evt = imu_lift_step(&lift, 300, -200, 1000 + (int32_t) ((idx % 2u) * 200u));
        UNIT_EQ(evt, IMU_LIFT_NONE);
    }
    UNIT_CHECK(imu_lift_clear(&lift));

    /* A jolt shorter than LIFT_SAMPLES: nothing. */
    for (idx = 0u; idx < (CFG_IMU_LIFT_SAMPLES - 1u); idx++)
    {
        UNIT_EQ(imu_lift_step(&lift, 0, 0, 1600), IMU_LIFT_NONE);
    }
    UNIT_EQ(imu_lift_step(&lift, 0, 0, 1000), IMU_LIFT_NONE);
    UNIT_CHECK(imu_lift_clear(&lift));

    /* Picked up: about 0.35 g up for longer: LIFTED on the Nth sample. */
    for (idx = 1u; idx < CFG_IMU_LIFT_SAMPLES; idx++)
    {
        UNIT_EQ(imu_lift_step(&lift, 50, 0, 1350), IMU_LIFT_NONE);
    }
    UNIT_EQ(imu_lift_step(&lift, 50, 0, 1350), IMU_LIFT_LIFTED);
    UNIT_CHECK(!imu_lift_clear(&lift));
    UNIT_EQ(imu_lift_step(&lift, 0, 0, 1000), IMU_LIFT_NONE);
    UNIT_CHECK(imu_lift_clear(&lift));

    /* A 30-degree hump (cos 0.87): never overturned. */
    for (idx = 0u; idx < 100u; idx++)
    {
        UNIT_EQ(imu_lift_step(&lift, 0, 500, 866), IMU_LIFT_NONE);
    }

    /* On its side (90 degrees): OVERTURNED after OVERTURN_SAMPLES. */
    for (idx = 1u; idx < CFG_IMU_OVERTURN_SAMPLES; idx++)
    {
        UNIT_EQ(imu_lift_step(&lift, 0, 1000, 0), IMU_LIFT_NONE);
    }
    UNIT_EQ(imu_lift_step(&lift, 0, 1000, 0), IMU_LIFT_OVERTURNED);
    /* Upside down stays overturned. */
    UNIT_EQ(imu_lift_step(&lift, 0, 0, -1000), IMU_LIFT_OVERTURNED);
    UNIT_EQ(imu_lift_step(&lift, 0, 0, 1000), IMU_LIFT_NONE);
    UNIT_CHECK(imu_lift_clear(&lift));

    /* Calibrated on a tilted car: "up" is the stored vector. */
    imu_lift_init(&lift, 0, 500, 866);
    for (idx = 0u; idx < 50u; idx++)
    {
        UNIT_EQ(imu_lift_step(&lift, 0, 500, 866), IMU_LIFT_NONE);
    }
    UNIT_END();
}

void
test_s4 (void)
{
    test_tilt();
    test_heading();
    test_norm_height();
    test_hump();
    test_classes();
    test_samples();
    test_lift();
    test_safety_latch();
    test_fault_mask();
    test_imu_events();
    test_fault_events();
}

/*** end of file ***/
