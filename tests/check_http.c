/**
 * Test suite for http.c
 */
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <check.h>

#include "../http.h"

#ifndef CCHECK_TEST
#define CCHECK_TEST 1
#endif


/* HTTP headers*/

/*
START_TEST()
{
}
END_TEST
*/

/* */


Suite * http_suite(void)
{
    Suite *s;
    TCase *tc_headers;

    s = suite_create("http");

    /* Test for HTTP header manipulation functions */
    tc_headers = tcase_create("Headers");

    suite_add_tcase(s, tc_headers);
    return s;
}


int main(void)
{
         int number_failed;
         Suite *s;
         SRunner *sr;

         s = http_suite();
         sr = srunner_create(s);
         srunner_run_all(sr, CK_NORMAL);
         number_failed = srunner_ntests_failed(sr);
         srunner_free(sr);
         return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
