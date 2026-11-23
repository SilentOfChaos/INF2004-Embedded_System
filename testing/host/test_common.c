/*
 *  test_common.c
 *  Host tests for the team's shared helpers: fixed-point maths
 *  (app_program/common/fixmath.c) and bit packing (common/bitpack.c,
 *  common/app_mbf.h).  Results: testing/common/TEST_LOG.md.
 *
 *  The expected packed words below were worked out by hand (and checked
 *  with a separate script), not taken from bitpack.c, so a change of
 *  layout or of a rule shows up here.
 */

#include <math.h>
#include <stdlib.h>

#include "common/app_mbf.h"
#include "common/bitpack.h"
#include "common/fixmath.h"
#include "unit.h"

/* The compile-time helpers work in a constant expression. */
BITPACK_ASSERT(12u == BITPACK_BITS_FOR(4095u), adc_bits);
BITPACK_ASSERT(11u == BITPACK_BITS_FOR_SIGNED(1000u), error_bits);
BITPACK_ASSERT(42u == BITPACK_ARRAY_WORDS(64u, 21u), array_words);

#define TEST_PI         3.14159265358979323846
#define TEST_CDEG_RAD   (TEST_PI / 18000.0)

static int32_t
test_round (double value)
{
    return ((int32_t) ((value >= 0.0) ? (value + 0.5) : (value - 0.5)));
}

static void
test_wrap_clamp_abs (void)
{
    UNIT_BEGIN("COM-U01", "clamp, abs and angle wrapping");
    UNIT_EQ(fixmath_clamp(5, 0, 10), 5);
    UNIT_EQ(fixmath_clamp(-5, 0, 10), 0);
    UNIT_EQ(fixmath_clamp(15, 0, 10), 10);
    UNIT_EQ(fixmath_abs(-7), 7);
    UNIT_EQ(fixmath_abs(INT32_MIN), INT32_MAX);
    UNIT_EQ(fixmath_wrap_cdeg(18000), 18000);
    UNIT_EQ(fixmath_wrap_cdeg(-18000), 18000);
    UNIT_EQ(fixmath_wrap_cdeg(36000), 0);
    UNIT_EQ(fixmath_wrap_cdeg(27000), -9000);
    UNIT_EQ(fixmath_wrap_cdeg(-27000), 9000);
    UNIT_EQ(fixmath_wrap_cdeg(-17999), -17999);
    UNIT_EQ(fixmath_wrap_cdeg(72100), 100);
    UNIT_END();
}

static void
test_atan2 (void)
{
    int32_t worst = 0;

    UNIT_BEGIN("COM-U02", "atan2 within 0.03 degree over the full circle");

    for (int32_t deg = -179; deg <= 180; deg++)
    {
        double const  rad   = (double) deg * 100.0 * TEST_CDEG_RAD;
        int32_t const x_val = test_round(10000.0 * cos(rad));
        int32_t const y_val = test_round(10000.0 * sin(rad));
        int32_t const error = fixmath_wrap_cdeg(
            fixmath_atan2_cdeg(y_val, x_val) - (deg * 100));

        worst = (abs(error) > worst) ? abs(error) : worst;
    }

    UNIT_CHECK(worst <= 3);
    UNIT_EQ(fixmath_atan2_cdeg(0, 0), 0);
    UNIT_EQ(fixmath_atan2_cdeg(0, 5), 0);
    UNIT_EQ(fixmath_atan2_cdeg(0, -5), 18000);
    UNIT_CHECK(abs(fixmath_atan2_cdeg(INT32_MAX, INT32_MAX) - 4500) <= 3);
    UNIT_CHECK(abs(fixmath_atan2_cdeg(-INT32_MAX, -INT32_MAX) + 13500) <= 3);
    UNIT_END();
}

static void
test_sin_cos (void)
{
    int32_t worst = 0;

    UNIT_BEGIN("COM-U03", "sin and cos within 3/32768 for any angle");

    for (int32_t cdeg = -40000; cdeg <= 40000; cdeg += 7)
    {
        double const  rad   = (double) cdeg * TEST_CDEG_RAD;
        int32_t const e_sin = fixmath_sin_q15(cdeg) -
                              test_round(32767.0 * sin(rad));
        int32_t const e_cos = fixmath_cos_q15(cdeg) -
                              test_round(32767.0 * cos(rad));

        worst = (abs(e_sin) > worst) ? abs(e_sin) : worst;
        worst = (abs(e_cos) > worst) ? abs(e_cos) : worst;
    }

    UNIT_CHECK(worst <= 3);
    UNIT_EQ(fixmath_sin_q15(9000), 32767);
    UNIT_EQ(fixmath_sin_q15(-9000), -32767);
    UNIT_EQ(fixmath_cos_q15(0), 32767);
    UNIT_EQ(fixmath_sin_q15(0), 0);
    UNIT_END();
}

