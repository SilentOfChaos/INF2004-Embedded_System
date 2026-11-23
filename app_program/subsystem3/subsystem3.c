/*
 *  subsystem3.c
 *  Subsystem 3 (Buddy 3): creates its objects and starts its task.
 *
 *  Files in this subsystem:
 *    line_task.c      Line Task: 5 ms loop, IF-01 events, IF-02 record
 *    line_track.c     LINE TRACKING region (pure logic)
 *    bc_capture.c     BARCODE region: capture of edges (pure logic)
 *    bc_decode.c      Code 39 decoder, both directions (pure logic)
 *    drv_barcode.c    Barcode ISR on GP7
 *    drv_line_adc.c   line sensors on ADC0/ADC1
 *    steering.c       steering calculation used by Navigation
 *    letter_map.c     letter-to-action map used by Navigation
 */

#include "line_task.h"
#include "subsystem3.h"

EXPORT ER
subsystem3_init (void)
{
    return (line_task_init());
}

EXPORT ER
subsystem3_start (void)
{
    return (line_task_start());
}

/*** end of file ***/
