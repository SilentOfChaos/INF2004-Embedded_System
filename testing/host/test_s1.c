/*
 *  test_s1.c
 *  Host tests for Buddy 1's pure modules: command parser, JSON writer
 *  and telemetry messages.  Results: testing/subsystem1/TEST_LOG.md.
 */

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "ref/ref_tlm_format.h"
#include "subsystem1/cmd_parse.h"
#include "subsystem1/tlm_format.h"
#include "subsystem1/tlm_json.h"
#include "subsystem1/tlm_msgs.h"
#include "subsystem3/letter_map.h"
#include "subsystem4/imu_msgs.h"
#include "subsystem5/scan_logic.h"
#include "unit.h"

#define TEST_BUF    512u

static bool
test_parse (char const * p_text, cmd_t * p_cmd)
{
    return (cmd_parse(p_text, (uint32_t) strlen(p_text), p_cmd));
}

static bool
test_has (char const * p_text, char const * p_part)
{
    return (NULL != strstr(p_text, p_part));
}

static void
test_cmd_valid (void)
{
    cmd_t cmd;

    UNIT_BEGIN("S1-U01", "each command form parses with its fields");

    UNIT_CHECK(test_parse("{\"seq\":12,\"cmd\":\"remap\",\"letter\":\"E\","
                          "\"action\":\"left\"}", &cmd));
    UNIT_EQ(cmd.code, CMD_CODE_REMAP);
    UNIT_EQ(cmd.seq, 12);
    UNIT_EQ(cmd.letter, 'E');
    UNIT_EQ(cmd.action, LETTER_ACTION_LEFT);

    UNIT_CHECK(test_parse("{\"cmd\": \"remap\", \"seq\": 7, \"action\": "
                          "\"uturn\", \"letter\": \"Z\"}", &cmd));
    UNIT_EQ(cmd.action, LETTER_ACTION_UTURN);
    UNIT_EQ(cmd.letter, 'Z');
    UNIT_EQ(cmd.seq, 7);

    UNIT_CHECK(test_parse("{\"seq\":13,\"cmd\":\"default_map\"}", &cmd));
    UNIT_EQ(cmd.code, CMD_CODE_MAP_DEFAULT);
    UNIT_CHECK(test_parse("{\"seq\":17,\"cmd\":\"start\"}", &cmd));
    UNIT_EQ(cmd.code, CMD_CODE_START);
    UNIT_EQ(cmd.seq, 17u);
    UNIT_CHECK(test_parse("{\"seq\":18,\"cmd\":\"reset\"}", &cmd));
    UNIT_EQ(cmd.code, CMD_CODE_RESET);
    UNIT_CHECK(!cmd_is_stop("{\"seq\":19,\"cmd\":\"start\"}", 24u));
    UNIT_CHECK(test_parse("{\"seq\":14,\"cmd\":\"summary\"}", &cmd));
    UNIT_EQ(cmd.code, CMD_CODE_SUMMARY);
    UNIT_CHECK(test_parse("{\"seq\":15,\"cmd\":\"stop\"}", &cmd));
    UNIT_EQ(cmd.code, CMD_CODE_STOP);

    UNIT_CHECK(test_parse("{\"seq\":16,\"cmd\":\"broker\",\"ip\":"
                          "\"192.168.1.20\",\"port\":1883}", &cmd));
    UNIT_EQ(cmd.code, CMD_CODE_BROKER);
    UNIT_CHECK(0 == strcmp(cmd.ip, "192.168.1.20"));
    UNIT_EQ(cmd.port, 1883);

    UNIT_CHECK(test_parse("{\"cmd\":\"stop\"}", &cmd));
    UNIT_EQ(cmd.seq, 0);                /* seq is optional */
    UNIT_END();
}

static void
test_cmd_invalid (void)
{
    static char const * const bad[] =
    {
        "{\"seq\":1,\"cmd\":\"jump\"}",
        "{\"seq\":1}",
        "{\"seq\":1,\"cmd\":\"remap\",\"letter\":\"e\",\"action\":\"left\"}",
        "{\"seq\":1,\"cmd\":\"remap\",\"letter\":\"EF\",\"action\":\"left\"}",
        "{\"seq\":1,\"cmd\":\"remap\",\"letter\":\"E\",\"action\":\"LEFT\"}",
        "{\"seq\":1,\"cmd\":\"remap\",\"letter\":\"E\",\"action\":\"back\"}",
        "{\"seq\":1,\"cmd\":\"remap\",\"letter\":\"E\"}",
        "{\"seq\":1,\"cmd\":\"remap\",\"action\":\"left\"}",
        "{\"seq\":1,\"cmd\":\"broker\",\"ip\":\"256.1.1.1\",\"port\":1883}",
        "{\"seq\":1,\"cmd\":\"broker\",\"ip\":\"1.2.3\",\"port\":1883}",
        "{\"seq\":1,\"cmd\":\"broker\",\"ip\":\"1.2.3.4.5\",\"port\":1883}",
        "{\"seq\":1,\"cmd\":\"broker\",\"ip\":\"1..2.3\",\"port\":1883}",
        "{\"seq\":1,\"cmd\":\"broker\",\"ip\":\"a.b.c.d\",\"port\":1883}",
        "{\"seq\":1,\"cmd\":\"broker\",\"ip\":\"1.2.3.4\",\"port\":0}",
        "{\"seq\":1,\"cmd\":\"broker\",\"ip\":\"1.2.3.4\",\"port\":65536}",
        "{\"seq\":1,\"cmd\":\"broker\",\"ip\":\"1.2.3.4\"}",
        "{\"seq\":1,\"cmd\":\"sto\\\"p\"}",
        "{\"seq\":1,\"cmd\":\"stop",
        ""
    };
    cmd_t cmd;
    bool  b_all_rejected = true;
    bool  b_seq_kept     = true;

    UNIT_BEGIN("S1-U02", "bad commands are rejected, seq kept for the ack");

    for (uint32_t idx = 0u; idx < (sizeof(bad) / sizeof(bad[0])); idx++)
    {
        bool const b_ok = test_parse(bad[idx], &cmd);

        b_all_rejected = b_all_rejected && !b_ok &&
                         (CMD_CODE_UNKNOWN == cmd.code);
        b_seq_kept = b_seq_kept && ((0u == strlen(bad[idx])) ||
                                    (1u == cmd.seq));
    }

    UNIT_CHECK(b_all_rejected);
    UNIT_CHECK(b_seq_kept);
    UNIT_CHECK(!cmd_parse(NULL, 4u, &cmd));
    UNIT_END();
}

static void
test_cmd_helpers (void)
{
    static char const text[] = "{\"a\" : \"xyz\",\"n\":-123,\"big\":"
                               "1234567890,\"s\":\"x\\\"y\",\"e\":\"\"}";
    uint32_t const len = (uint32_t) strlen(text);
    char           out[8];
    int32_t        value = 0;

    UNIT_BEGIN("S1-U03", "JSON field readers stay inside their buffers");
    UNIT_CHECK(cmd_json_string(text, len, "a", out, sizeof(out)));
    UNIT_CHECK(0 == strcmp(out, "xyz"));
    UNIT_CHECK(!cmd_json_string(text, len, "a", out, 3u));  /* too long */
    UNIT_CHECK(!cmd_json_string(text, len, "s", out, sizeof(out)));
    UNIT_CHECK(cmd_json_string(text, len, "e", out, sizeof(out)));
    UNIT_CHECK('\0' == out[0]);
    UNIT_CHECK(!cmd_json_string(text, len, "missing", out, sizeof(out)));
    UNIT_CHECK(cmd_json_int(text, len, "n", &value));
    UNIT_EQ(value, -123);
    UNIT_CHECK(!cmd_json_int(text, len, "big", &value));    /* 10 digits */
    UNIT_CHECK(!cmd_json_int(text, len, "a", &value));      /* a string */
    UNIT_CHECK(!cmd_json_int(text, 12u, "n", &value));      /* cut short */
    UNIT_END();
}

static void
test_cmd_ip (void)
{
    UNIT_BEGIN("S1-U04", "IPv4 address check");
    UNIT_CHECK(cmd_ip_valid("0.0.0.0"));
    UNIT_CHECK(cmd_ip_valid("255.255.255.255"));
    UNIT_CHECK(cmd_ip_valid("10.0.0.1"));
    UNIT_CHECK(!cmd_ip_valid("1.2.3.256"));
    UNIT_CHECK(!cmd_ip_valid("1.2.3.0004"));
    UNIT_CHECK(!cmd_ip_valid("1.2.3."));
    UNIT_CHECK(!cmd_ip_valid(".1.2.3"));
    UNIT_CHECK(!cmd_ip_valid("1.2.3.4 "));
    UNIT_CHECK(!cmd_ip_valid(""));
    UNIT_CHECK(!cmd_ip_valid(NULL));
    UNIT_END();
}

