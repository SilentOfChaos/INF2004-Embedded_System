/*
 *  imu_calc.h
 *  Buddy 4's IMU maths: tilt from the accelerometer, tilt-compensated
 *  compass heading, the hump detector (report 6.4: roll above a
 *  threshold = ON HUMP, level again = report the peak as a height) and
 *  the motion classifier (FR17).
 *
 *  Car frame: x forward, y left, z up.  Roll > 0 = left side up,
 *  heading and turn rate > 0 = turning left (counter-clockwise).
 *  Units: mg, centi-degrees, mm.
 *
 *  Pure C, no kernel headers: tested on the host (testing/host/).
 */

#ifndef IMU_CALC_H
#define IMU_CALC_H

#include <stdbool.h>
#include <stdint.h>

#include "../app_config.h"
#include "imu_msgs.h"

/* One accelerometer sample (report 6.4; D-42, D-81). */
typedef enum
{
    IMU_SAMPLE_OK = 0,          /* plausible: use it */
    IMU_SAMPLE_BAD,             /* read failed, or |a| below
                                   CFG_IMU_MIN_G_MG: invalid data */
    IMU_SAMPLE_OVERRANGE        /* |a| above CFG_IMU_MAX_G_MG: a jolt */
} imu_sample_t;

typedef enum
{
    IMU_HUMP_EVT_NONE = 0,
    IMU_HUMP_EVT_STARTED,
    IMU_HUMP_EVT_ENDED
} imu_hump_evt_t;

typedef struct
{
    bool     b_on_hump;
    uint32_t above_ms;          /* time above the ON threshold */
    int32_t  peak_cdeg;         /* largest |roll| while on the hump */
} imu_hump_t;

/* Lift / overturn detector (replaces collision detection). */
typedef enum
{
    IMU_LIFT_NONE = 0,
    IMU_LIFT_LIFTED,            /* picked up: detail 1 */
    IMU_LIFT_OVERTURNED         /* tipped past CFG_IMU_OVERTURN_COS_PM:
                                   detail 2 */
} imu_lift_evt_t;

typedef struct
{
    int32_t  up_x_mg;           /* gravity reaction at calibration */
    int32_t  up_y_mg;
    int32_t  up_z_mg;
    int32_t  up_mg;             /* its length, about 1000 */
    uint32_t lift_run;          /* samples in a row accelerating up */
    uint32_t flip_run;          /* samples in a row past the angle */
    int32_t  last_up_mg;        /* last acceleration along up, minus 1 g */
} imu_lift_t;

typedef struct
{
    int32_t  total_mg[CFG_IMU_CLASS_WINDOW];    /* |a| per sample */
    int32_t  forward_mg[CFG_IMU_CLASS_WINDOW];  /* x acceleration */
    int32_t  roll_cdeg[CFG_IMU_CLASS_WINDOW];   /* roll per sample */
    uint32_t index;                             /* next slot to write */
    uint32_t count;
} imu_classifier_t;

void        imu_calc_tilt(int32_t ax_mg, int32_t ay_mg, int32_t az_mg,
                          int32_t * p_roll_cdeg, int32_t * p_pitch_cdeg);
int32_t     imu_calc_heading(int32_t mx, int32_t my, int32_t mz,
                             int32_t roll_cdeg, int32_t pitch_cdeg);
uint32_t    imu_calc_norm(int32_t x_value, int32_t y_value,
                          int32_t z_value);
int32_t     imu_calc_hump_height(int32_t peak_roll_cdeg);
imu_sample_t imu_calc_check_sample(bool b_read_ok, uint32_t total_mg);
bool        imu_calc_cal_jolt(uint32_t total_mg);

void           imu_hump_init(imu_hump_t * p_hump);
imu_hump_evt_t imu_hump_step(imu_hump_t * p_hump, int32_t roll_cdeg,
                             uint32_t dt_ms);

void           imu_lift_init(imu_lift_t * p_lift, int32_t up_x_mg,
                             int32_t up_y_mg, int32_t up_z_mg);
imu_lift_evt_t imu_lift_step(imu_lift_t * p_lift, int32_t ax_mg,
                             int32_t ay_mg, int32_t az_mg);
bool           imu_lift_clear(imu_lift_t const * p_lift);

void        imu_classifier_init(imu_classifier_t * p_cls);
imu_class_t imu_classifier_step(imu_classifier_t * p_cls,
                                int32_t forward_mg, int32_t lateral_mg,
                                int32_t total_mg, int32_t turn_rate_cdps,
                                int32_t roll_cdeg, bool b_on_hump);

#endif /* IMU_CALC_H */

/*** end of file ***/
