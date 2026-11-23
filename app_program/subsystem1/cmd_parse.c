/*
 *  cmd_parse.c
 *  Buddy 1's MQTT command parser.  See cmd_parse.h.
 */

#include <stddef.h>

#include "../common/bitpack.h"
#include "../subsystem3/letter_map.h"
#include "cmd_parse.h"
#include "tlm_msgs.h"

#define CMD_NAME_MAX        16u
#define CMD_TEXT_MAX        16u
#define CMD_DECIMAL         10
#define CMD_INT_DIGITS_MAX  9u      /* keeps the value inside int32 */

/* A seq has at most 9 digits: CMD_SEQ_MAX (tlm_msgs.h) is the largest. */
BITPACK_ASSERT((9u == CMD_INT_DIGITS_MAX) && (999999999u == CMD_SEQ_MAX),
               cmd_seq_max);
#define CMD_PORT_MAX        65535
#define CMD_IP_PARTS        4u
#define CMD_IP_PART_MAX     255
#define CMD_IP_DIGITS_MAX   3u

/* IF-15 broker record layout (cmd_parse.h): each part's spelling as a
   code; 10 one-digit, 100 two-digit and 256 three-digit spellings. */
#define BROKER_ONE_DIGIT    0u      /* first code of each length */
#define BROKER_TWO_DIGITS   10u
#define BROKER_THREE_DIGITS 110u
#define BROKER_CODES        (BROKER_THREE_DIGITS + CMD_IP_PART_MAX + 1u)
#define BROKER_PART_BITS    9u
#define BROKER_PARTS_WORD0  3u      /* parts 0-2 in word 0, part 3 in 1 */
#define BROKER_PORT_SHIFT   16u     /* word 1 */
#define BROKER_PORT_BITS    16u

BITPACK_ASSERT(BITPACK_BITS_FOR(BROKER_CODES - 1u) == BROKER_PART_BITS,
               broker_part_bits);
BITPACK_ASSERT((BROKER_PARTS_WORD0 * BROKER_PART_BITS) <= 32u,
               broker_word0_fits);
BITPACK_ASSERT((BROKER_PART_BITS <= BROKER_PORT_SHIFT) &&
               ((BROKER_PORT_SHIFT + BROKER_PORT_BITS) <= 32u),
               broker_word1_fits);
BITPACK_ASSERT(BITPACK_BITS_FOR(CMD_PORT_MAX) == BROKER_PORT_BITS,
               broker_port_bits);

typedef struct
{
    char const * p_name;
    uint8_t      code;
} cmd_name_t;

/* Action names, as in the letter map (report FR6). */
static cmd_name_t const g_actions[] =
{
    { "none",     (uint8_t) LETTER_ACTION_NONE },
    { "straight", (uint8_t) LETTER_ACTION_STRAIGHT },
    { "left",     (uint8_t) LETTER_ACTION_LEFT },
    { "right",    (uint8_t) LETTER_ACTION_RIGHT },
    { "uturn",    (uint8_t) LETTER_ACTION_UTURN }
};

#define CMD_ACTIONS (sizeof(g_actions) / sizeof(g_actions[0]))

static cmd_name_t const g_commands[] =
{
    { "remap",       CMD_CODE_REMAP },
    { "default_map", CMD_CODE_MAP_DEFAULT },
    { "summary",     CMD_CODE_SUMMARY },
    { "stop",        CMD_CODE_STOP },
    { "broker",      CMD_CODE_BROKER },
    { "start",       CMD_CODE_START },
    { "reset",       CMD_CODE_RESET }
};

#define CMD_COUNT   (sizeof(g_commands) / sizeof(g_commands[0]))

static int32_t cmd_find_value(char const * p_text, uint32_t len,
                              char const * p_key);
static bool    cmd_text_equal(char const * p_left, char const * p_right);
static void    broker_set_part(comms_broker_t * p_broker, uint32_t part,
                               uint32_t code);
static uint32_t broker_part(comms_broker_t const * p_broker, uint32_t part);

/*
 * Parse one command.  Returns false (code CMD_CODE_UNKNOWN) when the
 * command name or a required field is missing or invalid; seq is still
 * filled when present so the rejection can be acknowledged.
 */
