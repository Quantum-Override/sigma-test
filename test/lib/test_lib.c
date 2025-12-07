// test_lib.c
#include "sigtest.h"
#include "math_utils.h"
#include <stdlib.h>

/*
 * Test set to test libsigtest.so as a shared library (.so) with a
 * full set of test cases to showcase the functionality of the library.
 */

const char *log_file = "logs/test_lib.log";

//	tstset configuration
static void set_config(FILE **log_stream)
{
	*log_stream = fopen(log_file, "w");
	if (!*log_stream)
		*log_stream = stdout;

	writef("Demonstration Test Log. Version %s", st_version());
}

//	testcases
static void test_add(void)
{
	double result = add(2.5, 3.5);
	Assert.floatWithin(result, 6.0, 6.0, "2.5 + 3.5 = 6.0");
}
static void test_subtract(void)
{
	double result = subtract(5.0, 2.0);
	Assert.floatWithin(result, 3.0, 3.0, "5.0 - 2.0 = 3.0");
}
static void test_divide(void)
{
	double result = divide(10.0, 2.0);
	Assert.floatWithin(result, 5.0, 5.0, "10.0 / 2.0 = 5.0");
	result = divide(10.0, 0.0);
	Assert.floatWithin(result, 0.0, 0.0, "10.0 / 0.0 = 0.0");
}
static void test_is_positive(void)
{
	Assert.isTrue(is_positive(1.0), "1.0 is positive");
	Assert.isFalse(is_positive(-1.0), "-1.0 is not positive");
}
static void test_null_pointer(void)
{
	double *ptr = NULL;
	Assert.isNull(ptr, "Pointer is null");
	ptr = malloc(sizeof(double));
	Assert.isNotNull(ptr, "Pointer is not null");
	free(ptr);
}
static void test_string(void)
{
	Assert.stringEqual("math_utils", "math_utils", 1, "String comparison");
	Assert.stringEqual("Math_Utils", "math_utils", 0, "Case-insensitive comparison");
}
static void test_skip(void)
{
	Assert.skip("Closed for remodel");
}

// Register test cases
__attribute__((constructor)) void init_sigtest_tests(void)
{
	testset("libsigtest", set_config, NULL);

	testcase("Add Function", test_add);
	testcase("Subtract Function", test_subtract);
	testcase("Divide Function", test_divide);
	testcase("Is Positive Function", test_is_positive);
	testcase("Null Pointer", test_null_pointer);
	testcase("String Comparison", test_string);
	fail_testcase("Skip Test", test_skip);
}