static void
test_mul_div (void)
{
    UNIT_BEGIN("COM-U04", "Q15 multiply and division round to nearest");
    UNIT_EQ(fixmath_mul_q15(1000, 16384), 500);
    UNIT_EQ(fixmath_mul_q15(-1000, 16384), -500);
    UNIT_EQ(fixmath_mul_q15(3, 16384), 2);
    UNIT_EQ(fixmath_mul_q15(-3, 16384), -2);
    UNIT_EQ(fixmath_mul_q15(4000, 32767), 4000);
    UNIT_EQ(fixmath_div_round(5, 2), 3);
    UNIT_EQ(fixmath_div_round(-5, 2), -3);
    UNIT_EQ(fixmath_div_round(5, -2), -3);
    UNIT_EQ(fixmath_div_round(4, 3), 1);
    UNIT_EQ(fixmath_div_round(-4, 3), -1);
    UNIT_EQ(fixmath_div_round(7, 0), 0);
    UNIT_EQ(fixmath_div_round(INT32_MAX, 1), INT32_MAX);
    UNIT_END();
}

static void
test_isqrt (void)
{
    bool b_ok = true;

    UNIT_BEGIN("COM-U05", "integer square root rounds down");

    for (uint32_t root = 0u; root < 2000u; root++)
    {
        b_ok = b_ok && (fixmath_isqrt(root * root) == root);
        b_ok = b_ok && ((0u == root) ||
                        (fixmath_isqrt((root * root) - 1u) == (root - 1u)));
    }

    UNIT_CHECK(b_ok);
    UNIT_EQ(fixmath_isqrt(UINT32_MAX), 65535);
    UNIT_EQ(fixmath_isqrt(48000000u), 6928);
    UNIT_END();
}

static void
test_interp (void)
{
    int32_t const xs[3] = { 0, 100, 300 };
    int32_t const ys[3] = { 10, 20, 0 };

    UNIT_BEGIN("COM-U06", "table interpolation and its ends");
    UNIT_EQ(fixmath_interp(-50, xs, ys, 3u), 10);
    UNIT_EQ(fixmath_interp(0, xs, ys, 3u), 10);
    UNIT_EQ(fixmath_interp(50, xs, ys, 3u), 15);
    UNIT_EQ(fixmath_interp(100, xs, ys, 3u), 20);
    UNIT_EQ(fixmath_interp(200, xs, ys, 3u), 10);
    UNIT_EQ(fixmath_interp(300, xs, ys, 3u), 0);
    UNIT_EQ(fixmath_interp(999, xs, ys, 3u), 0);
    UNIT_EQ(fixmath_interp(5, NULL, ys, 3u), 0);
    UNIT_END();
}

static void
test_bitpack_fields (void)
{
    uint32_t word = 0u;

    UNIT_BEGIN("COM-U07", "bitpack: unsigned fields and flags, exact words, "
                          "too-large values kept at the maximum");

    /* Three fields: 12 bits at 0, 12 bits at 12, 8 bits at 24. */
    UNIT_CHECK(bitpack_set(&word, 0u, 12u, 4095u));
    UNIT_EQ(word, 0x00000FFFu);
    UNIT_CHECK(bitpack_set(&word, 12u, 12u, 0x123u));
    UNIT_EQ(word, 0x00123FFFu);
    UNIT_CHECK(bitpack_set(&word, 24u, 8u, 0xA5u));
    UNIT_EQ(word, 0xA5123FFFu);
    UNIT_EQ(bitpack_get(word, 0u, 12u), 4095u);
    UNIT_EQ(bitpack_get(word, 12u, 12u), 0x123u);
    UNIT_EQ(bitpack_get(word, 24u, 8u), 0xA5u);

    /* Overwriting a field leaves its neighbours alone. */
    UNIT_CHECK(bitpack_set(&word, 12u, 12u, 0u));
    UNIT_EQ(word, 0xA5000FFFu);

    /* Too large: the maximum is stored and the call says so. */
    UNIT_CHECK(!bitpack_set(&word, 12u, 12u, 4096u));
    UNIT_EQ(word, 0xA5FFFFFFu);
    UNIT_CHECK(!bitpack_set(&word, 24u, 8u, UINT32_MAX));
    UNIT_EQ(bitpack_get(word, 24u, 8u), 0xFFu);

    /* A full 32-bit field. */
    UNIT_CHECK(bitpack_set(&word, 0u, 32u, 0x89ABCDEFu));
    UNIT_EQ(word, 0x89ABCDEFu);
    UNIT_EQ(bitpack_get(word, 0u, 32u), 0x89ABCDEFu);

    /* Flags. */
    word = 0u;
    bitpack_set_flag(&word, 31u, true);
    bitpack_set_flag(&word, 0u, true);
    UNIT_EQ(word, 0x80000001u);
    UNIT_CHECK(bitpack_get_flag(word, 31u));
    UNIT_CHECK(!bitpack_get_flag(word, 30u));
    bitpack_set_flag(&word, 31u, false);
    UNIT_EQ(word, 0x00000001u);
    UNIT_END();
}

