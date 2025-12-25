// test_asserts.c
#include "sigtest.h"
#include <stdlib.h>

/*
 * Test set for the new assert functions added to the IAssert interface
 * in version 0.1.0.
 */

static void set_config(FILE **log_stream) {
   *log_stream = fopen("logs/test_asserts.log", "w");
}

//	test cases - NULL checks
static void test_assert_is_null(void) {
   object ptr = NULL;
   Assert.isNull(ptr, "Pointer should be NULL");
}
static void test_assert_is_not_null(void) {
   object ptr = (object)malloc(sizeof(int));
   Assert.isNotNull(ptr, "Pointer should not be NULL");
   free(ptr);
}

// test cases - not equal
static void test_assert_int_not_equal(void) {
   int expected = 5, actual = 3;
   Assert.areNotEqual(&expected, &actual, INT, "%d should not equal %d", expected, actual);
}
static void test_assert_float_not_equal(void) {
   float expected = 5.0f, actual = 3.0f;
   Assert.areNotEqual(&expected, &actual, FLOAT, "%.2f should not equal %.2f", expected, actual);
}
static void test_assert_strings_not_comparable(void) {
   string expected = "Hello", actual = "World";
   Assert.areNotEqual(expected, actual, STRING, "%s should not equal %s", expected, actual);
}

//	test case - within tolerance
static void test_assert_float_within(void) {
   float value = 5.0f, min = 4.5f, max = 5.5f;
   Assert.floatWithin(value, min, max, "%.2f should be within %.2f and %.2f", value, min, max);
}
static void test_assert_float_not_within(void) {
   // this test should fail
   float value = 5.0f, min = 6.0f, max = 7.0f;
   Assert.floatWithin(value, min, max, "%.2f is not within %.2f and %.2f", value, min, max);
}

// test case - string comparisons
static void test_assert_string_equal(void) {
   string expected = "hello", actual = "hello";
   Assert.stringEqual(expected, actual, 0, "%s should equal %s", expected, actual);
}
static void test_assert_string_not_equal(void) {
   // this test should fail
   string expected = "hello", actual = "world";
   Assert.stringEqual(expected, actual, 0, "%s should not equal %s", expected, actual);
}
static void test_assert_string_case_insensitive(void) {
   string expected = "Hello", actual = "hello";
   Assert.stringEqual(expected, actual, 0, "%s should equal %s (case insensitive)", expected, actual);
}
static void test_assert_string_case_sensitive(void) {
   //	this test should fail
   string expected = "Hello", actual = "hello";
   Assert.stringEqual(expected, actual, 1, "%s should not equal %s (case sensitive)", expected, actual);
}

// test cases - test controls
static void test_fail(void) {
   Assert.fail("Trigger test case failure");
}
static void test_skip(void) {
   Assert.skip("Trigger test case skip");
}

// test cases - various failing assertions
static void test_assert_int_equal_fail(void) {
   int expected = 5, actual = 3;
   Assert.areEqual(&expected, &actual, INT, "%d should equal %d", expected, actual);
}
static void test_assert_float_equal_fail(void) {
   float expected = 5.0f, actual = 3.0f;
   Assert.areEqual(&expected, &actual, FLOAT, "%.2f should equal %.2f", expected, actual);
}
static void test_assert_pointer_equal_fail(void) {
   int a = 5, b = 5;
   Assert.areEqual(&a, &b, PTR, "Pointers %p and %p should be equal", &a, &b);
}

// Register test cases
__attribute__((constructor)) void init_asserts_tests(void) {
   testset("asserts_set", set_config, NULL);
   DebugLogger.log("Test Source: %s", __FILE__);

   testcase("Assert Is Null", test_assert_is_null);
   testcase("Assert Is Not Null", test_assert_is_not_null);

   testcase("Assert Int Not Equal", test_assert_int_not_equal);
   testcase("Assert Float Not Equal", test_assert_float_not_equal);
   fail_testcase("Assert Strings Not Comparable", test_assert_strings_not_comparable);

   testcase("Assert Float Within", test_assert_float_within);
   fail_testcase("Assert Float Not Within", test_assert_float_not_within);

   testcase("Assert String Equal", test_assert_string_equal);
   fail_testcase("Assert String Not Equal", test_assert_string_not_equal);
   testcase("Assert String Case Insensitive", test_assert_string_case_insensitive);
   fail_testcase("Assert String Case Sensitive", test_assert_string_case_sensitive);

   fail_testcase("Assert Fail Test Case", test_fail);
   fail_testcase("Assert Skip Test Case", test_skip);

   testcase("Assert Int Equal Fail", test_assert_int_equal_fail);
   testcase("Assert Float Equal Fail", test_assert_float_equal_fail);
   testcase("Assert Pointer Equal Fail", test_assert_pointer_equal_fail);
}
