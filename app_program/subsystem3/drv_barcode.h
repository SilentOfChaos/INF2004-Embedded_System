/*
 *  drv_barcode.h
 *  Barcode sensor driver: D0 on GP7 (report section 2).  The Barcode ISR
 *  only time-stamps edges (report section 4) and passes them to the Line
 *  task through the message buffer mbf_bc_edges with TMO_POL, so no
 *  variable is shared between the ISR and the task.  Each edge is one
 *  packed word (barcode_edge.h).
 */

#ifndef DRV_BARCODE_H
#define DRV_BARCODE_H

#include "../app.h"
#include "barcode_edge.h"

/* Edges the buffer holds: what its 1024 bytes held before the
   minimum-bit change (64 were asked for, with 8 bytes to spare per edge),
   kept so chatter overflows the queue exactly as before (D-83, D-85). */
#define BARCODE_EDGE_QUEUE  85u

ER   barcode_init(ID mbf_edges, ID flg_overflow, UINT overflow_bit);
bool barcode_is_dark(void);

#endif /* DRV_BARCODE_H */

/*** end of file ***/
