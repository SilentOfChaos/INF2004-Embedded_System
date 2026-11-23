/*
 *  ref_letter_map.c
 *  TEST ONLY: the letter map before the minimum-bit change, from
 *  app_program/subsystem3/letter_map.c at c644b68 with only its public
 *  names prefixed ref_ (see ref_letter_map.h).  Do not change it.
 */
#include <stddef.h>

#include "ref_letter_map.h"

typedef struct
{
    char const *    p_name;
    letter_action_t action;
} ref_letter_map_name_t;

/* Accepted action names (lower case), as used in MQTT remap commands. */
static ref_letter_map_name_t const g_action_names[] =
{
    { "none",     LETTER_ACTION_NONE },
    { "straight", LETTER_ACTION_STRAIGHT },
    { "left",     LETTER_ACTION_LEFT },
    { "right",    LETTER_ACTION_RIGHT },
    { "uturn",    LETTER_ACTION_UTURN }
};

#define LETTER_MAP_NAMES    (sizeof(g_action_names) / sizeof(g_action_names[0]))

static bool ref_letter_map_index(char letter, uint32_t * p_index);
static bool ref_letter_map_text_equal(char const * p_left, char const * p_right);

/* Load the default map: A straight, B left, C right, D U-turn (FR6). */
void
ref_letter_map_default (ref_letter_map_t * p_map)
{
    if (NULL != p_map)
    {
        for (uint32_t idx = 0u; idx < LETTER_MAP_SIZE; idx++)
        {
            p_map->action[idx] = (uint8_t) LETTER_ACTION_NONE;
        }

        p_map->action['A' - 'A'] = (uint8_t) LETTER_ACTION_STRAIGHT;
        p_map->action['B' - 'A'] = (uint8_t) LETTER_ACTION_LEFT;
        p_map->action['C' - 'A'] = (uint8_t) LETTER_ACTION_RIGHT;
        p_map->action['D' - 'A'] = (uint8_t) LETTER_ACTION_UTURN;
    }
}

/* Action for a letter; LETTER_ACTION_NONE for anything outside A-Z. */
letter_action_t
ref_letter_map_get (ref_letter_map_t const * p_map, char letter)
{
    letter_action_t action = LETTER_ACTION_NONE;
    uint32_t        index  = 0u;

    if ((NULL != p_map) && ref_letter_map_index(letter, &index))
    {
        uint8_t const stored = p_map->action[index];

        if (stored < (uint8_t) LETTER_ACTION_COUNT)
        {
            action = (letter_action_t) stored;  /* range checked above */
        }
    }

    return (action);
}

/* Remap one letter (FR6).  Returns false for a bad letter or action. */
bool
ref_letter_map_set (ref_letter_map_t * p_map, char letter, letter_action_t action)
{
    bool     b_done = false;
    uint32_t index  = 0u;

    if ((NULL != p_map) && (action < LETTER_ACTION_COUNT) &&
        ref_letter_map_index(letter, &index))
    {
        p_map->action[index] = (uint8_t) action;
        b_done = true;
    }

    return (b_done);
}

/* Parse an action name ("left", "Left", "LEFT" ...). */
bool
ref_letter_map_parse_action (char const * p_text, letter_action_t * p_action)
{
    bool b_found = false;

    if ((NULL != p_text) && (NULL != p_action))
    {
        for (uint32_t idx = 0u; (idx < LETTER_MAP_NAMES) && !b_found; idx++)
        {
            if (ref_letter_map_text_equal(p_text, g_action_names[idx].p_name))
            {
                *p_action = g_action_names[idx].action;
                b_found = true;
            }
        }
    }

    return (b_found);
}

/* Lower-case name of an action, for telemetry. */
char const *
ref_letter_map_action_name (letter_action_t action)
{
    char const * p_name = "none";

    for (uint32_t idx = 0u; idx < LETTER_MAP_NAMES; idx++)
    {
        if (action == g_action_names[idx].action)
        {
            p_name = g_action_names[idx].p_name;
        }
    }

    return (p_name);
}

/* Index 0..25 for 'A'..'Z' (lower case accepted). */
static bool
ref_letter_map_index (char letter, uint32_t * p_index)
{
    bool b_valid = false;

    if ((letter >= 'A') && (letter <= 'Z'))
    {
        *p_index = (uint32_t) (letter - 'A');   /* 0..25 */
        b_valid = true;
    }
    else if ((letter >= 'a') && (letter <= 'z'))
    {
        *p_index = (uint32_t) (letter - 'a');   /* 0..25 */
        b_valid = true;
    }
    else
    {
        /* Not a letter. */
    }

    return (b_valid);
}

/* Case-insensitive comparison of a text with a lower-case name. */
static bool
ref_letter_map_text_equal (char const * p_left, char const * p_right)
{
    bool     b_equal = true;
    uint32_t idx     = 0u;

    while (b_equal && (('\0' != p_left[idx]) || ('\0' != p_right[idx])))
    {
        char left = p_left[idx];

        if ((left >= 'A') && (left <= 'Z'))
        {
            left = (char) (left - 'A' + 'a');   /* stays within a-z */
        }

        b_equal = (left == p_right[idx]);
        idx++;
    }

    return (b_equal);
}

/*** end of file ***/
