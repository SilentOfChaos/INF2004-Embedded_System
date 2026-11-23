/*
 *  fixmath.c
 *  Fixed-point maths for IS19.  See fixmath.h for the units.
 *
 *  Barr-C 5.3.b forbids bitwise operators on signed values, so every
 *  shift here works on an unsigned magnitude and the sign is put back
 *  afterwards.
 */

#include <stddef.h>

#include "fixmath.h"

#define FIXMATH_SIN_STEPS       91      /* 0..90 degrees, 1 degree apart */
#define FIXMATH_CDEG_PER_STEP   100
#define FIXMATH_CDEG_QUARTER    9000
#define FIXMATH_CORDIC_STEPS    16
#define FIXMATH_MDEG_PER_CDEG   10
#define FIXMATH_MDEG_HALF_TURN  180000
#define FIXMATH_CORDIC_LIMIT    0x1FFFFFFF  /* keep sums inside int32 */
#define FIXMATH_CORDIC_NORM     0x10000000  /* small inputs scaled to here */
#define FIXMATH_Q15_SHIFT       15u
#define FIXMATH_Q15_HALF        16384u

/* sin(k degrees) for k = 0..90, scaled by 32767. */
static int16_t const g_sin_table[FIXMATH_SIN_STEPS] =
{
        0,   572,  1144,  1715,  2286,  2856,  3425,  3993,
     4560,  5126,  5690,  6252,  6813,  7371,  7927,  8481,
     9032,  9580, 10126, 10668, 11207, 11743, 12275, 12803,
    13328, 13848, 14364, 14876, 15383, 15886, 16383, 16876,
    17364, 17846, 18323, 18794, 19260, 19720, 20173, 20621,
    21062, 21497, 21925, 22347, 22762, 23170, 23571, 23964,
    24351, 24730, 25101, 25465, 25821, 26169, 26509, 26841,
    27165, 27481, 27788, 28087, 28377, 28659, 28932, 29196,
    29451, 29697, 29934, 30162, 30381, 30591, 30791, 30982,
    31163, 31335, 31498, 31650, 31794, 31927, 32051, 32165,
    32269, 32364, 32448, 32523, 32587, 32642, 32687, 32722,
    32747, 32762, 32767
};

/* atan(2^-i) in milli-degrees, for the CORDIC vectoring loop. */
static int32_t const g_cordic_mdeg[FIXMATH_CORDIC_STEPS] =
{
    45000, 26565, 14036, 7125, 3576, 1790, 895, 448,
    224, 112, 56, 28, 14, 7, 3, 2
};

static int32_t fixmath_shift_right(int32_t value, uint32_t shift);
static int32_t fixmath_sin_first_quarter(int32_t angle_cdeg);

/* Limit a value to the closed range [low, high]. */
int32_t
fixmath_clamp (int32_t value, int32_t low, int32_t high)
{
    int32_t result = value;

    if (value < low)
    {
        result = low;
    }
    else if (value > high)
    {
        result = high;
    }
    else
    {
        /* Already inside the range. */
    }

    return (result);
}

/* Absolute value; INT32_MIN is clamped to INT32_MAX. */
int32_t
fixmath_abs (int32_t value)
{
    int32_t result = value;

    if (value < 0)
    {
        result = (INT32_MIN == value) ? INT32_MAX : -value;
    }

    return (result);
}

/* Wrap an angle into (-18000, 18000] centi-degrees. */
int32_t
fixmath_wrap_cdeg (int32_t angle_cdeg)
{
    int32_t result = angle_cdeg % FIXMATH_CDEG_FULL_TURN;

    if (result > FIXMATH_CDEG_HALF_TURN)
    {
        result -= FIXMATH_CDEG_FULL_TURN;
    }
    else if (result <= -FIXMATH_CDEG_HALF_TURN)
    {
        result += FIXMATH_CDEG_FULL_TURN;
    }
    else
    {
        /* Already inside the range. */
    }

    return (result);
}

