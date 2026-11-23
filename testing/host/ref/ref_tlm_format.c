/*
 *  ref_tlm_format.c
 *  TEST ONLY: the telemetry formatter before the minimum-bit change,
 *  from app_program/subsystem1/tlm_format.c at c644b68 with only its
 *  public names prefixed ref_ (see ref_tlm_format.h).  Do not change it.
 */
#include <stddef.h>

#include "ref_tlm_format.h"
#include "subsystem1/tlm_json.h"
#include "subsystem3/letter_map.h"
#include "subsystem4/imu_msgs.h"
#include "subsystem5/scan_msgs.h"

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
    "lost_record", "line_data"
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

static char const * tlm_name(char const * const * p_table, uint32_t count,
                             uint32_t index);
static void tlm_head(json_writer_t * p_json, char * p_buf, uint32_t size,
                     uint32_t seq, uint32_t time_ms);
static void tlm_bump(uint16_t * p_count);

void
ref_tlm_state_init (ref_tlm_state_t * p_state)
{
    if (NULL != p_state)
    {
        ref_tlm_state_t const empty = { 0 };

        *p_state             = empty;
        p_state->p_mode_name = "BOOT";
        p_state->line.last_letter = '-';
        p_state->nav.held_letter  = '-';
    }
}

/*
 * Keep a route-log entry (FR13) and the run totals for the summary.
 * RUN_START begins a new run and clears the log.  Returns the entry's
 * index in the log, or -1 when the log is full (the totals still count).
 */
