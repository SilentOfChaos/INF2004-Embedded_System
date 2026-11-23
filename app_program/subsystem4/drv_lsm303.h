/*
 *  drv_lsm303.h
 *  GY-511 (LSM303DLHC) accelerometer and magnetometer on I2C1.
 *  Called only by the IMU task.  Values are returned in the car frame
 *  (x forward, y left, z up) using the mounting settings in
 *  app_config.h.
 */

#ifndef DRV_LSM303_H
#define DRV_LSM303_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int32_t x_mg;
    int32_t y_mg;
    int32_t z_mg;
} lsm303_accel_t;

typedef struct
{
    int32_t x_raw;              /* magnetometer counts, offsets removed */
    int32_t y_raw;
    int32_t z_raw;
} lsm303_mag_t;

bool lsm303_init(void);
bool lsm303_check_id(void);
bool lsm303_read_accel(lsm303_accel_t * p_accel);
bool lsm303_read_mag(lsm303_mag_t * p_mag, bool * p_new);

#endif /* DRV_LSM303_H */

/*** end of file ***/
