/*
 *  tlm_format.c
 *  Buddy 1's telemetry messages.  See tlm_format.h.
 */

#include <stddef.h>

#include "../common/bitpack.h"
#include "../subsystem3/letter_map.h"
#include "../subsystem4/imu_msgs.h"
#include "../subsystem5/scan_msgs.h"
#include "tlm_format.h"
#include "tlm_json.h"

#define TLM_COUNT_MAX       0xFFFFu
#define TLM_PAIR            2u
#define TLM_TRIPLE          3u

static char const * const g_class_names[IMU_CLASS_COUNT] =
{
    "stationary", "accelerating", "turning", "climbing", "descending",
    "impact", "steady"
};

static char const * const g_cause_names[] =
{
    "none", "lifted", "stop", "imu_data", "self_test", "queue_full",
    "lost_record", "line_data", "motor_stall", "encoder_fault", "no_echo",
    "wifi_loss"
};

static char const * const g_source_names[] =
{
    "none", "line", "nav", "imu", "motion", "scan", "command", "telemetry",
    "safety"
};

static char const * const g_cmd_names[] =
{
    "unknown", "remap", "summary", "stop", "broker", "default_map",
    "start", "reset"
};

static char const * const g_route_names[] =
{
    "none", "start", "barcode", "turn", "hump", "obstacle", "search",
    "safe_hold", "complete", "no_command", "fault_mode", "bc_fail"
};

static char const * const g_action_names[LETTER_ACTION_COUNT] =
{
    "none", "straight", "left", "right", "uturn"
};

static char const * const g_hold_names[] =
{
    "none", "no_command", "line_not_found", "no_safe_route"
};

static char const * const g_side_names[] =
{
    "none", "left", "right"
};

#define TLM_NAMES(table)    (sizeof(table) / sizeof((table)[0]))

/* A name for every fault cause (imu_msgs.h), and "none" for 0. */
BITPACK_ASSERT(TLM_NAMES(g_cause_names) == (FAULT_CAUSE_MAX + 1u),
               tlm_cause_names);

static char const * tlm_name(char const * const * p_table, uint32_t count,
                             uint32_t index);
static void tlm_head(json_writer_t * p_json, char * p_buf, uint32_t size,
                     uint32_t seq, uint32_t time_ms);
static void tlm_bump(uint16_t * p_count);

void
tlm_state_init (tlm_state_t * p_state)
{
    if (NULL != p_state)
    {
        tlm_state_t const empty = { 0 };

        /* Empty packed records read as no letter ('-'), as before. */
        *p_state             = empty;
        p_state->p_mode_name = "BOOT";
    }
}

/*
 * Keep a route-log entry (FR13) and the run totals for the summary.
 * RUN_START begins a new run and clears the log.  Returns the entry's
 * index in the log, or -1 when the log is full (the totals still count).
 */
int32_t
tlm_state_route (tlm_state_t * p_state, tlm_route_t const * p_route,
                 uint32_t time_ms)
{
    int32_t       index = -1;
    uint8_t const kind  = tlm_route_kind(p_route);

    if (ROUTE_RUN_START == kind)
    {
        p_state->b_run_started = true;
        p_state->run_start_ms = time_ms;
        p_state->run_start_mm = tlm_route_distance_mm(p_route);
        p_state->run_end_ms   = 0u;
        p_state->run_end_mm   = 0;
        p_state->end_kind     = 0u;
        p_state->barcodes     = 0u;
        p_state->turns        = 0u;
        p_state->humps        = 0u;
        p_state->max_hump_mm  = 0;
        p_state->obstacles    = 0u;
        p_state->log_count    = 0u;
    }
    else if (ROUTE_BARCODE == kind)
    {
        tlm_bump(&p_state->barcodes);
    }
    else if (ROUTE_TURN == kind)
    {
        tlm_bump(&p_state->turns);
    }
    else if (ROUTE_HUMP == kind)
    {
        int32_t const height = tlm_route_value(p_route);

        tlm_bump(&p_state->humps);
        p_state->max_hump_mm = (height > p_state->max_hump_mm)
                               ? height : p_state->max_hump_mm;
    }
    else if (ROUTE_OBSTACLE == kind)
    {
        tlm_bump(&p_state->obstacles);
    }
    else if (((ROUTE_COMPLETE == kind) || (ROUTE_SAFE_HOLD == kind) ||
              (ROUTE_FAULT == kind)) && (0u == p_state->end_kind))
    {
        /* The first of these ends the run; a later one is only logged. */
        p_state->run_end_ms = time_ms;
        p_state->run_end_mm = tlm_route_distance_mm(p_route);
        p_state->end_kind   = kind;
    }
    else
    {
        /* Search and no-command entries are only logged. */
    }

    if (p_state->log_count < CFG_TLM_ROUTE_MAX)
    {
        index = (int32_t) p_state->log_count;   /* < CFG_TLM_ROUTE_MAX */
        p_state->log[p_state->log_count].entry   = *p_route;
        p_state->log[p_state->log_count].time_ms = time_ms;
        p_state->log_count++;
    }

    return (index);
}

