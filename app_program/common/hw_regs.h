/*
 *  hw_regs.h
 *  RP2040 register addresses used by the IS19 drivers that the kernel's
 *  sysdef.h does not define.  Values from the RP2040 datasheet register
 *  tables (they match the Pico SDK's hardware/regs headers).
 *
 *  Use with the template's in_w()/out_w()/set_w()/clr_w() helpers.
 *  set_w()/clr_w() use the atomic SET/CLR aliases (+0x2000/+0x3000):
 *  never use them on SIO (0xd0000000) registers, which have no aliases.
 *
 *  Note: sysdef.h defines GPIO_OUT as SIO+0x008, which is GPIO_HI_IN on
 *  the RP2040 (GPIO_OUT is at SIO+0x010).  Nothing in IS19 uses that
 *  macro; use GPIO_OUT_SET / GPIO_OUT_CLR instead.
 */

#ifndef HW_REGS_H
#define HW_REGS_H

/* clk_sys and clk_peri: the kernel sets the system PLL to 125 MHz
   (kernel/sysdepend/pico_rp2040/cpu_clock.c). */
#define APP_CLK_HZ                  125000000u

/* ---- IO_BANK0: GPIO function select, overrides and interrupts ------- */
#define HW_IO_BANK0_BASE            0x40014000u
#define HW_GPIO_CTRL(pin)           (HW_IO_BANK0_BASE + 0x004u + ((pin) * 8u))
#define HW_GPIO_CTRL_FUNCSEL_MASK   0x0000001Fu
#define HW_GPIO_CTRL_OUTOVER_MASK   0x00000300u
#define HW_GPIO_CTRL_OUTOVER_LOW    0x00000200u /* drive output low */
#define HW_GPIO_CTRL_OEOVER_MASK    0x00003000u
#define HW_GPIO_CTRL_OEOVER_ENABLE  0x00003000u /* force output enable */
#define HW_FUNCSEL_I2C              3u
#define HW_FUNCSEL_PWM              4u
#define HW_FUNCSEL_SIO              5u

/* Raw interrupt status (write 1 to clear an edge event).  Four bits per
   pin (level low, level high, edge low, edge high), 8 pins per register. */
#define HW_IO_INTR(idx)             (HW_IO_BANK0_BASE + 0x0F0u + ((idx) * 4u))
#define HW_IO_PROC0_INTE(idx)       (HW_IO_BANK0_BASE + 0x100u + ((idx) * 4u))
#define HW_IO_PROC0_INTS(idx)       (HW_IO_BANK0_BASE + 0x120u + ((idx) * 4u))
#define HW_IO_EVT_LEVEL_LOW         0x1u
#define HW_IO_EVT_LEVEL_HIGH        0x2u
#define HW_IO_EVT_EDGE_LOW          0x4u
#define HW_IO_EVT_EDGE_HIGH         0x8u
#define HW_IO_IRQ_BANK0             13u     /* NVIC interrupt number */

/* ---- PADS_BANK0 (pad register addresses are GPIO(n) in sysdef.h) ---- */
#define HW_PAD_IE                   0x40u
#define HW_PAD_OD                   0x80u
#define HW_PAD_PUE                  0x08u
#define HW_PAD_PDE                  0x04u
#define HW_PAD_SCHMITT              0x02u

/* ---- SIO GPIO (no atomic aliases) ----------------------------------- */
#define HW_SIO_GPIO_IN              0xD0000004u
#define HW_SIO_GPIO_OUT_SET         0xD0000014u
#define HW_SIO_GPIO_OUT_CLR         0xD0000018u
#define HW_SIO_GPIO_OUT_XOR         0xD000001Cu
#define HW_SIO_GPIO_OE_SET          0xD0000024u
#define HW_SIO_GPIO_OE_CLR          0xD0000028u

/* ---- TIMER: free-running 1 MHz counter ------------------------------ */
#define HW_TIMER_TIMERAWH           0x40054024u
#define HW_TIMER_TIMERAWL           0x40054028u