static void
test_bitpack_signed (void)
{
    uint32_t word   = 0u;
    bool     b_ok   = true;

    UNIT_BEGIN("COM-U08", "bitpack: signed fields, two's complement in "
                          "their width, out of range kept at the end");

    /* An 11-bit field at bit 5: -1000..1000 fits (-1024..1023). */
    UNIT_CHECK(bitpack_set_signed(&word, 5u, 11u, -1000));
    UNIT_EQ(word, 0x00008300u);
    UNIT_CHECK(bitpack_set_signed(&word, 5u, 11u, 1000));
    UNIT_EQ(word, 0x00007D00u);
    UNIT_CHECK(bitpack_set_signed(&word, 5u, 11u, -1));
    UNIT_EQ(word, 0x0000FFE0u);
    UNIT_CHECK(bitpack_set_signed(&word, 5u, 11u, -1024));
    UNIT_EQ(word, 0x00008000u);
    UNIT_CHECK(bitpack_set_signed(&word, 5u, 11u, 1023));
    UNIT_EQ(word, 0x00007FE0u);

    /* Every value round-trips; the other bits stay set. */
    for (int32_t value = -1024; value <= 1023; value++)
    {
        word = UINT32_MAX;
        b_ok = b_ok && bitpack_set_signed(&word, 5u, 11u, value) &&
               (bitpack_get_signed(word, 5u, 11u) == value) &&
               (0x0000001Fu == (word & 0x0000001Fu)) &&
               (0xFFFF0000u == (word & 0xFFFF0000u));
    }

    UNIT_CHECK(b_ok);

    /* Out of range: the nearest end, and false. */
    UNIT_CHECK(!bitpack_set_signed(&word, 5u, 11u, 1024));
    UNIT_EQ(bitpack_get_signed(word, 5u, 11u), 1023);
    UNIT_CHECK(!bitpack_set_signed(&word, 5u, 11u, -1025));
    UNIT_EQ(bitpack_get_signed(word, 5u, 11u), -1024);
    UNIT_CHECK(!bitpack_set_signed(&word, 5u, 11u, INT32_MIN));
    UNIT_EQ(bitpack_get_signed(word, 5u, 11u), -1024);
    UNIT_CHECK(!bitpack_set_signed(&word, 5u, 11u, INT32_MAX));
    UNIT_EQ(bitpack_get_signed(word, 5u, 11u), 1023);

    /* The narrowest and widest signed fields. */
    word = 0u;
    UNIT_CHECK(bitpack_set_signed(&word, 31u, 1u, -1));
    UNIT_EQ(word, 0x80000000u);
    UNIT_EQ(bitpack_get_signed(word, 31u, 1u), -1);
    UNIT_CHECK(!bitpack_set_signed(&word, 31u, 1u, 1));
    UNIT_EQ(bitpack_get_signed(word, 31u, 1u), 0);
    UNIT_CHECK(bitpack_set_signed(&word, 0u, 31u, -1073741824));
    UNIT_EQ(bitpack_get_signed(word, 0u, 31u), -1073741824);
    UNIT_CHECK(bitpack_set_signed(&word, 0u, 31u, 1073741823));
    UNIT_EQ(bitpack_get_signed(word, 0u, 31u), 1073741823);
    UNIT_END();
}

