/*
 *  drv_buzzer.h
 *  On-board piezo buzzer on GP22 (PWM slice 3 A), owned by the Safety
 *  task (report section 2).  The board's mute switch must be off.
 */

#ifndef DRV_BUZZER_H
#define DRV_BUZZER_H

#include <stdbool.h>

void buzzer_init(void);
void buzzer_on(bool b_on);

#endif /* DRV_BUZZER_H */

/*** end of file ***/
