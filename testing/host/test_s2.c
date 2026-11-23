/*
 *  test_s2.c
 *  Host tests for Buddy 2's motion maths (motion_ctl.c).
 *  Results: testing/subsystem2/TEST_LOG.md.
 *
 *  Expected values are worked out from the UNVALIDATED geometry in
 *  app_config.h (65 mm wheel, 130 mm track, 528 counts per turn).  If the
 *  measured values replace them, the checks marked "geometry" change too.
 */

#include <stdlib.h>

#include "app_config.h"
#include "subsystem2/drv_encoder.h"
#include "subsystem2/motion_ctl.h"
#include "subsystem2/motion_msgs.h"
#include "unit.h"

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
test_distance (void)
{
    UNIT_BEGIN("S2-U01", "counts <-> mm (geometry: 528 counts = 204 mm)");
    UNIT_EQ(motion_counts_to_mm(CFG_ENC_COUNTS_PER_REV), 204);
    UNIT_EQ(motion_counts_to_mm(-CFG_ENC_COUNTS_PER_REV), -204);
    UNIT_EQ(motion_counts_to_mm(0), 0);
    UNIT_CHECK(abs(motion_mm_to_counts(204) - CFG_ENC_COUNTS_PER_REV) <= 1);
    UNIT_CHECK(abs(motion_counts_to_mm(motion_mm_to_counts(1000)) - 1000)
               <= 1);
    UNIT_EQ(motion_mm_to_counts(100000), 258565);   /* 100 m, no overflow */
    UNIT_END();
}

static void
test_turn (void)
{
    UNIT_BEGIN("S2-U02", "pivot-turn counts (geometry: 90 deg = 264)");
    UNIT_EQ(motion_turn_counts(9000), 264);
    UNIT_EQ(motion_turn_counts(-9000), 264);
    UNIT_EQ(motion_turn_counts(18000), 528);
    UNIT_EQ(motion_turn_counts(0), 0);
    UNIT_END();
}

static void
test_speed_heading (void)
{
    UNIT_BEGIN("S2-U03", "wheel speed from counts over the real time "
                         "between reads (a missed release does not double "
                         "it) and odometry heading signs");
    UNIT_EQ(motion_speed_mmps(CFG_ENC_COUNTS_PER_REV, 1000000u), 204);
    UNIT_EQ(motion_speed_mmps(-CFG_ENC_COUNTS_PER_REV, 1000000u), -204);
    UNIT_EQ(motion_speed_mmps(13, 20000u), 251);
    UNIT_EQ(motion_speed_mmps(100, 0u), 0);

    /* A missed release: 40 ms of counts read at once give the same
       speed as 20 ms of half as many, and a late read (23 ms) less. */
    UNIT_EQ(motion_speed_mmps(26, 40000u), motion_speed_mmps(13, 20000u));
    UNIT_EQ(motion_speed_mmps(13, 23000u), 218);

    /* Right wheel further than left = turned left = positive. */
    UNIT_EQ(motion_heading_cdeg(0, 528), 9000);
    UNIT_EQ(motion_heading_cdeg(528, 0), -9000);
    UNIT_EQ(motion_heading_cdeg(-264, 264), 9000);
    UNIT_EQ(motion_heading_cdeg(0, 1584), -9000);   /* 270 = -90 */
    UNIT_EQ(motion_heading_cdeg(1000, 1000), 0);
    UNIT_END();
}

static void
test_pid (void)
{
    motion_pid_t pid;
    int32_t      duty = 0;

    UNIT_BEGIN("S2-U04", "PID: feed-forward, limits, anti-windup, stop");
    motion_pid_reset(&pid);

    /* On target: feed-forward only (100 per mille per 100 mm/s). */
    UNIT_EQ(motion_pid_step(&pid, 250, 250, 20u), 250);
    UNIT_EQ(motion_pid_step(&pid, -250, -250, 20u), -250);

    /* Stalled wheel: the output saturates at the duty limit and the
       integral stops at its limit. */
    motion_pid_reset(&pid);

    for (uint32_t step = 0u; step < 500u; step++)
    {
        duty = motion_pid_step(&pid, 400, 0, 20u);
    }

    UNIT_EQ(duty, CFG_MOTOR_MAX_DUTY_PM);
    UNIT_EQ(pid.integral_x1000, CFG_PID_I_LIMIT_PM * 1000);

    /* A zero target brakes and clears the history. */
    UNIT_EQ(motion_pid_step(&pid, 0, 100, 20u), 0);
    UNIT_EQ(pid.integral_x1000, 0);
    UNIT_CHECK(!pid.b_have_prev);
    UNIT_END();
}

