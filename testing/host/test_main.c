/*
 *  test_main.c
 *  Runs every host unit test.  Build and run with "make" in this folder.
 */

#include "unit.h"

int
main (void)
{
    /* The Makefile defines TEST_SUITE_<NAME> for each test_<name>.c it
       finds, so each subsystem's tests arrive with its own code. */
#ifdef TEST_SUITE_COMMON
    test_common();
#endif
#ifdef TEST_SUITE_S1
    test_s1();
#endif
#ifdef TEST_SUITE_S2
    test_s2();
#endif
#ifdef TEST_SUITE_S3
    test_s3();
#endif
#ifdef TEST_SUITE_S4
    test_s4();
#endif
#ifdef TEST_SUITE_S5
    test_s5();
#endif
#ifdef TEST_SUITE_NAV
    test_nav();
#endif

    return (unit_summary());
}

/*** end of file ***/
