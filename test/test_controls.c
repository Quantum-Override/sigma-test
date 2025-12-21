// test_controls.c
#include "sigtest.h"

static int testcase_setup_count = 0;
static int testcase_teardown_count = 0;

//	test set config
static void set_config(FILE **log_stream) {
   *log_stream = fopen("logs/test_controls.log", "w");
}
//	test case setup and teardown
static void testcase_setup(void) {
   testcase_setup_count++;
   DebugLogger.log("Testcase setup called, count: %d", testcase_setup_count);
}
static void testcase_teardown(void) {
   testcase_teardown_count++;
   DebugLogger.log("Testcase teardown called, count: %d", testcase_teardown_count);
}
//	test cases
void test_float_fail(void) {
   float exp = 3.14528, act = 3.0;
   Assert.areEqual(&exp, &act, FLOAT, "%.5f is not equal to %.5f", exp, act);
}

void test_string_fail(void) {
   string exp = "foo", act = "bar";
   Assert.areEqual(&exp, &act, STRING, "%s is not equal to %s", exp, act);
}

void test_expect_fail_passes(void) {
   int exp = 5, act = 6;
   Assert.areEqual(&exp, &act, INT, "%d should equal %d", exp, act);
}

void test_complex_failure(void) {
   float exp = 3.14528, act = 3.0;
   Assert.areEqual(&exp, &act, FLOAT, "%.5f != %.5f", exp, act);
   Assert.isTrue(1 == 0, "This should not run");
}

void test_expect_throw(void) {
   Assert.throw("Test explicitly thrown");
}

// Register test cases
__attribute__((constructor)) void init_controls_tests(void) {
   testset("constrols_set", set_config, NULL);
   DebugLogger.log("Test Source: %s", __FILE__);

   setup_testcase(testcase_setup);
   teardown_testcase(testcase_teardown);

   fail_testcase("float_fail", test_float_fail);
   fail_testcase("string_fail", test_string_fail);
   fail_testcase("expect_fail_passes", test_expect_fail_passes);
   fail_testcase("complex_failure", test_complex_failure);

   testcase_throws("test_expect_exception", test_expect_throw);
}
