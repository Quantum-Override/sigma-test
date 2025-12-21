// test_configs.c
#include "sigtest.h"

static int suite_config_count = 0;
static int suite_cleanup_count = 0;
static int testcase_setup_count = 0;
static int testcase_teardown_count = 0;

//	test set config and cleanup
static void set_config(FILE **log_stream) {
   *log_stream = fopen("logs/test_configs.log", "w");

   suite_config_count++;
}
static void set_cleanup(void) {
   suite_cleanup_count++;
}
//	test case setup and teardown
static void testcase_setup(void) {

   testcase_setup_count++;
}
static void testcase_teardown(void) {
   testcase_teardown_count++;
}
// test cases
void test_varargs_message(void) {
   int x = 42, y = 43;
   Assert.isFalse(x == y, "Values %d and %d should not be equal", x, y);
}
void test_suite_config_cleanup(void) {
   Assert.isTrue(suite_config_count == 1, "Set config should be called once, got %d", suite_config_count);
   Assert.isTrue(suite_cleanup_count == 0, "Set cleanup should not yet be called");
}
void test_testcase_setup_teardown(void) {
   Assert.isTrue(testcase_setup_count > 0, "Testcase setup should be called, got %d", testcase_setup_count);
   Assert.isTrue(testcase_teardown_count >= 0, "Testcase teardown should not yet be called for this test, got %d", testcase_teardown_count);
}

// Register test cases
__attribute__((constructor)) void init_configs_tests(void) {
   testset("configs_set", set_config, set_cleanup);
   DebugLogger.log("Test Source: %s", __FILE__);

   setup_testcase(testcase_setup);
   teardown_testcase(testcase_teardown);

   testcase("varargs_message", test_varargs_message);
   testcase("suite_config_cleanup", test_suite_config_cleanup);
   testcase("testcase_setup_teardown", test_testcase_setup_teardown);
}
