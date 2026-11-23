/*
 *  drv_lsm303.c
 *  GY-511 (LSM303DLHC) driver.  Register addresses and values are from
 *  the LSM303DLHC datasheet (DocID018771 Rev 2), section 7.
 *
 *  The accelerometer has no WHO_AM_I register; the identity check uses
 *  the magnetometer's IRA/IRB/IRC registers ("H43"), and the self-test
 *  reads accelerometer data instead (report 8.3).
 */

#include "../app_config.h"
#include "drv_i2c1.h"
#include "drv_lsm303.h"

/* Accelerometer registers (datasheet Table 17). */
#define LSM_CTRL_REG1_A     0x20u
#define LSM_CTRL_REG4_A     0x23u
#define LSM_OUT_X_L_A       0x28u
#define LSM_AUTO_INC        0x80u   /* SUB(7) = 1: address auto-increment */

/* Magnetometer registers (datasheet Table 17). */
#define LSM_CRA_REG_M       0x00u
#define LSM_CRB_REG_M       0x01u
#define LSM_MR_REG_M        0x02u
#define LSM_OUT_X_H_M       0x03u   /* order X, Z, Y, high byte first */
#define LSM_SR_REG_M        0x09u
#define LSM_IRA_REG_M       0x0Au
#define LSM_SR_DRDY         0x01u

/* Identification values (datasheet Tables 81-83). */
#define LSM_IRA_VALUE       0x48u
#define LSM_IRB_VALUE       0x34u
#define LSM_IRC_VALUE       0x33u

#define LSM_AXES_BYTES      6u
#define LSM_ID_BYTES        3u
#define LSM_12BIT_DIVISOR   16      /* 12-bit data, left-justified */
#define LSM_BYTE_SHIFT      8u

static int32_t lsm303_word(uint8_t high, uint8_t low);
static void    lsm303_to_car(int32_t sx, int32_t sy, int32_t sz,
                             int32_t * p_x, int32_t * p_y, int32_t * p_z);

/* Accelerometer 100 Hz +/-4 g, magnetometer 75 Hz continuous (NFR4). */
bool
lsm303_init (void)
{
    bool b_ok = i2c1_write_reg(CFG_IMU_ACC_ADDR, LSM_CTRL_REG1_A,
                               CFG_IMU_CTRL1_A);

    b_ok = b_ok && i2c1_write_reg(CFG_IMU_ACC_ADDR, LSM_CTRL_REG4_A,
                                  CFG_IMU_CTRL4_A);
    b_ok = b_ok && i2c1_write_reg(CFG_IMU_MAG_ADDR, LSM_CRA_REG_M,
                                  CFG_IMU_CRA_M);
    b_ok = b_ok && i2c1_write_reg(CFG_IMU_MAG_ADDR, LSM_CRB_REG_M,
                                  CFG_IMU_CRB_M);
    b_ok = b_ok && i2c1_write_reg(CFG_IMU_MAG_ADDR, LSM_MR_REG_M,
                                  CFG_IMU_MR_M);

    return (b_ok);
}

/* Self-test identity check: magnetometer IRA/IRB/IRC = 'H', '4', '3'. */
bool
lsm303_check_id (void)
{
    uint8_t id[LSM_ID_BYTES] = { 0u, 0u, 0u };
    bool    b_ok = i2c1_read_regs(CFG_IMU_MAG_ADDR, LSM_IRA_REG_M, id,
                                  LSM_ID_BYTES);

    return (b_ok && (LSM_IRA_VALUE == id[0]) && (LSM_IRB_VALUE == id[1]) &&
            (LSM_IRC_VALUE == id[2]));
}

/* Acceleration in mg, car frame. */
bool
lsm303_read_accel (lsm303_accel_t * p_accel)
{
    uint8_t raw[LSM_AXES_BYTES] = { 0u };
    bool    b_ok = i2c1_read_regs(CFG_IMU_ACC_ADDR,
                                  (uint8_t) (LSM_OUT_X_L_A | LSM_AUTO_INC),
                                  raw, LSM_AXES_BYTES);

    if (b_ok)
    {
        /* Little-endian (BLE = 0), 12-bit values in the top bits. */
        int32_t const sx = (lsm303_word(raw[1], raw[0]) / LSM_12BIT_DIVISOR)
                           * CFG_IMU_ACC_MG_PER_LSB;
        int32_t const sy = (lsm303_word(raw[3], raw[2]) / LSM_12BIT_DIVISOR)
                           * CFG_IMU_ACC_MG_PER_LSB;
        int32_t const sz = (lsm303_word(raw[5], raw[4]) / LSM_12BIT_DIVISOR)
                           * CFG_IMU_ACC_MG_PER_LSB;

        lsm303_to_car(sx, sy, sz, &p_accel->x_mg, &p_accel->y_mg,
                      &p_accel->z_mg);
    }

    return (b_ok);
}

/*
 * Magnetometer counts when a new sample is ready (75 Hz); *p_new tells
 * whether *p_mag was updated.  Hard-iron offsets are removed.
 */
bool
lsm303_read_mag (lsm303_mag_t * p_mag, bool * p_new)
{
    uint8_t status = 0u;
    bool    b_ok   = i2c1_read_regs(CFG_IMU_MAG_ADDR, LSM_SR_REG_M,
                                    &status, 1u);

    *p_new = false;

    if (b_ok && (0u != (status & LSM_SR_DRDY)))
    {
        uint8_t raw[LSM_AXES_BYTES] = { 0u };

        b_ok = i2c1_read_regs(CFG_IMU_MAG_ADDR, LSM_OUT_X_H_M, raw,
                              LSM_AXES_BYTES);

        if (b_ok)
        {
            /* Big-endian, order X, Z, Y (datasheet 7.2.4-7.2.6). */
            int32_t const sx = lsm303_word(raw[0], raw[1]) -
                               CFG_IMU_MAG_OFF_X;
            int32_t const sz = lsm303_word(raw[2], raw[3]) -
                               CFG_IMU_MAG_OFF_Z;
            int32_t const sy = lsm303_word(raw[4], raw[5]) -
                               CFG_IMU_MAG_OFF_Y;

            lsm303_to_car(sx, sy, sz, &p_mag->x_raw, &p_mag->y_raw,
                          &p_mag->z_raw);
            *p_new = true;
        }
    }

    return (b_ok);
}

/* Two's-complement 16-bit value from two bytes. */
static int32_t
lsm303_word (uint8_t high, uint8_t low)
{
    uint16_t const bits = (uint16_t) (((uint16_t) high << LSM_BYTE_SHIFT) |
                                      (uint16_t) low);

    /* Reinterpreting the 16 bits as signed is the documented format. */
    return ((int32_t) (int16_t) bits);
}

/* Sensor axes to car axes (CFG_IMU_SWAP_XY, CFG_IMU_INVERT_x). */
static void
lsm303_to_car (int32_t sx, int32_t sy, int32_t sz, int32_t * p_x,
               int32_t * p_y, int32_t * p_z)
{
    int32_t const x = (0 != CFG_IMU_SWAP_XY) ? sy : sx;
    int32_t const y = (0 != CFG_IMU_SWAP_XY) ? sx : sy;

    *p_x = (0 != CFG_IMU_INVERT_X) ? -x : x;
    *p_y = (0 != CFG_IMU_INVERT_Y) ? -y : y;
    *p_z = (0 != CFG_IMU_INVERT_Z) ? -sz : sz;
}

/*** end of file ***/
