/*
 *  drv_i2c1.h
 *  I2C1 master on GP2 (SDA) / GP3 (SCL), Grove 2 (report section 2).
 *  The kernel's I2C driver only supports I2C0, so the IMU task owns this
 *  small polled driver (TEAM.md section 3).  Every wait is bounded, so a
 *  missing or stuck sensor gives an error instead of a hang.
 */

#ifndef DRV_I2C1_H
#define DRV_I2C1_H

#include <stdbool.h>
#include <stdint.h>

void i2c1_init(uint32_t baud_hz);
bool i2c1_write_reg(uint8_t addr, uint8_t reg, uint8_t value);
bool i2c1_read_regs(uint8_t addr, uint8_t reg, uint8_t * p_buf,
                    uint32_t len);

#endif /* DRV_I2C1_H */

/*** end of file ***/
