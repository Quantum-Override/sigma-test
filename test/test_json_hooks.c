// test/test_json_hook.c
#include "hooks/json_hooks.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Test case for the new hooks feature to extend test reporting in a JSON
 * format.
 */
static void set_config(FILE **log_stream) {
  // initialize the log stream
  *log_stream = fopen("logs/json_hooks.json", "w");
}

void hooks_test_true(void) { Assert.isTrue(1 == 1, "1 should equal 1"); }
void hooks_test_fail(void) { Assert.isTrue(1 == 0, "1 should not equal 0"); }
void expect_fail(void) { Assert.isFalse(1 == 1, "1 should equal 1"); }
void hooks_test_skip(void) { Assert.skip("This test is skipped"); }
void hooks_test_throws(void) { Assert.throw("This test is explicitly thrown"); }

// Register test cases
__attribute__((constructor)) void init_sigtest_tests(void) {
  static struct JsonHookContext ctx = {
      .count = 0,
      .verbose = 0,
      .start = {0, 0},
      .end = {0, 0},
      .set = NULL,
  };
  // Register the test set
  testset("hooks_set", set_config, NULL);

  // Register the test hooks
  json_hooks.context = &ctx;
  register_hooks((ST_Hooks)&json_hooks);

  // Register the test cases
  testcase("JSON: Should Pass", hooks_test_true);
  testcase("JSON: Should Fail", hooks_test_fail);
  fail_testcase("JSON: Should Expect Fail", expect_fail);
  testcase("JSON: Should Skip", hooks_test_skip);
  testcase("JSON: Should Throw", hooks_test_throws);
}
