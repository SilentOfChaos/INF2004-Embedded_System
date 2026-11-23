/*
 *  subsystem4_test.c
 *  On-target tests for subsystem 4.  Built only with: make TEST=1
 *
 *  Record results in testing/subsystem4/TEST_LOG.md.  On-target IDs start
 *  at S4-U20; S4-U01..S4-U19 are the host tests (testing/host/).
 *  These checks never latch a fault, so the test build can run on the
 *  bench with the wheels off the ground.
 */

#include "../test_report.h"
#include "subsystem4.h"

#if APP_TEST

#define S4_TEST_SETTLE_MS   1500u   /* IMU start-up + 1 s calibration */

EXPORT void
subsystem4_test (void)
{
    T_RFLG rflg = { 0 };
    T_RMBF rmbf = { 0 };

    TEST_INIT();

    (void) tk_dly_tsk(S4_TEST_SETTLE_MS);
    (void) tk_ref_flg(flg_safety, &rflg);

    /* S4-U20: no fault is latched after start-up on a still car. */
    TEST_CHECK("S4-U20", !safety_is_faulted(),
               "no fault latched after start-up");

    /* S4-U21: IMU identified, calibrated and healthy (IMU_OK level). */
    TEST_CHECK("S4-U21", 0u != (rflg.flgptn & SAFETY_FLG_IMU_OK),
               "GY-511 identified and calibrated");

    /* S4-U22: the fault queue exists and Safety keeps it drained. */
    TEST_CHECK("S4-U22",
               (E_OK == tk_ref_mbf(mbf_fault, &rmbf)) && (0 == rmbf.msgsz),
               "fault queue exists and is drained");

    TEST_SUMMARY("subsystem4");
}

#endif /* APP_TEST */

/*** end of file ***/