bool
cmd_parse (char const * p_text, uint32_t len, cmd_t * p_cmd)
{
    char    name[CMD_NAME_MAX];
    char    text[CMD_TEXT_MAX];
    int32_t value = 0;
    bool    b_ok  = false;

    if ((NULL == p_text) || (NULL == p_cmd))
    {
        return (false);
    }

    p_cmd->code   = CMD_CODE_UNKNOWN;
    p_cmd->seq    = 0u;
    p_cmd->letter = '\0';
    p_cmd->action = (uint8_t) LETTER_ACTION_NONE;
    p_cmd->ip[0]  = '\0';
    p_cmd->port   = 0u;

    if (cmd_json_int(p_text, len, "seq", &value) && (value >= 0))
    {
        p_cmd->seq = (uint32_t) value;      /* non-negative, checked */
    }

    if (cmd_json_string(p_text, len, "cmd", name, CMD_NAME_MAX))
    {
        for (uint32_t idx = 0u; idx < CMD_COUNT; idx++)
        {
            if (cmd_text_equal(name, g_commands[idx].p_name))
            {
                p_cmd->code = g_commands[idx].code;
            }
        }
    }

    switch (p_cmd->code)
    {
        case CMD_CODE_REMAP:
            b_ok = cmd_json_string(p_text, len, "letter", text, CMD_TEXT_MAX)
                   && (text[0] >= 'A') && (text[0] <= 'Z') &&
                   ('\0' == text[1]) &&
                   cmd_json_string(p_text, len, "action", name, CMD_NAME_MAX);

            if (b_ok)
            {
                b_ok = false;
                p_cmd->letter = text[0];

                for (uint32_t idx = 0u; idx < CMD_ACTIONS; idx++)
                {
                    if (cmd_text_equal(name, g_actions[idx].p_name))
                    {
                        p_cmd->action = g_actions[idx].code;
                        b_ok = true;
                    }
                }
            }
        break;

        case CMD_CODE_BROKER:
            b_ok = cmd_json_string(p_text, len, "ip", p_cmd->ip, CMD_IP_MAX)
                   && cmd_ip_valid(p_cmd->ip) &&
                   cmd_json_int(p_text, len, "port", &value) &&
                   (value > 0) && (value <= CMD_PORT_MAX);

            if (b_ok)
            {
                p_cmd->port = (uint16_t) value;     /* 1..65535 */
            }
        break;

        case CMD_CODE_MAP_DEFAULT:
        case CMD_CODE_SUMMARY:
        case CMD_CODE_STOP:
        case CMD_CODE_START:
        case CMD_CODE_RESET:
            b_ok = true;
        break;

        default:
            b_ok = false;
        break;
    }

    if (!b_ok)
    {
        p_cmd->code = CMD_CODE_UNKNOWN;
    }

    return (b_ok);
}

/* True when the text is a valid STOP command (24 Oct review, R-04). */
bool
cmd_is_stop (char const * p_text, uint32_t len)
{
    cmd_t cmd;

    return (cmd_parse(p_text, len, &cmd) && (CMD_CODE_STOP == cmd.code));
}

/* The string value of "key" (no escapes), copied with a NUL. */
bool
cmd_json_string (char const * p_text, uint32_t len, char const * p_key,
                 char * p_out, uint32_t out_size)
{
    int32_t const start = cmd_find_value(p_text, len, p_key);
    bool          b_ok  = false;

    if ((start >= 0) && ('"' == p_text[start]) && (out_size > 0u))
    {
        uint32_t pos   = (uint32_t) start + 1u;    /* start >= 0 */
        uint32_t count = 0u;

        while ((pos < len) && ('"' != p_text[pos]) &&
               ('\\' != p_text[pos]) && (count < (out_size - 1u)))
        {
            p_out[count] = p_text[pos];
            count++;
            pos++;
        }

        /* Valid only when the closing quote was reached. */
        b_ok = ((pos < len) && ('"' == p_text[pos]));
        p_out[count] = '\0';
    }

    return (b_ok);
}