/* ---- PWM slices ------------------------------------------------------ */
#define HW_PWM_BASE                 0x40050000u
#define HW_PWM_CSR(slice)           (HW_PWM_BASE + 0x00u + ((slice) * 0x14u))
#define HW_PWM_DIV(slice)           (HW_PWM_BASE + 0x04u + ((slice) * 0x14u))
#define HW_PWM_CTR(slice)           (HW_PWM_BASE + 0x08u + ((slice) * 0x14u))
#define HW_PWM_CC(slice)            (HW_PWM_BASE + 0x0Cu + ((slice) * 0x14u))
#define HW_PWM_TOP(slice)           (HW_PWM_BASE + 0x10u + ((slice) * 0x14u))
#define HW_PWM_CSR_EN               0x1u
#define HW_PWM_DIV_INT_SHIFT        4u      /* 8.4 fixed-point divider */

/* ---- RESETS ---------------------------------------------------------- */
#define HW_RESETS_RESET             0x4000C000u
#define HW_RESETS_DONE              0x4000C008u
#define HW_RESETS_I2C1              0x00000010u
#define HW_RESETS_PWM               0x00004000u

/* ---- I2C1 (Synopsys DW_apb_i2c) -------------------------------------- */
#define HW_I2C1_BASE                0x40048000u
#define HW_I2C_CON                  0x00u
#define HW_I2C_TAR                  0x04u
#define HW_I2C_DATA_CMD             0x10u
#define HW_I2C_FS_SCL_HCNT          0x1Cu
#define HW_I2C_FS_SCL_LCNT          0x20u
#define HW_I2C_RAW_INTR_STAT        0x34u
#define HW_I2C_CLR_INTR             0x40u
#define HW_I2C_CLR_TX_ABRT          0x54u
#define HW_I2C_CLR_STOP_DET         0x60u
#define HW_I2C_ENABLE               0x6Cu
#define HW_I2C_STATUS               0x70u
#define HW_I2C_TXFLR                0x74u
#define HW_I2C_RXFLR                0x78u
#define HW_I2C_SDA_HOLD             0x7Cu
#define HW_I2C_TX_ABRT_SOURCE       0x80u
#define HW_I2C_ENABLE_STATUS        0x9Cu
#define HW_I2C_FS_SPKLEN            0xA0u
#define HW_I2C_CON_MASTER_MODE      0x0001u
#define HW_I2C_CON_SPEED_FAST       0x0004u
#define HW_I2C_CON_RESTART_EN       0x0020u
#define HW_I2C_CON_SLAVE_DISABLE    0x0040u
#define HW_I2C_CON_TX_EMPTY_CTRL    0x0100u
#define HW_I2C_DATA_CMD_READ        0x0100u
#define HW_I2C_DATA_CMD_STOP        0x0200u
#define HW_I2C_DATA_CMD_RESTART     0x0400u
#define HW_I2C_RAW_TX_EMPTY         0x0010u
#define HW_I2C_RAW_TX_ABRT          0x0040u
#define HW_I2C_RAW_STOP_DET         0x0200u
#define HW_I2C_STATUS_ACTIVITY      0x0001u
#define HW_I2C_SDA_TX_HOLD_MASK     0x0000FFFFu

/* ---- WATCHDOG and PSM ------------------------------------------------- */
#define HW_WATCHDOG_CTRL            0x40058000u
#define HW_WATCHDOG_LOAD            0x40058004u
#define HW_WATCHDOG_REASON          0x40058008u
#define HW_WATCHDOG_TICK            0x4005802Cu
#define HW_WATCHDOG_TICK_ENABLE     0x00000200u
#define HW_WATCHDOG_TICK_RUNNING    0x00000400u
#define HW_XOSC_MHZ                 12u         /* clk_ref = 12 MHz XOSC */
#define HW_WATCHDOG_CTRL_ENABLE     0x40000000u
#define HW_WATCHDOG_CTRL_PAUSE_ALL  0x07000000u /* pause on debug/JTAG */
#define HW_WATCHDOG_LOAD_MAX        0x00FFFFFFu
#define HW_PSM_WDSEL                0x40010008u
/* Reset everything except the two oscillators on a watchdog timeout. */
#define HW_PSM_WDSEL_ALL_BUT_OSC    0x0001FFFCu

#endif /* HW_REGS_H */

/*** end of file ***/
