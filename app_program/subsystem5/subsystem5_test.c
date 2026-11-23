/*
 *  subsystem5_test.c
 *  On-target tests for subsystem 5.  Built only with: make TEST=1
 *
 *  Record results in testing/subsystem5/TEST_LOG.md.  On-target IDs start
 *  at S5-U20; S5-U01..S5-U19 are the host tests (testing/host/).
 */

#include "../navigation/navigation.h"
#include "../test_report.h"
#include "subsystem5.h"

#if APP_TEST

#define S5_TEST_SETTLE_MS   500u    /* several 65 ms health pings */
#define S5_TEST_POLL_MS     10u
#define S5_TEST_POLLS       20u     /* 200 ms: three scan periods */

/*
 * S5-U21 helper: the request queue exists and is seen empty at least
 * once in the window (a request can wait up to 65 ms for the task).
 */
static bool
subsystem5_test_drained (void)
{
    bool b_empty = false;

    for (uint32_t poll = 0u; (poll < S5_TEST_POLLS) && !b_empty; poll++)
    {
        T_RMBF rmbf = { 0 };

        if (E_OK != tk_ref_mbf(mbf_scan_req, &rmbf))
        {
            break;                      /* the queue does not exist */
        }

        b_empty = (0 == rmbf.msgsz);

        if (!b_empty)
        {
            (void) tk_dly_tsk(S5_TEST_POLL_MS);
        }
    }

    return (b_empty);
}

EXPORT void
subsystem5_test (void)
{
    T_RFLG rflg = { 0 };

    TEST_INIT();

    (void) tk_dly_tsk(S5_TEST_SETTLE_MS);
    (void) tk_ref_flg(flg_nav, &rflg);

    /* S5-U20: the ultrasonic sensor answers its pings, a valid range or
       not (A5, D-104). */
    TEST_CHECK("S5-U20", 0u != (rflg.flgptn & NAV_FLG_SCAN_OK),
               "HC-SR04P answers its pings");

    /* S5-U21: the request queue exists and is drained. */
    TEST_CHECK("S5-U21", subsystem5_test_drained(),
               "scan request queue exists and is drained");

    TEST_SUMMARY("subsystem5");
}

#endif /* APP_TEST */

/*** end of file ***/