static void
test_ramp (void)
{
    UNIT_BEGIN("S2-U05", "soft start up, immediate slow-down, stop on reverse");
    UNIT_EQ(motion_ramp(0, 1000), CFG_MOTOR_RAMP_PM);
    UNIT_EQ(motion_ramp(300, 1000), 300 + CFG_MOTOR_RAMP_PM);
    UNIT_EQ(motion_ramp(950, 1000), 1000);
    UNIT_EQ(motion_ramp(0, -500), -CFG_MOTOR_RAMP_PM);
    UNIT_EQ(motion_ramp(800, 200), 200);
    UNIT_EQ(motion_ramp(800, 0), 0);
    UNIT_EQ(motion_ramp(500, -500), 0);
    UNIT_EQ(motion_ramp(-500, 500), 0);
    UNIT_END();
}

static void
test_output_duty (void)
{
    int32_t const floor_pm = CFG_MOTOR_MIN_DUTY_PM;

    UNIT_BEGIN("S2-U06", "drive from the PID: never against the target's "
                         "direction, the start-up duty only while the "
                         "wheel stands still, zero for a zero target "
                         "(24 Oct review, R-06)");

    /* Forward target, wheel too fast: less drive, never reverse. */
    UNIT_EQ(motion_output_duty(-100, 60, 120), 0);
    UNIT_EQ(motion_output_duty(-600, 60, 300), 0);
    /* Backward target, wheel too fast backwards: likewise. */
    UNIT_EQ(motion_output_duty(100, -60, -120), 0);
    UNIT_EQ(motion_output_duty(600, -60, -300), 0);

    /* Turning: the PID may ask for less than the start-up duty. */
    UNIT_EQ(motion_output_duty(80, 60, 55), 80);
    UNIT_EQ(motion_output_duty(-80, -60, -55), -80);

    /* Standing still: at least the start-up duty, in the target's
       direction, whatever the PID asks. */
    UNIT_EQ(motion_output_duty(80, 60, 0), floor_pm);
    UNIT_EQ(motion_output_duty(-300, 60, 0), floor_pm);
    UNIT_EQ(motion_output_duty(-80, -60, 0), -floor_pm);
    UNIT_EQ(motion_output_duty(500, 250, 0), 500);
    UNIT_EQ(motion_output_duty(-500, -250, 0), -500);

    /* Zero target: zero (brake), whatever the PID or the wheel. */
    UNIT_EQ(motion_output_duty(300, 0, 0), 0);
    UNIT_EQ(motion_output_duty(-300, 0, 100), 0);
    UNIT_END();
}

/* The Motion task's cycle on one wheel held still: the PID, the output
   rule and the ramp, as motion_duty() runs them, at 20 ms.  Returns the
   duty applied this cycle. */
static int32_t
test_held_cycle (motion_pid_t * p_pid, int32_t applied, int32_t target)
{
    return (motion_ramp(applied,
                        motion_output_duty(motion_pid_step(p_pid, target, 0,
                                                           20u),
                                           target, 0)));
}