static void
test_json_writer (void)
{
    char          buf[32];
    json_writer_t json;
    int32_t const values[3] = { 1, -2, INT32_MIN };

    UNIT_BEGIN("S1-U05", "JSON writer: exact output, overflow gives 0");

    json_begin(&json, buf, sizeof(buf));
    json_int(&json, "a", INT32_MIN);
    UNIT_EQ(json_end(&json), 17);
    UNIT_CHECK(0 == strcmp(buf, "{\"a\":-2147483648}"));

    json_begin(&json, buf, sizeof(buf));
    json_str(&json, "s", "q\"b\\c\n");
    json_char(&json, "c", 'Z');
    UNIT_CHECK(0u != json_end(&json));
    UNIT_CHECK(0 == strcmp(buf, "{\"s\":\"q_b_c_\",\"c\":\"Z\"}"));

    json_begin(&json, buf, sizeof(buf));
    json_ints(&json, "v", values, 3u);
    UNIT_CHECK(0u != json_end(&json));
    UNIT_CHECK(0 == strcmp(buf, "{\"v\":[1,-2,-2147483648]}"));

    /* "{"u":4294967295}" is 16 characters: fits 17 bytes, not 16. */
    (void) memset(buf, 'x', sizeof(buf));
    json_begin(&json, buf, 17u);
    json_uint(&json, "u", UINT32_MAX);
    UNIT_EQ(json_end(&json), 16);

    (void) memset(buf, 'x', sizeof(buf));
    json_begin(&json, buf, 16u);
    json_uint(&json, "u", UINT32_MAX);
    UNIT_EQ(json_end(&json), 0);
    UNIT_CHECK('x' == buf[16]);         /* nothing written past the end */

    json_begin(&json, NULL, 0u);
    UNIT_EQ(json_end(&json), 0);
    UNIT_END();
}

/* A route entry with only a kind, a distance and a first value. */
static tlm_route_t
test_route (uint8_t kind, int32_t distance_mm, int32_t value)
{
    tlm_route_t route;

    (void) tlm_route_pack(&route, kind, '\0', 0u, distance_mm, value, 0);

    return (route);
}

static void
test_route_state (void)
{
    static tlm_state_t state;
    tlm_route_t        route;
    int32_t            index = 0;

    UNIT_BEGIN("S1-U06", "route log: totals, indexes, full log, new run");
    tlm_state_init(&state);

    route = test_route(ROUTE_RUN_START, 100, 0);
    UNIT_EQ(tlm_state_route(&state, &route, 1000u), 0);
    route = test_route(ROUTE_BARCODE, 100, 0);
    UNIT_EQ(tlm_state_route(&state, &route, 1100u), 1);
    route = test_route(ROUTE_HUMP, 100, 23);
    UNIT_EQ(tlm_state_route(&state, &route, 1200u), 2);
    route = test_route(ROUTE_HUMP, 100, 11);
    (void) tlm_state_route(&state, &route, 1300u);
    UNIT_EQ(state.humps, 2);
    UNIT_EQ(state.max_hump_mm, 23);
    UNIT_EQ(state.barcodes, 1);

    for (uint32_t idx = 0u; idx < (CFG_TLM_ROUTE_MAX + 5u); idx++)
    {
        route = test_route(ROUTE_TURN, 100, 0);
        index = tlm_state_route(&state, &route, 2000u + idx);
    }

    UNIT_EQ(index, -1);                 /* log full ... */
    UNIT_EQ(state.log_count, CFG_TLM_ROUTE_MAX);
    UNIT_EQ(state.turns, CFG_TLM_ROUTE_MAX + 5u);   /* ... totals go on */

    route = test_route(ROUTE_RUN_START, 100, 0);
    UNIT_EQ(tlm_state_route(&state, &route, 9000u), 0);
    UNIT_EQ(state.log_count, 1);
    UNIT_EQ(state.turns, 0);
    UNIT_EQ(state.humps, 0);
    UNIT_END();
}

static void
test_summary (void)
{
    static tlm_state_t state;
    tlm_route_t        route;
    char               buf[TEST_BUF];

    UNIT_BEGIN("S1-U07", "run summary: time, distance and end of the run");
    tlm_state_init(&state);

    UNIT_CHECK(0u != tlm_format_summary(&state, 1u, 500u, buf, TEST_BUF));
    UNIT_CHECK(test_has(buf, "\"run\":0,\"run_ms\":0,\"dist\":0"));

    route = test_route(ROUTE_RUN_START, 1000, 0);
    (void) tlm_state_route(&state, &route, 10000u);
    UNIT_CHECK(tlm_motion_pack(&state.motion, 0, 0, 0, 0, 1500));
    UNIT_CHECK(0u != tlm_format_summary(&state, 2u, 12000u, buf, TEST_BUF));
    UNIT_CHECK(test_has(buf, "\"run\":1,\"run_ms\":2000,\"dist\":500"));
    UNIT_CHECK(test_has(buf, "\"end\":\"none\""));

    route = test_route(ROUTE_SAFE_HOLD, 3000, 0);
    (void) tlm_state_route(&state, &route, 20000u);
    route = test_route(ROUTE_FAULT, 3100, 0);   /* later: logged, not the end */
    (void) tlm_state_route(&state, &route, 25000u);
    UNIT_CHECK(tlm_motion_pack(&state.motion, 0, 0, 0, 0, 3200));
    UNIT_CHECK(0u != tlm_format_summary(&state, 3u, 30000u, buf, TEST_BUF));
    UNIT_CHECK(test_has(buf, "\"run_ms\":10000,\"dist\":2000"));
    UNIT_CHECK(test_has(buf, "\"end\":\"safe_hold\""));
    UNIT_CHECK(test_has(buf, "\"log\":3"));
    UNIT_END();
}

/* The extremes each packed field can hold that print longest. */
#define TEST_DIST_MIN       (-1073741824)       /* 31 bits */

static void
test_lengths (void)
{
    static tlm_state_t state;
    char               buf[TEST_BUF];
    tlm_route_t        route;
    tlm_ack_t          ack;
    tlm_fault_t        fault;
    tlm_link_t         link;
    tlm_obstacle_t     obst;
    uint32_t           len   = 0u;
    uint32_t           worst = 0u;

    UNIT_BEGIN("S1-U08", "every message fits CFG_MQTT_PAYLOAD_MAX");
    tlm_state_init(&state);

    /* Every field at the extreme its packed record can hold (D-92). */
    state.p_mode_name = "AVOIDING OBSTACLE";
    UNIT_CHECK(tlm_motion_pack(&state.motion, INT16_MIN, INT16_MIN,
                               INT32_MIN, INT32_MIN, TEST_DIST_MIN));
    UNIT_CHECK(tlm_line_pack(&state.line, 4095u, 4095u, -1024, 255u, 'Z'));
    UNIT_CHECK(tlm_imu_pack(&state.imu, IMU_CLASS_ACCELERATING, 255u, 127,
                            INT16_MIN));
    UNIT_CHECK(tlm_scan_pack(&state.scan, 4095, UINT16_MAX));
    UNIT_CHECK(tlm_nav_pack(&state.nav, 7u, 'Z', 2u));
    UNIT_CHECK(tlm_link_pack(&state.link, true, -4));
    UNIT_CHECK(tlm_safety_pack(&state.safety, true, FAULT_CAUSE_ENCODER, true,
                               255u, UINT16_MAX, UINT16_MAX,
                               FAULT_MASK_ALL));
    state.dropped              = UINT32_MAX;
    state.b_run_started        = true;
    state.end_kind             = ROUTE_NO_COMMAND;
    state.run_end_mm           = INT32_MIN;
    state.barcodes             = UINT16_MAX;
    state.turns                = UINT16_MAX;
    state.humps                = UINT16_MAX;
    state.max_hump_mm          = INT32_MIN;
    state.obstacles            = UINT16_MAX;

    len = tlm_format_telemetry(&state, UINT32_MAX, UINT32_MAX, buf,
                               TEST_BUF);
    worst = (len > worst) ? len : worst;
    UNIT_EQ(len, 249);                  /* the figure in app_config.h */
    len = tlm_format_heartbeat(&state, UINT32_MAX, UINT32_MAX, UINT32_MAX,
                               buf, TEST_BUF);
    worst = (len > worst) ? len : worst;
    len = tlm_format_summary(&state, UINT32_MAX, UINT32_MAX, buf, TEST_BUF);
    worst = (len > worst) ? len : worst;

    for (uint8_t kind = ROUTE_RUN_START; kind <= ROUTE_FAULT; kind++)
    {
        /* COMPLETE carries all 32 bits; TURN 16 bits each; the rest
           are smaller (tlm_msgs.h). */
        int32_t const value  = (ROUTE_COMPLETE == kind) ? INT32_MIN
                                                        : INT16_MIN;

        (void) tlm_route_pack(&route, kind, 'Z', LETTER_ACTION_STRAIGHT,
                              TEST_DIST_MIN, value, INT16_MIN);
        len = tlm_format_route(&route, UINT32_MAX, UINT32_MAX,
                               (int32_t) CFG_TLM_ROUTE_MAX, true, buf,
                               TEST_BUF);
        worst = (len > worst) ? len : worst;
        UNIT_CHECK(0u != len);
    }

    UNIT_CHECK(tlm_ack_pack(&ack, 0x3FFFFFFFu, CMD_CODE_MAP_DEFAULT, true));
    len = tlm_format_ack(&ack, UINT32_MAX, UINT32_MAX, buf, TEST_BUF);
    worst = (len > worst) ? len : worst;
    UNIT_CHECK(tlm_fault_pack(&fault, FAULT_CAUSE_ENCODER, FAULT_SRC_TELEMETRY,
                              8191u, true));
    len = tlm_format_fault(&fault, UINT32_MAX, UINT32_MAX, buf, TEST_BUF);
    worst = (len > worst) ? len : worst;
    len = tlm_format_motion_event(IMU_CLASS_ACCELERATING, INT16_MIN,
                                  UINT32_MAX, UINT32_MAX, buf, TEST_BUF);
    worst = (len > worst) ? len : worst;
    UNIT_CHECK(tlm_link_pack(&link, true, -4));
    len = tlm_format_link(&link, UINT32_MAX, UINT32_MAX, buf, TEST_BUF);
    worst = (len > worst) ? len : worst;
    UNIT_CHECK(tlm_obstacle_pack(&obst, 1u, true, 511, -512, -1024, 511,
                                 -1, -1));
    len = tlm_format_obstacle(&obst, UINT32_MAX, UINT32_MAX, buf, TEST_BUF);
    worst = (len > worst) ? len : worst;
    UNIT_CHECK(0u != len);

    UNIT_CHECK(worst <= CFG_MQTT_PAYLOAD_MAX);

    /* Too small a buffer gives 0, never a cut message. */
    UNIT_EQ(tlm_format_telemetry(&state, 1u, 1u, buf, 100u), 0);
    UNIT_END();
}

