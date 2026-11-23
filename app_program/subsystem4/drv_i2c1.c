/*
 *  drv_i2c1.c
 *  Polled I2C1 master (RP2040 DW_apb_i2c).  Register sequence and timing
 *  follow the Pico SDK's hardware_i2c driver (i2c_set_baudrate,
 *  i2c_write/read_blocking), with a time limit on every wait.
 */

#include "../app.h"
#include "../common/app_time.h"
#include "../common/hw_regs.h"
#include "drv_i2c1.h"

#define I2C1_REG(offset)        (HW_I2C1_BASE + (offset))
#define I2C1_TIMEOUT_US         2000u   /* per byte; 400 kHz byte = 23 us */
#define I2C1_HOLD_NS_X3         3u      /* 300 ns SDA hold, as 3e-7 s */
#define I2C1_HOLD_DIV           10000000u
#define I2C1_SPKLEN_MIN_LCNT    16u
#define I2C1_LCNT_NUM           3u      /* low period 3/5 of the cycle */
#define I2C1_LCNT_DEN           5u
#define I2C1_DATA_MASK          0xFFu

static void i2c1_set_target(uint8_t addr);
static bool i2c1_wait_bits(uint32_t offset, uint32_t bits);
static bool i2c1_send(uint32_t word, bool b_stop);
static void i2c1_finish_abort(void);
static void i2c1_bus_recover(void);
static void i2c1_line(uint32_t pin, bool b_release);
static void i2c1_bit_delay(void);

#define I2C1_RECOVER_CLOCKS     9u      /* one byte + ACK (I2C spec 3.1.16) */
#define I2C1_RECOVER_HALF_US    5u      /* 100 kHz bit-banged clock */

/*
 * Bring I2C1 out of reset, set fast mode at baud_hz and connect GP2/GP3.
 * The GY-511 board has its own pull-ups; the internal pull-ups are also
 * enabled, as the Pico SDK examples do.
 */
void
i2c1_init (uint32_t baud_hz)
{
    uint32_t const period = (APP_CLK_HZ + (baud_hz / 2u)) / baud_hz;
    uint32_t const lcnt   = (period * I2C1_LCNT_NUM) / I2C1_LCNT_DEN;
    uint32_t const hcnt   = period - lcnt;
    uint32_t const hold   = ((APP_CLK_HZ * I2C1_HOLD_NS_X3) / I2C1_HOLD_DIV)
                            + 1u;
    uint32_t       sda    = 0u;

    /* Free the bus first: after RST the GY-511 may still hold SDA low. */
    i2c1_bus_recover();

    clr_w(HW_RESETS_RESET, HW_RESETS_I2C1);

    while (0u == (in_w(HW_RESETS_DONE) & HW_RESETS_I2C1))
    {
        /* Wait for I2C1 to leave reset (a few cycles). */
    }

    out_w(I2C1_REG(HW_I2C_ENABLE), 0u);
    out_w(I2C1_REG(HW_I2C_CON), (HW_I2C_CON_MASTER_MODE |
                                 HW_I2C_CON_SPEED_FAST |
                                 HW_I2C_CON_RESTART_EN |
                                 HW_I2C_CON_SLAVE_DISABLE |
                                 HW_I2C_CON_TX_EMPTY_CTRL));
    out_w(I2C1_REG(HW_I2C_FS_SCL_HCNT), hcnt);
    out_w(I2C1_REG(HW_I2C_FS_SCL_LCNT), lcnt);
    out_w(I2C1_REG(HW_I2C_FS_SPKLEN),
          (lcnt < I2C1_SPKLEN_MIN_LCNT) ? 1u : (lcnt / I2C1_SPKLEN_MIN_LCNT));
    sda = in_w(I2C1_REG(HW_I2C_SDA_HOLD)) & ~HW_I2C_SDA_TX_HOLD_MASK;
    out_w(I2C1_REG(HW_I2C_SDA_HOLD), (sda | hold));
    out_w(I2C1_REG(HW_I2C_ENABLE), 1u);

    clr_w(GPIO(CFG_PIN_IMU_SDA), (HW_PAD_OD | HW_PAD_PDE));
    clr_w(GPIO(CFG_PIN_IMU_SCL), (HW_PAD_OD | HW_PAD_PDE));
    set_w(GPIO(CFG_PIN_IMU_SDA), (HW_PAD_IE | HW_PAD_PUE | HW_PAD_SCHMITT));
    set_w(GPIO(CFG_PIN_IMU_SCL), (HW_PAD_IE | HW_PAD_PUE | HW_PAD_SCHMITT));
    out_w(HW_GPIO_CTRL(CFG_PIN_IMU_SDA), HW_FUNCSEL_I2C);
    out_w(HW_GPIO_CTRL(CFG_PIN_IMU_SCL), HW_FUNCSEL_I2C);
}