static void
test_bitpack_wrap (void)
{
    UNIT_BEGIN("COM-U09", "bitpack: differences of wrapping counters and "
                          "time stamps");

    /* 16-bit counter across its wrap. */
    UNIT_EQ(bitpack_sub(0x0005u, 0xFFFBu, 16u), 10u);
    UNIT_EQ(bitpack_sub_signed(0x0005u, 0xFFFBu, 16u), 10);
    UNIT_EQ(bitpack_sub_signed(0xFFFBu, 0x0005u, 16u), -10);
    UNIT_EQ(bitpack_sub_signed(0x8000u, 0x0000u, 16u), -32768);
    UNIT_EQ(bitpack_sub_signed(0x7FFFu, 0x0000u, 16u), 32767);
    UNIT_EQ(bitpack_sub(0x1234u, 0x1234u, 16u), 0u);

    /* Bits above the width are ignored. */
    UNIT_EQ(bitpack_sub(0xABCD0005u, 0x1234FFFBu, 16u), 10u);

    /* 31-bit time stamps across their wrap. */
    UNIT_EQ(bitpack_sub(0x00000100u, 0x7FFFFF00u, 31u), 0x200u);
    UNIT_EQ(bitpack_sub(0x7FFFFFFFu, 0x00000000u, 31u), 0x7FFFFFFFu);

    /* 8-bit sample counter: 200 samples later, across the wrap. */
    UNIT_EQ(bitpack_sub((250u + 200u) & 0xFFu, 250u, 8u), 200u);
    UNIT_END();
}

static void
test_bitpack_array (void)
{
    uint32_t words[BITPACK_ARRAY_WORDS(64u, 21u)];
    uint32_t small[3] = { 0u, 0u, 0u };
    uint32_t map[BITPACK_ARRAY_WORDS(26u, 3u)] = { 0u, 0u, 0u };
    bool     b_ok = true;

    UNIT_BEGIN("COM-U10", "bitpack: bit arrays across word boundaries, "
                          "exact words");

    /* Four 21-bit fields, the second and fourth crossing a word. */
    UNIT_CHECK(bitpack_array_set(small, 0u, 21u, 0x1FFFFFu));
    UNIT_CHECK(bitpack_array_set(small, 1u, 21u, 0x000001u));
    UNIT_CHECK(bitpack_array_set(small, 2u, 21u, 0x155555u));
    UNIT_CHECK(bitpack_array_set(small, 3u, 21u, 0x0AAAAAu));
    UNIT_EQ(small[0], 0x003FFFFFu);
    UNIT_EQ(small[1], 0x55555400u);
    UNIT_EQ(small[2], 0x00055555u);

    /* One field across words 0 and 1, from zero. */
    small[0] = 0u;
    small[1] = 0u;
    small[2] = 0u;
    UNIT_CHECK(bitpack_array_set(small, 1u, 21u, 0x1ABCDEu));
    UNIT_EQ(small[0], 0x9BC00000u);
    UNIT_EQ(small[1], 0x00000357u);
    UNIT_EQ(bitpack_array_get(small, 1u, 21u), 0x1ABCDEu);

    /* 64 fields round-trip, and changing one leaves the others. */
    for (uint32_t idx = 0u; idx < 64u; idx++)
    {
        b_ok = b_ok && bitpack_array_set(words, idx, 21u,
                                         ((idx * 32749u) + 12345u) &
                                         0x1FFFFFu);
    }

    for (uint32_t idx = 0u; idx < 64u; idx++)
    {
        b_ok = b_ok && (bitpack_array_get(words, idx, 21u) ==
                        (((idx * 32749u) + 12345u) & 0x1FFFFFu));
    }

    UNIT_CHECK(b_ok);
    UNIT_CHECK(bitpack_array_set(words, 30u, 21u, 0u));
    UNIT_EQ(bitpack_array_get(words, 29u, 21u),
            ((29u * 32749u) + 12345u) & 0x1FFFFFu);
    UNIT_EQ(bitpack_array_get(words, 31u, 21u),
            ((31u * 32749u) + 12345u) & 0x1FFFFFu);
    UNIT_EQ(bitpack_array_get(words, 63u, 21u),
            ((63u * 32749u) + 12345u) & 0x1FFFFFu);

    /* Too large: kept at the maximum. */
    UNIT_CHECK(!bitpack_array_set(words, 5u, 21u, 0x200000u));
    UNIT_EQ(bitpack_array_get(words, 5u, 21u), 0x1FFFFFu);

    /* 26 three-bit fields (a letter map) fit in 3 words. */
    for (uint32_t idx = 0u; idx < 26u; idx++)
    {
        (void) bitpack_array_set(map, idx, 3u, idx % 5u);
    }

    b_ok = true;

    for (uint32_t idx = 0u; idx < 26u; idx++)
    {
        b_ok = b_ok && (bitpack_array_get(map, idx, 3u) == (idx % 5u));
    }

    UNIT_CHECK(b_ok);
    UNIT_EQ(bitpack_array_get(NULL, 0u, 3u), 0u);
    UNIT_CHECK(!bitpack_array_set(map, 0u, 0u, 0u));
    UNIT_END();
}