static void
test_route_format (void)
{
    tlm_route_t route;
    char        buf[TEST_BUF];

    UNIT_BEGIN("S1-U09", "route entries carry the fields of their kind");

    UNIT_CHECK(tlm_route_pack(&route, ROUTE_BARCODE, 'B',
                              LETTER_ACTION_LEFT, 1234, 1, 0));
    UNIT_CHECK(0u != tlm_format_route(&route, 5u, 600u, 3, false, buf,
                                      TEST_BUF));
    UNIT_CHECK(0 == strcmp(buf, "{\"seq\":5,\"t\":600,\"ev\":\"barcode\","
                                "\"d\":1234,\"log\":3,\"letter\":\"B\","
                                "\"act\":\"left\",\"rev\":1}"));

    UNIT_CHECK(tlm_route_pack(&route, ROUTE_TURN, 'B', LETTER_ACTION_RIGHT,
                              1234, -9000, -8950));
    UNIT_CHECK(0u != tlm_format_route(&route, 6u, 700u, -1, true, buf,
                                      TEST_BUF));
    UNIT_CHECK(test_has(buf, "\"ev\":\"turn\""));
    UNIT_CHECK(test_has(buf, "\"re\":1"));
    UNIT_CHECK(!test_has(buf, "\"log\""));
    UNIT_CHECK(test_has(buf, "\"cmd\":-9000,\"meas\":-8950"));

    UNIT_CHECK(tlm_route_pack(&route, ROUTE_OBSTACLE, '\0', 2u, 1234, 120,
                              280));
    UNIT_CHECK(0u != tlm_format_route(&route, 7u, 800u, 4, false, buf,
                                      TEST_BUF));
    UNIT_CHECK(test_has(buf, "\"side\":\"right\",\"w\":120,\"close\":280"));

    UNIT_CHECK(tlm_route_pack(&route, ROUTE_SAFE_HOLD, '\0', 0u, 1234, 1,
                              0));
    UNIT_CHECK(0u != tlm_format_route(&route, 8u, 900u, 5, false, buf,
                                      TEST_BUF));
    UNIT_CHECK(test_has(buf, "\"why\":\"no_command\""));
    UNIT_END();
}

static void
test_names (void)
{
    tlm_ack_t   ack;
    tlm_route_t route;
    char        buf[TEST_BUF];

    UNIT_BEGIN("S1-U10", "unknown codes print as \"?\", never out of range");
    UNIT_CHECK(0 == strcmp(tlm_class_name(IMU_CLASS_COUNT), "?"));
    UNIT_CHECK(0 == strcmp(tlm_cause_name(200u), "?"));
    UNIT_CHECK(0 == strcmp(tlm_cause_name(FAULT_CAUSE_MAX + 1u), "?"));
    UNIT_CHECK(0 == strcmp(tlm_cause_name(FAULT_CAUSE_MOTOR_STALL),
                           "motor_stall"));
    UNIT_CHECK(0 == strcmp(tlm_cause_name(FAULT_CAUSE_ENCODER),
                           "encoder_fault"));
    UNIT_CHECK(0 == strcmp(tlm_cause_name(FAULT_CAUSE_NO_ECHO), "no_echo"));
    UNIT_CHECK(0 == strcmp(tlm_cause_name(FAULT_CAUSE_WIFI_LOSS),
                           "wifi_loss"));
    UNIT_CHECK(0 == strcmp(tlm_source_name(9u), "?"));
    UNIT_CHECK(0 == strcmp(tlm_cmd_name(8u), "?"));
    UNIT_CHECK(0 == strcmp(tlm_cmd_name(CMD_CODE_BROKER), "broker"));
    UNIT_CHECK(0 == strcmp(tlm_class_name(IMU_CLASS_STEADY), "steady"));
    /* All 8 codes of the 3-bit field are now named (start, reset added
       for the dashboard); a value too big for the field is reported and
       kept at the largest, which prints as "reset". */
    UNIT_CHECK(tlm_ack_pack(&ack, 9u, CMD_CODE_START, false));
    UNIT_CHECK(0u != tlm_format_ack(&ack, 1u, 2u, buf, TEST_BUF));
    UNIT_CHECK(test_has(buf, "\"cmd\":\"start\""));
    UNIT_CHECK(!tlm_ack_pack(&ack, 9u, 200u, false));
    UNIT_CHECK(0u != tlm_format_ack(&ack, 1u, 2u, buf, TEST_BUF));
    UNIT_CHECK(test_has(buf, "\"cmd\":\"reset\""));
    UNIT_CHECK(tlm_route_pack(&route, 15u, '\0', 0u, 0, 0, 0));
    UNIT_CHECK(0u != tlm_format_route(&route, 1u, 2u, 0, false, buf,
                                      TEST_BUF));
    UNIT_CHECK(test_has(buf, "\"ev\":\"?\""));
    UNIT_CHECK(!tlm_route_pack(&route, 99u, '\0', 99u, 0, 0, 0));
    UNIT_CHECK(0u != tlm_format_route(&route, 1u, 2u, 0, false, buf,
                                      TEST_BUF));
    UNIT_CHECK(test_has(buf, "\"ev\":\"?\""));
    UNIT_END();
}

/* ---- 24 Oct review: S1-U12 ---- */

static void
test_cmd_is_stop (void)
{
    static char const stop_seq[]  = "{\"seq\":15,\"cmd\":\"stop\"}";
    static char const stop_bare[] = "{\"cmd\":\"stop\"}";
    static char const summary[]   = "{\"seq\":14,\"cmd\":\"summary\"}";
    static char const cut[]       = "{\"cmd\":\"sto";

    UNIT_BEGIN("S1-U12", "a STOP is recognised from the raw payload, with "
                         "or without seq, so a full command queue cannot "
                         "lose it; other and cut-off commands are not "
                         "(24 Oct review, R-04)");
    UNIT_CHECK(cmd_is_stop(stop_seq, (uint32_t) (sizeof(stop_seq) - 1u)));
    UNIT_CHECK(cmd_is_stop(stop_bare, (uint32_t) (sizeof(stop_bare) - 1u)));
    UNIT_CHECK(!cmd_is_stop(summary, (uint32_t) (sizeof(summary) - 1u)));
    UNIT_CHECK(!cmd_is_stop(cut, (uint32_t) (sizeof(cut) - 1u)));
    UNIT_CHECK(!cmd_is_stop(stop_seq, 5u));
    UNIT_CHECK(!cmd_is_stop(NULL, 0u));
    UNIT_END();
}

static void
test_obstacle_format (void)
{
    tlm_obstacle_t obst;
    char           buf[TEST_BUF];

    UNIT_BEGIN("S1-U11", "obstacle profile: position, width, closest point, "
                         "clearances (FR10)");

    UNIT_CHECK(tlm_obstacle_pack(&obst, 1u /* OBST_SIDE_LEFT */, false,
                                 210, -15, 160, 215, 4000, 120));
    UNIT_CHECK(0u != tlm_format_obstacle(&obst, 9u, 1000u, buf, TEST_BUF));
    UNIT_CHECK(0 == strcmp(buf, "{\"seq\":9,\"t\":1000,\"ev\":\"profile\","
                                "\"x\":210,\"y\":-15,\"w\":160,"
                                "\"close\":215,\"cl\":4000,\"cr\":120,"
                                "\"side\":\"left\",\"rev\":0}"));

    /* No room on either side, after the reverse; an unknown side code
       prints as "?". */
    UNIT_CHECK(tlm_obstacle_pack(&obst, 0u, true, 210, -15, 160, 215, 4000,
                                 120));
    UNIT_CHECK(0u != tlm_format_obstacle(&obst, 10u, 1100u, buf, TEST_BUF));
    UNIT_CHECK(test_has(buf, "\"side\":\"none\",\"rev\":1"));
    UNIT_CHECK(tlm_obstacle_pack(&obst, 3u, true, 210, -15, 160, 215, 4000,
                                 120));
    UNIT_CHECK(0u != tlm_format_obstacle(&obst, 11u, 1200u, buf, TEST_BUF));
    UNIT_CHECK(test_has(buf, "\"side\":\"?\""));

    /* Too small a buffer: 0, never a cut message. */
    UNIT_EQ(tlm_format_obstacle(&obst, 1u, 1u, buf, 40u), 0);
    UNIT_END();
}

