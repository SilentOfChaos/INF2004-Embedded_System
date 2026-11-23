/*
 *  subsystem3.h
 *  Subsystem 3 (Buddy 3, Nga Wei Yang): line following and barcode.
 *    Line Task    priority 6, core 0, every 5 ms   (report section 3)
 *    Barcode ISR  GP7, core 0
 *  Buddy 3 also owns the steering calculation and the barcode
 *  letter-to-action map that Navigation uses (report section 3).
 *
 *  Public interface of subsystem 3.
 */

#ifndef SUBSYSTEM3_H
#define SUBSYSTEM3_H

#include "../app.h"
#include "letter_map.h"
#include "line_msgs.h"
#include "steering.h"

ER   line_err_read(line_err_t * p_out);

/* IF-11: Navigation -> Line controls (event flag owned by Line). */
void line_set_adaptation(bool b_enable);    /* off during HUMP CROSSING */
void line_set_run_over(bool b_run_over);    /* RUN OVER: sensing idle */

#endif /* SUBSYSTEM3_H */

/*** end of file ***/