/* The integer value of "key": an optional minus and up to 9 digits. */
bool
cmd_json_int (char const * p_text, uint32_t len, char const * p_key,
              int32_t * p_out)
{
    int32_t const start = cmd_find_value(p_text, len, p_key);
    bool          b_ok  = false;

    if (start >= 0)
    {
        uint32_t pos        = (uint32_t) start;    /* start >= 0 */
        bool     b_negative = false;
        int32_t  value      = 0;
        uint32_t digits     = 0u;

        if ((pos < len) && ('-' == p_text[pos]))
        {
            b_negative = true;
            pos++;
        }

        while ((pos < len) && (p_text[pos] >= '0') && (p_text[pos] <= '9')
               && (digits < CMD_INT_DIGITS_MAX))
        {
            value = (value * CMD_DECIMAL) + (int32_t) (p_text[pos] - '0');
            digits++;
            pos++;
        }

        b_ok = (digits > 0u) &&
               ((pos >= len) || (p_text[pos] < '0') || (p_text[pos] > '9'));

        if (b_ok)
        {
            *p_out = b_negative ? -value : value;
        }
    }

    return (b_ok);
}

/* A dotted-quad IPv4 address, each part 0..255. */
bool
cmd_ip_valid (char const * p_ip)
{
    uint32_t parts  = 0u;
    uint32_t digits = 0u;
    int32_t  value  = 0;
    bool     b_ok   = (NULL != p_ip);

    for (uint32_t pos = 0u; b_ok && ('\0' != p_ip[pos]); pos++)
    {
        char const chr = p_ip[pos];

        if ((chr >= '0') && (chr <= '9'))
        {
            value = (value * CMD_DECIMAL) + (int32_t) (chr - '0');
            digits++;
            b_ok = (digits <= 3u) && (value <= CMD_IP_PART_MAX);
        }
        else if (('.' == chr) && (digits > 0u))
        {
            parts++;
            digits = 0u;
            value  = 0;
            b_ok   = (parts < CMD_IP_PARTS);
        }
        else
        {
            b_ok = false;
        }
    }

    return (b_ok && (digits > 0u) && ((CMD_IP_PARTS - 1u) == parts));
}

/*
 * Index of the first character of the value of "key", after the colon
 * and any spaces, or -1 if the key is not there.
 */
static int32_t
cmd_find_value (char const * p_text, uint32_t len, char const * p_key)
{
    int32_t found = -1;

    for (uint32_t pos = 0u; (pos < len) && (found < 0); pos++)
    {
        if ('"' == p_text[pos])
        {
            uint32_t key_len = 0u;
            uint32_t scan    = pos + 1u;

            while ((scan < len) && ('\0' != p_key[key_len]) &&
                   (p_text[scan] == p_key[key_len]))
            {
                key_len++;
                scan++;
            }

            if (('\0' == p_key[key_len]) && (scan < len) &&
                ('"' == p_text[scan]))
            {
                scan++;

                while ((scan < len) && ((' ' == p_text[scan]) ||
                                        (':' == p_text[scan])))
                {
                    scan++;
                }

                if (scan < len)
                {
                    found = (int32_t) scan;     /* len < 2^31 */
                }
            }
        }
    }

    return (found);
}

static bool
cmd_text_equal (char const * p_left, char const * p_right)
{
    uint32_t idx = 0u;

    while (('\0' != p_left[idx]) && (p_left[idx] == p_right[idx]))
    {
        idx++;
    }

    return (p_left[idx] == p_right[idx]);
}

/*
 * IF-15: pack a broker: an address that passed cmd_ip_valid() (the
 * Command task packs no other), each part as the code of its spelling,
 * and the port.  Returns false for any other text, which is stored as
 * "0.0.0.0".
 */
