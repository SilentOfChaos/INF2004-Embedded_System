/*
 *  nav_types.h
 *  Navigation modes and SAFE HOLD reasons.
 *
 *  Pure C (no kernel headers), so host unit tests can use these types.
 */

#ifndef NAV_TYPES_H
#define NAV_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/* Health bits checked in SELF-TEST (same values as the NAV_FLG_xxx_OK
   bits of Navigation's event flag). */
#define NAV_HEALTH_LINE         0x0100u /* line ADC readings plausible */
#define NAV_HEALTH_IMU          0x0200u /* IMU identified and calibrated */
#define NAV_HEALTH_SCAN         0x0400u /* ultrasonic answers (D-104) */
#define NAV_HEALTH_ALL          (NAV_HEALTH_LINE | NAV_HEALTH_IMU | \
                                 NAV_HEALTH_SCAN)

/* System modes (report section 5, Fig 5). */
typedef enum
{
    NAV_MODE_BOOT = 0,
    NAV_MODE_SELF_TEST,
    NAV_MODE_READY,
    NAV_MODE_LINE_FOLLOWING,
    NAV_MODE_HUMP_CROSSING,
    NAV_MODE_LINE_SEARCH,
    NAV_MODE_AT_JUNCTION,
    NAV_MODE_AVOIDING,
    NAV_MODE_SAFE_HOLD,
    NAV_MODE_FAULT,
    NAV_MODE_COMPLETE,
    NAV_MODE_COUNT
} nav_mode_t;

/* Why the car is in SAFE HOLD (reported in telemetry). */
typedef enum
{
    NAV_HOLD_NONE = 0,
    NAV_HOLD_NO_COMMAND,        /* junction with no command held (FR5) */
    NAV_HOLD_LINE_NOT_FOUND,    /* no line within 3 s of a turn */
    NAV_HOLD_NO_SAFE_ROUTE,     /* obstacle: no route (FR11) */
    NAV_HOLD_COUNT
} nav_hold_t;

#endif /* NAV_TYPES_H */

/*** end of file ***/
