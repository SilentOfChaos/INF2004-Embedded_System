/*
 *  drv_line_adc.h
 *  Line sensor ADC driver: LINE_L on GP26/ADC0 and LINE_R on GP27/ADC1
 *  (report section 2), read through the kernel's "adca" ADC driver.
 *  Called only by the Line task, which owns the ADC (TEAM.md section 3).
 */

#ifndef DRV_LINE_ADC_H
#define DRV_LINE_ADC_H

#include "../app.h"

ER line_adc_open(void);
ER line_adc_read(uint32_t * p_left, uint32_t * p_right);

#endif /* DRV_LINE_ADC_H */

/*** end of file ***/
