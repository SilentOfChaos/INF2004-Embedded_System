/*
 *  drv_barcode.c
 *  Barcode sensor driver: D0 on GP7, edges time-stamped in the GPIO
 *  interrupt (NFR2: 1 us resolution) and queued for the Line task.
 */

#include "../app.h"
#include "../common/app_gpio.h"
#include "../common/gpio_irq.h"
#include "drv_barcode.h"

#define BARCODE_BOTH_EDGES  (GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL)
/* Pull GP7 towards the "white" level, the opposite of the dark level. */
#define BARCODE_PULL        ((0 != CFG_BC_DARK_LEVEL) ? APP_GPIO_PULL_DOWN \
                                                      : APP_GPIO_PULL_UP)

/* Object IDs for the ISR.  Written once in barcode_init(), before the
   pin's interrupt is enabled; only read afterwards. */
static ID   g_edge_mbf  = 0;
static ID   g_ovf_flg   = 0;
static UINT g_ovf_bit   = 0u;

static void barcode_edge_isr(uint32_t pin, uint32_t events,
                             uint32_t time_us);

/*
 * Configure GP7 as an input and register the Barcode ISR for both edges.
 * The module's LM393 output has its own pull-up resistor and pulls low
 * itself.  The internal pull goes to the white level (pull-down when
 * CFG_BC_DARK_LEVEL is 1), so an unplugged sensor reads white and never
 * starts a capture instead of floating; the weak internal pull-down
 * (about 50 kOhm) does not stop the module's own pull-up giving a valid
 * high level.
 */
ER
barcode_init (ID mbf_edges, ID flg_overflow, UINT overflow_bit)
{
    g_edge_mbf = mbf_edges;
    g_ovf_flg  = flg_overflow;
    g_ovf_bit  = overflow_bit;

    app_gpio_input(CFG_PIN_BARCODE, BARCODE_PULL);

    return ((ER) gpio_irq_register(CFG_PIN_BARCODE, BARCODE_BOTH_EDGES,
                                   barcode_edge_isr));
}

/* Current level of the barcode sensor: true = over a bar. */
bool
barcode_is_dark (void)
{
    bool const b_high = app_gpio_read(CFG_PIN_BARCODE);

    return (b_high == (0 != CFG_BC_DARK_LEVEL));
}

/*
 * Barcode ISR (called by the shared dispatcher on core 0).  Only records
 * the edge time and level (report section 4).  If both edge types are
 * pending together the pulse was shorter than the interrupt latency, a
 * glitch, so nothing is recorded.  If the queue is full the Line task is
 * told through an event flag bit, and the capture fails.
 */
static void
barcode_edge_isr (uint32_t pin, uint32_t events, uint32_t time_us)
{
    (void) pin;

    if (BARCODE_BOTH_EDGES != (events & BARCODE_BOTH_EDGES))
    {
        bool const           b_high = (0u != (events & GPIO_IRQ_EDGE_RISE));
        barcode_edge_t const edge   = barcode_edge_pack(time_us,
                                          (b_high == (0 != CFG_BC_DARK_LEVEL)));

        if (E_OK != tk_snd_mbf(g_edge_mbf, &edge, (INT) sizeof(edge),
                               TMO_POL))
        {
            (void) tk_set_flg(g_ovf_flg, g_ovf_bit);
        }
    }
}

/*** end of file ***/