/*
 * atan2(y, x) in centi-degrees, (-18000, 18000], by CORDIC vectoring.
 * Accuracy is about 0.01 degree for any input size (host test COM-U02).
 * atan2(0, 0) returns 0.
 */
int32_t
fixmath_atan2_cdeg (int32_t y_value, int32_t x_value)
{
    int32_t xx = x_value;
    int32_t yy = y_value;
    int32_t angle_mdeg = 0;
    int32_t result = 0;

    /* Scale both inputs down until the CORDIC sums cannot overflow ... */
    while ((fixmath_abs(xx) > FIXMATH_CORDIC_LIMIT) ||
           (fixmath_abs(yy) > FIXMATH_CORDIC_LIMIT))
    {
        xx = fixmath_shift_right(xx, 1u);
        yy = fixmath_shift_right(yy, 1u);
    }

    /* ... and small ones up, so the shifted terms keep their precision
       (the angle does not change).  Afterwards the larger magnitude is
       at least FIXMATH_CORDIC_NORM and at most FIXMATH_CORDIC_LIMIT. */
    while (((0 != xx) || (0 != yy)) &&
           (fixmath_abs(xx) < FIXMATH_CORDIC_NORM) &&
           (fixmath_abs(yy) < FIXMATH_CORDIC_NORM))
    {
        xx *= 2;
        yy *= 2;
    }

    if ((0 != xx) || (0 != yy))
    {
        /* Move the left half-plane to the right half-plane first. */
        if (xx < 0)
        {
            xx = -xx;
            yy = -yy;
            angle_mdeg = FIXMATH_MDEG_HALF_TURN;
        }

        for (uint32_t step = 0u; step < FIXMATH_CORDIC_STEPS; step++)
        {
            int32_t const x_shifted = fixmath_shift_right(xx, step);
            int32_t const y_shifted = fixmath_shift_right(yy, step);

            if (yy > 0)
            {
                xx += y_shifted;
                yy -= x_shifted;
                angle_mdeg += g_cordic_mdeg[step];
            }
            else
            {
                xx -= y_shifted;
                yy += x_shifted;
                angle_mdeg -= g_cordic_mdeg[step];
            }
        }

        result = fixmath_wrap_cdeg(
            fixmath_div_round(angle_mdeg, FIXMATH_MDEG_PER_CDEG));
    }

    return (result);
}

/* sin(angle) in Q15 (32767 = 1.0), any angle in centi-degrees. */
int32_t
fixmath_sin_q15 (int32_t angle_cdeg)
{
    int32_t angle  = fixmath_wrap_cdeg(angle_cdeg);
    bool    b_negative = false;
    int32_t result = 0;

    if (angle < 0)
    {
        angle = -angle;
        b_negative = true;
    }

    /* sin(180 - a) = sin(a): fold (90, 180] onto [0, 90). */
    if (angle > FIXMATH_CDEG_QUARTER)
    {
        angle = FIXMATH_CDEG_HALF_TURN - angle;
    }

    result = fixmath_sin_first_quarter(angle);

    return (b_negative ? -result : result);
}

/* cos(angle) in Q15, any angle in centi-degrees. */
int32_t
fixmath_cos_q15 (int32_t angle_cdeg)
{
    return (fixmath_sin_q15(fixmath_wrap_cdeg(angle_cdeg +
                                              FIXMATH_CDEG_QUARTER)));
}

/* value * ratio_q15 / 32768, rounded to nearest. */
int32_t
fixmath_mul_q15 (int32_t value, int32_t ratio_q15)
{
    int64_t const product    = (int64_t) value * (int64_t) ratio_q15;
    bool const    b_negative = (product < 0);
    uint64_t      magnitude  = b_negative ? (uint64_t) (-product)
                                          : (uint64_t) product;

    magnitude = (magnitude + FIXMATH_Q15_HALF) >> FIXMATH_Q15_SHIFT;

    /* |value| < 2^31 and |ratio| <= 2^15, so the result fits int32. */
    return (b_negative ? -(int32_t) magnitude : (int32_t) magnitude);
}

