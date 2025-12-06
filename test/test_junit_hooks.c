// test/test_junit_hook.c
#include "hooks/junit_hooks.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Test case for the new hooks feature to extend test reporting in a JUnit
 * XML format.
 */
static void set_config(FILE **log_stream) {
  // initialize the log stream - conventional CI location for JUnit reports
  *log_stream = fopen("reports/junit-report.xml", "w");
  if (!*log_stream) {
    perror("Failed to open `reports/junit-report.xml`; make sure the reports/ directory exists");
    exit(1);
  }
}

void passing_test(void) { Assert.isTrue(1 == 1, "1 should equal 1"); }
void failing_test(void) { Assert.isTrue(1 == 0, "1 should not equal 0"); }
void expect_fail(void) { Assert.isFalse(1 == 1, "1 should equal 1"); }
void skipped_test(void) { Assert.skip("This test is skipped"); }
void throw_test(void) { Assert.throw("This test is explicitly thrown"); }

// Register test cases
__attribute__((constructor)) void init_sigtest_tests(void) {
  static struct JunitHookContext ctx = {
      .verbose = 0,
      .timestamp = {0},
      .hostname = {0},
      .set = NULL,
  };

  // Register the test set
  testset("hooks_set", set_config, NULL);

  // Register the test hooks
  junit_hooks.context = &ctx;
  register_hooks((ST_Hooks)&junit_hooks);

  // Register the test cases
  testcase("JUnit: Should Pass", passing_test);
  testcase("JUnit: Should Fail", failing_test);
  fail_testcase("JUnit: Should Expect Fail", expect_fail);
  testcase("JUnit: Should Skip", skipped_test);
  testcase("JUnit: Should Throw", throw_test);
}