static void
test_bitpack_sizes (void)
{
    UNIT_BEGIN("COM-U11", "bits needed for a range; kernel message buffer "
                          "bytes for a number of messages");
    UNIT_EQ(BITPACK_BITS_FOR(0u), 1u);
    UNIT_EQ(BITPACK_BITS_FOR(1u), 1u);
    UNIT_EQ(BITPACK_BITS_FOR(2u), 2u);
    UNIT_EQ(BITPACK_BITS_FOR(7u), 3u);
    UNIT_EQ(BITPACK_BITS_FOR(8u), 4u);
    UNIT_EQ(BITPACK_BITS_FOR(255u), 8u);
    UNIT_EQ(BITPACK_BITS_FOR(256u), 9u);
    UNIT_EQ(BITPACK_BITS_FOR(1000u), 10u);
    UNIT_EQ(BITPACK_BITS_FOR(4095u), 12u);
    UNIT_EQ(BITPACK_BITS_FOR(4096u), 13u);
    UNIT_EQ(BITPACK_BITS_FOR(65535u), 16u);
    UNIT_EQ(BITPACK_BITS_FOR(2097151u), 21u);
    UNIT_EQ(BITPACK_BITS_FOR(2097152u), 22u);
    UNIT_EQ(BITPACK_BITS_FOR(0x7FFFFFFFu), 31u);
    UNIT_EQ(BITPACK_BITS_FOR(0xFFFFFFFFu), 32u);
    UNIT_EQ(BITPACK_BITS_FOR_SIGNED(1000u), 11u);
    UNIT_EQ(BITPACK_BITS_FOR_SIGNED(1024u), 12u);
    UNIT_EQ(BITPACK_BITS_FOR_SIGNED(32767u), 16u);
    UNIT_EQ(BITPACK_ARRAY_WORDS(26u, 3u), 3u);
    UNIT_EQ(BITPACK_ARRAY_WORDS(1u, 32u), 1u);

    /* 4-byte header + message rounded up to 4 (kernel messagebuf.h). */
    UNIT_EQ(APP_MBF_MSG_BYTES(2u), 8u);
    UNIT_EQ(APP_MBF_MSG_BYTES(4u), 8u);
    UNIT_EQ(APP_MBF_MSG_BYTES(8u), 12u);
    UNIT_EQ(APP_MBF_MSG_BYTES(28u), 32u);
    UNIT_EQ(APP_MBF_MSG_BYTES(321u), 328u);
    UNIT_EQ(APP_MBF_BYTES(21u, 2u), 168u);
    UNIT_EQ(APP_MBF_BYTES(85u, 4u), 680u);
    UNIT_END();
}

static void
test_bitpack_word_signed (void)
{
    uint32_t seed = 0x5EED0101u;
    bool     b_ok = true;

    UNIT_BEGIN("COM-U12", "bitpack: a whole word back to int32_t without "
                          "an implementation-defined cast");
    UNIT_EQ(bitpack_word_signed(0x00000000u), 0);
    UNIT_EQ(bitpack_word_signed(0x00000001u), 1);
    UNIT_EQ(bitpack_word_signed(0xFFFFFFFFu), -1);
    UNIT_EQ(bitpack_word_signed(0xFFFFFFFEu), -2);
    UNIT_EQ(bitpack_word_signed(0x7FFFFFFFu), INT32_MAX);
    UNIT_EQ(bitpack_word_signed(0x80000000u), INT32_MIN);
    UNIT_EQ(bitpack_word_signed(0x80000001u), INT32_MIN + 1);

    for (uint32_t idx = 0u; idx < 200000u; idx++)
    {
        uint32_t x = seed;

        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        seed = x;

        /* Storing the result again gives the same word back. */
        b_ok = b_ok && ((uint32_t) bitpack_word_signed(x) == x);
    }

    UNIT_CHECK(b_ok);
    UNIT_END();
}

void
test_common (void)
{
    test_wrap_clamp_abs();
    test_atan2();
    test_sin_cos();
    test_mul_div();
    test_isqrt();
    test_interp();
    test_bitpack_fields();
    test_bitpack_signed();
    test_bitpack_wrap();
    test_bitpack_array();
    test_bitpack_sizes();
    test_bitpack_word_signed();
}

/*** end of file ***/
