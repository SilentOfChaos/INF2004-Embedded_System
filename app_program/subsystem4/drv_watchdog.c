/*
 *  drv_watchdog.c
 *  RP2040 hardware watchdog, same register sequence as the Pico SDK's
 *  watchdog_enable().  The RP2040 counter decrements twice per 1 us tick
 *  (erratum RP2040-E1), so the load value is twice the timeout in us.
 */

#include "../app.h"
#include "../common/hw_regs.h"
#include "drv_watchdog.h"

#define WDT_US_PER_MS       1000u
#define WDT_TICKS_PER_US    2u      /* RP2040-E1 */
#define WDT_REASON_TIMER    0x1u

static uint32_t g_load = 0u;        /* written once in watchdog_arm() */

/*
 * Arm the watchdog with a timeout.  A timeout resets every block except
 * the oscillators (PSM WDSEL), as the SDK does.  The watchdog pauses
 * while a debugger halts the cores.
 */
void
watchdog_arm (uint32_t timeout_ms)
{
    uint32_t load = timeout_ms * WDT_US_PER_MS * WDT_TICKS_PER_US;

    if (load > HW_WATCHDOG_LOAD_MAX)
    {
        load = HW_WATCHDOG_LOAD_MAX;
    }

    g_load = load;
    clr_w(HW_WATCHDOG_CTRL, HW_WATCHDOG_CTRL_ENABLE);
    set_w(HW_PSM_WDSEL, HW_PSM_WDSEL_ALL_BUT_OSC);
    set_w(HW_WATCHDOG_CTRL, HW_WATCHDOG_CTRL_PAUSE_ALL);
    out_w(HW_WATCHDOG_LOAD, g_load);
    set_w(HW_WATCHDOG_CTRL, HW_WATCHDOG_CTRL_ENABLE);
}

/* Reload the counter. */
void
watchdog_feed (void)
{
    out_w(HW_WATCHDOG_LOAD, g_load);
}

/* True if the last reset was a watchdog timeout (for the console). */
bool
watchdog_caused_reset (void)
{
    return (0u != (in_w(HW_WATCHDOG_REASON) & WDT_REASON_TIMER));
}

/*** end of file ***/
