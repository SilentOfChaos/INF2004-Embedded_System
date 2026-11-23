/*
 *  tlm_json.c
 *  A small bounded JSON writer.  See tlm_json.h.
 */

#include <stddef.h>

#include "tlm_json.h"

#define JSON_DIGITS_MAX     11u     /* "-2147483648" */
#define JSON_DECIMAL        10u

static void json_put(json_writer_t * p_json, char chr);
static void json_text(json_writer_t * p_json, char const * p_text);
static void json_key(json_writer_t * p_json, char const * p_key);
static void json_number(json_writer_t * p_json, bool b_negative,
                        uint32_t magnitude);

/* Start an object in p_buf (size bytes, including the final NUL). */
void
json_begin (json_writer_t * p_json, char * p_buf, uint32_t size)
{
    p_json->p_buf      = p_buf;
    p_json->size       = size;
    p_json->len        = 0u;
    p_json->b_first    = true;
    p_json->b_overflow = ((NULL == p_buf) || (0u == size));
    json_put(p_json, '{');
}

void
json_int (json_writer_t * p_json, char const * p_key, int32_t value)
{
    json_key(p_json, p_key);

    if (value < 0)
    {
        /* Unsigned negation is defined for INT32_MIN too. */
        json_number(p_json, true, 0u - (uint32_t) value);
    }
    else
    {
        json_number(p_json, false, (uint32_t) value);
    }
}

void
json_uint (json_writer_t * p_json, char const * p_key, uint32_t value)
{
    json_key(p_json, p_key);
    json_number(p_json, false, value);
}

/* A string value; quotes and backslashes are replaced by '_'. */
void
json_str (json_writer_t * p_json, char const * p_key, char const * p_value)
{
    json_key(p_json, p_key);
    json_put(p_json, '"');

    for (uint32_t idx = 0u; (NULL != p_value) && ('\0' != p_value[idx]);
         idx++)
    {
        char const chr = p_value[idx];

        json_put(p_json, (('"' == chr) || ('\\' == chr) || (chr < ' '))
                         ? '_' : chr);
    }

    json_put(p_json, '"');
}

/* A one-character string value. */
void
json_char (json_writer_t * p_json, char const * p_key, char value)
{
    char const text[2] = { value, '\0' };

    json_str(p_json, p_key, text);
}

/* An array of integers. */
void
json_ints (json_writer_t * p_json, char const * p_key,
           int32_t const * p_values, uint32_t count)
{
    json_key(p_json, p_key);
    json_put(p_json, '[');

    for (uint32_t idx = 0u; idx < count; idx++)
    {
        if (idx > 0u)
        {
            json_put(p_json, ',');
        }

        if (p_values[idx] < 0)
        {
            json_number(p_json, true, 0u - (uint32_t) p_values[idx]);
        }
        else
        {
            json_number(p_json, false, (uint32_t) p_values[idx]);
        }
    }

    json_put(p_json, ']');
}

/* Close the object.  Returns its length, or 0 if it did not fit. */
uint32_t
json_end (json_writer_t * p_json)
{
    uint32_t length = 0u;

    json_put(p_json, '}');

    if (!p_json->b_overflow && (p_json->len < p_json->size))
    {
        p_json->p_buf[p_json->len] = '\0';
        length = p_json->len;
    }

    return (length);
}

/* One character, keeping room for the final NUL. */
static void
json_put (json_writer_t * p_json, char chr)
{
    if (!p_json->b_overflow && ((p_json->len + 1u) < p_json->size))
    {
        p_json->p_buf[p_json->len] = chr;
        p_json->len++;
    }
    else
    {
        p_json->b_overflow = true;
    }
}

static void
json_text (json_writer_t * p_json, char const * p_text)
{
    for (uint32_t idx = 0u; '\0' != p_text[idx]; idx++)
    {
        json_put(p_json, p_text[idx]);
    }
}

/* ,"key": */
static void
json_key (json_writer_t * p_json, char const * p_key)
{
    if (!p_json->b_first)
    {
        json_put(p_json, ',');
    }

    p_json->b_first = false;
    json_put(p_json, '"');
    json_text(p_json, p_key);
    json_put(p_json, '"');
    json_put(p_json, ':');
}

/* Decimal digits of a magnitude, with a minus sign if negative. */
static void
json_number (json_writer_t * p_json, bool b_negative, uint32_t magnitude)
{
    char     digits[JSON_DIGITS_MAX];
    uint32_t count = 0u;
    uint32_t rest  = magnitude;

    do
    {
        /* rest % 10 is 0..9, so the character is a digit. */
        digits[count] = (char) ('0' + (char) (rest % JSON_DECIMAL));
        count++;
        rest = rest / JSON_DECIMAL;
    } while ((0u != rest) && (count < JSON_DIGITS_MAX));

    if (b_negative)
    {
        json_put(p_json, '-');
    }

    while (count > 0u)
    {
        count--;
        json_put(p_json, digits[count]);
    }
}

/*** end of file ***/
