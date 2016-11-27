/**
 * Test suite for utils.c
 */
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <check.h>

#include "../utils.h"

#ifndef CCHECK_TEST
#define CCHECK_TEST 1
#endif


/* Memory */
START_TEST(proxenet_xzero_test)
{
        char *buf = proxenet_xmalloc(10);
        memset(buf, '\x41', 10);
        proxenet_xzero(buf, 10);
        ck_assert_int_eq(buf[0], 0x00);
        proxenet_xfree(buf);
}
END_TEST


START_TEST(proxenet_xstrdup2_test)
{
        char *buf = proxenet_xstrdup2("THIS IS A TEST");
        ck_assert_str_eq(buf, "THIS IS A TEST");
        proxenet_xfree(buf);
}
END_TEST
/* */

/* String */
START_TEST(proxenet_lstrip_test)
{
        const char orig[] = "X-Header-Test: blahblah";
        char *str;

        str = proxenet_xstrdup2(orig);
        proxenet_lstrip(str);
        ck_assert_str_eq(str, orig);
        proxenet_xfree(str);

        str = proxenet_xstrdup2("   \t \rX-Header-Test: blahblah");
        proxenet_lstrip(str);
        ck_assert_str_eq(str, orig);
        proxenet_xfree(str);
}
END_TEST


START_TEST(proxenet_rstrip_test)
{
        const char orig[] = "X-Header-Test: blahblah";
        char *str;

        str = proxenet_xstrdup2(orig);
        proxenet_rstrip(str);
        ck_assert_str_eq(str, orig);
        proxenet_xfree(str);

        str = proxenet_xstrdup2("X-Header-Test: blahblah   \t \r\n");
        proxenet_rstrip(str);
        ck_assert_str_eq(str, orig);
        proxenet_xfree(str);
}
END_TEST


START_TEST(proxenet_strip_test)
{
        const char orig[] = "X-Header-Test: blahblah";
        char *str;

        str = proxenet_xstrdup2(orig);
        proxenet_strip(str);
        ck_assert_str_eq(str, orig);
        proxenet_xfree(str);

        str = proxenet_xstrdup2("   \t \nX-Header-Test: blahblah   \t \r\n");
        proxenet_strip(str);
        ck_assert_str_eq(str, orig);
        proxenet_xfree(str);
}
END_TEST
/* */

/* Filesystem */
START_TEST(expand_file_path_test)
{
        const char path_test_ok[] = "~/../..//.././../../../..//////etc/./passwd";
        const char path_test_nok[] = "~/../..//.././..//////kawa/./bunga/";
        char *res;

        res = expand_file_path( (char*)path_test_ok );
        ck_assert_str_eq(res, "/etc/passwd");
        proxenet_xfree(res);

        res = expand_file_path( (char*)path_test_nok );
        ck_assert(res == NULL);
        ck_assert_int_eq(errno, ENOENT);
}
END_TEST


START_TEST(is_file_test)
{
        const char path_test_ok[] = "~/../../../../../../tmp/proxenet.tmp";
        const char path_test_nok[] = "/../..//.././..//////kawa/./bunga/";
        char *res;

        res = expand_file_path( (char*)path_test_ok );
        ck_assert(res != NULL);
        ck_assert_int_eq(is_file(res), true);
        proxenet_xfree(res);

        ck_assert_int_eq(is_writable_file((char*)path_test_nok), false);
}
END_TEST


START_TEST(is_readable_file_test)
{
        const char path_test_ok[] = "~/../../../../../../tmp/proxenet.tmp";
        const char path_test_nok[] = "./../..//.././..//////kawa/./bunga/";
        char *res;

        res = expand_file_path( (char*)path_test_ok );
        ck_assert(res != NULL);
        ck_assert_int_eq(is_readable_file(res), true);
        proxenet_xfree(res);

        ck_assert_int_eq(is_writable_file((char*)path_test_nok), false);
}
END_TEST


START_TEST(is_writable_file_test)
{
        const char path_test_ok[] = "~/../../../../../../tmp/proxenet.tmp";
        const char path_test_nok[] = "../../..//.././..//////kawa/./bunga/";
        char *res;

        res = expand_file_path( (char*)path_test_ok );
        ck_assert(res != NULL);
        ck_assert_int_eq( is_writable_file(res), true);
        proxenet_xfree(res);

        ck_assert_int_eq( is_writable_file((char*)path_test_nok), false);
}
END_TEST


START_TEST(is_dir_test)
{
        const char path_test_ok[] = "~/../..//.././../../../..//////tmp";
        const char path_test_nok[] = "~/../..//.././..//////kawa/./bunga/";
        char *res;

        res = expand_file_path( (char*)path_test_ok );
        ck_assert(res != NULL);
        ck_assert_int_eq( is_dir(res), true);
        proxenet_xfree(res);

        ck_assert_int_eq( is_dir((char*)path_test_nok), false);
}
END_TEST

/* */


Suite * utils_suite(void)
{
    Suite *s;
    TCase *tc_memory, *tc_string, *tc_fs;

    s = suite_create("utils");

    /* Test for memory alloc/de-alloc functions */
    tc_memory = tcase_create("Memory");
    tcase_add_test(tc_memory, proxenet_xzero_test);

    /* Test for string manipulation functions */
    tc_string = tcase_create("String");
    tcase_add_test(tc_string, proxenet_xstrdup2_test);
    tcase_add_test(tc_string, proxenet_lstrip_test);
    tcase_add_test(tc_string, proxenet_rstrip_test);
    tcase_add_test(tc_string, proxenet_strip_test);

    /* Test for filesystem path manipulation functions */
    tc_fs = tcase_create("Filesystem");
    tcase_add_test(tc_fs, expand_file_path_test);
    tcase_add_test(tc_fs, is_file_test);
    tcase_add_test(tc_fs, is_readable_file_test);
    tcase_add_test(tc_fs, is_writable_file_test);
    tcase_add_test(tc_fs, is_dir_test);

    suite_add_tcase(s, tc_memory);
    suite_add_tcase(s, tc_string);
    suite_add_tcase(s, tc_fs);
    return s;
}


int main(void)
{
         int number_failed;
         Suite *s;
         SRunner *sr;

         system("touch /tmp/proxenet.tmp");

         s = utils_suite();
         sr = srunner_create(s);

         srunner_run_all(sr, CK_NORMAL);
         number_failed = srunner_ntests_failed(sr);
         srunner_free(sr);

         system("rm -f /tmp/proxenet.tmp");

         return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