/* ---- Minimum-bit change (D-91): S1-U40 and up ---- */

/* xorshift32: the same sequence on every host. */
static uint32_t
test_rand (uint32_t * p_state)
{
    uint32_t x = *p_state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *p_state = x;

    return (x);
}

static void
test_broker_record (void)
{
    static char const * const kept[] =
    {
        "0.0.0.0", "255.255.255.255", "010.0.0.1", "1.02.003.4"
    };
    comms_broker_t broker;
    cmd_t          cmd;
    char           ip[CMD_IP_MAX];
    char           text[96];
    uint32_t       seed = 0x5EED0101u;
    bool           b_ok = true;

    UNIT_BEGIN("S1-U40", "IF-15 broker record packed into two words, each "
                         "address part's spelling in 9 bits: the exact "
                         "expected bits; every spelling of a part and every "
                         "address the broker command accepts come back "
                         "character for character, with the port (D-91)");
    UNIT_EQ(sizeof(comms_broker_t), 8u);

    /* "192" = 110 + 192, "168" = 110 + 168, "1" = 1, "20" = 10 + 20. */
    UNIT_CHECK(comms_broker_pack(&broker, "192.168.1.20", 1883u));
    UNIT_EQ(broker.word[0], 0x00062D2Eu);
    UNIT_EQ(broker.word[1], 0x075B001Eu);
    comms_broker_ip(&broker, ip, CMD_IP_MAX);
    UNIT_CHECK(0 == strcmp(ip, "192.168.1.20"));
    UNIT_EQ(comms_broker_port(&broker), 1883);

    /* Every spelling of a part (10 + 100 + 256), in every position; the
       spare bits stay 0. */
    for (uint32_t code = 0u; code < 366u; code++)
    {
        char           part[4];
        char           addr[CMD_IP_MAX];
        uint32_t const digits = (code < 10u) ? 1u : ((code < 110u) ? 2u : 3u);
        uint32_t const value  = (code < 10u) ? code
                                : ((code < 110u) ? (code - 10u)
                                                 : (code - 110u));

        (void) snprintf(part, sizeof(part), "%0*u", (int) digits,
                        (unsigned) value);
        (void) snprintf(addr, sizeof(addr), "%s.%s.%s.%s", part, part, part,
                        part);
        b_ok = b_ok && comms_broker_pack(&broker, addr, (uint16_t) code) &&
               (broker.word[0] == (code | (code << 9) | (code << 18))) &&
               (broker.word[1] == (code | (code << 16)));
        comms_broker_ip(&broker, ip, CMD_IP_MAX);
        b_ok = b_ok && (0 == strcmp(ip, addr)) &&
               (code == comms_broker_port(&broker));
    }

    UNIT_CHECK(b_ok);

    /* Leading zeros stay: lwIP would read "010" as octal 8. */
    for (uint32_t idx = 0u; idx < (sizeof(kept) / sizeof(kept[0])); idx++)
    {
        b_ok = b_ok && comms_broker_pack(&broker, kept[idx], 65535u);
        comms_broker_ip(&broker, ip, CMD_IP_MAX);
        b_ok = b_ok && (0 == strcmp(ip, kept[idx])) &&
               (65535u == comms_broker_port(&broker));
    }

    UNIT_CHECK(b_ok);

    /* 10,000 random broker commands, as the parser gives them. */
    for (uint32_t run = 0u; run < 10000u; run++)
    {
        uint32_t const port = 1u + (test_rand(&seed) % 65535u);
        uint32_t       pos  = 0u;
        char           addr[CMD_IP_MAX];

        for (uint32_t part = 0u; part < 4u; part++)
        {
            uint32_t const value  = test_rand(&seed) % 256u;
            uint32_t       digits = (value >= 100u) ? 3u
                                    : ((value >= 10u) ? 2u : 1u);

            digits += test_rand(&seed) % (4u - digits);     /* zeros */

            for (uint32_t d = digits; d > 0u; d--)
            {
                uint32_t div = 1u;

                for (uint32_t k = 1u; k < d; k++)
                {
                    div *= 10u;
                }

                addr[pos] = (char) ('0' + (int32_t) ((value / div) % 10u));
                pos++;
            }

            addr[pos] = (3u == part) ? '\0' : '.';
            pos++;
        }

        (void) snprintf(text, sizeof(text), "{\"seq\":%u,\"cmd\":\"broker\","
                        "\"ip\":\"%s\",\"port\":%u}", (unsigned) run, addr,
                        (unsigned) port);
        b_ok = b_ok && test_parse(text, &cmd) &&
               comms_broker_pack(&broker, cmd.ip, cmd.port);
        comms_broker_ip(&broker, ip, CMD_IP_MAX);
        b_ok = b_ok && (0 == strcmp(ip, addr)) &&
               (port == comms_broker_port(&broker));
    }

    UNIT_CHECK(b_ok);

    /* Not an address (the Command task never packs one): false, and
       "0.0.0.0" with the port. */
    UNIT_CHECK(!comms_broker_pack(&broker, "1.2.3.a", 80u));
    UNIT_EQ(broker.word[0], 0x00000000u);
    UNIT_EQ(broker.word[1], 0x00500000u);
    comms_broker_ip(&broker, ip, CMD_IP_MAX);
    UNIT_CHECK(0 == strcmp(ip, "0.0.0.0"));
    UNIT_CHECK(!comms_broker_pack(&broker, "1234567890123456", 80u));
    UNIT_CHECK(!comms_broker_pack(&broker, "1.2.3.256", 80u));
    comms_broker_ip(&broker, ip, CMD_IP_MAX);
    UNIT_CHECK(0 == strcmp(ip, "0.0.0.0"));
    UNIT_EQ(comms_broker_port(&broker), 80);
    UNIT_CHECK(!comms_broker_pack(NULL, "1.2.3.4", 80u));
    UNIT_CHECK(!comms_broker_pack(&broker, NULL, 80u));
    UNIT_EQ(comms_broker_port(NULL), 0);

    /* A code that cannot occur reads as "255". */
    broker.word[0] = 0x1FFu;
    broker.word[1] = 0u;
    comms_broker_ip(&broker, ip, CMD_IP_MAX);
    UNIT_CHECK(0 == strcmp(ip, "255.0.0.0"));

    /* A buffer too small is left alone. */
    ip[0] = 'x';
    comms_broker_ip(&broker, ip, CMD_IP_MAX - 1u);
    UNIT_EQ(ip[0], 'x');
    UNIT_END();
}

