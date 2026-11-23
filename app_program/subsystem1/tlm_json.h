/*
 *  tlm_json.h
 *  A small bounded JSON writer for telemetry (no printf, no heap).
 *  Every call checks the remaining space; on overflow the writer stops
 *  and json_end() returns 0, so a message is never sent truncated.
 *
 *  Pure C, no kernel headers: tested on the host (testing/host/).
 */

#ifndef TLM_JSON_H
#define TLM_JSON_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    char *   p_buf;
    uint32_t size;
    uint32_t len;
    bool     b_first;       /* no field written yet */
    bool     b_overflow;
} json_writer_t;

void     json_begin(json_writer_t * p_json, char * p_buf, uint32_t size);
void     json_int(json_writer_t * p_json, char const * p_key, int32_t value);
void     json_uint(json_writer_t * p_json, char const * p_key,
                   uint32_t value);
void     json_str(json_writer_t * p_json, char const * p_key,
                  char const * p_value);
void     json_char(json_writer_t * p_json, char const * p_key, char value);
void     json_ints(json_writer_t * p_json, char const * p_key,
                   int32_t const * p_values, uint32_t count);
uint32_t json_end(json_writer_t * p_json);

#endif /* TLM_JSON_H */

/*** end of file ***/
