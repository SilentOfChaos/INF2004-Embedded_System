/*
 *  subsystem3_test.c
 *  On-target tests for subsystem 3.  Built only with: make TEST=1
 *
 *  Each TEST_CHECK prints a PASS/FAIL line on the console.  Save the
 *  console output to testing/subsystem3/logs/ and record the result in
 *  testing/subsystem3/TEST_LOG.md.  On-target IDs start at S3-U20;
 *  S3-U01..S3-U19 are the host tests (testing/host/).
 */

#include "../common/app_time.h"
#include "../test_report.h"
#include "subsystem3.h"

#if APP_TEST

#define S3_TEST_WINDOW_MS       1000u
#define S3_TEST_PERIOD_US       ((uint32_t) CFG_PERIOD_LINE_MS * 1000u)
#define S3_TEST_RATE_TOL_PCT    5u      /* +/-5% of the expected count */
#define S3_TEST_PCT             100u
#define S3_TEST_ADC_MIN         5u
#define S3_TEST_ADC_MAX         4090u

EXPORT void
subsystem3_test (void)
{
    line_err_t first;
    line_err_t last;
    ER         ercd_first = E_OK;
    ER         ercd_last  = E_OK;
    uint32_t   start_us   = 0u;
    uint32_t   window_us  = 0u;
    uint32_t   expected   = 0u;
    uint32_t   samples    = 0u;

    TEST_INIT();

    /* S3-U20: the samples counted over a measured window of about 1 s
       match a 5 ms period on average.  A count shows the average rate
       only, not each interval: the 5 +/- 1 ms of NFR1 is S3-U33, with
       the timing probe.  The record counts samples modulo 256
       (line_msgs.h): this window holds about 200. */
    start_us   = app_time_us();
    ercd_first = line_err_read(&first);
    (void) tk_dly_tsk(S3_TEST_WINDOW_MS);
    ercd_last  = line_err_read(&last);
    window_us  = app_time_us() - start_us;
    expected   = window_us / S3_TEST_PERIOD_US;
    samples    = line_err_samples(&last, &first);
    tm_printf((UB *) "S3-U20: %d samples in %d us\n", (INT) samples,
              (INT) window_us);
    TEST_CHECK("S3-U20",
               (E_OK == ercd_first) && (E_OK == ercd_last) &&
               ((samples * S3_TEST_PCT) >=
                (expected * (S3_TEST_PCT - S3_TEST_RATE_TOL_PCT))) &&
               ((samples * S3_TEST_PCT) <=
                (expected * (S3_TEST_PCT + S3_TEST_RATE_TOL_PCT))),
               "line sampled at a 5 ms period on average over 1 s "
               "(NFR1 rate; each interval: S3-U33)");

    /* S3-U21: both ADC readings are inside the plausible range. */
    TEST_CHECK("S3-U21",
               line_err_valid(&last) &&
               (line_err_raw_l(&last) >= S3_TEST_ADC_MIN) &&
               (line_err_raw_l(&last) <= S3_TEST_ADC_MAX) &&
               (line_err_raw_r(&last) >= S3_TEST_ADC_MIN) &&
               (line_err_raw_r(&last) <= S3_TEST_ADC_MAX),
               "line sensors give plausible ADC readings");

    /* S3-U22: the line error stays inside -1000..1000. */
    TEST_CHECK("S3-U22",
               (line_err_error_pm(&last) >= -1000) &&
               (line_err_error_pm(&last) <= 1000),
               "line error within -1000..1000");

    TEST_SUMMARY("subsystem3");
}

#endif /* APP_TEST */

/*** end of file ***/