int32_t
ref_tlm_state_route (ref_tlm_state_t * p_state, ref_tlm_route_t const * p_route,
                     uint32_t time_ms)
{
    int32_t index = -1;

    if (ROUTE_RUN_START == p_route->kind)
    {
        p_state->b_run_started = true;
        p_state->run_start_ms = time_ms;
        p_state->run_start_mm = p_route->distance_mm;
        p_state->run_end_ms   = 0u;
        p_state->run_end_mm   = 0;
        p_state->end_kind     = 0u;
        p_state->end_reason   = 0u;
        p_state->barcodes     = 0u;
        p_state->turns        = 0u;
        p_state->humps        = 0u;
        p_state->max_hump_mm  = 0;
        p_state->obstacles    = 0u;
        p_state->log_count    = 0u;
    }
    else if (ROUTE_BARCODE == p_route->kind)
    {
        tlm_bump(&p_state->barcodes);
    }
    else if (ROUTE_TURN == p_route->kind)
    {
        tlm_bump(&p_state->turns);
    }
    else if (ROUTE_HUMP == p_route->kind)
    {
        tlm_bump(&p_state->humps);
        p_state->max_hump_mm = (p_route->value > p_state->max_hump_mm)
                               ? p_route->value : p_state->max_hump_mm;
    }
    else if (ROUTE_OBSTACLE == p_route->kind)
    {
        tlm_bump(&p_state->obstacles);
    }
    else if (((ROUTE_COMPLETE == p_route->kind) ||
              (ROUTE_SAFE_HOLD == p_route->kind) ||
              (ROUTE_FAULT == p_route->kind)) &&
             (0u == p_state->end_kind))
    {
        /* The first of these ends the run; a later one is only logged. */
        p_state->run_end_ms = time_ms;
        p_state->run_end_mm = p_route->distance_mm;
        p_state->end_kind   = p_route->kind;
        p_state->end_reason = p_route->reason;
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
ref_tlm_format_telemetry (ref_tlm_state_t const * p_state, uint32_t seq,
                          uint32_t time_ms, char * p_buf, uint32_t size)
{
    json_writer_t json;
    int32_t const speeds[TLM_PAIR] =
    {
        p_state->motion.speed_l_mmps, p_state->motion.speed_r_mmps
    };
    int32_t const counts[TLM_PAIR] =
    {
        p_state->motion.count_l, p_state->motion.count_r
    };
    int32_t const line[TLM_TRIPLE] =
    {
        p_state->line.raw_l, p_state->line.raw_r, p_state->line.error_pm
    };
    int32_t const hump[TLM_PAIR] =
    {
        p_state->imu.humps, p_state->imu.max_height_mm
    };
    int32_t const obstacle[TLM_PAIR] =
    {
        (int32_t) p_state->scan.obstacles, p_state->scan.range_mm
    };

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_str(&json, "mode", p_state->p_mode_name);
    json_ints(&json, "spd", speeds, TLM_PAIR);
    json_ints(&json, "enc", counts, TLM_PAIR);
    json_int(&json, "dist", p_state->motion.distance_mm);
    json_ints(&json, "line", line, TLM_TRIPLE);
    json_uint(&json, "bc", p_state->line.letters);
    json_char(&json, "last", p_state->line.last_letter);
    json_ints(&json, "hump", hump, TLM_PAIR);
    json_ints(&json, "obs", obstacle, TLM_PAIR);
    json_str(&json, "imu", ref_tlm_class_name(p_state->imu.motion_class));
    json_int(&json, "rate", p_state->imu.turn_rate_dps);

    return (json_end(&json));
}

/* Heartbeat on the status topic, 1 Hz (NFR9). */
uint32_t
ref_tlm_format_heartbeat (ref_tlm_state_t const * p_state, uint32_t seq,
                          uint32_t time_ms, uint32_t beat, char * p_buf,
                          uint32_t size)
{
    json_writer_t json;

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_uint(&json, "hb", beat);
    json_str(&json, "mode", p_state->p_mode_name);
    json_char(&json, "held", p_state->nav.held_letter);
    json_str(&json, "hold", tlm_name(g_hold_names, TLM_NAMES(g_hold_names),
                                     p_state->nav.hold_reason));
    json_int(&json, "wifi", p_state->link.wifi_status);
    json_uint(&json, "fault", p_state->safety.b_faulted);
    json_str(&json, "cause", ref_tlm_cause_name(p_state->safety.cause));
    json_uint(&json, "wdt", p_state->safety.b_wdt_armed);
    json_uint(&json, "qfull", p_state->safety.queue_full);
    json_uint(&json, "miss", p_state->safety.missed_checkins);
    json_uint(&json, "drop", p_state->dropped);

    return (json_end(&json));
}

/*
 * Route-log entry (FR13).  time_ms is when the entry was logged;
 * log_index is its place in the log (-1: the log was full); b_replay
 * marks an entry published again after a summary.
 */
uint32_t
ref_tlm_format_route (ref_tlm_route_t const * p_route, uint32_t seq,
                      uint32_t time_ms, int32_t log_index, bool b_replay,
                      char * p_buf, uint32_t size)
{
    json_writer_t json;

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_str(&json, "ev", tlm_name(g_route_names, TLM_NAMES(g_route_names),
                                   p_route->kind));
    json_int(&json, "d", p_route->distance_mm);

    if (log_index >= 0)
    {
        json_int(&json, "log", log_index);
    }

    if (b_replay)
    {
        json_uint(&json, "re", 1u);
    }

    switch (p_route->kind)
    {
        case ROUTE_BARCODE:
            json_char(&json, "letter", p_route->letter);
            json_str(&json, "act", tlm_name(g_action_names,
                                            LETTER_ACTION_COUNT,
                                            p_route->action));
            json_int(&json, "rev", p_route->value);
        break;

        case ROUTE_TURN:
            json_str(&json, "act", tlm_name(g_action_names,
                                            LETTER_ACTION_COUNT,
                                            p_route->action));
            json_int(&json, "cmd", p_route->value);
            json_int(&json, "meas", p_route->value2);
        break;

        case ROUTE_HUMP:
            json_int(&json, "h", p_route->value);
            json_int(&json, "peak", p_route->value2);
        break;

        case ROUTE_OBSTACLE:
            json_str(&json, "side", tlm_name(g_side_names,
                                             TLM_NAMES(g_side_names),
                                             p_route->action));
            json_int(&json, "w", p_route->value);
            json_int(&json, "close", p_route->value2);
        break;

        case ROUTE_LINE_SEARCH:
            json_int(&json, "on", p_route->value);
        break;

        case ROUTE_SAFE_HOLD:
            json_str(&json, "why", tlm_name(g_hold_names,
                                            TLM_NAMES(g_hold_names),
                                            (uint32_t) p_route->value));
        break;

        case ROUTE_COMPLETE:
            json_int(&json, "run_ms", p_route->value);
        break;

        default:
            /* start, no_command, fault_mode: no extra fields. */
        break;
    }

    return (json_end(&json));
}

/* Answer on the ack topic. */
uint32_t
ref_tlm_format_ack (ref_tlm_ack_t const * p_ack, uint32_t seq, uint32_t time_ms,
                    char * p_buf, uint32_t size)
{
    json_writer_t json;

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_uint(&json, "ack", p_ack->cmd_seq);
    json_str(&json, "cmd", ref_tlm_cmd_name(p_ack->cmd_code));
    json_uint(&json, "ok", p_ack->b_ok);

    return (json_end(&json));
}

/* Fault latched or released (event topic). */
uint32_t
ref_tlm_format_fault (ref_tlm_fault_t const * p_fault, uint32_t seq,
                      uint32_t time_ms, char * p_buf, uint32_t size)
{
    json_writer_t json;

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_str(&json, "ev", "fault");
    json_str(&json, "cause", ref_tlm_cause_name(p_fault->cause));
    json_str(&json, "src", ref_tlm_source_name(p_fault->source));
    json_uint(&json, "detail", p_fault->detail);
    json_uint(&json, "on", p_fault->b_latched);

    return (json_end(&json));
}

/* Motion class changed (FR17 motion events). */
uint32_t
ref_tlm_format_motion_event (uint8_t motion_class, int32_t rate_dps,
                             uint32_t seq, uint32_t time_ms, char * p_buf,
                             uint32_t size)
{
    json_writer_t json;

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_str(&json, "ev", "motion");
    json_str(&json, "class", ref_tlm_class_name(motion_class));
    json_int(&json, "rate", rate_dps);

    return (json_end(&json));
}

/* Wi-Fi / MQTT link state changed. */
uint32_t
ref_tlm_format_link (ref_tlm_link_t const * p_link, uint32_t seq,
                     uint32_t time_ms,
                     char * p_buf, uint32_t size)
{
    json_writer_t json;

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_str(&json, "ev", "link");
    json_int(&json, "wifi", p_link->wifi_status);
    json_uint(&json, "mqtt", p_link->b_online);

    return (json_end(&json));
}

/*
 * Obstacle profile (FR10, event topic): position (x forward, y left of
 * the car), width, closest point and the clearance on each side, all in
 * mm, and the side chosen for the bypass ("none" = no room).
 */
uint32_t
ref_tlm_format_obstacle (ref_tlm_obstacle_t const * p_obstacle, uint32_t seq,
                         uint32_t time_ms, char * p_buf, uint32_t size)
{
    json_writer_t json;

    tlm_head(&json, p_buf, size, seq, time_ms);
    json_str(&json, "ev", "profile");
    json_int(&json, "x", p_obstacle->distance_mm);
    json_int(&json, "y", p_obstacle->offset_mm);
    json_int(&json, "w", p_obstacle->width_mm);
    json_int(&json, "close", p_obstacle->closest_mm);
    json_int(&json, "cl", p_obstacle->clear_left_mm);
    json_int(&json, "cr", p_obstacle->clear_right_mm);
    json_str(&json, "side", tlm_name(g_side_names, TLM_NAMES(g_side_names),
                                     p_obstacle->side));
    json_uint(&json, "rev", p_obstacle->b_reversed);

    return (json_end(&json));
}

/* Run summary (FR14 on request, FR18 at the end of the run). */
uint32_t
ref_tlm_format_summary (ref_tlm_state_t const * p_state, uint32_t seq,
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
                           : p_state->motion.distance_mm) -
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
    json_uint(&json, "faults", p_state->safety.faults);
    json_str(&json, "end", tlm_name(g_route_names, TLM_NAMES(g_route_names),
                                    p_state->end_kind));
    json_uint(&json, "log", p_state->log_count);

    return (json_end(&json));
}

char const *
ref_tlm_class_name (uint8_t motion_class)
{
    return (tlm_name(g_class_names, IMU_CLASS_COUNT, motion_class));
}

char const *
ref_tlm_cause_name (uint8_t cause)
{
    return (tlm_name(g_cause_names, TLM_NAMES(g_cause_names), cause));
}

char const *
ref_tlm_source_name (uint8_t source)
{
    return (tlm_name(g_source_names, TLM_NAMES(g_source_names), source));
}

char const *
ref_tlm_cmd_name (uint8_t cmd_code)
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
