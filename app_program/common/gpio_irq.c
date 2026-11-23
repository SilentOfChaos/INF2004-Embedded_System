/*
 *  gpio_irq.c
 *  Shared GPIO interrupt dispatcher (IO_IRQ_BANK0, core 0 only).
 *  See gpio_irq.h for the rules handlers must follow.
 */

#include "../app.h"
#include "app_stats.h"
#include "app_time.h"
#include "gpio_irq.h"
#include "hw_regs.h"

#define GPIO_IRQ_PINS           30u
#define GPIO_IRQ_PINS_PER_REG   8u
#define GPIO_IRQ_REGS           4u
#define GPIO_IRQ_EDGE_MASK      (GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE)
#define GPIO_IRQ_MAX_PASSES     4u  /* bound on re-reading the status */

/*
 * NVIC priority level 1 (INTPRI_MAX_EXTINT_PRI in sysdef.h), the highest
 * level the port allows for peripheral interrupts, so edge time stamps
 * are taken with the least delay (NFR2, A8).
 */
#define GPIO_IRQ_NVIC_LEVEL     INTPRI_MAX_EXTINT_PRI

/* Handler per pin.  Written only before the pin's events are enabled,
   read only by the interrupt handler. */
static gpio_irq_handler_t g_handlers[GPIO_IRQ_PINS];

/* Dispatcher entries since boot.  Shared variable, justified: diagnostic
   only (A8 interrupt-rate check), one writer (this handler on core 0),
   aligned 32-bit reads cannot tear. */
static uint32_t volatile g_irq_count;

static void gpio_irq_isr(UINT intno);

/*
 * Install the dispatcher in the kernel's vector table.  Called once from
 * usermain's init phase.  The interrupt stays masked in the NVIC until
 * gpio_irq_enable_core0() runs on core 0.
 */
int32_t
gpio_irq_init (void)
{
    T_DINT  dint;
    ER      ercd;

    dint.intatr = TA_HLNG;
    dint.inthdr = (FP) gpio_irq_isr;    /* kernel calls it with intno */
    ercd = tk_def_int(HW_IO_IRQ_BANK0, &dint);

    return ((int32_t) ercd);            /* ER is a 32-bit error code */
}

/*
 * Register a handler for one pin and enable its edge events for core 0.
 * The handler pointer is stored before the events are enabled, so the
 * dispatcher never sees an enabled pin without a handler.
 */
int32_t
gpio_irq_register (uint32_t pin, uint32_t events,
                   gpio_irq_handler_t handler)
{
    int32_t result = E_PAR;

    if ((pin < GPIO_IRQ_PINS) && (NULL != handler) &&
        (0u != (events & GPIO_IRQ_EDGE_MASK)))
    {
        uint32_t const reg   = pin / GPIO_IRQ_PINS_PER_REG;
        uint32_t const shift = 4u * (pin % GPIO_IRQ_PINS_PER_REG);

        g_handlers[pin] = handler;

        /* Clear stale edge events, then enable the requested ones. */
        out_w(HW_IO_INTR(reg), (GPIO_IRQ_EDGE_MASK << shift));
        set_w(HW_IO_PROC0_INTE(reg),
              ((events & GPIO_IRQ_EDGE_MASK) << shift));
        result = E_OK;
    }

    return (result);
}

/*
 * Enable IO_IRQ_BANK0 in this core's NVIC.  Each core has its own NVIC,
 * so this must run on core 0 (report section 3).
 */
int32_t
gpio_irq_enable_core0 (void)
{
    int32_t result = E_OK;

#if TK_SUPPORT_SMP
    if (1 != tk_get_prc())      /* tk_get_prc() counts cores from 1 */
    {
        result = E_CTX;
    }
#endif

    if (E_OK == result)
    {
        ClearInt(HW_IO_IRQ_BANK0);
        EnableInt(HW_IO_IRQ_BANK0, GPIO_IRQ_NVIC_LEVEL);
    }

    return (result);
}

/* Number of dispatcher entries since boot (A8 diagnostics). */
uint32_t
gpio_irq_count (void)
{
    return (g_irq_count);
}

/*
 * The dispatcher.  For every pin with pending edge events: acknowledge
 * the events first (so an edge that arrives while the handler runs is
 * not lost), then call the pin's handler with the time the events were
 * read.  Each status read has its own time stamp: an edge found on a
 * later pass, or in a register read after other handlers ran, happened
 * after the earlier reads, so the time of entry would date it too early
 * (D-93).
 */
static void
gpio_irq_isr (UINT intno)
{
#if (0 != CFG_STATS_ENABLE)
    uint32_t const entry_us = app_time_us();
#endif
    uint32_t       passes   = 0u;
    bool           b_more   = true;

    (void) intno;
    g_irq_count = g_irq_count + 1u;

    while (b_more && (passes < GPIO_IRQ_MAX_PASSES))
    {
        b_more = false;
        passes++;

        for (uint32_t reg = 0u; reg < GPIO_IRQ_REGS; reg++)
        {
            uint32_t const status = in_w(HW_IO_PROC0_INTS(reg));

            if (0u != status)
            {
                /* Read after the status: no edge in it is newer. */
                uint32_t const time_us = app_time_us();

                b_more = true;
                out_w(HW_IO_INTR(reg), status);     /* ack edge events */

                for (uint32_t slot = 0u; slot < GPIO_IRQ_PINS_PER_REG;
                     slot++)
                {
                    uint32_t const pin    = (reg * GPIO_IRQ_PINS_PER_REG)
                                            + slot;
                    uint32_t const events = (status >> (4u * slot))
                                            & GPIO_IRQ_EDGE_MASK;

                    if ((0u != events) && (pin < GPIO_IRQ_PINS) &&
                        (NULL != g_handlers[pin]))
                    {
#if (0 != CFG_STATS_ENABLE)
                        uint32_t const start_us = app_time_us();
#endif
                        g_handlers[pin](pin, events, time_us);
#if (0 != CFG_STATS_ENABLE)
                        app_stats_isr(pin, start_us, app_time_us());
#endif
                    }
                }
            }
        }
    }

#if (0 != CFG_STATS_ENABLE)
    app_stats_dispatch(entry_us, app_time_us());
#endif
}

/*** end of file ***/
