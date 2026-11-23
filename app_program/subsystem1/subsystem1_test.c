/*
 *  subsystem1_test.c
 *  On-target tests for subsystem 1.  Built only with: make TEST=1
 *
 *  Record results in testing/subsystem1/TEST_LOG.md.  On-target IDs start
 *  at S1-U20; S1-U01..S1-U19 are the host tests (testing/host/).
 */

#include "../test_report.h"
#include "comms.h"
#include "subsystem1.h"

#if APP_TEST

#define S1_TEST_SETTLE_MS   500u    /* more than two telemetry ticks */
#define S1_TEST_POLL_MS     10u
#define S1_TEST_POLLS       50u     /* 500 ms: more than two drains */

/*
 * S1-U20 helper: mbf_tlm exists and is seen empty at least once within
 * the window.  Status records wait in it until Telemetry's next 200 ms
 * tick, so a single look at a random moment can find one and prove
 * nothing; a queue that is never empty is not being drained.
 */
static bool
subsystem1_test_drained (void)
{
    bool b_empty = false;

    for (uint32_t poll = 0u; (poll < S1_TEST_POLLS) && !b_empty; poll++)
    {
        T_RMBF rmbf = { 0 };

        if (E_OK != tk_ref_mbf(mbf_tlm, &rmbf))
        {
            break;                      /* the queue does not exist */
        }

        b_empty = (0 == rmbf.msgsz);

        if (!b_empty)
        {
            (void) tk_dly_tsk(S1_TEST_POLL_MS);
        }
    }

    return (b_empty);
}

EXPORT void
subsystem1_test (void)
{
    T_RMBF rmbf = { 0 };
    T_RFLG rflg = { 0 };

    TEST_INIT();

    (void) tk_dly_tsk(S1_TEST_SETTLE_MS);

    /* S1-U20: Telemetry drains its input queue every 200 ms. */
    TEST_CHECK("S1-U20", subsystem1_test_drained(),
               "telemetry queue exists and is drained");

    /* S1-U21: every subsystem 1 object exists. */
    TEST_CHECK("S1-U21",
               (E_OK == tk_ref_mbf(mbf_publish, &rmbf)) &&
               (E_OK == tk_ref_mbf(mbf_cmd_in, &rmbf)) &&
               (E_OK == tk_ref_mbf(mbf_mqtt_cfg, &rmbf)) &&
               (E_OK == tk_ref_flg(g_flg_comms, &rflg)),
               "publish, command and broker queues and flag exist");

    /* S1-U22: with no broker (MQTT=0 build or broker off) nothing waits
       in the publish queue (NFR10: offline messages are not queued). */
    TEST_CHECK("S1-U22",
               comms_mqtt_up() ||
               ((E_OK == tk_ref_mbf(mbf_publish, &rmbf)) &&
                (0 == rmbf.msgsz)),
               "nothing queued for MQTT while the session is down");

    TEST_SUMMARY("subsystem1");
}

#endif /* APP_TEST */

/*** end of file ***/
