/*
 *  letter_map.c
 *  Buddy 3's barcode letter-to-action map.  See letter_map.h.
 */

#include <stddef.h>

#include "../common/bitpack.h"
#include "letter_map.h"

/* The layout (letter_map.h) holds every action. */
BITPACK_ASSERT(BITPACK_BITS_FOR(LETTER_ACTION_COUNT - 1u) ==
               LETTER_MAP_ACTION_BITS, map_action_bits);
BITPACK_ASSERT(BITPACK_ARRAY_WORDS(LETTER_MAP_SIZE,
                                   LETTER_MAP_ACTION_BITS) ==
               LETTER_MAP_WORDS, map_words);

typedef struct
{
    char const *    p_name;
    letter_action_t action;
} letter_map_name_t;

/* Accepted action names (lower case), as used in MQTT remap commands. */
static letter_map_name_t const g_action_names[] =
{
    { "none",     LETTER_ACTION_NONE },
    { "straight", LETTER_ACTION_STRAIGHT },
    { "left",     LETTER_ACTION_LEFT },
    { "right",    LETTER_ACTION_RIGHT },
    { "uturn",    LETTER_ACTION_UTURN }
};

#define LETTER_MAP_NAMES    (sizeof(g_action_names) / sizeof(g_action_names[0]))

static bool letter_map_index(char letter, uint32_t * p_index);
static bool letter_map_text_equal(char const * p_left, char const * p_right);

/* Load the default map: A straight, B left, C right, D U-turn (FR6). */
void
letter_map_default (letter_map_t * p_map)
{
    if (NULL != p_map)
    {
        for (uint32_t idx = 0u; idx < LETTER_MAP_WORDS; idx++)
        {
            p_map->word[idx] = 0u;      /* every letter NONE */
        }

        (void) letter_map_set(p_map, 'A', LETTER_ACTION_STRAIGHT);
        (void) letter_map_set(p_map, 'B', LETTER_ACTION_LEFT);
        (void) letter_map_set(p_map, 'C', LETTER_ACTION_RIGHT);
        (void) letter_map_set(p_map, 'D', LETTER_ACTION_UTURN);
    }
}

/* Action for a letter; LETTER_ACTION_NONE for anything outside A-Z. */
letter_action_t
letter_map_get (letter_map_t const * p_map, char letter)
{
    letter_action_t action = LETTER_ACTION_NONE;
    uint32_t        index  = 0u;

    if ((NULL != p_map) && letter_map_index(letter, &index))
    {
        uint32_t const stored = bitpack_array_get(p_map->word, index,
                                                  LETTER_MAP_ACTION_BITS);

        if (stored < (uint32_t) LETTER_ACTION_COUNT)
        {
            action = (letter_action_t) stored;  /* range checked above */
        }
    }

    return (action);
}

/* Remap one letter (FR6).  Returns false for a bad letter or action. */
bool
letter_map_set (letter_map_t * p_map, char letter, letter_action_t action)
{
    bool     b_done = false;
    uint32_t index  = 0u;

    if ((NULL != p_map) && (action < LETTER_ACTION_COUNT) &&
        letter_map_index(letter, &index))
    {
        /* action < LETTER_ACTION_COUNT: fits its 3 bits. */
        (void) bitpack_array_set(p_map->word, index, LETTER_MAP_ACTION_BITS,
                                 (uint32_t) action);
        b_done = true;
    }

    return (b_done);
}

/* Parse an action name ("left", "Left", "LEFT" ...). */
bool
letter_map_parse_action (char const * p_text, letter_action_t * p_action)
{
    bool b_found = false;

    if ((NULL != p_text) && (NULL != p_action))
    {
        for (uint32_t idx = 0u; (idx < LETTER_MAP_NAMES) && !b_found; idx++)
        {
            if (letter_map_text_equal(p_text, g_action_names[idx].p_name))
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
letter_map_action_name (letter_action_t action)
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
letter_map_index (char letter, uint32_t * p_index)
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
letter_map_text_equal (char const * p_left, char const * p_right)
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
