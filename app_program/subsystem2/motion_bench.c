/*
 *  motion_bench.c
 *  Buddy 2's bench test: motors, encoders and the wheel-speed PID on the
 *  real car, with NOTHING else running.  Built only with:  make BENCH=1
 *
 *  In a BENCH=1 image usermain() starts only this task: no Navigation,
 *  Safety, Line, IMU, Scan, Command or Telemetry.  So no other buddy's
 *  code can turn, stop or brake the car, and the results are Motion's
 *  alone.  The watchdog is never armed.  Output is on the USB console
 *  (115200 8N1); press START (GP20) to begin each step.  Pressing START
 *  while the motors run stops them at once.
 *
 *  It uses the same code as the real Motion task: drv_motor.c (with the
 *  CFG_MOTOR_x_INVERT settings), drv_encoder.c (CFG_ENC_x_INVERT, the
 *  Encoder ISR), and motion_ctl.c (PID, output rule, ramp, geometry).
 *
 *  Steps (record the results in testing/subsystem2/TEST_LOG.md):
 *    1  encoders by hand            S2-U32 (encoder half), S2-U30
 *    2  each motor alone, in air    S2-U32 (motor half): right wheel?
 *                                   right direction? encoder follows?
 *    3  start-up duty and top speed CFG_MOTOR_MIN_DUTY_PM, feed-forward
 *    4  speed PID, in air           NFR18 (each wheel within 10%)
 *    5  on the floor: 500 mm forward, 90 deg left, 90 deg right,
 *       500 mm back                 S2-U34 to S2-U36 (measure with a ruler)
 */

#include "../app.h"
#include "../common/app_gpio.h"
#include "../common/app_time.h"
#include "../common/fixmath.h"
#include "../common/gpio_irq.h"
#include "drv_encoder.h"
#include "drv_motor.h"
#include "motion_bench.h"
#include "motion_ctl.h"

#if defined(APP_BENCH) && APP_BENCH

#define BENCH_TEST_PM       400     /* open-loop duty for step 2 */
#define BENCH_SPIN_MS       1500u
#define BENCH_MIN_COUNTS    10      /* counts that prove a wheel turned */
#define BENCH_RAMP_STEP_PM  20
#define BENCH_RAMP_HOLD_MS  200u
#define BENCH_HAND_MS       10000u
#define BENCH_PID_MS        3000u
#define BENCH_MOVE_MM       500
#define BENCH_TURN_CDEG     9000
#define BENCH_TIMEOUT_MS    6000u   /* a floor move that never ends */
#define BENCH_PRINT_EVERY   10u     /* PID lines: every 10 x 20 ms */

/* What step 1 found, so step 2 can tell a reversed motor from a
   reversed encoder. */
typedef struct
{
    bool b_enc_ok[2];       /* hand-turned forward counted up */
    bool b_enc_seen[2];     /* any count at all */
    bool b_motors_ok;       /* step 2 passed: safe to drive on the floor */
} bench_result_t;

static void    bench_task(INT stacd, void * p_exinf);
static bool    bench_pressed(void);
static void    bench_wait_start(char const * p_what);
static void    bench_counts(int32_t * p_l, int32_t * p_r);
static void    bench_step_hand(bench_result_t * p_res);
static void    bench_step_motors(bench_result_t * p_res);
static void    bench_step_start_duty(void);
static void    bench_step_pid(int32_t target_mmps);
static bool    bench_move(char const * p_name, char const * p_check,
                          int32_t dir_l, int32_t dir_r, int32_t counts,
                          int32_t speed_mmps);
static int32_t bench_pct(int32_t value, int32_t target);

static int32_t g_total_l = 0;       /* encoder totals since boot */
static int32_t g_total_r = 0;

ER
motion_bench_start (void)
{
    ID const tid = app_start_task_stack((FP) bench_task, CFG_PRI_MOTION,
                                        APP_CORE0, CFG_STACK_DEFAULT,
                                        "motion_bench");

    return ((tid < E_OK) ? (ER) tid : E_OK);
}