/* Integer square root, rounded down. */
uint32_t
fixmath_isqrt (uint32_t value)
{
    uint32_t remainder = value;
    uint32_t root      = 0u;
    uint32_t place     = 0x40000000u;  /* highest power of four */

    while (place > remainder)
    {
        place >>= 2u;
    }

    while (0u != place)
    {
        if (remainder >= (root + place))
        {
            remainder -= (root + place);
            root = (root >> 1u) + place;
        }
        else
        {
            root >>= 1u;
        }

        place >>= 2u;
    }

    return (root);
}

/*
 * Linear interpolation in a table of points sorted by x.  Below the first
 * point the first y is returned; above the last point, the last y.
 */
int32_t
fixmath_interp (int32_t x_value, int32_t const * p_xs,
                int32_t const * p_ys, uint32_t count)
{
    int32_t result = 0;

    if ((NULL != p_xs) && (NULL != p_ys) && (count > 0u))
    {
        result = p_ys[0];

        if (x_value >= p_xs[count - 1u])
        {
            result = p_ys[count - 1u];
        }
        else
        {
            for (uint32_t idx = 1u; idx < count; idx++)
            {
                if ((x_value >= p_xs[idx - 1u]) && (x_value < p_xs[idx]))
                {
                    int32_t const span_x = p_xs[idx] - p_xs[idx - 1u];
                    int32_t const span_y = p_ys[idx] - p_ys[idx - 1u];

                    result = p_ys[idx - 1u] +
                             fixmath_div_round((x_value - p_xs[idx - 1u]) *
                                               span_y, span_x);
                    break;
                }
            }
        }
    }

    return (result);
}

/* numerator / denominator rounded to nearest (halves away from zero). */
int32_t
fixmath_div_round (int32_t numerator, int32_t denominator)
{
    int32_t result = 0;

    if (0 != denominator)
    {
        bool const b_negative = ((numerator < 0) != (denominator < 0));
        uint32_t const num = (uint32_t) fixmath_abs(numerator);
        uint32_t const den = (uint32_t) fixmath_abs(denominator);
        uint32_t const quotient = (num + (den / 2u)) / den;

        /* quotient <= |numerator| < 2^31, so it fits int32. */
        result = b_negative ? -(int32_t) quotient : (int32_t) quotient;
    }

    return (result);
}

/* Shift right of a signed value without signed bit ops; rounds towards
   zero, which keeps the CORDIC steps symmetric. */
static int32_t
fixmath_shift_right (int32_t value, uint32_t shift)
{
    int32_t result = value;

    if (0u != shift)
    {
        if (value >= 0)
        {
            result = (int32_t) ((uint32_t) value >> shift);
        }
        else
        {
            /* Unsigned negation is defined even for INT32_MIN, and after
               a shift of at least 1 the magnitude fits in int32. */
            uint32_t const magnitude = 0u - (uint32_t) value;

            result = -(int32_t) (magnitude >> shift);
        }
    }

    return (result);
}

/* sin for 0..9000 centi-degrees by table lookup and interpolation. */
static int32_t
fixmath_sin_first_quarter (int32_t angle_cdeg)
{
    int32_t const index    = angle_cdeg / FIXMATH_CDEG_PER_STEP;
    int32_t const fraction = angle_cdeg % FIXMATH_CDEG_PER_STEP;
    int32_t       result   = g_sin_table[FIXMATH_SIN_STEPS - 1];

    if (index < (FIXMATH_SIN_STEPS - 1))
    {
        int32_t const low  = g_sin_table[index];
        int32_t const high = g_sin_table[index + 1];

        result = low + fixmath_div_round((high - low) * fraction,
                                         FIXMATH_CDEG_PER_STEP);
    }

    return (result);
}

/*** end of file ***/