static void
test_nav_command (void)
{
    static char const * const actions[] =
    {
        "none", "straight", "left", "right", "uturn"
    };
    nav_cmd_t nav;
    cmd_t     cmd;
    char      text[96];
    uint32_t  seed = 0x5EED0102u;
    bool      b_ok = true;

    UNIT_BEGIN("S1-U41", "IF-10 Navigation command packed into two words: "
                         "the exact expected bits; Navigation reads every "
                         "parsed command exactly as the old record carried "
                         "it; an invalid one is refused (D-91)");
    UNIT_EQ(sizeof(nav_cmd_t), 8u);

    UNIT_CHECK(nav_cmd_pack(&nav, NAV_CMD_REMAP, 'E',
                            (uint8_t) LETTER_ACTION_RIGHT, 42u));
    UNIT_EQ(nav.word[0], 0x000000CAu);
    UNIT_EQ(nav.word[1], 0x0000002Au);
    UNIT_CHECK(nav_cmd_pack(&nav, NAV_CMD_MAP_DEFAULT, 'Q',
                            (uint8_t) LETTER_ACTION_LEFT, CMD_SEQ_MAX));
    UNIT_EQ(nav.word[0], 0x00000001u);          /* no letter, no action */
    UNIT_EQ(nav.word[1], 0x3B9AC9FFu);
    UNIT_EQ(nav_cmd_type(&nav), NAV_CMD_MAP_DEFAULT);
    UNIT_EQ(nav_cmd_letter(&nav), '\0');
    UNIT_EQ(nav_cmd_seq(&nav), CMD_SEQ_MAX);

    /* Every letter and action, through the parser as Command does it. */
    for (uint32_t run = 0u; run < 26000u; run++)
    {
        char const     letter = (char) ('A' + (int32_t) (run % 26u));
        uint32_t const action = (run / 26u) % 5u;
        uint32_t const seq    = (0u == (run % 7u)) ? CMD_SEQ_MAX
                                : (test_rand(&seed) % (CMD_SEQ_MAX + 1u));
        bool const     b_def  = (0u == (run % 11u));

        if (b_def)
        {
            (void) snprintf(text, sizeof(text), "{\"seq\":%u,"
                            "\"cmd\":\"default_map\"}", (unsigned) seq);
        }
        else
        {
            (void) snprintf(text, sizeof(text), "{\"seq\":%u,\"cmd\":"
                            "\"remap\",\"letter\":\"%c\",\"action\":"
                            "\"%s\"}", (unsigned) seq, letter,
                            actions[action]);
        }

        b_ok = b_ok && test_parse(text, &cmd) &&
               nav_cmd_pack(&nav, (CMD_CODE_REMAP == cmd.code)
                                  ? NAV_CMD_REMAP : NAV_CMD_MAP_DEFAULT,
                            cmd.letter, cmd.action, cmd.seq) &&
               (nav_cmd_seq(&nav) == cmd.seq) && (cmd.seq == seq);

        if (b_def)
        {
            b_ok = b_ok && (NAV_CMD_MAP_DEFAULT == nav_cmd_type(&nav));
        }
        else
        {
            b_ok = b_ok && (NAV_CMD_REMAP == nav_cmd_type(&nav)) &&
                   (nav_cmd_letter(&nav) == cmd.letter) &&
                   (nav_cmd_action(&nav) == cmd.action) &&
                   (cmd.action == action);
        }
    }

    UNIT_CHECK(b_ok);

    /* Not a valid command: a REMAP of no letter, which Navigation
       refuses (NAV-U12); the seq is still kept for the ack. */
    UNIT_CHECK(!nav_cmd_pack(&nav, NAV_CMD_REMAP, '7',
                             (uint8_t) LETTER_ACTION_LEFT, 5u));
    UNIT_EQ(nav_cmd_type(&nav), NAV_CMD_REMAP);
    UNIT_EQ(nav_cmd_letter(&nav), '\0');
    UNIT_EQ(nav_cmd_seq(&nav), 5);
    UNIT_CHECK(!nav_cmd_pack(&nav, NAV_CMD_REMAP, 'E',
                             (uint8_t) LETTER_ACTION_COUNT, 5u));
    UNIT_EQ(nav_cmd_letter(&nav), '\0');
    UNIT_CHECK(!nav_cmd_pack(&nav, 0u, 'E', (uint8_t) LETTER_ACTION_LEFT,
                             5u));
    UNIT_EQ(nav_cmd_type(&nav), NAV_CMD_REMAP);
    UNIT_EQ(nav_cmd_letter(&nav), '\0');
    UNIT_CHECK(!nav_cmd_pack(&nav, 3u, 'E', (uint8_t) LETTER_ACTION_LEFT,
                             5u));
    UNIT_EQ(nav_cmd_letter(&nav), '\0');

    /* A seq above 999,999,999 is reported; 30 bits keep up to 2^30 - 1. */
    UNIT_CHECK(!nav_cmd_pack(&nav, NAV_CMD_MAP_DEFAULT, '\0', 0u,
                             CMD_SEQ_MAX + 1u));
    UNIT_EQ(nav_cmd_seq(&nav), CMD_SEQ_MAX + 1u);
    UNIT_CHECK(!nav_cmd_pack(&nav, NAV_CMD_MAP_DEFAULT, '\0', 0u,
                             UINT32_MAX));
    UNIT_EQ(nav_cmd_seq(&nav), 0x3FFFFFFFu);
    UNIT_CHECK(!nav_cmd_pack(NULL, NAV_CMD_REMAP, 'E', 0u, 1u));
    UNIT_EQ(nav_cmd_type(NULL), 0);
    UNIT_EQ(nav_cmd_seq(NULL), 0);
    UNIT_END();
}

/* ---- Minimum-bit change (D-92): telemetry records ---- */