/* Runs on core 0: the encoder interrupts are enabled from here. */
static void
bench_task (INT stacd, void * p_exinf)
{
    bench_result_t res = { { false, false }, { false, false }, false };

    (void) stacd;
    (void) p_exinf;

    motor_init();
    motor_set(0, 0);
    encoder_init();
    CHECK(gpio_irq_enable_core0(), "gpio_irq_enable_core0");
    app_gpio_input(CFG_PIN_START, (0 != CFG_BUTTON_ACTIVE_LOW)
                                  ? APP_GPIO_PULL_UP : APP_GPIO_PULL_DOWN);
    bench_counts(NULL, NULL);

    tm_printf((UB *) "\n=== BUDDY 2 MOTION BENCH (make BENCH=1) ===\n"
              "Only Motion runs: nothing else can steer or brake.\n"
              "Settings: MOTOR_L_INVERT %d  MOTOR_R_INVERT %d  "
              "ENC_L_INVERT %d  ENC_R_INVERT %d  CPR %d\n"
              "Press START during any motor step to stop at once.\n",
              (INT) CFG_MOTOR_L_INVERT, (INT) CFG_MOTOR_R_INVERT,
              (INT) CFG_ENC_L_INVERT, (INT) CFG_ENC_R_INVERT,
              (INT) CFG_ENC_COUNTS_PER_REV);

    for (;;)
    {
        bench_wait_start("STEP 1: motors off. Turn each wheel FORWARD by hand");
        bench_step_hand(&res);

        bench_wait_start("STEP 2: LIFT THE CAR (wheels off the ground)");
        bench_step_motors(&res);

        bench_wait_start("STEP 3: start-up duty and top speed (in the air)");
        bench_step_start_duty();

        bench_wait_start("STEP 4: speed PID at 150 mm/s (in the air)");
        bench_step_pid(150);
        bench_wait_start("STEP 4b: speed PID at 250 mm/s (in the air)");
        bench_step_pid(250);

        if (!res.b_motors_ok)
        {
            tm_printf((UB *) "\nSTEP 5 (floor) SKIPPED: fix what step 1 "
                      "and step 2 reported in app_config.h, rebuild, and "
                      "run the bench again.\n");
        }
        else
        {
            bench_wait_start("STEP 5: PUT THE CAR ON THE FLOOR, 1 m clear "
                             "ahead");
            if (bench_move("forward 500 mm", "MEASURE: 475-525 mm, straight",
                           1, 1, motion_mm_to_counts(BENCH_MOVE_MM),
                           CFG_MOTION_MOVE_MMPS))
            {
                bench_wait_start("STEP 5b: turn LEFT 90 deg on the spot");
                (void) bench_move("left 90 deg", "MEASURE: 85-95 deg (S2-U36)",
                                  -1, 1, motion_turn_counts(BENCH_TURN_CDEG),
                                  CFG_MOTION_TURN_MMPS);
                bench_wait_start("STEP 5c: turn RIGHT 90 deg on the spot");
                (void) bench_move("right 90 deg",
                                  "MEASURE: 85-95 deg (S2-U36)", 1, -1,
                                  motion_turn_counts(BENCH_TURN_CDEG),
                                  CFG_MOTION_TURN_MMPS);
                bench_wait_start("STEP 5d: back 500 mm");
                (void) bench_move("back 500 mm", "MEASURE: 475-525 mm (S2-U35)",
                                  -1, -1, motion_mm_to_counts(BENCH_MOVE_MM),
                                  CFG_MOTION_MOVE_MMPS);
            }
        }

        tm_printf((UB *) "\n=== bench done; press START to run it again ===\n");
    }
}

/* START is down (debounced by the callers' timing). */
static bool
bench_pressed (void)
{
    bool const b_level = app_gpio_read(CFG_PIN_START);

    return (0 != CFG_BUTTON_ACTIVE_LOW) ? !b_level : b_level;
}

/* Motors off; wait for a press and its release. */
static void
bench_wait_start (char const * p_what)
{
    motor_set(0, 0);
    tm_printf((UB *) "\n>>> %s\n>>> press START\n", p_what);

    while (!bench_pressed())
    {
        (void) tk_dly_tsk(20);
    }

    while (bench_pressed())
    {
        (void) tk_dly_tsk(20);
    }

    (void) tk_dly_tsk(300);     /* let the hand leave the button */
}