static void
test_stall_watch (void)
{
    int32_t const  quiet = CFG_MOTION_STALL_DUTY_PM - 1;
    int32_t const  drive = CFG_MOTION_STALL_DUTY_PM;
    uint32_t const ticks = CFG_MOTION_STALL_MS / 20u;
    motion_stall_t watch;
    uint8_t        wheels = 0u;
    bool           b_ok   = true;

    UNIT_BEGIN("S2-U07", "stall and encoder-fault watch: a driven wheel "
                         "with no count for the set time is reported "
                         "once (both: stall; one while the other counts: "
                         "encoder fault); less drive or any count starts "
                         "it again; with today's PID gains a held wheel "
                         "is not reported before the PID gives nearly "
                         "all it can, at any target speed (24 Oct "
                         "review, R-01)");

    /* Both wheels driven hard and silent: one stall report, exactly
       when the set time is reached, then nothing more. */
    motion_stall_reset(&watch);

    for (uint32_t tick = 1u; tick < ticks; tick++)
    {
        b_ok = b_ok && (MOTION_STALL_NONE ==
                        motion_stall_step(&watch, drive, -drive, 0, 0, 20u,
                                          &wheels)) && (0u == wheels);
    }

    UNIT_CHECK(b_ok);
    UNIT_EQ(motion_stall_silent_ms(&watch, false), CFG_MOTION_STALL_MS - 20);
    UNIT_EQ(watch.bits, (uint32_t) (CFG_MOTION_STALL_MS - 20) |
                        ((uint32_t) (CFG_MOTION_STALL_MS - 20) << 10));
    UNIT_EQ(motion_stall_step(&watch, drive, -drive, 0, 0, 20u, &wheels),
            MOTION_STALL_MOTOR);
    UNIT_EQ(wheels, MOTION_STALL_LEFT | MOTION_STALL_RIGHT);
    UNIT_EQ(watch.bits, (uint32_t) CFG_MOTION_STALL_MS |
                        ((uint32_t) CFG_MOTION_STALL_MS << 10) | (1u << 20));
    UNIT_EQ(motion_stall_step(&watch, drive, -drive, 0, 0, 20u, &wheels),
            MOTION_STALL_NONE);
    UNIT_EQ(wheels, MOTION_STALL_LEFT | MOTION_STALL_RIGHT);

    /* Counting again re-arms it: a new stall is reported again. */
    UNIT_EQ(motion_stall_step(&watch, drive, drive, 1, -1, 20u, &wheels),
            MOTION_STALL_NONE);
    UNIT_EQ(watch.bits, 0u);
    UNIT_EQ(motion_stall_step(&watch, drive, drive, 0, 0,
                              CFG_MOTION_STALL_MS, &wheels),
            MOTION_STALL_MOTOR);

    /* One wheel silent while the other counts: an encoder fault, on
       either side, reverse drive included. */
    motion_stall_reset(&watch);
    b_ok = true;

    for (uint32_t tick = 1u; tick < ticks; tick++)
    {
        b_ok = b_ok && (MOTION_STALL_NONE ==
                        motion_stall_step(&watch, -drive, drive, 0, 3, 20u,
                                          &wheels));
    }

    UNIT_CHECK(b_ok);
    UNIT_EQ(motion_stall_step(&watch, -drive, drive, 0, 3, 20u, &wheels),
            MOTION_STALL_ENCODER);
    UNIT_EQ(wheels, MOTION_STALL_LEFT);
    motion_stall_reset(&watch);
    UNIT_EQ(motion_stall_step(&watch, 500, 500, -2, 0,
                              CFG_MOTION_STALL_MS, &wheels),
            MOTION_STALL_ENCODER);
    UNIT_EQ(wheels, MOTION_STALL_RIGHT);

    /* One silent, the other neither driven nor counting: a stall. */
    motion_stall_reset(&watch);
    UNIT_EQ(motion_stall_step(&watch, drive, 0, 0, 0, CFG_MOTION_STALL_MS,
                              &wheels), MOTION_STALL_MOTOR);
    UNIT_EQ(wheels, MOTION_STALL_LEFT);

    /* Less than the set duty, or one count in the window: never. */
    motion_stall_reset(&watch);
    b_ok = true;

    for (uint32_t tick = 0u; tick < (10u * ticks); tick++)
    {
        int32_t const count = (0u == (tick % (ticks - 1u))) ? 1 : 0;

        b_ok = b_ok && (MOTION_STALL_NONE ==
                        motion_stall_step(&watch, quiet, -quiet, 0, 0, 20u,
                                          &wheels));
        b_ok = b_ok && (0u == watch.bits);
        b_ok = b_ok && (MOTION_STALL_NONE ==
                        motion_stall_step(&watch, drive, drive, count, count,
                                          0u, &wheels));
    }

    UNIT_CHECK(b_ok);
    motion_stall_reset(&watch);
    b_ok = true;

    for (uint32_t tick = 0u; tick < (10u * ticks); tick++)
    {
        int32_t const count = (0u == (tick % (ticks - 1u))) ? 1 : 0;

        b_ok = b_ok && (MOTION_STALL_NONE ==
                        motion_stall_step(&watch, drive, drive, count, count,
                                          20u, &wheels));
    }

    UNIT_CHECK(b_ok);

    /* A very late cycle saturates; nothing overflows. */
    motion_stall_reset(&watch);
    UNIT_EQ(motion_stall_step(&watch, drive, drive, 0, 0, UINT32_MAX,
                              &wheels), MOTION_STALL_MOTOR);
    UNIT_EQ(motion_stall_silent_ms(&watch, true), CFG_MOTION_STALL_MS);

    /* With today's PID gains, a wheel held still for each target speed
       from 10 to 350 mm/s is not reported while the PID's output is
       still more than 20 per mille short of the most it gives: a hump
       the PID can climb never trips the watch.  It is reported once it
       is at that most for long enough. */
    b_ok = true;

    for (int32_t target = 10; target <= 350; target += 10)
    {
        motion_pid_t pid;
        int32_t      applied  = 0;
        int32_t      most     = 0;
        bool         b_report = false;

        motion_pid_reset(&pid);
        motion_stall_reset(&watch);

        /* 40 s: at 10 mm/s the I term takes about 15 s to fill. */
        for (uint32_t tick = 0u; tick < 2000u; tick++)
        {
            applied = test_held_cycle(&pid, applied, target);
            most    = (applied > most) ? applied : most;
        }

        motion_pid_reset(&pid);
        applied = 0;

        for (uint32_t tick = 0u; (tick < 2000u) && !b_report; tick++)
        {
            b_report = (MOTION_STALL_NONE !=
                        motion_stall_step(&watch, applied, applied, 0, 0,
                                          20u, &wheels));
            b_ok = b_ok && (!b_report || (applied >= (most - 20)));
            applied = test_held_cycle(&pid, applied, target);
        }

        b_ok = b_ok && b_report;
    }

    UNIT_CHECK(b_ok);

    /* No pointer: nothing happens. */
    motion_stall_reset(NULL);
    UNIT_EQ(motion_stall_step(NULL, drive, drive, 0, 0, 20u, &wheels),
            MOTION_STALL_NONE);
    UNIT_EQ(wheels, 0u);
    UNIT_EQ(motion_stall_step(&watch, drive, drive, 0, 0, 20u, NULL),
            MOTION_STALL_NONE);
    UNIT_EQ(motion_stall_silent_ms(NULL, false), 0u);
    UNIT_END();
}

