/*
 *  tlm_format.h
 *  Buddy 1's telemetry messages (report 6.1: short JSON records, each
 *  with a sequence number "seq" and a time stamp "t" in ms):
 *
 *    telemetry  5 Hz (NFR9): state, wheel speeds, encoder counts,
 *               distance, line sensors, barcodes, hump and obstacle
 *               data, motion class and turn rate (FR12, FR17)
 *    status     1 Hz heartbeat (NFR9) with link and fault state
 *    event      route-log entries (FR13), obstacle profiles (FR10),
 *               faults, motion events, run summary (FR14, FR18)
 *    ack        the answer to each command (report 6.1)
 *
 *  The aggregated state lives in the Telemetry task; these functions
 *  only format.  Each returns the JSON length, or 0 if it did not fit.
 *
 *  Pure C, no kernel headers: tested on the host (testing/host/).
 */

#ifndef TLM_FORMAT_H
#define TLM_FORMAT_H

#include <stdbool.h>
#include <stdint.h>

#include "../app_config.h"
#include "tlm_msgs.h"

/* A route-log entry: the packed entry (IF-12) and when it happened. */
typedef struct
{
    tlm_route_t entry;
    uint32_t    time_ms;
} tlm_log_t;

/* What Telemetry keeps.  The status records are kept packed, as they
   arrive (IF-12, D-92). */
typedef struct
{
    tlm_line_t   line;
    tlm_motion_t motion;
    tlm_imu_t    imu;
    tlm_scan_t   scan;
    tlm_nav_t    nav;
    tlm_safety_t safety;
    tlm_link_t   link;
    char const * p_mode_name;       /* name of nav.mode */
    bool         b_run_started;     /* a ROUTE_RUN_START was seen */
    uint32_t     run_start_ms;      /* ROUTE_RUN_START time */
    int32_t      run_start_mm;      /* distance at the start */
    uint32_t     run_end_ms;        /* time of the end entry */
    int32_t      run_end_mm;        /* distance at the end */
    uint8_t      end_kind;          /* ROUTE_xxx that ended the run */
    uint16_t     barcodes;
    uint16_t     turns;
    uint16_t     humps;
    int32_t      max_hump_mm;
    uint16_t     obstacles;
    uint32_t     dropped;           /* messages that could not be sent */
    tlm_log_t    log[CFG_TLM_ROUTE_MAX];
    uint32_t     log_count;
} tlm_state_t;

void     tlm_state_init(tlm_state_t * p_state);
int32_t  tlm_state_route(tlm_state_t * p_state, tlm_route_t const * p_route,
                         uint32_t time_ms);

uint32_t tlm_format_telemetry(tlm_state_t const * p_state, uint32_t seq,
                              uint32_t time_ms, char * p_buf,
                              uint32_t size);
uint32_t tlm_format_heartbeat(tlm_state_t const * p_state, uint32_t seq,
                              uint32_t time_ms, uint32_t beat,
                              char * p_buf, uint32_t size);
uint32_t tlm_format_route(tlm_route_t const * p_route, uint32_t seq,
                          uint32_t time_ms, int32_t log_index,
                          bool b_replay, char * p_buf, uint32_t size);
uint32_t tlm_format_ack(tlm_ack_t const * p_ack, uint32_t seq,
                        uint32_t time_ms, char * p_buf, uint32_t size);
uint32_t tlm_format_fault(tlm_fault_t const * p_fault, uint32_t seq,
                          uint32_t time_ms, char * p_buf, uint32_t size);
uint32_t tlm_format_motion_event(uint8_t motion_class, int32_t rate_dps,
                                 uint32_t seq, uint32_t time_ms,
                                 char * p_buf, uint32_t size);
uint32_t tlm_format_link(tlm_link_t const * p_link, uint32_t seq,
                         uint32_t time_ms, char * p_buf, uint32_t size);
uint32_t tlm_format_obstacle(tlm_obstacle_t const * p_obstacle,
                             uint32_t seq, uint32_t time_ms, char * p_buf,
                             uint32_t size);
uint32_t tlm_format_summary(tlm_state_t const * p_state, uint32_t seq,
                            uint32_t time_ms, char * p_buf, uint32_t size);

char const * tlm_class_name(uint8_t motion_class);
char const * tlm_cause_name(uint8_t cause);
char const * tlm_source_name(uint8_t source);
char const * tlm_cmd_name(uint8_t cmd_code);

#endif /* TLM_FORMAT_H */

/*** end of file ***/
