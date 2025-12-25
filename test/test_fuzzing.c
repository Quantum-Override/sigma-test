// test/test_fuzzing.c
#include "fuzzing.h"
#include "helpers/safe_math.h"
#include "sigtest.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Test set for fuzzing test cases.
 * version: 1.0.1-pre
 */

static void set_config(FILE **log_stream) {
   *log_stream = fopen("logs/test_fuzzing.log", "w");
}

// A test case to show value iteration
static int count = 0;
static void simple_fuzz_test(void *param) {
   size_t value = *(size_t *)param;
   DebugLogger.log("[%d] %zu", count++, value);
}
// A fuzzy test to check for valid size malloc
static void fuzz_malloc_stress(void *param) {
   size_t size = *(size_t *)param;
   void *p = malloc(size);
   Assert.isTrue(p != NULL || size == 0, "malloc(%zu) failed unexpectedly", size);
   if (p)
      free(p);
}
// A fuzzy test to check integer addition
static void fuzz_int_overflow(void *param) {
   int value = *(int *)param;
   int result = safe_add_int(value, 100);
   Assert.isTrue(result >= value, "overflow should not decrease value");
}
// A fuzzy test to check float special values
static void fuzz_float_specials(void *param) {
   float value = *(float *)param;
   if (value != value) { // NaN check
      Assert.isTrue(value != value, "Value should be NaN");
   } else if (value == INFINITY) {
      Assert.isTrue(value > 0, "Value should be positive infinity");
   } else if (value == -INFINITY) {
      Assert.isTrue(value < 0, "Value should be negative infinity");
   } else {
      Assert.isTrue(value == value, "Value should be a normal float");
   }
}
// A fuzzy test to check byte input validation
static void fuzz_byte_validation(void *param) {
   unsigned char value = *(unsigned char *)param;
   Assert.isTrue(value <= 255, "Byte value out of range: %u", value);
}

__attribute__((constructor)) void register_fuzz_tests(void) {
   testset("stability_fuzz", set_config, NULL);

   fuzz_testcase("fuzz value iteration", simple_fuzz_test, FUZZ_SIZE_T);
   fuzz_testcase("malloc boundary stress", fuzz_malloc_stress, FUZZ_SIZE_T);
   fuzz_testcase("int overflow handling", fuzz_int_overflow, FUZZ_INT);
   fuzz_testcase("float special values", fuzz_float_specials, FUZZ_FLOAT);
   fuzz_testcase("byte input validation", fuzz_byte_validation, FUZZ_BYTE);
}