/* ---- Minimum-bit change (D-88): S2-U40 and up ---- */

static void
test_motion_cmd (void)
{
    motion_cmd_t cmd;
    bool         b_ok = true;

    UNIT_BEGIN("S2-U40", "IF-07 command packed into one word: the exact "
                         "expected bits for each kind; every value "
                         "round-trips (a move's size in 15 bits); fields a "
                         "kind does not carry read 0 (D-88)");
    UNIT_EQ(sizeof(motion_cmd_t), 4u);

    UNIT_CHECK(motion_cmd_pack(&cmd, MOTION_CMD_DRIVE, 9u, -1000, 200, 77));
    UNIT_EQ(cmd.bits, 0x003220C0u);
    UNIT_EQ(motion_cmd_type(&cmd), MOTION_CMD_DRIVE);
    UNIT_EQ(motion_cmd_speed(&cmd), -1000);
    UNIT_EQ(motion_cmd_diff(&cmd), 200);
    UNIT_EQ(motion_cmd_move_id(&cmd), 0u);  /* not carried by DRIVE */
    UNIT_EQ(motion_cmd_value(&cmd), 0);

    UNIT_CHECK(motion_cmd_pack(&cmd, MOTION_CMD_FORWARD, 17u, 5, 5, 800));
    UNIT_EQ(cmd.bits, 0x00190089u);
    UNIT_EQ(motion_cmd_move_id(&cmd), 17u);
    UNIT_EQ(motion_cmd_value(&cmd), 800);
    UNIT_EQ(motion_cmd_speed(&cmd), 0);     /* not carried by a move */

    UNIT_CHECK(motion_cmd_pack(&cmd, MOTION_CMD_TURN_LEFT, 255u, 0, 0,
                               18000));
    UNIT_EQ(cmd.bits, 0x023287FBu);
    UNIT_CHECK(motion_cmd_pack(&cmd, MOTION_CMD_HOLD_SPEED, 0u, 0, 0, 1));
    UNIT_EQ(cmd.bits, 0x0000000Eu);
    UNIT_EQ(motion_cmd_value(&cmd), 1);
    UNIT_CHECK(motion_cmd_pack(&cmd, MOTION_CMD_STOP, 3u, 4, 5, 6));
    UNIT_EQ(cmd.bits, 0x00000005u);

    /* Every speed and difference; every move ID and a range of values. */
    for (int32_t speed = -1000; speed <= 1000; speed++)
    {
        b_ok = b_ok &&
               motion_cmd_pack(&cmd, MOTION_CMD_DRIVE, 0u, speed, -speed, 0) &&
               (motion_cmd_speed(&cmd) == speed) &&
               (motion_cmd_diff(&cmd) == -speed);
    }

    /* Every move and turn value 0..32767, with every move ID; the bits
       above the value stay 0. */
    for (int32_t value = 0; value <= MOTION_CMD_VALUE_MAX; value++)
    {
        uint8_t const move = (uint8_t) (value & 0xFF);

        for (uint8_t type = MOTION_CMD_FORWARD; type <= MOTION_CMD_TURN_RIGHT;
             type++)
        {
            b_ok = b_ok &&
                   motion_cmd_pack(&cmd, type, move, 0, 0, value) &&
                   (motion_cmd_type(&cmd) == type) &&
                   (motion_cmd_move_id(&cmd) == move) &&
                   (motion_cmd_value(&cmd) == value) &&
                   (0u == (cmd.bits & 0xFC000000u));
        }
    }

    UNIT_CHECK(b_ok);

    /* A negative move is its size, as Motion always took it: it fits.
       A size above 32767 does not fit: reported (motion_send() then
       refuses the command) and kept at the nearest end. */
    UNIT_CHECK(motion_cmd_pack(&cmd, MOTION_CMD_TURN_RIGHT, 1u, 0, 0, -9000));
    UNIT_EQ(motion_cmd_value(&cmd), 9000);
    UNIT_CHECK(motion_cmd_pack(&cmd, MOTION_CMD_BACKWARD, 1u, 0, 0, -32767));
    UNIT_EQ(motion_cmd_value(&cmd), 32767);
    UNIT_CHECK(!motion_cmd_pack(&cmd, MOTION_CMD_FORWARD, 1u, 0, 0, 32768));
    UNIT_EQ(motion_cmd_value(&cmd), 32767);
    UNIT_CHECK(!motion_cmd_pack(&cmd, MOTION_CMD_FORWARD, 1u, 0, 0, 40000));
    UNIT_EQ(motion_cmd_value(&cmd), 32767);
    UNIT_CHECK(!motion_cmd_pack(&cmd, MOTION_CMD_TURN_LEFT, 1u, 0, 0, -32768));
    UNIT_CHECK(!motion_cmd_pack(&cmd, MOTION_CMD_BACKWARD, 1u, 0, 0,
                                INT32_MIN));
    UNIT_EQ(motion_cmd_value(&cmd), 32767);

    /* Values that cannot occur: kept at the nearest end, reported; a
       type that is not a command is kept as KEEPALIVE. */
    UNIT_CHECK(!motion_cmd_pack(&cmd, MOTION_CMD_DRIVE, 0u, 1100, -1100, 0));
    UNIT_EQ(motion_cmd_speed(&cmd), 1023);
    UNIT_EQ(motion_cmd_diff(&cmd), -1024);
    UNIT_CHECK(!motion_cmd_pack(&cmd, 0u, 0u, 0, 0, 0));
    UNIT_EQ(motion_cmd_type(&cmd), MOTION_CMD_KEEPALIVE);
    UNIT_CHECK(!motion_cmd_pack(&cmd, 9u, 0u, 0, 0, 0));
    UNIT_EQ(motion_cmd_type(&cmd), MOTION_CMD_KEEPALIVE);
    UNIT_EQ(motion_cmd_type(NULL), 0u);
    UNIT_END();
}

