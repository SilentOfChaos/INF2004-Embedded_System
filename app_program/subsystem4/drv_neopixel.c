/*
 *  drv_neopixel.c
 *  WS2812 NeoPixel through PIO, using the Pico SDK's hardware_pio code
 *  that the WIFI=cyw43 build already compiles for the radio.  The SDK's
 *  claim functions pick a free PIO block, state machine and program
 *  space, so the LED never collides with the cyw43 driver's PIO program.
 *
 *  The PIO program is the pico-examples ws2812 program (T1 = 2, T2 = 5,
 *  T3 = 3 cycles per bit, 800 kHz):
 *      0: out x, 1        side 0 [2]
 *      1: jmp !x, 3       side 1 [1]
 *      2: jmp 0           side 1 [4]
 *      3: nop             side 0 [4]
 */

#include "drv_neopixel.h"

#if defined(TM_WIFI_CYW43) && TM_WIFI_CYW43

#include "hardware/pio.h"

#define NEO_BIT_HZ          800000u
#define NEO_CYCLES_PER_BIT  10u
#define NEO_CLK_HZ          125000000u
#define NEO_FRAC_SCALE      256u
#define NEO_BITS            24u
#define NEO_GREEN_SHIFT     24u
#define NEO_RED_SHIFT       16u
#define NEO_BLUE_SHIFT      8u
#define NEO_WRAP_TARGET     0u
#define NEO_WRAP            3u

static uint16_t const g_neo_instructions[] =
{
    0x6221u,    /* out x, 1        side 0 [2] */
    0x1123u,    /* jmp !x, 3       side 1 [1] */
    0x1400u,    /* jmp 0           side 1 [4] */
    0xa442u     /* nop             side 0 [4] */
};

static pio_program_t const g_neo_program =
{
    .instructions = g_neo_instructions,
    .length       = 4,
    .origin       = -1
};

static PIO  g_neo_pio  = NULL;
static uint g_neo_sm   = 0u;
static bool g_neo_ready = false;

/* Claim a state machine, load the program and start it. */
bool
neopixel_init (uint32_t pin)
{
    uint offset = 0u;

    g_neo_ready = pio_claim_free_sm_and_add_program_for_gpio_range(
                      &g_neo_program, &g_neo_pio, &g_neo_sm, &offset,
                      (uint) pin, 1u, true);

    if (g_neo_ready)
    {
        pio_sm_config config = pio_get_default_sm_config();
        uint32_t const div_x256 = (NEO_CLK_HZ * NEO_FRAC_SCALE) /
                                  (NEO_BIT_HZ * NEO_CYCLES_PER_BIT);

        pio_gpio_init(g_neo_pio, (uint) pin);
        (void) pio_sm_set_consecutive_pindirs(g_neo_pio, g_neo_sm,
                                              (uint) pin, 1u, true);
        sm_config_set_wrap(&config, offset + NEO_WRAP_TARGET,
                           offset + NEO_WRAP);
        sm_config_set_sideset(&config, 1u, false, false);
        sm_config_set_sideset_pins(&config, (uint) pin);
        sm_config_set_out_shift(&config, false, true, NEO_BITS);
        sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);
        /* 125 MHz / (800 kHz x 10) = 15.625: integer 15, fraction 160. */
        sm_config_set_clkdiv_int_frac8(&config, div_x256 / NEO_FRAC_SCALE,
                                       (uint8_t) (div_x256 % NEO_FRAC_SCALE));
        (void) pio_sm_init(g_neo_pio, g_neo_sm, offset, &config);
        pio_sm_set_enabled(g_neo_pio, g_neo_sm, true);
    }

    return (g_neo_ready);
}

/* Show a colour; skipped if the FIFO is full (the LED is not critical). */
void
neopixel_set (uint8_t red, uint8_t green, uint8_t blue)
{
    if (g_neo_ready && !pio_sm_is_tx_fifo_full(g_neo_pio, g_neo_sm))
    {
        uint32_t const grb = ((uint32_t) green << NEO_GREEN_SHIFT) |
                             ((uint32_t) red << NEO_RED_SHIFT) |
                             ((uint32_t) blue << NEO_BLUE_SHIFT);

        pio_sm_put(g_neo_pio, g_neo_sm, grb);
    }
}

#else /* WIFI=none build: no SDK PIO code, LED stays off */

bool
neopixel_init (uint32_t pin)
{
    (void) pin;
    return (false);
}

void
neopixel_set (uint8_t red, uint8_t green, uint8_t blue)
{
    (void) red;
    (void) green;
    (void) blue;
}

#endif /* TM_WIFI_CYW43 */

/*** end of file ***/
