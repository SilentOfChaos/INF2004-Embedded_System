/*
 *  navigation_test.c
 *  On-target tests for Navigation.  Built only with: make TEST=1
 *
 *  Record results in testing/navigation/TEST_LOG.md.  On-target IDs start
 *  at NAV-U20; NAV-U01..NAV-U19 are the host tests (testing/host/).
 */

#include "../test_report.h"
#include "navigation.h"

#if APP_TEST

#define NAV_TEST_SETTLE_MS      100u
#define NAV_TEST_POLLS          20u     /* 20 x 1 ms: four Navigation ticks */

/*
 * True when a message buffer exists and is seen empty at least once in
 * the polling window.  This test runs in the initial task (priority 1,
 * core 0), so a message posted from core 1 can wait for Navigation for
 * a moment; a single look could catch it and fail for no reason.
 */
static bool
navigation_test_drained (ID mbfid)
{
    bool b_empty = false;

    for (uint32_t poll = 0u; (poll < NAV_TEST_POLLS) && !b_empty; poll++)
    {
        T_RMBF   rmbf = { 0 };
        ER const ercd = tk_ref_mbf(mbfid, &rmbf);

        if (E_OK != ercd)
        {
            break;                      /* the queue does not exist */
        }

        b_empty = (0 == rmbf.msgsz);

        if (!b_empty)
        {
            (void) tk_dly_tsk(1);       /* let Navigation run */
        }
    }

    return (b_empty);
}

EXPORT void
navigation_test (void)
{
    T_RFLG rflg = { 0 };

    TEST_INIT();

    (void) tk_dly_tsk(NAV_TEST_SETTLE_MS);

    /* NAV-U20: every event queue exists and Navigation keeps it drained. */
    TEST_CHECK("NAV-U20",
               navigation_test_drained(mbf_line_evt) &&
               navigation_test_drained(mbf_imu_evt) &&
               navigation_test_drained(mbf_obstacle) &&
               navigation_test_drained(mbf_motion_sts) &&
               navigation_test_drained(mbf_nav_cmd),
               "Navigation's event queues exist and are drained");

    /* NAV-U21: Navigation's event flag exists. */
    TEST_CHECK("NAV-U21", E_OK == tk_ref_flg(flg_nav, &rflg),
               "Navigation's event flag exists");

    TEST_SUMMARY("navigation");
}

#endif /* APP_TEST */

/*** end of file ***/