static void
test_motion_sts (void)
{
    motion_sts_t sts;
    bool         b_ok = true;
    uint32_t     seed = 0x5EED0201u;

    UNIT_BEGIN("S2-U41", "IF-08 status packed into two words: the exact "
                         "expected bits; heading, move ID and every 31-bit "
                         "distance round-trip; the farthest distances the "
                         "counters can give fit (D-88)");
    UNIT_EQ(sizeof(motion_sts_t), 8u);

    UNIT_CHECK(motion_sts_odometry(&sts, -123456, -17999));
    UNIT_EQ(sts.word[0], 0x00017362u);
    UNIT_EQ(sts.word[1], 0x7FFE1DC0u);
    UNIT_EQ(motion_sts_type(&sts), MOTION_STS_ODOMETRY);
    UNIT_EQ(motion_sts_distance_mm(&sts), -123456);
    UNIT_EQ(motion_sts_heading_cdeg(&sts), -17999);
    UNIT_EQ(motion_sts_move_id(&sts), 0u);

    motion_sts_move_done(&sts, 200u);
    UNIT_EQ(sts.word[0], 0x00000191u);
    UNIT_EQ(sts.word[1], 0x00000000u);
    UNIT_EQ(motion_sts_move_id(&sts), 200u);
    UNIT_EQ(motion_sts_distance_mm(&sts), 0);

    for (int32_t heading = -17999; heading <= 18000; heading++)
    {
        b_ok = b_ok && motion_sts_odometry(&sts, heading * 1000, heading) &&
               (motion_sts_heading_cdeg(&sts) == heading) &&
               (motion_sts_distance_mm(&sts) == (heading * 1000));
    }

    /* The farthest distances Motion can report: the counters' half-sum
       at its ends (motion_send_odometry()). */
    UNIT_CHECK(motion_sts_odometry(&sts, motion_counts_to_mm(INT32_MIN),
                                   0));
    UNIT_EQ(motion_sts_distance_mm(&sts), motion_counts_to_mm(INT32_MIN));
    UNIT_CHECK(motion_sts_odometry(&sts,
                                   motion_counts_to_mm(INT32_MAX - 1), 0));
    UNIT_EQ(motion_sts_distance_mm(&sts),
            motion_counts_to_mm(INT32_MAX - 1));

    /* Every 31-bit distance, ends included. */
    UNIT_CHECK(motion_sts_odometry(&sts, -1073741824, 0));
    UNIT_EQ(motion_sts_distance_mm(&sts), -1073741824);
    UNIT_CHECK(motion_sts_odometry(&sts, 1073741823, 0));
    UNIT_EQ(motion_sts_distance_mm(&sts), 1073741823);

    for (uint32_t idx = 0u; idx < 100000u; idx++)
    {
        uint32_t const raw = test_rand(&seed) & 0x7FFFFFFFu;
        int32_t const distance = bitpack_to_signed(raw, 31u);

        b_ok = b_ok && motion_sts_odometry(&sts, distance, 0) &&
               (motion_sts_distance_mm(&sts) == distance) &&
               (sts.word[1] == raw);
    }

    UNIT_CHECK(b_ok);
    UNIT_CHECK(!motion_sts_odometry(&sts, 0, 40000));
    UNIT_CHECK(!motion_sts_odometry(&sts, 1073741824, 0));
    UNIT_EQ(motion_sts_distance_mm(&sts), 1073741823);  /* nearest end */
    UNIT_CHECK(!motion_sts_odometry(&sts, INT32_MIN, 0));
    UNIT_EQ(motion_sts_distance_mm(&sts), -1073741824);
    UNIT_EQ(motion_sts_type(NULL), 0u);
    UNIT_END();
}