/* Telemetry record, 5 Hz (FR12, FR17). */
uint32_t
tlm_format_telemetry (tlm_state_t const * p_state, uint32_t seq,
                      uint32_t time_ms, char * p_buf, uint32_t size)
{
    json_writer_t json;
    /* Packed fields read back below 2^31 (12, 8 and 16 bits): the
       casts to int32_t keep their values. */
    int32_t const speeds[TLM_PAIR] =
    {
        tlm_motion_speed_l(&p_state->motion),
        tlm_motion_speed_r(&p_state->motion)
    };
    int32_t const counts[TLM_PAIR] =
    {
        tlm_motion_count_l(&p_state->motion),
        tlm_motion_count_r(&p_state->motion)
    };
    int32_t const line[TLM_TRIPLE] =
    {
        (int32_t) tlm_line_raw_l(&p_state->line),
        (int32_t) tlm_line_raw_r(&p_state->line),
        tlm_line_error_pm(&p_state->line)
    };
    int32_t const hump[TLM_PAIR] =
    {
        (int32_t) tlm_imu_humps(&p_state->imu),
        tlm_imu_max_height_mm(&p_state->imu)
    };
    int32_t const obstacle[TLM_PAIR] =
    {
        (int32_t) tlm_scan_obstacles(&p_state->scan),
        tlm_scan_range_mm(&p_state->scan)
    };

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_str(&json, "mode", p_state->p_mode_name);
    json_ints(&json, "spd", speeds, TLM_PAIR);
    json_ints(&json, "enc", counts, TLM_PAIR);
    json_int(&json, "dist", tlm_motion_distance_mm(&p_state->motion));
    json_ints(&json, "line", line, TLM_TRIPLE);
    json_uint(&json, "bc", tlm_line_letters(&p_state->line));
    json_char(&json, "last", tlm_line_last_letter(&p_state->line));
    json_ints(&json, "hump", hump, TLM_PAIR);
    json_ints(&json, "obs", obstacle, TLM_PAIR);
    json_str(&json, "imu", tlm_class_name(tlm_imu_class(&p_state->imu)));
    json_int(&json, "rate", tlm_imu_rate_dps(&p_state->imu));

    return (json_end(&json));
}

/* Heartbeat on the status topic, 1 Hz (NFR9). */
uint32_t
tlm_format_heartbeat (tlm_state_t const * p_state, uint32_t seq,
                      uint32_t time_ms, uint32_t beat, char * p_buf,
                      uint32_t size)
{
    json_writer_t json;

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_uint(&json, "hb", beat);
    json_str(&json, "mode", p_state->p_mode_name);
    json_char(&json, "held", tlm_nav_held_letter(&p_state->nav));
    json_str(&json, "hold", tlm_name(g_hold_names, TLM_NAMES(g_hold_names),
                                     tlm_nav_hold_reason(&p_state->nav)));
    json_int(&json, "wifi", tlm_link_wifi(&p_state->link));
    json_uint(&json, "fault",
              tlm_safety_faulted(&p_state->safety) ? 1u : 0u);
    json_str(&json, "cause",
             tlm_cause_name(tlm_safety_cause(&p_state->safety)));
    json_uint(&json, "mask", tlm_safety_mask(&p_state->safety));
    json_uint(&json, "wdt",
              tlm_safety_wdt_armed(&p_state->safety) ? 1u : 0u);
    json_uint(&json, "qfull", tlm_safety_queue_full(&p_state->safety));
    json_uint(&json, "miss", tlm_safety_missed(&p_state->safety));
    json_uint(&json, "drop", p_state->dropped);

    return (json_end(&json));
}

