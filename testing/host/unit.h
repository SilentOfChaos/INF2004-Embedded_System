/*
 *  unit.h
 *  A minimal host unit-test helper for the pure logic modules.
 *
 *  Each test ID is one function made of several checks; the ID passes
 *  only if every check passes.  The output uses the same line format as
 *  the on-target tests (app_program/test_report.h), so a saved log can be
 *  searched the same way:
 *
 *      [TEST] S3-U01 PASS  every letter decodes forwards and backwards
 *      [TEST] S3-U02 FAIL  ...   (test_s3.c:120: widths[3] == 2)
 *      [TEST] host SUMMARY 41 passed, 1 failed
 */

#ifndef UNIT_H
#define UNIT_H

#include <stdbool.h>
#include <stdint.h>

void unit_begin(char const * p_id, char const * p_desc);
void unit_check(bool b_ok, char const * p_expr, char const * p_file,
                int line);
void unit_check_int(int64_t actual, int64_t expected, char const * p_expr,
                    char const * p_file, int line);
void unit_end(void);
int  unit_summary(void);

#define UNIT_BEGIN(id, desc)    unit_begin((id), (desc))
#define UNIT_CHECK(cond)        unit_check((cond), #cond, __FILE__, __LINE__)
#define UNIT_EQ(actual, expected)                                         \
    unit_check_int((int64_t) (actual), (int64_t) (expected), #actual,     \
                   __FILE__, __LINE__)
#define UNIT_END()              unit_end()

/* The test suites, one per subsystem (test_main.c runs them all). */
void test_common(void);
void test_s1(void);
void test_s2(void);
void test_s3(void);
void test_s4(void);
void test_s5(void);
void test_nav(void);

#endif /* UNIT_H */

/*** end of file ***/
