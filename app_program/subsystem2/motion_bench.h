/*
 *  motion_bench.h
 *  Buddy 2's bench test of motors, encoders and the speed PID on the
 *  real car, with no other task running (motion_bench.c).
 *  Built only with:  make BENCH=1
 */

#ifndef MOTION_BENCH_H
#define MOTION_BENCH_H

#include "../app.h"

/* Start the bench task on core 0 (instead of every subsystem). */
ER motion_bench_start(void);

#endif /* MOTION_BENCH_H */

/*** end of file ***/