/*
 * Route-log entry (FR13).  time_ms is when the entry was logged;
 * log_index is its place in the log (-1: the log was full); b_replay
 * marks an entry published again after a summary.
 */
uint32_t
tlm_format_route (tlm_route_t const * p_route, uint32_t seq,
                  uint32_t time_ms, int32_t log_index, bool b_replay,
                  char * p_buf, uint32_t size)
{
    json_writer_t json;
    uint8_t const kind = tlm_route_kind(p_route);

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_str(&json, "ev", tlm_name(g_route_names, TLM_NAMES(g_route_names),
                                   kind));
    json_int(&json, "d", tlm_route_distance_mm(p_route));

    if (log_index >= 0)
    {
        json_int(&json, "log", log_index);
    }

    if (b_replay)
    {
        json_uint(&json, "re", 1u);
    }

    switch (kind)
    {
        case ROUTE_BARCODE:
            json_char(&json, "letter", tlm_route_letter(p_route));
            json_str(&json, "act", tlm_name(g_action_names,
                                            LETTER_ACTION_COUNT,
                                            tlm_route_action(p_route)));
            json_int(&json, "rev", tlm_route_value(p_route));
        break;

        case ROUTE_TURN:
            json_str(&json, "act", tlm_name(g_action_names,
                                            LETTER_ACTION_COUNT,
                                            tlm_route_action(p_route)));
            json_int(&json, "cmd", tlm_route_value(p_route));
            json_int(&json, "meas", tlm_route_value2(p_route));
        break;

        case ROUTE_HUMP:
            json_int(&json, "h", tlm_route_value(p_route));
            json_int(&json, "peak", tlm_route_value2(p_route));
        break;

        case ROUTE_OBSTACLE:
            json_str(&json, "side", tlm_name(g_side_names,
                                             TLM_NAMES(g_side_names),
                                             tlm_route_action(p_route)));
            json_int(&json, "w", tlm_route_value(p_route));
            json_int(&json, "close", tlm_route_value2(p_route));
        break;

        case ROUTE_LINE_SEARCH:
            json_int(&json, "on", tlm_route_value(p_route));
        break;

        case ROUTE_SAFE_HOLD:
            /* 0..3: the cast keeps it. */
            json_str(&json, "why", tlm_name(g_hold_names,
                                            TLM_NAMES(g_hold_names),
                                            (uint32_t)
                                            tlm_route_value(p_route)));
        break;

        case ROUTE_COMPLETE:
            json_int(&json, "run_ms", tlm_route_value(p_route));
        break;

        default:
            /* start, no_command, fault_mode: no extra fields. */
        break;
    }

    return (json_end(&json));
}

/* Answer on the ack topic. */
uint32_t
tlm_format_ack (tlm_ack_t const * p_ack, uint32_t seq, uint32_t time_ms,
                char * p_buf, uint32_t size)
{
    json_writer_t json;

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_uint(&json, "ack", tlm_ack_seq(p_ack));
    json_str(&json, "cmd", tlm_cmd_name(tlm_ack_code(p_ack)));
    json_uint(&json, "ok", tlm_ack_ok(p_ack) ? 1u : 0u);

    return (json_end(&json));
}

/* Fault latched or released (event topic). */
uint32_t
tlm_format_fault (tlm_fault_t const * p_fault, uint32_t seq,
                  uint32_t time_ms, char * p_buf, uint32_t size)
{
    json_writer_t json;

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_str(&json, "ev", "fault");
    json_str(&json, "cause", tlm_cause_name(tlm_fault_cause(p_fault)));
    json_str(&json, "src", tlm_source_name(tlm_fault_source(p_fault)));
    json_uint(&json, "detail", tlm_fault_detail(p_fault));
    json_uint(&json, "on", tlm_fault_latched(p_fault) ? 1u : 0u);

    return (json_end(&json));
}

/* Motion class changed (FR17 motion events). */
uint32_t
tlm_format_motion_event (uint8_t motion_class, int32_t rate_dps,
                         uint32_t seq, uint32_t time_ms, char * p_buf,
                         uint32_t size)
{
    json_writer_t json;

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_str(&json, "ev", "motion");
    json_str(&json, "class", tlm_class_name(motion_class));
    json_int(&json, "rate", rate_dps);

    return (json_end(&json));
}