/* Both totals; the change since the last call into *p_l / *p_r. */
static void
bench_counts (int32_t * p_l, int32_t * p_r)
{
    int32_t const old_l = g_total_l;
    int32_t const old_r = g_total_r;

    encoder_add_counts(&g_total_l, &g_total_r);

    if (NULL != p_l)
    {
        *p_l = g_total_l - old_l;
    }

    if (NULL != p_r)
    {
        *p_r = g_total_r - old_r;
    }
}

/* ---- Step 1: encoders by hand -------------------------------------- */
static void
bench_step_hand (bench_result_t * p_res)
{
    int32_t        sum[2]  = { 0, 0 };
    uint32_t const start   = app_time_ms();
    char const *   name[2] = { "LEFT ", "RIGHT" };

    tm_printf((UB *) "10 s: roll each wheel FORWARD (the way the car drives)"
              " about one turn.\n");

    while ((app_time_ms() - start) < BENCH_HAND_MS)
    {
        int32_t dl = 0;
        int32_t dr = 0;

        (void) tk_dly_tsk(250);
        bench_counts(&dl, &dr);
        sum[0] += dl;
        sum[1] += dr;

        if ((0 != dl) || (0 != dr))
        {
            tm_printf((UB *) "  counts  L %6d   R %6d\n",
                      (INT) sum[0], (INT) sum[1]);
        }
    }

    for (uint32_t w = 0u; w < 2u; w++)
    {
        p_res->b_enc_seen[w] = (fixmath_abs(sum[w]) >= BENCH_MIN_COUNTS);
        p_res->b_enc_ok[w]   = (sum[w] >= BENCH_MIN_COUNTS);

        if (!p_res->b_enc_seen[w])
        {
            tm_printf((UB *) "  %s encoder: NO COUNTS. Check its cable "
                      "(GP%d/GP%d), power, and that it is plugged into the "
                      "right Grove port.\n", (UB *) name[w],
                      (INT) ((0u == w) ? CFG_PIN_ENC_L_A : CFG_PIN_ENC_R_A),
                      (INT) ((0u == w) ? CFG_PIN_ENC_L_B : CFG_PIN_ENC_R_B));
        }
        else if (!p_res->b_enc_ok[w])
        {
            tm_printf((UB *) "  %s encoder: counts DOWN when rolled "
                      "forward -> set CFG_ENC_%s_INVERT to %d\n",
                      (UB *) name[w], (UB *) ((0u == w) ? "L" : "R"),
                      (INT) ((0 != ((0u == w) ? CFG_ENC_L_INVERT
                                              : CFG_ENC_R_INVERT)) ? 0 : 1));
        }
        else
        {
            tm_printf((UB *) "  %s encoder: OK (%d counts; one wheel turn "
                      "should be about %d)\n", (UB *) name[w], (INT) sum[w],
                      (INT) CFG_ENC_COUNTS_PER_REV);
        }
    }
}