static void
test_tlm_records (void)
{
    tlm_msg_t      msg;
    tlm_line_t     line;
    tlm_motion_t   motion;
    tlm_imu_t      imu;
    tlm_scan_t     scan;
    tlm_nav_t      nav;
    tlm_safety_t   safety;
    tlm_route_t    route;
    tlm_ack_t      ack;
    tlm_fault_t    fault;
    tlm_link_t     link;
    tlm_obstacle_t obst;

    UNIT_BEGIN("S1-U42", "IF-12 records packed: the exact expected bits of "
                         "every record type; the message is 5 words; "
                         "values outside a field are reported and kept at "
                         "the nearest end (D-92)");
    UNIT_EQ(sizeof(tlm_msg_t), 20u);
    UNIT_EQ(sizeof(tlm_log_t), 16u);
    tlm_msg_init(&msg, TLM_MSG_OBSTACLE);
    UNIT_EQ(msg.head, 0x0000000Bu);
    UNIT_EQ(tlm_msg_type(&msg), TLM_MSG_OBSTACLE);
    UNIT_EQ(msg.u.ev.time_ms, 0u);

    UNIT_CHECK(tlm_line_pack(&line, 4095u, 0x123u, -1000, 200u, 'Z'));
    UNIT_EQ(line.word[0], 0xC8123FFFu);
    UNIT_EQ(line.word[1], 0x0000D418u);
    UNIT_EQ(tlm_line_raw_l(&line), 4095u);
    UNIT_EQ(tlm_line_raw_r(&line), 0x123u);
    UNIT_EQ(tlm_line_error_pm(&line), -1000);
    UNIT_EQ(tlm_line_letters(&line), 200u);
    UNIT_EQ(tlm_line_last_letter(&line), 'Z');
    UNIT_CHECK(tlm_line_pack(&line, 0u, 0u, 0, 0u, TLM_NO_LETTER));
    UNIT_EQ(tlm_line_last_letter(&line), '-');
    UNIT_CHECK(!tlm_line_pack(&line, 4096u, 0u, 1024, 256u, '?'));
    UNIT_EQ(tlm_line_raw_l(&line), 4095u);
    UNIT_EQ(tlm_line_error_pm(&line), 1023);
    UNIT_EQ(tlm_line_letters(&line), 255u);
    UNIT_EQ(tlm_line_last_letter(&line), '-');

    UNIT_CHECK(tlm_motion_pack(&motion, INT16_MIN, INT16_MAX, -1, 123456,
                               -1073741824));
    UNIT_EQ(motion.word[0], 0x7FFF8000u);
    UNIT_EQ(motion.word[1], 0xFFFFFFFFu);
    UNIT_EQ(motion.word[2], 0x0001E240u);
    UNIT_EQ(motion.word[3], 0x40000000u);
    UNIT_EQ(tlm_motion_speed_l(&motion), INT16_MIN);
    UNIT_EQ(tlm_motion_speed_r(&motion), INT16_MAX);
    UNIT_EQ(tlm_motion_count_l(&motion), -1);
    UNIT_EQ(tlm_motion_count_r(&motion), 123456);
    UNIT_EQ(tlm_motion_distance_mm(&motion), -1073741824);
    UNIT_CHECK(tlm_motion_pack(&motion, 0, 0, INT32_MIN, INT32_MAX,
                               1073741823));
    UNIT_EQ(tlm_motion_count_l(&motion), INT32_MIN);
    UNIT_EQ(tlm_motion_count_r(&motion), INT32_MAX);
    UNIT_EQ(tlm_motion_distance_mm(&motion), 1073741823);
    UNIT_CHECK(!tlm_motion_pack(&motion, 40000, 0, 0, 0, INT32_MIN));
    UNIT_EQ(tlm_motion_speed_l(&motion), INT16_MAX);
    UNIT_EQ(tlm_motion_distance_mm(&motion), -1073741824);

    UNIT_CHECK(tlm_imu_pack(&imu, IMU_CLASS_STEADY, 255u, 65, -300));
    UNIT_EQ(imu.word[0], 0x07FEFED4u);
    UNIT_EQ(imu.word[1], 0x00000041u);
    UNIT_EQ(tlm_imu_class(&imu), IMU_CLASS_STEADY);
    UNIT_EQ(tlm_imu_humps(&imu), 255u);
    UNIT_EQ(tlm_imu_max_height_mm(&imu), 65);
    UNIT_EQ(tlm_imu_rate_dps(&imu), -300);
    UNIT_CHECK(!tlm_imu_pack(&imu, IMU_CLASS_COUNT, 0u, 128, 0));
    UNIT_EQ(tlm_imu_class(&imu), IMU_CLASS_COUNT);     /* prints "?" */
    UNIT_EQ(tlm_imu_max_height_mm(&imu), 127);

    UNIT_CHECK(tlm_scan_pack(&scan, 4000, 65535u));
    UNIT_EQ(scan.word[0], 0x0FFFFFA0u);
    UNIT_EQ(tlm_scan_range_mm(&scan), 4000);
    UNIT_EQ(tlm_scan_obstacles(&scan), 65535u);
    UNIT_CHECK(!tlm_scan_pack(&scan, -1, 0u));
    UNIT_EQ(tlm_scan_range_mm(&scan), 0);

    UNIT_CHECK(tlm_nav_pack(&nav, 10u, 'C', 3u));
    UNIT_EQ(nav.word[0], 0x0000063Au);
    UNIT_EQ(tlm_nav_mode(&nav), 10u);
    UNIT_EQ(tlm_nav_held_letter(&nav), 'C');
    UNIT_EQ(tlm_nav_hold_reason(&nav), 3u);
    UNIT_CHECK(tlm_nav_pack(&nav, 0u, TLM_NO_LETTER, 0u));
    UNIT_EQ(tlm_nav_held_letter(&nav), '-');

    UNIT_CHECK(tlm_safety_pack(&safety, true, FAULT_CAUSE_MAX, true, 3u,
                               1000u, 65535u, FAULT_MASK_ALL));
    UNIT_EQ(safety.word[0], 0x00FA00F7u);
    UNIT_EQ(safety.word[1], 0x07FFFFFFu);
    UNIT_CHECK(tlm_safety_faulted(&safety));
    UNIT_EQ(tlm_safety_cause(&safety), FAULT_CAUSE_MAX);
    UNIT_CHECK(tlm_safety_wdt_armed(&safety));
    UNIT_EQ(tlm_safety_queue_full(&safety), 3u);
    UNIT_EQ(tlm_safety_faults(&safety), 1000u);
    UNIT_EQ(tlm_safety_missed(&safety), 65535u);
    UNIT_EQ(tlm_safety_mask(&safety), FAULT_MASK_ALL);
    UNIT_CHECK(!tlm_safety_pack(&safety, false, 0u, false, 0u, 0u, 0u,
                                FAULT_MASK_ALL + 1u));
    UNIT_EQ(tlm_safety_mask(&safety), FAULT_MASK_ALL);
    UNIT_EQ(tlm_safety_mask(NULL), 0u);

    UNIT_CHECK(tlm_route_pack(&route, ROUTE_TURN, 'Q', LETTER_ACTION_RIGHT,
                              -5, -9000, 8950));
    UNIT_EQ(route.word[0], 0x7FFFFFFBu);
    UNIT_EQ(route.word[1], 0x006E6C33u);
    UNIT_EQ(route.word[2], 0x000022F6u);
    UNIT_EQ(tlm_route_letter(&route), '\0');            /* not carried */
    UNIT_EQ(tlm_route_distance_mm(&route), -5);
    UNIT_CHECK(tlm_route_pack(&route, ROUTE_COMPLETE, '\0', 0u, 1000,
                              600000, 0));
    UNIT_EQ(route.word[0], 0x000003E8u);
    UNIT_EQ(route.word[1], 0x00000008u);
    UNIT_EQ(route.word[2], 0x000927C0u);
    UNIT_CHECK(tlm_route_pack(&route, ROUTE_BARCODE, 'E', LETTER_ACTION_LEFT,
                              0, 1, 77));
    UNIT_EQ(route.word[1], 0x00001452u);
    UNIT_EQ(tlm_route_value2(&route), 0);               /* not carried */
    UNIT_CHECK(tlm_route_pack(&route, ROUTE_HUMP, '\0', 0u, 0, 34, 1500));
    UNIT_EQ(route.word[1], 0x002EE224u);
    UNIT_CHECK(tlm_route_pack(&route, ROUTE_OBSTACLE, '\0', 1u, 0, -1000,
                              500));
    UNIT_EQ(route.word[1], 0x03E90615u);
    UNIT_CHECK(tlm_route_pack(&route, ROUTE_LINE_SEARCH, '\0', 0u, 0, 1, 0));
    UNIT_EQ(route.word[1], 0x00000016u);
    UNIT_CHECK(tlm_route_pack(&route, ROUTE_SAFE_HOLD, '\0', 0u, 0, 3, 0));
    UNIT_EQ(route.word[1], 0x00000037u);
    UNIT_CHECK(!tlm_route_pack(&route, ROUTE_HUMP, '\0', 0u, 0, 128, -1));
    UNIT_EQ(tlm_route_value(&route), 127);
    UNIT_EQ(tlm_route_value2(&route), 0);
    UNIT_CHECK(!tlm_route_pack(&route, ROUTE_BARCODE, '7', 0u, 0, 0, 0));
    UNIT_EQ(tlm_route_letter(&route), '\0');

    UNIT_CHECK(tlm_ack_pack(&ack, CMD_SEQ_MAX, CMD_CODE_MAP_DEFAULT, true));
    UNIT_EQ(ack.word[0], 0x3B9AC9FFu);
    UNIT_EQ(ack.word[1], 0x0000000Du);
    UNIT_EQ(tlm_ack_seq(&ack), CMD_SEQ_MAX);
    UNIT_EQ(tlm_ack_code(&ack), CMD_CODE_MAP_DEFAULT);
    UNIT_CHECK(tlm_ack_ok(&ack));

    UNIT_CHECK(tlm_fault_pack(&fault, FAULT_CAUSE_MAX, FAULT_SRC_SAFETY,
                              FAULT_DETAIL_MAX, true));
    UNIT_EQ(fault.word[0], 0x001FFFFBu);
    UNIT_EQ(tlm_fault_cause(&fault), FAULT_CAUSE_MAX);
    UNIT_EQ(tlm_fault_source(&fault), FAULT_SRC_SAFETY);
    UNIT_EQ(tlm_fault_detail(&fault), FAULT_DETAIL_MAX);
    UNIT_CHECK(tlm_fault_latched(&fault));
    UNIT_CHECK(!tlm_fault_pack(&fault, 0u, 0u, 8192u, false));
    UNIT_EQ(tlm_fault_source(&fault), FAULT_SRC_SAFETY);
    UNIT_EQ(tlm_fault_detail(&fault), FAULT_DETAIL_MAX);

    UNIT_CHECK(tlm_link_pack(&link, true, -3));
    UNIT_EQ(link.word[0], 0x0000000Bu);
    UNIT_CHECK(tlm_link_online(&link));
    UNIT_EQ(tlm_link_wifi(&link), -3);
    UNIT_CHECK(tlm_link_pack(&link, false, 3));
    UNIT_EQ(tlm_link_wifi(&link), 3);
    UNIT_CHECK(!tlm_link_pack(&link, false, 4));
    UNIT_EQ(tlm_link_wifi(&link), 3);

    UNIT_CHECK(tlm_obstacle_pack(&obst, 2u, true, 210, -15, 160, 215, 4000,
                                 -1));
    UNIT_EQ(obst.word[0], 0x35FF1696u);
    UNIT_EQ(obst.word[1], 0x007D08A0u);
    UNIT_EQ(obst.word[2], 0x00000000u);
    UNIT_EQ(tlm_obstacle_side(&obst), 2u);
    UNIT_CHECK(tlm_obstacle_reversed(&obst));
    UNIT_EQ(tlm_obstacle_distance_mm(&obst), 210);
    UNIT_EQ(tlm_obstacle_offset_mm(&obst), -15);
    UNIT_EQ(tlm_obstacle_width_mm(&obst), 160);
    UNIT_EQ(tlm_obstacle_closest_mm(&obst), 215);
    UNIT_EQ(tlm_obstacle_clear_left_mm(&obst), 4000);
    UNIT_EQ(tlm_obstacle_clear_right_mm(&obst), -1);
    UNIT_CHECK(!tlm_obstacle_pack(&obst, 0u, false, -1, 0, 0, 0, INT32_MAX,
                                  -2));
    UNIT_EQ(tlm_obstacle_distance_mm(&obst), 0);
    UNIT_EQ(tlm_obstacle_clear_left_mm(&obst), 4094);
    UNIT_EQ(tlm_obstacle_clear_right_mm(&obst), -1);

    UNIT_EQ(tlm_msg_type(NULL), 0u);
    UNIT_EQ(tlm_route_kind(NULL), 0u);
    UNIT_EQ(tlm_route_value(NULL), 0);
    UNIT_END();
}

/* low..high (high - low below 2^31). */
static int32_t
test_between (uint32_t * p_seed, int32_t low, int32_t high)
{
    return (low + (int32_t) (test_rand(p_seed) %
                             ((uint32_t) (high - low) + 1u)));
}

/* 'A'..'Z', or none one time in 27. */
static char
test_letter (uint32_t * p_seed, char none)
{
    uint32_t const code = test_rand(p_seed) % 27u;

    return ((0u == code) ? none : (char) ('A' + (int32_t) (code - 1u)));
}

/* One random value of a kind's route fields, as Navigation sends them,
   into both the old record (with random values in the fields the kind
   does not carry) and the packed one. */