/* Wi-Fi / MQTT link state changed. */
uint32_t
tlm_format_link (tlm_link_t const * p_link, uint32_t seq, uint32_t time_ms,
                 char * p_buf, uint32_t size)
{
    json_writer_t json;

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_str(&json, "ev", "link");
    json_int(&json, "wifi", tlm_link_wifi(p_link));
    json_uint(&json, "mqtt", tlm_link_online(p_link) ? 1u : 0u);

    return (json_end(&json));
}

/*
 * Obstacle profile (FR10, event topic): position (x forward, y left of
 * the car), width, closest point and the clearance on each side, all in
 * mm, and the side chosen for the bypass ("none" = no room).
 */
uint32_t
tlm_format_obstacle (tlm_obstacle_t const * p_obstacle, uint32_t seq,
                     uint32_t time_ms, char * p_buf, uint32_t size)
{
    json_writer_t json;

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_str(&json, "ev", "profile");
    json_int(&json, "x", tlm_obstacle_distance_mm(p_obstacle));
    json_int(&json, "y", tlm_obstacle_offset_mm(p_obstacle));
    json_int(&json, "w", tlm_obstacle_width_mm(p_obstacle));
    json_int(&json, "close", tlm_obstacle_closest_mm(p_obstacle));
    json_int(&json, "cl", tlm_obstacle_clear_left_mm(p_obstacle));
    json_int(&json, "cr", tlm_obstacle_clear_right_mm(p_obstacle));
    json_str(&json, "side", tlm_name(g_side_names, TLM_NAMES(g_side_names),
                                     tlm_obstacle_side(p_obstacle)));
    json_uint(&json, "rev", tlm_obstacle_reversed(p_obstacle) ? 1u : 0u);

    return (json_end(&json));
}

/* Run summary (FR14 on request, FR18 at the end of the run). */
uint32_t
tlm_format_summary (tlm_state_t const * p_state, uint32_t seq,
                    uint32_t time_ms, char * p_buf, uint32_t size)
{
    json_writer_t json;
    uint32_t      run_ms  = 0u;
    int32_t       dist_mm = 0;

    if (p_state->b_run_started)
    {
        bool const b_ended = (0u != p_state->end_kind);

        run_ms  = (b_ended ? p_state->run_end_ms : time_ms) -
                  p_state->run_start_ms;
        dist_mm = (b_ended ? p_state->run_end_mm
                           : tlm_motion_distance_mm(&p_state->motion)) -
                  p_state->run_start_mm;
    }

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_str(&json, "ev", "summary");
    json_uint(&json, "run", p_state->b_run_started ? 1u : 0u);
    json_uint(&json, "run_ms", run_ms);
    json_int(&json, "dist", dist_mm);
    json_uint(&json, "bc", p_state->barcodes);
    json_uint(&json, "turns", p_state->turns);
    json_uint(&json, "humps", p_state->humps);
    json_int(&json, "hmax", p_state->max_hump_mm);
    json_uint(&json, "obs", p_state->obstacles);
    json_uint(&json, "faults", tlm_safety_faults(&p_state->safety));
    json_str(&json, "end", tlm_name(g_route_names, TLM_NAMES(g_route_names),
                                    p_state->end_kind));
    json_uint(&json, "log", p_state->log_count);

    return (json_end(&json));
}

char const *
tlm_class_name (uint8_t motion_class)
{
    return (tlm_name(g_class_names, IMU_CLASS_COUNT, motion_class));
}

char const *
tlm_cause_name (uint8_t cause)
{
    return (tlm_name(g_cause_names, TLM_NAMES(g_cause_names), cause));
}

char const *
tlm_source_name (uint8_t source)
{
    return (tlm_name(g_source_names, TLM_NAMES(g_source_names), source));
}

char const *
tlm_cmd_name (uint8_t cmd_code)
{
    return (tlm_name(g_cmd_names, TLM_NAMES(g_cmd_names), cmd_code));
}

static char const *
tlm_name (char const * const * p_table, uint32_t count, uint32_t index)
{
    return ((index < count) ? p_table[index] : "?");
}

/* {"seq":N,"t":T */
static void
tlm_head (json_writer_t * p_json, char * p_buf, uint32_t size, uint32_t seq,
          uint32_t time_ms)
{
    json_begin(p_json, p_buf, size);
    json_uint(p_json, "seq", seq);
    json_uint(p_json, "t", time_ms);
}

static void
tlm_bump (uint16_t * p_count)
{
    if (*p_count < TLM_COUNT_MAX)
    {
        (*p_count)++;
    }
}

/*** end of file ***/
