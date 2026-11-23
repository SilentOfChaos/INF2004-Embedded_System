/*
 *  motion_ctl.h
 *  Buddy 2's motion maths: wheel-speed PID with feed-forward, soft-start
 *  ramp, the stall and encoder-fault watch, and the conversions between
 *  encoder counts, millimetres and angles.  Units: speeds mm/s, duty per
 *  mille (+ forward), angles centi-degrees (+ = left / counter-clockwise).
 *
 *  Pure C, no kernel headers: tested on the host (testing/host/).
 */

#ifndef MOTION_CTL_H
#define MOTION_CTL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int32_t integral_x1000;     /* I term, duty per mille x 1000 */
    int32_t prev_error_mmps;
    bool    b_have_prev;
} motion_pid_t;

void    motion_pid_reset(motion_pid_t * p_pid);
int32_t motion_pid_step(motion_pid_t * p_pid, int32_t target_mmps,
                        int32_t measured_mmps, uint32_t dt_ms);
int32_t motion_ramp(int32_t current_pm, int32_t wanted_pm);
int32_t motion_output_duty(int32_t pid_duty, int32_t target_mmps,
                           int32_t measured_mmps);

/* motion_counts_to_mm() of any int32_t count is below 2^30 mm in size
   (motion_ctl.c checks that pi x D / CPR < 1/2), so every distance made
   from the encoder counters fits 31 bits, signed (minimum-bit change). */
#define MOTION_DISTANCE_BITS    31u

/*
 * Stall and encoder-fault watch (24 Oct review, R-01; Change
 * Requirement 1).  A wheel is silent while it is driven at
 * CFG_MOTION_STALL_DUTY_PM or more and its encoder gives no count.
 * Once a wheel has been silent for CFG_MOTION_STALL_MS:
 *   both wheels silent            MOTION_STALL_MOTOR: a stall or jam,
 *                                 or both encoders dead
 *   one, while the other counts   MOTION_STALL_ENCODER: that wheel's
 *                                 encoder is dead, or that wheel is held
 *                                 (the two look the same: risk R5)
 *   one, the other not counting   MOTION_STALL_MOTOR
 * It is reported once, until no wheel is silent any more.  The state is
 * one word (minimum-bit change):
 *   bits  0-9   the left wheel's silent time, ms, 0..CFG_MOTION_STALL_MS
 *   bits 10-19  the right wheel's
 *   bit   20    reported
 */
typedef struct
{
    uint32_t bits;
} motion_stall_t;

typedef enum
{
    MOTION_STALL_NONE = 0,
    MOTION_STALL_MOTOR,
    MOTION_STALL_ENCODER
} motion_stall_out_t;

#define MOTION_STALL_LEFT       1u  /* *p_wheels: the silent wheels */
#define MOTION_STALL_RIGHT      2u

void               motion_stall_reset(motion_stall_t * p_watch);
motion_stall_out_t motion_stall_step(motion_stall_t * p_watch,
                                     int32_t duty_l, int32_t duty_r,
                                     int32_t delta_l, int32_t delta_r,
                                     uint32_t dt_ms, uint8_t * p_wheels);
uint32_t           motion_stall_silent_ms(motion_stall_t const * p_watch,
                                          bool b_right);

int32_t motion_counts_to_mm(int32_t counts);
int32_t motion_mm_to_counts(int32_t distance_mm);
int32_t motion_turn_counts(int32_t angle_cdeg);
int32_t motion_speed_mmps(int32_t delta_counts, uint32_t dt_us);
int32_t motion_heading_cdeg(int32_t count_l, int32_t count_r);

#endif /* MOTION_CTL_H */

/*** end of file ***/