/* ---- Step 2: each motor alone, both directions, open loop ---------- */
static void
bench_step_motors (bench_result_t * p_res)
{
    static struct
    {
        char const * p_name;
        int32_t      duty_l;
        int32_t      duty_r;
        uint32_t     wheel;     /* 0 left, 1 right */
        int32_t      dir;
    } const runs[4] =
    {
        { "LEFT  forward ",  BENCH_TEST_PM, 0, 0u,  1 },
        { "LEFT  backward", -BENCH_TEST_PM, 0, 0u, -1 },
        { "RIGHT forward ", 0,  BENCH_TEST_PM, 1u,  1 },
        { "RIGHT backward", 0, -BENCH_TEST_PM, 1u, -1 },
    };

    tm_printf((UB *) "Each motor runs 1.5 s at %d per mille. WATCH which "
              "wheel turns and which way.\n", (INT) BENCH_TEST_PM);
    p_res->b_motors_ok = true;

    for (uint32_t i = 0u; i < 4u; i++)
    {
        int32_t  dl = 0;
        int32_t  dr = 0;
        uint32_t t0 = 0u;

        (void) tk_dly_tsk(700);
        bench_counts(NULL, NULL);
        tm_printf((UB *) "  %s ... ", (UB *) runs[i].p_name);
        motor_set(runs[i].duty_l, runs[i].duty_r);
        t0 = app_time_ms();

        while (((app_time_ms() - t0) < BENCH_SPIN_MS) && !bench_pressed())
        {
            (void) tk_dly_tsk(20);
        }

        motor_set(0, 0);
        (void) tk_dly_tsk(300);     /* let it coast to a stop */
        bench_counts(&dl, &dr);

        {
            int32_t const own   = (0u == runs[i].wheel) ? dl : dr;
            int32_t const other = (0u == runs[i].wheel) ? dr : dl;
            bool const    b_enc = p_res->b_enc_ok[runs[i].wheel];

            tm_printf((UB *) "counts L %6d  R %6d  ", (INT) dl, (INT) dr);

            if ((fixmath_abs(own) < BENCH_MIN_COUNTS) &&
                (fixmath_abs(other) >= BENCH_MIN_COUNTS))
            {
                p_res->b_motors_ok = false;
                tm_printf((UB *) "WRONG WHEEL: the other wheel moved -> "
                          "left and right motor cables are swapped\n");
            }
            else if (fixmath_abs(own) < BENCH_MIN_COUNTS)
            {
                p_res->b_motors_ok = false;
                tm_printf((UB *) "NO COUNTS: if the wheel spun, its "
                          "encoder is not counting; if it did not spin, "
                          "check motor cable, battery and power switch\n");
            }
            else if (!p_res->b_enc_seen[runs[i].wheel])
            {
                p_res->b_motors_ok = false;
                tm_printf((UB *) "encoder direction unknown (step 1 saw no "
                          "counts): check by eye that it turned %s\n",
                          (UB *) ((runs[i].dir > 0) ? "FORWARD"
                                                    : "BACKWARD"));
            }
            else
            {
                /* The wheel's real direction: the counts' sign, flipped
                   if step 1 found the encoder counting backwards. */
                int32_t const real = b_enc ? own : -own;

                if ((real * runs[i].dir) > 0)
                {
                    tm_printf((UB *) "OK: turned %s\n",
                              (UB *) ((runs[i].dir > 0) ? "FORWARD"
                                                        : "BACKWARD"));
                }
                else
                {
                    p_res->b_motors_ok = false;
                    tm_printf((UB *) "MOTOR REVERSED -> set "
                              "CFG_MOTOR_%s_INVERT to %d (or swap that "
                              "motor's two wires)\n",
                              (UB *) ((0u == runs[i].wheel) ? "L" : "R"),
                              (INT) ((0 != ((0u == runs[i].wheel)
                                            ? CFG_MOTOR_L_INVERT
                                            : CFG_MOTOR_R_INVERT)) ? 0 : 1));
                }
            }

            if (!b_enc)
            {
                p_res->b_motors_ok = false;     /* step 1's fix first */
            }
        }
    }
}

