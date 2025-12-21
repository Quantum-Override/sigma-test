// test_logging.c
#include "sigtest.h"

static int setup_count = 0;
static int teardown_count = 0;
static int has_log_stream = 0;

int add(int, int);
float divide(float, float);

//	test set config and cleanup
static void set_config(FILE **log_stream) {
   *log_stream = fopen("logs/test_logging.log", "w");
   has_log_stream = *log_stream ? TRUE : FALSE;
}
static void set_cleanup(void) {
}
//	test case setup and teardown
static void testcase_setup(void) {
   setup_count++;
   DebugLogger.log("Testcase setup called, count: %d", setup_count);
}
static void testcase_teardown(void) {
   teardown_count++;
   DebugLogger.log("Testcase teardown called, count: %d", teardown_count);
}
/* Test cases */
static void test_add(void) {
   int expected = 5, actual = add(2, 3);
   Assert.areEqual(&expected, &actual, INT, "%d should equal %d", expected, actual);
}
static void test_divide(void) {
   float expected = 2.0f, actual = divide(4.0f, 2.0f);
   Assert.areEqual(&expected, &actual, FLOAT, "%.2f should equal %.2f", expected, actual);
}
static void test_divide_by_zero_fails(void) {
   float expected = 0.0f, actual = divide(4.0f, 0.0f);
   Assert.areNotEqual(&expected, &actual, FLOAT, "Division by zero should return 0");
}
static void test_divide_by_zero_throws(void) {
   float actual = divide(4.0f, 0.0f);
   if (actual == 0.0f) {
      Assert.throw("Division by zero detected");
   }
}

// Register test cases
__attribute__((constructor)) void init_logging_tests(void) {
   testset("logging_set", set_config, set_cleanup);
   DebugLogger.log("Test Source: %s", __FILE__);

   setup_testcase(testcase_setup);
   teardown_testcase(testcase_teardown);

   testcase("add", test_add);
   testcase("divide", test_divide);
   fail_testcase("divide_by_zero_fails", test_divide_by_zero_fails);
   testcase_throws("divide_by_zero_throws", test_divide_by_zero_throws);
}

int add(int a, int b) {
   return a + b;
}

float divide(float a, float b) {
   if (b == 0.0f) {
      // Simulate an assertion failure or exception
      return 0.0f; // For simplicity, return 0 (we'll test this)
   }
   return a / b;
}