static void
test_route_pair (uint32_t * p_seed, uint8_t kind, ref_tlm_route_t * p_old,
                 tlm_route_t * p_now)
{
    char    letter = (char) test_between(p_seed, 0, 127);
    uint8_t action = (uint8_t) test_rand(p_seed);
    int32_t value  = (int32_t) test_rand(p_seed);
    int32_t value2 = (int32_t) (test_rand(p_seed) & 0x7FFFFFFFu);

    switch (kind)
    {
        case ROUTE_BARCODE:
            letter = test_letter(p_seed, 'A');
            action = (uint8_t) test_between(p_seed, 0, 7);
            value  = test_between(p_seed, 0, 1);
        break;

        case ROUTE_TURN:
            action = (uint8_t) test_between(p_seed, 0, 7);
            value  = test_between(p_seed, INT16_MIN, INT16_MAX);
            value2 = test_between(p_seed, INT16_MIN, INT16_MAX);
        break;

        case ROUTE_HUMP:
            value  = test_between(p_seed, 0, 127);
            value2 = test_between(p_seed, 0, 32767);
        break;

        case ROUTE_OBSTACLE:
            action = (uint8_t) test_between(p_seed, 0, 3);
            value  = test_between(p_seed, -1024, 1023);
            value2 = test_between(p_seed, 0, 511);
        break;

        case ROUTE_LINE_SEARCH:
            value = test_between(p_seed, 0, 1);
        break;

        case ROUTE_SAFE_HOLD:
            value = test_between(p_seed, 0, 3);
        break;

        default:
            /* COMPLETE keeps the random 32-bit value; others carry none. */
        break;
    }

    p_old->kind        = kind;
    p_old->letter      = letter;
    p_old->action      = action;
    p_old->reason      = (uint8_t) test_rand(p_seed);   /* never printed */
    p_old->distance_mm = test_between(p_seed, -1073741824, 1073741823);
    p_old->value       = value;
    p_old->value2      = value2;
    (void) tlm_route_pack(p_now, kind, letter, action, p_old->distance_mm,
                          value, value2);
}

/* The heartbeat now also carries Safety's fault mask, right after
   "cause" (24 Oct review, R-02): the old message with that one field
   added is the new one. */
static void
test_add_mask (char * p_buf, uint32_t mask)
{
    char * const p_wdt = strstr(p_buf, ",\"wdt\":");
    char         tail[TEST_BUF];

    if (NULL != p_wdt)
    {
        (void) snprintf(tail, sizeof(tail), "%s", p_wdt);
        (void) snprintf(p_wdt, TEST_BUF - (size_t) (p_wdt - p_buf),
                        ",\"mask\":%u%s", (unsigned) mask, tail);
    }
}

static void
test_tlm_equivalence (void)
{
    static tlm_state_t     now;
    static ref_tlm_state_t old;
    static char const * const modes[] =
    {
        "BOOT", "LINE FOLLOWING", "AVOIDING OBSTACLE"
    };
    char      now_buf[TEST_BUF];
    char      old_buf[TEST_BUF];
    uint32_t  seed       = 0x5EED0103u;
    uint32_t  mismatches = 0u;
    uint32_t  compared   = 0u;
    uint32_t  mask       = 0u;

    UNIT_BEGIN("S1-U43", "the packed records give byte-identical MQTT "
                         "messages to the old records and formatter (kept "
                         "in testing/host/ref/): 20,000 random states and "
                         "120,000 random events of every type; the fields "
                         "dropped were never printed (D-92); the only "
                         "addition is the heartbeat's fault mask (R-02)");
    tlm_state_init(&now);
    ref_tlm_state_init(&old);

    for (uint32_t run = 0u; run < 20000u; run++)
    {
        uint32_t const seq  = test_rand(&seed);
        uint32_t const time = test_rand(&seed);
        uint32_t const beat = test_rand(&seed);
        int32_t  const v[8] =
        {
            test_between(&seed, 0, 4095), test_between(&seed, 0, 4095),
            test_between(&seed, -1024, 1023), test_between(&seed, 0, 255),
            test_between(&seed, INT16_MIN, INT16_MAX),
            test_between(&seed, INT16_MIN, INT16_MAX),
            (int32_t) test_rand(&seed),
            test_between(&seed, -1073741824, 1073741823)
        };
        int32_t  const count_r = (int32_t) test_rand(&seed);
        char     const last    = test_letter(&seed, TLM_NO_LETTER);
        char     const held    = test_letter(&seed, TLM_NO_LETTER);

        /* Line, Motion: the same values in both. */
        old.line.raw_l       = (int16_t) v[0];
        old.line.raw_r       = (int16_t) v[1];
        old.line.error_pm    = (int16_t) v[2];
        old.line.letters     = (uint8_t) v[3];
        old.line.last_letter = last;
        old.line.b_lost      = (uint8_t) test_rand(&seed);  /* dropped */
        old.line.failed      = (uint8_t) test_rand(&seed);  /* dropped */
        (void) tlm_line_pack(&now.line, (uint32_t) v[0], (uint32_t) v[1],
                             v[2], (uint32_t) v[3], last);
        old.motion.speed_l_mmps = (int16_t) v[4];
        old.motion.speed_r_mmps = (int16_t) v[5];
        old.motion.count_l      = v[6];
        old.motion.count_r      = count_r;
        old.motion.distance_mm  = v[7];
        old.motion.state        = (uint8_t) test_rand(&seed);
        (void) tlm_motion_pack(&now.motion, v[4], v[5], v[6], count_r, v[7]);

        /* IMU, Scan, Navigation, Safety, link. */
        old.imu.motion_class  = (uint8_t) test_between(&seed, 0, 7);
        old.imu.humps         = (uint8_t) test_rand(&seed);
        old.imu.max_height_mm = (int16_t) test_between(&seed, 0, 127);
        old.imu.turn_rate_dps = (int16_t) test_between(&seed, INT16_MIN,
                                                       INT16_MAX);
        old.imu.heading_deg   = (int16_t) test_rand(&seed);
        old.imu.roll_ddeg     = (int16_t) test_rand(&seed);
        (void) tlm_imu_pack(&now.imu, old.imu.motion_class, old.imu.humps,
                            old.imu.max_height_mm, old.imu.turn_rate_dps);
        old.scan.range_mm    = (int16_t) test_between(&seed, 0, 4095);
        old.scan.obstacles   = (uint16_t) test_rand(&seed);
        old.scan.state       = (uint8_t) test_rand(&seed);
        old.scan.invalid_run = (uint8_t) test_rand(&seed);
        old.scan.angle_deg   = (int16_t) test_rand(&seed);
        (void) tlm_scan_pack(&now.scan, old.scan.range_mm,
                             old.scan.obstacles);
        old.nav.mode         = (uint8_t) test_between(&seed, 0, 15);
        old.nav.held_letter  = held;
        old.nav.hold_reason  = (uint8_t) test_between(&seed, 0, 3);
        old.nav.held_action  = (uint8_t) test_rand(&seed);
        old.nav.heading_cdeg = (int32_t) test_rand(&seed);
        old.nav.run_ms       = test_rand(&seed);
        (void) tlm_nav_pack(&now.nav, old.nav.mode, held,
                            old.nav.hold_reason);
        old.safety.b_faulted       = (uint8_t) test_between(&seed, 0, 1);
        old.safety.cause           = (uint8_t) test_between(&seed, 0, 7);
        old.safety.b_wdt_armed     = (uint8_t) test_between(&seed, 0, 1);
        old.safety.queue_full      = (uint8_t) test_rand(&seed);
        old.safety.faults          = (uint16_t) test_rand(&seed);
        old.safety.missed_checkins = (uint16_t) test_rand(&seed);
        mask = test_rand(&seed) & FAULT_MASK_ALL;
        (void) tlm_safety_pack(&now.safety, (0u != old.safety.b_faulted),
                               old.safety.cause,
                               (0u != old.safety.b_wdt_armed),
                               old.safety.queue_full, old.safety.faults,
                               old.safety.missed_checkins, mask);
        old.link.b_online    = (uint8_t) test_between(&seed, 0, 1);
        old.link.wifi_status = test_between(&seed, -4, 3);
        (void) tlm_link_pack(&now.link, (0u != old.link.b_online),
                             old.link.wifi_status);
        old.p_mode_name = modes[run % 3u];
        now.p_mode_name = modes[run % 3u];
        old.dropped     = test_rand(&seed);
        now.dropped     = old.dropped;

        (void) tlm_format_telemetry(&now, seq, time, now_buf, TEST_BUF);
        (void) ref_tlm_format_telemetry(&old, seq, time, old_buf, TEST_BUF);
        mismatches += (0 != strcmp(now_buf, old_buf)) ? 1u : 0u;
        (void) tlm_format_heartbeat(&now, seq, time, beat, now_buf,
                                    TEST_BUF);
        (void) ref_tlm_format_heartbeat(&old, seq, time, beat, old_buf,
                                        TEST_BUF);
        test_add_mask(old_buf, mask);
        mismatches += (0 != strcmp(now_buf, old_buf)) ? 1u : 0u;
        compared   += 2u;
    }

    /* Events: route entries (and the run totals), acks, faults, links
       and obstacle profiles. */
    for (uint32_t run = 0u; run < 20000u; run++)
    {
        uint32_t const  seq   = test_rand(&seed);
        uint32_t const  time  = test_rand(&seed);
        uint32_t const  pick  = test_rand(&seed) % 40u;
        /* Mostly real kinds; RUN_START now and then starts a new run. */
        uint8_t const   kind  = (0u == pick) ? ROUTE_RUN_START
                                : ((1u == pick)
                                   ? (uint8_t) test_between(&seed, 11, 15)
                                   : (uint8_t) test_between(&seed, 2, 10));
        bool const      b_re  = (0u == (test_rand(&seed) % 3u));
        ref_tlm_route_t old_route;
        tlm_route_t     now_route;
        int32_t         index_now = 0;
        int32_t         index_old = 0;
        ref_tlm_ack_t   old_ack   = { 0 };
        tlm_ack_t       now_ack;
        ref_tlm_fault_t old_fault = { 0 };
        tlm_fault_t     now_fault;
        ref_tlm_link_t  old_link  = { 0 };
        tlm_link_t      now_link;
        ref_tlm_obstacle_t old_obst = { 0 };
        tlm_obstacle_t  now_obst;

        test_route_pair(&seed, kind, &old_route, &now_route);
        index_now = tlm_state_route(&now, &now_route, time);
        index_old = ref_tlm_state_route(&old, &old_route, time);
        mismatches += (index_now != index_old) ? 1u : 0u;
        (void) tlm_format_route(&now_route, seq, time, index_now, b_re,
                                now_buf, TEST_BUF);
        (void) ref_tlm_format_route(&old_route, seq, time, index_old, b_re,
                                    old_buf, TEST_BUF);
        mismatches += (0 != strcmp(now_buf, old_buf)) ? 1u : 0u;

        (void) tlm_format_summary(&now, seq, time, now_buf, TEST_BUF);
        (void) ref_tlm_format_summary(&old, seq, time, old_buf, TEST_BUF);
        mismatches += (0 != strcmp(now_buf, old_buf)) ? 1u : 0u;

        old_ack.cmd_seq  = test_rand(&seed) & 0x3FFFFFFFu;
        old_ack.cmd_code = (uint8_t) test_between(&seed, 0, 7);
        old_ack.b_ok     = (uint8_t) test_between(&seed, 0, 1);
        old_ack.detail   = (uint8_t) test_rand(&seed);  /* never printed */
        (void) tlm_ack_pack(&now_ack, old_ack.cmd_seq, old_ack.cmd_code,
                            (0u != old_ack.b_ok));
        (void) tlm_format_ack(&now_ack, seq, time, now_buf, TEST_BUF);
        (void) ref_tlm_format_ack(&old_ack, seq, time, old_buf, TEST_BUF);
        mismatches += (0 != strcmp(now_buf, old_buf)) ? 1u : 0u;

        old_fault.cause     = (uint8_t) test_between(&seed, 0, 7);
        old_fault.source    = (uint8_t) test_between(&seed, 1, 8);
        old_fault.detail    = (uint16_t) test_between(&seed, 0, 8191);
        old_fault.b_latched = (uint8_t) test_between(&seed, 0, 1);
        (void) tlm_fault_pack(&now_fault, old_fault.cause, old_fault.source,
                              old_fault.detail, (0u != old_fault.b_latched));
        (void) tlm_format_fault(&now_fault, seq, time, now_buf, TEST_BUF);
        (void) ref_tlm_format_fault(&old_fault, seq, time, old_buf,
                                    TEST_BUF);
        mismatches += (0 != strcmp(now_buf, old_buf)) ? 1u : 0u;

        old_link.b_online    = (uint8_t) test_between(&seed, 0, 1);
        old_link.wifi_status = test_between(&seed, -4, 3);
        (void) tlm_link_pack(&now_link, (0u != old_link.b_online),
                             old_link.wifi_status);
        (void) tlm_format_link(&now_link, seq, time, now_buf, TEST_BUF);
        (void) ref_tlm_format_link(&old_link, seq, time, old_buf, TEST_BUF);
        mismatches += (0 != strcmp(now_buf, old_buf)) ? 1u : 0u;

        old_obst.side           = (uint8_t) test_between(&seed, 0, 3);
        old_obst.b_reversed     = (uint8_t) test_between(&seed, 0, 1);
        old_obst.distance_mm    = (int16_t) test_between(&seed, 0, 511);
        old_obst.offset_mm      = (int16_t) test_between(&seed, -512, 511);
        old_obst.width_mm       = (int16_t) test_between(&seed, -1024, 1023);
        old_obst.closest_mm     = (int16_t) test_between(&seed, 0, 511);
        old_obst.clear_left_mm  = (int16_t) test_between(&seed, -1, 4094);
        old_obst.clear_right_mm = (int16_t) test_between(&seed, -1, 4094);
        (void) tlm_obstacle_pack(&now_obst, old_obst.side,
                                 (0u != old_obst.b_reversed),
                                 old_obst.distance_mm, old_obst.offset_mm,
                                 old_obst.width_mm, old_obst.closest_mm,
                                 old_obst.clear_left_mm,
                                 old_obst.clear_right_mm);
        (void) tlm_format_obstacle(&now_obst, seq, time, now_buf, TEST_BUF);
        (void) ref_tlm_format_obstacle(&old_obst, seq, time, old_buf,
                                       TEST_BUF);
        mismatches += (0 != strcmp(now_buf, old_buf)) ? 1u : 0u;
        compared   += 6u;
    }

    /* The logs replay the same messages. */
    UNIT_EQ(now.log_count, old.log_count);

    for (uint32_t idx = 0u; idx < now.log_count; idx++)
    {
        (void) tlm_format_route(&now.log[idx].entry, 1u, now.log[idx].time_ms,
                                (int32_t) idx, true, now_buf, TEST_BUF);
        (void) ref_tlm_format_route(&old.log[idx].entry, 1u,
                                    old.log[idx].time_ms, (int32_t) idx,
                                    true, old_buf, TEST_BUF);
        mismatches += (0 != strcmp(now_buf, old_buf)) ? 1u : 0u;
        compared++;
    }

    UNIT_EQ(mismatches, 0u);
    UNIT_CHECK(compared > 160000u);
    UNIT_END();
}