/* ---- Step 3: smallest duty that turns each wheel; top speed -------- */
static void
bench_step_start_duty (void)
{
    for (uint32_t w = 0u; w < 2u; w++)
    {
        int32_t found = 0;

        for (int32_t duty = BENCH_RAMP_STEP_PM;
             (duty <= CFG_MOTOR_MAX_DUTY_PM) && (0 == found) &&
             !bench_pressed();
             duty += BENCH_RAMP_STEP_PM)
        {
            int32_t dl = 0;
            int32_t dr = 0;

            bench_counts(NULL, NULL);
            motor_set((0u == w) ? duty : 0, (0u == w) ? 0 : duty);
            (void) tk_dly_tsk(BENCH_RAMP_HOLD_MS);
            bench_counts(&dl, &dr);

            if (fixmath_abs((0u == w) ? dl : dr) >= 3)
            {
                found = duty;
            }
        }

        motor_set(0, 0);
        (void) tk_dly_tsk(500);
        tm_printf((UB *) "  %s wheel starts at %d per mille (config "
                  "CFG_MOTOR_MIN_DUTY_PM = %d)\n",
                  (UB *) ((0u == w) ? "LEFT " : "RIGHT"), (INT) found,
                  (INT) CFG_MOTOR_MIN_DUTY_PM);
    }

    /* Top speed, both wheels at full duty for 1 s (in the air). */
    {
        int32_t dl = 0;
        int32_t dr = 0;

        motor_set(CFG_MOTOR_MAX_DUTY_PM, CFG_MOTOR_MAX_DUTY_PM);
        (void) tk_dly_tsk(500);
        bench_counts(NULL, NULL);
        (void) tk_dly_tsk(1000);
        bench_counts(&dl, &dr);
        motor_set(0, 0);
        tm_printf((UB *) "  top speed in the air: L %d  R %d mm/s "
                  "(the PID's targets must stay well below this)\n",
                  (INT) motion_speed_mmps(dl, 1000000u),
                  (INT) motion_speed_mmps(dr, 1000000u));
    }
}

/* One 20 ms cycle of the real speed loop for both wheels, as
   motion_task.c runs it.  Returns false if START was pressed. */
static bool
bench_cycle (motion_pid_t * p_pid, int32_t * p_duty, int32_t target_l,
             int32_t target_r, int32_t * p_speed, int32_t * p_dl,
             int32_t * p_dr, uint32_t * p_last_us)
{
    uint32_t const now = app_time_us();
    uint32_t const dt  = now - *p_last_us;
    uint32_t const ms  = (dt + 500u) / 1000u;

    *p_last_us = now;
    bench_counts(p_dl, p_dr);
    p_speed[0] = motion_speed_mmps(*p_dl, dt);
    p_speed[1] = motion_speed_mmps(*p_dr, dt);
    p_duty[0]  = motion_ramp(p_duty[0], motion_output_duty(
                     motion_pid_step(&p_pid[0], target_l, p_speed[0], ms),
                     target_l, p_speed[0]));
    p_duty[1]  = motion_ramp(p_duty[1], motion_output_duty(
                     motion_pid_step(&p_pid[1], target_r, p_speed[1], ms),
                     target_r, p_speed[1]));
    motor_set(p_duty[0], p_duty[1]);
    (void) tk_dly_tsk(CFG_PERIOD_MOTION_MS);

    return (!bench_pressed());
}

/* ---- Step 4: wheel-speed PID, in the air --------------------------- */
static void
bench_step_pid (int32_t target_mmps)
{
    motion_pid_t pid[2];
    int32_t      duty[2]  = { 0, 0 };
    int32_t      speed[2] = { 0, 0 };
    int32_t      sum[2]   = { 0, 0 };
    int32_t      n        = 0;
    uint32_t     last_us  = app_time_us();
    uint32_t     cycle    = 0u;
    bool         b_run    = true;

    motion_pid_reset(&pid[0]);
    motion_pid_reset(&pid[1]);
    bench_counts(NULL, NULL);
    tm_printf((UB *) "  target %d mm/s; columns: time, speed L R (mm/s), "
              "duty L R (per mille)\n", (INT) target_mmps);

    while (b_run && ((cycle * CFG_PERIOD_MOTION_MS) < BENCH_PID_MS))
    {
        int32_t dl = 0;
        int32_t dr = 0;

        b_run = bench_cycle(pid, duty, target_mmps, target_mmps, speed,
                            &dl, &dr, &last_us);
        cycle++;

        if ((cycle * CFG_PERIOD_MOTION_MS) >= 1000u)    /* settled */
        {
            sum[0] += speed[0];
            sum[1] += speed[1];
            n++;
        }

        if (0u == (cycle % BENCH_PRINT_EVERY))
        {
            tm_printf((UB *) "  %4d ms  %5d %5d   %5d %5d\n",
                      (INT) (cycle * CFG_PERIOD_MOTION_MS),
                      (INT) speed[0], (INT) speed[1],
                      (INT) duty[0], (INT) duty[1]);
        }
    }

    motor_set(0, 0);

    if (n > 0)
    {
        int32_t const avg_l = sum[0] / n;
        int32_t const avg_r = sum[1] / n;
        int32_t const err_l = bench_pct(avg_l - target_mmps, target_mmps);
        int32_t const err_r = bench_pct(avg_r - target_mmps, target_mmps);

        tm_printf((UB *) "  average after 1 s: L %d (%d%%)  R %d (%d%%)  -> "
                  "%s\n", (INT) avg_l, (INT) err_l, (INT) avg_r, (INT) err_r,
                  (UB *) (((fixmath_abs(err_l) <= 10) &&
                           (fixmath_abs(err_r) <= 10))
                          ? "PASS (NFR18: within 10%)"
                          : "FAIL: if a speed is negative or runs away, "
                            "fix step 2 first; else tune the PID"));
    }
}

