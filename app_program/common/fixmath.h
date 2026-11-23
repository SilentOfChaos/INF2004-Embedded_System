/*
 *  fixmath.h
 *  Fixed-point maths for IS19.  The build has no floating-point library
 *  (no -lm) and Barr-C 5.4 asks for fixed point where it will do.
 *
 *  Units used across the project:
 *    angles      centi-degrees (cdeg), 9000 = 90 degrees
 *    ratios      Q15, 32768 = 1.0
 *    distances   mm, speeds mm/s, accelerations mg
 *
 *  Pure C: no kernel headers, so the host unit tests can build it.
 */

#ifndef FIXMATH_H
#define FIXMATH_H

#include <stdbool.h>
#include <stdint.h>

#define FIXMATH_Q15_ONE         32768
#define FIXMATH_CDEG_HALF_TURN  18000
#define FIXMATH_CDEG_FULL_TURN  36000

int32_t  fixmath_clamp(int32_t value, int32_t low, int32_t high);
int32_t  fixmath_abs(int32_t value);
int32_t  fixmath_wrap_cdeg(int32_t angle_cdeg);
int32_t  fixmath_atan2_cdeg(int32_t y_value, int32_t x_value);
int32_t  fixmath_sin_q15(int32_t angle_cdeg);
int32_t  fixmath_cos_q15(int32_t angle_cdeg);
int32_t  fixmath_mul_q15(int32_t value, int32_t ratio_q15);
uint32_t fixmath_isqrt(uint32_t value);
int32_t  fixmath_interp(int32_t x_value, int32_t const * p_xs,
                        int32_t const * p_ys, uint32_t count);
int32_t  fixmath_div_round(int32_t numerator, int32_t denominator);

#endif /* FIXMATH_H */

/*** end of file ***/