static void
test_encoder_pair (void)
{
    encoder_counts_t volatile shared;
    encoder_counts_t          last;
    encoder_counts_t          now;
    int32_t                   ref_l   = 0;      /* the old int32 counters */
    int32_t                   ref_r   = 0;
    int32_t                   total_l = 0;      /* Motion's totals */
    int32_t                   total_r = 0;
    uint32_t                  seed    = 0x5EED0202u;
    uint32_t                  mismatches = 0u;

    UNIT_BEGIN("S2-U42", "encoder pair in one word: each wheel changes only "
                         "its own 16-bit half; totals from the signed "
                         "16-bit changes, added by the driver's own "
                         "function, equal 32-bit counters through many "
                         "wraps (D-88)");
    UNIT_EQ(sizeof(encoder_counts_t), 4u);

    shared.word = 0u;
    last.word   = 0u;

    /* A step on one wheel never touches the other half, even across
       the wrap of the stepped half. */
    encoder_count_step(&shared, ENCODER_LEFT, -1);
    now.word = shared.word;
    UNIT_EQ(now.half[ENCODER_LEFT], 0xFFFFu);
    UNIT_EQ(now.half[ENCODER_RIGHT], 0x0000u);
    encoder_count_step(&shared, ENCODER_RIGHT, 1);
    encoder_count_step(&shared, ENCODER_LEFT, 1);
    now.word = shared.word;
    UNIT_EQ(now.half[ENCODER_LEFT], 0x0000u);
    UNIT_EQ(now.half[ENCODER_RIGHT], 0x0001u);
    UNIT_EQ(encoder_count_change(&now, &last, ENCODER_RIGHT), 1);

    /* The driver's add: each wheel to its own total, and a second read
       with no counts in between adds nothing. */
    shared.word = 0u;
    encoder_count_step(&shared, ENCODER_LEFT, 1);
    encoder_count_step(&shared, ENCODER_LEFT, 1);
    encoder_count_step(&shared, ENCODER_RIGHT, -1);
    encoder_add_pair(&shared, &last, &total_l, &total_r);
    UNIT_EQ(total_l, 2);
    UNIT_EQ(total_r, -1);
    encoder_add_pair(&shared, &last, &total_l, &total_r);
    UNIT_EQ(total_l, 2);
    UNIT_EQ(total_r, -1);
    UNIT_EQ(last.word, shared.word);

    shared.word = 0u;
    last.word   = 0u;
    total_l     = 0;
    total_r     = 0;

    for (uint32_t read = 0u; read < 20000u; read++)
    {
        /* Up to 3000 counts per wheel between reads (far more than the
           ~100 in 20 ms at 2 m/s), mostly one way, so the halves wrap
           many times. */
        uint32_t const ticks = test_rand(&seed) % 3000u;
        bool const     b_back_l = (0u == ((read / 500u) % 2u));
        bool const     b_back_r = (0u == ((read / 700u) % 3u));

        for (uint32_t tick = 0u; tick < ticks; tick++)
        {
            uint32_t const pick = test_rand(&seed) % 10u;

            if (pick < 5u)
            {
                int32_t const step = ((pick < 4u) != b_back_l) ? 1 : -1;

                ref_l += step;
                encoder_count_step(&shared, ENCODER_LEFT, step);
            }
            else
            {
                int32_t const step = ((pick < 9u) != b_back_r) ? 1 : -1;

                ref_r += step;
                encoder_count_step(&shared, ENCODER_RIGHT, step);
            }
        }

        encoder_add_pair(&shared, &last, &total_l, &total_r);
        mismatches += ((total_l != ref_l) || (total_r != ref_r)) ? 1u : 0u;
    }

    UNIT_EQ(mismatches, 0u);
    UNIT_CHECK((abs(ref_l) > 200000) || (abs(ref_r) > 200000));

    /* The limit: 32767 counts between reads are exact; 32768 would read
       as -32768 (Motion reads every 20 ms, far below either). */
    shared.word = 0u;
    last.word   = 0u;

    for (uint32_t tick = 0u; tick < 32767u; tick++)
    {
        encoder_count_step(&shared, ENCODER_LEFT, 1);
    }

    now.word = shared.word;
    UNIT_EQ(encoder_count_change(&now, &last, ENCODER_LEFT), 32767);
    UNIT_END();
}

void
test_s2 (void)
{
    test_distance();
    test_turn();
    test_speed_heading();
    test_pid();
    test_ramp();
    test_output_duty();
    test_stall_watch();
    test_motion_cmd();
    test_motion_sts();
    test_encoder_pair();
}

/*** end of file ***/
