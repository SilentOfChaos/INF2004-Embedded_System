/*
 *  gpio_irq.h
 *  Shared GPIO interrupt dispatcher (IO_IRQ_BANK0, core 0 only).
 *
 *  The RP2040 has one GPIO interrupt line per core for all 30 pins, so
 *  the barcode, echo and encoder handlers cannot each own it.  This
 *  module owns IO_IRQ_BANK0 for the whole team: each subsystem registers
 *  a handler for its own pin, and the dispatcher calls it with the pin's
 *  events and the time they were read from the status register.
 *
 *  Report section 3: all GPIO interrupts run on core 0.  Only the PROC0
 *  enable registers are used, and gpio_irq_enable_core0() must be called
 *  from a task pinned to core 0 (Navigation does this when it starts).
 *
 *  Handlers run in interrupt context: keep them short, never wait, no
 *  tm_printf (NFR15).  They may call tk_snd_mbf() with TMO_POL,
 *  tk_set_flg(), tk_sig_sem() and tk_wup_tsk().
 */

#ifndef GPIO_IRQ_H
#define GPIO_IRQ_H

#include <stdint.h>

#define GPIO_IRQ_EDGE_FALL      0x4u    /* HW_IO_EVT_EDGE_LOW */
#define GPIO_IRQ_EDGE_RISE      0x8u    /* HW_IO_EVT_EDGE_HIGH */

/* Called in interrupt context with the pin, its edge events and the
   time (app_time_us) read just after the events were read: no edge in
   them is newer (D-93). */
typedef void (*gpio_irq_handler_t)(uint32_t pin, uint32_t events,
                                   uint32_t time_us);

int32_t gpio_irq_init(void);
int32_t gpio_irq_register(uint32_t pin, uint32_t events,
                          gpio_irq_handler_t handler);
int32_t gpio_irq_enable_core0(void);
uint32_t gpio_irq_count(void);

#endif /* GPIO_IRQ_H */

/*** end of file ***/