static void
test_profile_fits (void)
{
    scan_set_t     set;
    scan_profile_t prof;
    tlm_obstacle_t obst;
    uint32_t       seed     = 0x5EED0104u;
    uint32_t       profiles = 0u;
    bool           b_ok     = true;

    UNIT_BEGIN("S1-U44", "every obstacle profile the scan code makes from "
                         "20,000 random scans fits the packed profile "
                         "record and reads back exactly (D-92)");

    /* As S5-U40: obstacles near and far, gaps, invalid readings; 6
       degree steps, so all 31 readings fit the 32 a scan set holds. */
    for (uint32_t run = 0u; run < 20000u; run++)
    {
        uint32_t const near = 20u + (test_rand(&seed) % 530u);
        int32_t const  from = (int32_t) (test_rand(&seed) % 150u);
        int32_t const  to   = from + (int32_t) (test_rand(&seed) % 90u);

        scan_set_clear(&set);

        for (int32_t angle = 0; angle <= 180; angle += 6)
        {
            uint32_t const pick  = test_rand(&seed) % 10u;
            int32_t        range = (int32_t) (test_rand(&seed) % 4001u);

            if ((angle >= from) && (angle <= to) && (pick < 8u))
            {
                range = (int32_t) near + (int32_t) (test_rand(&seed) % 120u);
            }

            scan_set_add(&set, angle, range, (0u != (pick % 7u)));
        }

        scan_profile(&set, &prof);

        if (prof.b_found)               /* a profile is sent only then */
        {
            bool const b_rev = (0u != (run & 1u));

            profiles++;
            b_ok = b_ok &&
                   tlm_obstacle_pack(&obst, prof.side, b_rev,
                                     prof.distance_mm, prof.offset_mm,
                                     prof.width_mm, prof.closest_mm,
                                     prof.clear_left_mm,
                                     prof.clear_right_mm) &&
                   (tlm_obstacle_side(&obst) == prof.side) &&
                   (tlm_obstacle_reversed(&obst) == b_rev) &&
                   (tlm_obstacle_distance_mm(&obst) == prof.distance_mm) &&
                   (tlm_obstacle_offset_mm(&obst) == prof.offset_mm) &&
                   (tlm_obstacle_width_mm(&obst) == prof.width_mm) &&
                   (tlm_obstacle_closest_mm(&obst) == prof.closest_mm) &&
                   (tlm_obstacle_clear_left_mm(&obst) ==
                    prof.clear_left_mm) &&
                   (tlm_obstacle_clear_right_mm(&obst) ==
                    prof.clear_right_mm);
        }
    }

    UNIT_CHECK(b_ok);
    UNIT_CHECK(profiles > 10000u);
    UNIT_END();
}

void
test_s1 (void)
{
    test_cmd_valid();
    test_cmd_invalid();
    test_cmd_helpers();
    test_cmd_ip();
    test_json_writer();
    test_route_state();
    test_summary();
    test_lengths();
    test_route_format();
    test_names();
    test_cmd_is_stop();
    test_obstacle_format();
    test_broker_record();
    test_nav_command();
    test_tlm_records();
    test_tlm_equivalence();
    test_profile_fits();
}

/*** end of file ***/
