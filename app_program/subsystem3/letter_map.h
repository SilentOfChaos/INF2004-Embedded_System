/*
 *  letter_map.h
 *  Buddy 3's barcode letter-to-action map (report section 3, FR6).
 *
 *  Default mapping A = straight, B = left, C = right, D = U-turn
 *  (FR6, A7); every other letter has no action until remapped over MQTT.
 *  The map instance belongs to Navigation, the only task that uses it,
 *  so no locking is needed; remaps reach it through Navigation's
 *  command queue (IF-10).
 *
 *  The map is 26 fields of 3 bits (actions 0..4) back to back in three
 *  words: letter n ('A' = 0) in bits 3n..3n+2 counted across the words
 *  from bit 0 of word 0 (minimum-bit change, D-86).
 *
 *  Pure C, no kernel headers: tested on the host (testing/host/).
 */

#ifndef LETTER_MAP_H
#define LETTER_MAP_H

#include <stdbool.h>
#include <stdint.h>

#define LETTER_MAP_SIZE     26u     /* 'A'..'Z' */

typedef enum
{
    LETTER_ACTION_NONE = 0,         /* letter has no command */
    LETTER_ACTION_STRAIGHT,
    LETTER_ACTION_LEFT,
    LETTER_ACTION_RIGHT,
    LETTER_ACTION_UTURN,
    LETTER_ACTION_COUNT
} letter_action_t;

#define LETTER_MAP_ACTION_BITS  3u      /* LETTER_ACTION_NONE..UTURN */
#define LETTER_MAP_WORDS        3u      /* 26 x 3 = 78 bits */

typedef struct
{
    uint32_t word[LETTER_MAP_WORDS];    /* layout above */
} letter_map_t;

void            letter_map_default(letter_map_t * p_map);
letter_action_t letter_map_get(letter_map_t const * p_map, char letter);
bool            letter_map_set(letter_map_t * p_map, char letter,
                               letter_action_t action);
bool            letter_map_parse_action(char const * p_text,
                                        letter_action_t * p_action);
char const *    letter_map_action_name(letter_action_t action);

#endif /* LETTER_MAP_H */

/*** end of file ***/
