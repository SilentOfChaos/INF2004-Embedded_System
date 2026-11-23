/*
 *  navigation.c
 *  Navigation (team-shared): kernel objects, nav_post() and the health
 *  levels.  The task itself is in nav_task.c, the statechart in nav_sm.c.
 */

#include "../common/app_mbf.h"
#include "../common/app_stats.h"
#include "../subsystem1/tlm_msgs.h"
#include "../subsystem2/motion_msgs.h"
#include "../subsystem3/line_msgs.h"
#include "../subsystem4/subsystem4.h"
#include "../subsystem5/scan_msgs.h"
#include "nav_task.h"
#include "navigation.h"

/* Queue depths (messages).  Navigation drains every queue every 5 ms;
   the depth covers bursts.  [DECISION D-59]  Each is the number of
   messages the buffer really held before the minimum-bit change (16, 16,
   8, 16 and 8 were asked for, with 8 bytes per message to spare), kept
   so a burst overflows exactly as before; the bytes now follow from the
   message size (common/app_mbf.h, D-83). */
#define NAV_DEPTH_LINE          21u
#define NAV_DEPTH_IMU           17u
#define NAV_DEPTH_OBSTACLE      9u
#define NAV_DEPTH_MOTION        18u
#define NAV_DEPTH_CMD           10u

EXPORT ID flg_nav        = 0;
EXPORT ID mbf_line_evt   = 0;
EXPORT ID mbf_imu_evt    = 0;
EXPORT ID mbf_obstacle   = 0;
EXPORT ID mbf_motion_sts = 0;
EXPORT ID mbf_nav_cmd    = 0;

static char const * const g_mode_names[NAV_MODE_COUNT] =
{
    "BOOT", "SELF-TEST", "READY", "LINE FOLLOWING", "HUMP CROSSING",
    "LINE SEARCH", "AT JUNCTION", "AVOIDING OBSTACLE", "SAFE HOLD",
    "FAULT", "COMPLETE"
};

static ID navigation_make_mbf(INT msg_size, uint32_t depth);

/* Create Navigation's objects (usermain phase 1). */
EXPORT ER
navigation_init (void)
{
    T_CFLG cflg = { 0 };
    ER     ercd = E_OK;

    cflg.flgatr  = TA_TFIFO | TA_WMUL;
    cflg.iflgptn = 0u;
    flg_nav = tk_cre_flg(&cflg);

    mbf_line_evt   = navigation_make_mbf((INT) sizeof(line_evt_t),
                                         NAV_DEPTH_LINE);
    mbf_imu_evt    = navigation_make_mbf((INT) sizeof(imu_evt_t),
                                         NAV_DEPTH_IMU);
    mbf_obstacle   = navigation_make_mbf((INT) sizeof(obstacle_evt_t),
                                         NAV_DEPTH_OBSTACLE);
    mbf_motion_sts = navigation_make_mbf((INT) sizeof(motion_sts_t),
                                         NAV_DEPTH_MOTION);
    mbf_nav_cmd    = navigation_make_mbf((INT) sizeof(nav_cmd_t),
                                         NAV_DEPTH_CMD);
    app_stats_queue("line_evt", mbf_line_evt);
    app_stats_queue("imu_evt", mbf_imu_evt);
    app_stats_queue("obstacle", mbf_obstacle);
    app_stats_queue("motion_sts", mbf_motion_sts);
    app_stats_queue("nav_cmd", mbf_nav_cmd);

    if (flg_nav < E_OK)
    {
        ercd = flg_nav;
    }
    else if ((mbf_line_evt < E_OK) || (mbf_imu_evt < E_OK) ||
             (mbf_obstacle < E_OK) || (mbf_motion_sts < E_OK) ||
             (mbf_nav_cmd < E_OK))
    {
        ercd = E_NOMEM;
    }
    else
    {
        ercd = nav_task_init();
    }

    return (ercd);
}

/* Start the Navigation task (usermain phase 2). */
EXPORT ER
navigation_start (void)
{
    return (nav_task_start());
}

/*
 * Post an event to one of Navigation's queues.  Never blocks.  A full
 * queue is counted by Safety and treated as a fault (report section 4):
 * a lost junction or letter would be unsafe.
 */
ER
nav_post (ID mbfid, void const * p_msg, INT size, uint8_t source)
{
    ER ercd = tk_snd_mbf(mbfid, (void *) p_msg, size, TMO_POL);

    if (E_OK == ercd)
    {
        (void) tk_set_flg(flg_nav, NAV_FLG_EVENT);
    }
    else
    {
        /* Object IDs are small positive numbers, so they fit 16 bits. */
        safety_report_fault(FAULT_CAUSE_QUEUE_FULL, source,
                            (uint16_t) mbfid);
    }

    return (ercd);
}

/* Press START remotely (dashboard "start" command): Navigation reads and
   clears the bit at its next tick and treats it as a GP20 press. */
void
nav_request_start (void)
{
    (void) tk_set_flg(flg_nav, NAV_FLG_REMOTE_START);
}

/* Set or clear a health level checked in SELF-TEST. */
void
nav_set_health (UINT health_bit, bool b_ok)
{
    if (b_ok)
    {
        (void) tk_set_flg(flg_nav, health_bit);
    }
    else
    {
        (void) tk_clr_flg(flg_nav, ~health_bit);
    }
}

/* Name of a mode for the console and telemetry. */
char const *
nav_mode_name (nav_mode_t mode)
{
    char const * p_name = "?";

    if (mode < NAV_MODE_COUNT)
    {
        p_name = g_mode_names[mode];
    }

    return (p_name);
}

static ID
navigation_make_mbf (INT msg_size, uint32_t depth)
{
    T_CMBF cmbf = { 0 };

    cmbf.mbfatr = TA_TFIFO;
    cmbf.maxmsz = msg_size;
    /* Exactly depth messages; small values, no overflow. */
    cmbf.bufsz  = (SZ) APP_MBF_BYTES(depth, msg_size);

    return (tk_cre_mbf(&cmbf));
}

/*** end of file ***/
