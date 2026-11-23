/*
 *  drv_neopixel.h
 *  On-board NeoPixel status LED on GP18, driven by a PIO state machine
 *  (report section 2), owned by the Safety task.
 *
 *  Plain C types only: drv_neopixel.c is compiled in the Pico SDK's
 *  namespace (hardware/pio.h), which must never see tk/tkernel.h in the
 *  same file (size_t clash, see lib/liblwip/.../cc.h).  The SDK's PIO
 *  code is only built with WIFI=cyw43; with WIFI=none the LED is off.
 */

#ifndef DRV_NEOPIXEL_H
#define DRV_NEOPIXEL_H

#include <stdbool.h>
#include <stdint.h>

bool neopixel_init(uint32_t pin);
void neopixel_set(uint8_t red, uint8_t green, uint8_t blue);

#endif /* DRV_NEOPIXEL_H */

/*** end of file ***/
