/*
 *  unit.c
 *  The host unit-test helper.  See unit.h.
 */

#include <stdio.h>

#include "unit.h"

static char const * g_id      = "?";
static char const * g_desc    = "";
static bool         g_b_ok    = true;
static char         g_first[256];     /* first failed check of the test */
static int          g_passed  = 0;
static int          g_failed  = 0;

void
unit_begin (char const * p_id, char const * p_desc)
{
    g_id       = p_id;
    g_desc     = p_desc;
    g_b_ok     = true;
    g_first[0] = '\0';
}

void
unit_check (bool b_ok, char const * p_expr, char const * p_file, int line)
{
    if (!b_ok)
    {
        if (g_b_ok)
        {
            (void) snprintf(g_first, sizeof(g_first), "%s:%d: %s", p_file,
                            line, p_expr);
        }

        g_b_ok = false;
    }
}

void
unit_check_int (int64_t actual, int64_t expected, char const * p_expr,
                char const * p_file, int line)
{
    if (actual != expected)
    {
        if (g_b_ok)
        {
            (void) snprintf(g_first, sizeof(g_first),
                            "%s:%d: %s is %lld, expected %lld", p_file, line,
                            p_expr, (long long) actual, (long long) expected);
        }

        g_b_ok = false;
    }
}

void
unit_end (void)
{
    if (g_b_ok)
    {
        g_passed++;
        (void) printf("[TEST] %s PASS  %s\n", g_id, g_desc);
    }
    else
    {
        g_failed++;
        (void) printf("[TEST] %s FAIL  %s   (%s)\n", g_id, g_desc, g_first);
    }
}

/* Print the totals; the process exit code is 1 if anything failed. */
int
unit_summary (void)
{
    (void) printf("[TEST] host SUMMARY %d passed, %d failed\n", g_passed,
                  g_failed);

    return ((0 == g_failed) ? 0 : 1);
}

/*** end of file ***/