/* Write one register: [reg, value] then STOP. */
bool
i2c1_write_reg (uint8_t addr, uint8_t reg, uint8_t value)
{
    bool b_ok = false;

    i2c1_set_target(addr);
    b_ok = i2c1_send((uint32_t) reg, false);

    if (b_ok)
    {
        b_ok = i2c1_send((uint32_t) value, true);
    }

    return (b_ok);
}

/* Read len registers from reg: [reg], repeated START, read, STOP. */
bool
i2c1_read_regs (uint8_t addr, uint8_t reg, uint8_t * p_buf, uint32_t len)
{
    bool b_ok = ((NULL != p_buf) && (len > 0u));

    if (b_ok)
    {
        i2c1_set_target(addr);
        b_ok = i2c1_send((uint32_t) reg, false);
    }

    for (uint32_t idx = 0u; b_ok && (idx < len); idx++)
    {
        uint32_t word = HW_I2C_DATA_CMD_READ;

        if (0u == idx)
        {
            word |= HW_I2C_DATA_CMD_RESTART;
        }

        if ((len - 1u) == idx)
        {
            word |= HW_I2C_DATA_CMD_STOP;
        }

        out_w(I2C1_REG(HW_I2C_DATA_CMD), word);
        b_ok = i2c1_wait_bits(HW_I2C_RXFLR, 0xFFu);

        if (b_ok)
        {
            /* The data byte is the low 8 bits of DATA_CMD. */
            p_buf[idx] = (uint8_t) (in_w(I2C1_REG(HW_I2C_DATA_CMD)) &
                                    I2C1_DATA_MASK);
        }
    }

    if (!b_ok)
    {
        i2c1_finish_abort();
    }

    return (b_ok);
}

/*
 * I2C bus recovery (I2C-bus spec UM10204 section 3.1.16).
 *
 * The RST button (RUN pin) resets only the RP2040.  The GY-511 stays
 * powered, so if RST lands while it is sending a byte it keeps waiting
 * for the rest of the clock pulses and holds SDA low.  The I2C block
 * then can never START: identification fails, the IMU never becomes
 * healthy, SELF-TEST fails and Safety latches FAULT (the beeping), and
 * RESET cannot clear it because the cause is still there.
 *
 * Fix: before handing the pins to I2C1, drive SCL as an open-drain
 * output by hand and clock up to 9 pulses until the sensor lets SDA go,
 * then send a STOP so it is back to idle.  Lines are only ever pulled
 * low or released (never driven high), so nothing fights the sensor.
 */
static void
i2c1_bus_recover (void)
{
    uint32_t const sda_bit = (1u << CFG_PIN_IMU_SDA);

    /* Both lines: SIO function, input with pull-up, output value 0. */
    out_w(HW_SIO_GPIO_OUT_CLR, (sda_bit | (1u << CFG_PIN_IMU_SCL)));
    i2c1_line(CFG_PIN_IMU_SDA, true);
    i2c1_line(CFG_PIN_IMU_SCL, true);
    clr_w(GPIO(CFG_PIN_IMU_SDA), (HW_PAD_OD | HW_PAD_PDE));
    clr_w(GPIO(CFG_PIN_IMU_SCL), (HW_PAD_OD | HW_PAD_PDE));
    set_w(GPIO(CFG_PIN_IMU_SDA), (HW_PAD_IE | HW_PAD_PUE | HW_PAD_SCHMITT));
    set_w(GPIO(CFG_PIN_IMU_SCL), (HW_PAD_IE | HW_PAD_PUE | HW_PAD_SCHMITT));
    out_w(HW_GPIO_CTRL(CFG_PIN_IMU_SDA), HW_FUNCSEL_SIO);
    out_w(HW_GPIO_CTRL(CFG_PIN_IMU_SCL), HW_FUNCSEL_SIO);
    i2c1_bit_delay();

    for (uint32_t clk = 0u;
         (clk < I2C1_RECOVER_CLOCKS) &&
         (0u == (in_w(HW_SIO_GPIO_IN) & sda_bit));
         clk++)
    {
        i2c1_line(CFG_PIN_IMU_SCL, false);      /* SCL low */
        i2c1_bit_delay();
        i2c1_line(CFG_PIN_IMU_SCL, true);       /* SCL high */
        i2c1_bit_delay();
    }

    /* STOP: SDA low while SCL is low, then SCL high, then SDA high. */
    i2c1_line(CFG_PIN_IMU_SCL, false);
    i2c1_bit_delay();
    i2c1_line(CFG_PIN_IMU_SDA, false);
    i2c1_bit_delay();
    i2c1_line(CFG_PIN_IMU_SCL, true);
    i2c1_bit_delay();
    i2c1_line(CFG_PIN_IMU_SDA, true);
    i2c1_bit_delay();
}