bool
comms_broker_pack (comms_broker_t * p_broker, char const * p_ip,
                   uint16_t port)
{
    bool b_ok = false;

    if (NULL != p_broker)
    {
        p_broker->word[0] = 0u;
        p_broker->word[1] = 0u;
        b_ok = cmd_ip_valid(p_ip);

        if (b_ok)
        {
            uint32_t part   = 0u;
            uint32_t digits = 0u;
            uint32_t value  = 0u;

            /* cmd_ip_valid(): four parts of 1 to 3 digits, 0..255, each
               ended by a dot or, the last, by the NUL. */
            for (uint32_t pos = 0u; part < CMD_IP_PARTS; pos++)
            {
                char const chr = p_ip[pos];

                if ((chr >= '0') && (chr <= '9'))
                {
                    value = (value * (uint32_t) CMD_DECIMAL) +
                            (uint32_t) (chr - '0');
                    digits++;
                }
                else
                {
                    uint32_t const first = (1u == digits) ? BROKER_ONE_DIGIT
                                           : ((2u == digits)
                                              ? BROKER_TWO_DIGITS
                                              : BROKER_THREE_DIGITS);

                    broker_set_part(p_broker, part, first + value);
                    part++;
                    digits = 0u;
                    value  = 0u;
                }
            }
        }

        (void) bitpack_set(&p_broker->word[1], BROKER_PORT_SHIFT,
                           BROKER_PORT_BITS, (uint32_t) port);
    }

    return (b_ok);
}

/* The address text exactly as packed, NUL-terminated (size at least
   CMD_IP_MAX). */
void
comms_broker_ip (comms_broker_t const * p_broker, char * p_ip, uint32_t size)
{
    if ((NULL != p_broker) && (NULL != p_ip) && (size >= CMD_IP_MAX))
    {
        uint32_t len = 0u;

        for (uint32_t part = 0u; part < CMD_IP_PARTS; part++)
        {
            uint32_t const code   = broker_part(p_broker, part);
            uint32_t       digits = 1u;
            uint32_t       value  = code;
            uint32_t       div    = 1u;

            if (code >= BROKER_THREE_DIGITS)
            {
                digits = 3u;
                value  = code - BROKER_THREE_DIGITS;    /* 0..255 */
            }
            else if (code >= BROKER_TWO_DIGITS)
            {
                digits = 2u;
                value  = code - BROKER_TWO_DIGITS;      /* 0..99 */
            }
            else
            {
                /* One digit, 0..9. */
            }

            for (uint32_t d = 1u; d < digits; d++)
            {
                div *= (uint32_t) CMD_DECIMAL;
            }

            for (; div > 0u; div /= (uint32_t) CMD_DECIMAL)
            {
                /* A digit 0..9: fits char. */
                p_ip[len] = (char) ('0' + (int32_t) ((value / div) %
                                                     (uint32_t) CMD_DECIMAL));
                len++;
            }

            p_ip[len] = (part < (CMD_IP_PARTS - 1u)) ? '.' : '\0';
            len++;
        }
    }
}

uint16_t
comms_broker_port (comms_broker_t const * p_broker)
{
    /* A 16-bit field: fits uint16_t. */
    return ((NULL != p_broker)
            ? (uint16_t) bitpack_get(p_broker->word[1], BROKER_PORT_SHIFT,
                                     BROKER_PORT_BITS)
            : 0u);
}

/* Part 0..3: parts 0-2 in word 0, part 3 at the bottom of word 1.
   Codes are 0..365 (BROKER_CODES), which fit. */
static void
broker_set_part (comms_broker_t * p_broker, uint32_t part, uint32_t code)
{
    uint32_t const word = (part < BROKER_PARTS_WORD0) ? 0u : 1u;
    uint32_t const shift = (part < BROKER_PARTS_WORD0)
                           ? (part * BROKER_PART_BITS) : 0u;

    (void) bitpack_set(&p_broker->word[word], shift, BROKER_PART_BITS, code);
}

/* A part's code; one that cannot occur (above 365) reads as "255". */
static uint32_t
broker_part (comms_broker_t const * p_broker, uint32_t part)
{
    uint32_t const word = (part < BROKER_PARTS_WORD0) ? 0u : 1u;
    uint32_t const shift = (part < BROKER_PARTS_WORD0)
                           ? (part * BROKER_PART_BITS) : 0u;
    uint32_t const code = bitpack_get(p_broker->word[word], shift,
                                      BROKER_PART_BITS);

    return ((code < BROKER_CODES) ? code : (BROKER_CODES - 1u));
}

/*** end of file ***/
