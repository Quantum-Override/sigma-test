// test/test_junit_hook.c
#include "hooks/junit_hooks.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Test case for the new hooks feature to extend test reporting in a JUnit
 * XML format.
 */
static void set_config(FILE **log_stream) {
   // initialize the log stream - for JUnit, use stdout to avoid file creation
   *log_stream = stdout;
}

void passing_test(void) { Assert.isTrue(1 == 1, "1 should equal 1"); }
void failing_test(void) { Assert.isTrue(1 == 0, "1 should not equal 0"); }
void expect_fail(void) { Assert.isFalse(1 == 1, "1 should equal 1"); }
void skipped_test(void) { Assert.skip("This test is skipped"); }
void throw_test(void) { Assert.throw("This test is explicitly thrown"); }

// Register test cases
__attribute__((constructor)) void init_sigtest_tests(void) {
   static struct JunitHookContext ctx = {
       .info = {
           .count = 0,
           .verbose = 0,
           .start = {0, 0},
           .end = {0, 0},
           .state = 0,
           .logger = NULL,
       },
       .data = NULL,
   };

   // Register the test set
   testset("junit_hooks", set_config, NULL);

   // Register the test hooks
   junit_hooks.context = (tc_context *)&ctx;
   register_hooks((ST_Hooks)&junit_hooks);

   // Register the test cases
   testcase("JUnit: Should Pass", passing_test);
   testcase("JUnit: Should Fail", failing_test);
   fail_testcase("JUnit: Should Expect Fail", expect_fail);
   testcase("JUnit: Should Skip", skipped_test);
   testcase("JUnit: Should Throw", throw_test);
}