/* Open-drain line: release (pull-up makes it high) or pull low. */
static void
i2c1_line (uint32_t pin, bool b_release)
{
    if (b_release)
    {
        out_w(HW_SIO_GPIO_OE_CLR, (1u << pin));
    }
    else
    {
        out_w(HW_SIO_GPIO_OE_SET, (1u << pin));
    }
}

/* Half a 100 kHz clock period. */
static void
i2c1_bit_delay (void)
{
    uint32_t const start = app_time_us();

    while ((app_time_us() - start) < I2C1_RECOVER_HALF_US)
    {
        /* Busy-wait: a few microseconds, once per recovery. */
    }
}

/* Set the 7-bit target address (only possible while disabled). */
static void
i2c1_set_target (uint8_t addr)
{
    out_w(I2C1_REG(HW_I2C_ENABLE), 0u);
    out_w(I2C1_REG(HW_I2C_TAR), (uint32_t) addr);
    out_w(I2C1_REG(HW_I2C_ENABLE), 1u);
}

/* Wait until any of the bits is set in a register, or an abort, or the
   time limit.  Returns true only if a bit was set without an abort. */
static bool
i2c1_wait_bits (uint32_t offset, uint32_t bits)
{
    uint32_t const start = app_time_us();
    bool           b_set = false;
    bool           b_stop_waiting = false;

    while (!b_stop_waiting)
    {
        b_set = (0u != (in_w(I2C1_REG(offset)) & bits));

        if (b_set ||
            (0u != (in_w(I2C1_REG(HW_I2C_RAW_INTR_STAT)) &
                    HW_I2C_RAW_TX_ABRT)) ||
            ((app_time_us() - start) >= I2C1_TIMEOUT_US))
        {
            b_stop_waiting = true;
        }
    }

    return (b_set && (0u == in_w(I2C1_REG(HW_I2C_TX_ABRT_SOURCE))));
}

/* Send one byte; with b_stop the transfer ends with a STOP. */
static bool
i2c1_send (uint32_t word, bool b_stop)
{
    bool b_ok = false;

    out_w(I2C1_REG(HW_I2C_DATA_CMD),
          (word | (b_stop ? HW_I2C_DATA_CMD_STOP : 0u)));
    b_ok = i2c1_wait_bits(HW_I2C_RAW_INTR_STAT, HW_I2C_RAW_TX_EMPTY);

    if (b_ok && b_stop)
    {
        b_ok = i2c1_wait_bits(HW_I2C_RAW_INTR_STAT, HW_I2C_RAW_STOP_DET);
        (void) in_w(I2C1_REG(HW_I2C_CLR_STOP_DET));     /* read clears */
    }

    if (!b_ok)
    {
        i2c1_finish_abort();
    }

    return (b_ok);
}

/* Clear an abort; the hardware sends STOP by itself after an abort. */
static void
i2c1_finish_abort (void)
{
    uint32_t const start = app_time_us();

    (void) in_w(I2C1_REG(HW_I2C_CLR_TX_ABRT));          /* read clears */

    while ((0u == (in_w(I2C1_REG(HW_I2C_RAW_INTR_STAT)) &
                   HW_I2C_RAW_STOP_DET)) &&
           ((app_time_us() - start) < I2C1_TIMEOUT_US))
    {
        /* Wait for the STOP the hardware sends after an abort. */
    }

    (void) in_w(I2C1_REG(HW_I2C_CLR_STOP_DET));
}

/*** end of file ***/
