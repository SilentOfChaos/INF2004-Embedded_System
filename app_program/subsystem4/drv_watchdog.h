/*
 *  drv_watchdog.h
 *  RP2040 hardware watchdog (report section 3): armed and fed only by
 *  the Safety task, which feeds it only while Line, Navigation, Motion,
 *  IMU and Obstacle Scan have all checked in within their deadlines.
 *  Once armed it cannot be stopped; a missed feed resets the chip.
 */

#ifndef DRV_WATCHDOG_H
#define DRV_WATCHDOG_H

#include <stdbool.h>
#include <stdint.h>

void watchdog_arm(uint32_t timeout_ms);
void watchdog_feed(void);
bool watchdog_caused_reset(void);

#endif /* DRV_WATCHDOG_H */

/*** end of file ***/