/* ---- Step 5: a counted move or turn on the floor ------------------- */
static bool
bench_move (char const * p_name, char const * p_check, int32_t dir_l,
            int32_t dir_r, int32_t counts, int32_t speed_mmps)
{
    motion_pid_t   pid[2];
    motion_stall_t watch;
    int32_t        duty[2]  = { 0, 0 };
    int32_t        speed[2] = { 0, 0 };
    int32_t        moved[2] = { 0, 0 };
    uint32_t       last_us  = app_time_us();
    uint32_t const t0       = app_time_ms();
    bool           b_run    = true;
    bool           b_done   = false;
    char const *   p_why    = "";

    motion_pid_reset(&pid[0]);
    motion_pid_reset(&pid[1]);
    motion_stall_reset(&watch);
    bench_counts(NULL, NULL);
    tm_printf((UB *) "  %s: %d counts per wheel at %d mm/s\n",
              (UB *) p_name, (INT) counts, (INT) speed_mmps);

    while (b_run && !b_done)
    {
        int32_t       dl       = 0;
        int32_t       dr       = 0;
        int32_t const progress = (moved[0] + moved[1]) / 2;
        int32_t       target   = speed_mmps;
        uint8_t       wheels   = 0u;
        int32_t const old_l    = duty[0];
        int32_t const old_r    = duty[1];

        if ((counts - progress) < motion_mm_to_counts(CFG_MOTION_SLOWDOWN_MM))
        {
            target = target / 2;        /* as motion_task.c does */
        }

        b_run = bench_cycle(pid, duty, dir_l * target, dir_r * target, speed,
                            &dl, &dr, &last_us);
        moved[0] += fixmath_abs(dl);
        moved[1] += fixmath_abs(dr);
        b_done = (((moved[0] + moved[1]) / 2) >= counts);

        if (MOTION_STALL_NONE != motion_stall_step(&watch, old_l, old_r, dl,
                                                   dr, CFG_PERIOD_MOTION_MS,
                                                   &wheels))
        {
            p_why = "STOPPED: a driven wheel gave no counts for 1 s "
                    "(stall or dead encoder)";
            b_run = false;
        }
        else if ((app_time_ms() - t0) > BENCH_TIMEOUT_MS)
        {
            p_why = "STOPPED: took over 6 s";
            b_run = false;
        }
        else if (!b_run)
        {
            p_why = "STOPPED: START pressed";
        }
        else
        {
            /* keep going */
        }
    }

    motor_set(0, 0);
    (void) tk_dly_tsk(500);
    bench_counts(NULL, NULL);
    tm_printf((UB *) "  %s  wheels moved L %d  R %d counts (%d mm, %d mm); "
              "odometry heading now %d cdeg\n",
              (UB *) (b_done ? "DONE." : p_why), (INT) moved[0],
              (INT) moved[1], (INT) motion_counts_to_mm(moved[0]),
              (INT) motion_counts_to_mm(moved[1]),
              (INT) motion_heading_cdeg(g_total_l, g_total_r));
    tm_printf((UB *) "  %s\n", (UB *) p_check);

    return (b_done);
}

/* value / target in percent. */
static int32_t
bench_pct (int32_t value, int32_t target)
{
    return ((0 != target) ? ((value * 100) / target) : 0);
}

#endif /* APP_BENCH */

/*** end of file ***/
