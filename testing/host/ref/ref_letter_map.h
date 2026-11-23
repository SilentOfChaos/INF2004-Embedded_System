/*
 *  ref_letter_map.h
 *  TEST ONLY.  A frozen copy of the letter map as it was before the
 *  minimum-bit change (app_program/subsystem3/letter_map.h at c644b68),
 *  renamed ref_*, for side-by-side tests (S3-U47).  Never used in the
 *  firmware.  Do not change it: it is the reference.
 */

#ifndef REF_LETTER_MAP_H
#define REF_LETTER_MAP_H

#include <stdbool.h>
#include <stdint.h>

#include "subsystem3/letter_map.h"  /* letter_action_t: unchanged */

typedef struct
{
    uint8_t action[LETTER_MAP_SIZE];    /* letter_action_t per letter */
} ref_letter_map_t;

void            ref_letter_map_default(ref_letter_map_t * p_map);
letter_action_t ref_letter_map_get(ref_letter_map_t const * p_map,
                                   char letter);
bool            ref_letter_map_set(ref_letter_map_t * p_map, char letter,
                                   letter_action_t action);
bool            ref_letter_map_parse_action(char const * p_text,
                                            letter_action_t * p_action);
char const *    ref_letter_map_action_name(letter_action_t action);

#endif /* REF_LETTER_MAP_H */

/*** end of file ***/
