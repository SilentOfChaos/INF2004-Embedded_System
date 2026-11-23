/*
 *  subsystem2_test.c
 *  On-target tests for subsystem 2.  Built only with: make TEST=1
 *
 *  These checks never drive the motors (the test build may run on the
 *  bench).  Record results in testing/subsystem2/TEST_LOG.md.
 *  On-target IDs start at S2-U20; S2-U01..S2-U19 are the host tests.
 */

#include "../test_report.h"
#include "subsystem2.h"

#if APP_TEST

#define S2_TEST_WINDOW_MS   1000u
#define S2_TEST_POLL_MS     5u
#define S2_TEST_POLLS       20u     /* 100 ms: five Motion cycles */

/*
 * S2-U20 helper: the command queue exists and is seen empty at least
 * once in the window.  A command can wait up to 20 ms for Motion's next
 * cycle, so one look at a random moment proves nothing.
 */
static bool
subsystem2_test_drained (void)
{
    bool b_empty = false;

    for (uint32_t poll = 0u; (poll < S2_TEST_POLLS) && !b_empty; poll++)
    {
        T_RMBF rmbf = { 0 };

        if (E_OK != tk_ref_mbf(mbf_motion_cmd, &rmbf))
        {
            break;                      /* the queue does not exist */
        }

        b_empty = (0 == rmbf.msgsz);

        if (!b_empty)
        {
            (void) tk_dly_tsk(S2_TEST_POLL_MS);
        }
    }

    return (b_empty);
}

EXPORT void
subsystem2_test (void)
{
    T_RFLG rflg = { 0 };

    TEST_INIT();

    /* S2-U20: the command queue exists and Motion keeps it drained. */
    (void) tk_dly_tsk(S2_TEST_WINDOW_MS);
    TEST_CHECK("S2-U20", subsystem2_test_drained(),
               "motion command queue exists and is drained");

    /* S2-U21: Motion's event flag exists (cyclic release and reset). */
    TEST_CHECK("S2-U21", E_OK == tk_ref_flg(flg_motion, &rflg),
               "motion event flag exists");

    TEST_SUMMARY("subsystem2");
}

#endif /* APP_TEST */

/*** end of file ***/
