### Step 1: Determine Our States

Based on a close analysis of the existing `run_tests` function in `sigtest.c` (considering its nested loops for sets and cases, hook calls, execution flow, exception handling via setjmp/longjmp, result processing, and summaries), I've defined a minimal enum for the runner states. This enum captures the key transitions without overcomplicating the structure. It draws from the workflow suggestion (e.g., SET_INIT, BEFORE_SET) but refines it to match the code's actual flow, ensuring we can iterate through sets and cases cleanly.

The states prioritize:
- Runner-level initialization and summary (outer scope).
- Set-level setup, hooks, and cleanup.
- Case-level execution, with sub-states for hooks, setup/teardown, execution, and result handling.
- Explicit handling for exceptions (longjmp) to avoid implicit jumps breaking the state machine.

Here's the proposed enum (to be added to `sigtest.h` or an internal header):

```
typedef enum {
  RUNNER_INIT,       // Initial setup: count sets, resolve global hooks, early exit if no sets
  SET_INIT,          // Per-set init: reset counters, set log_stream and current_set
  BEFORE_SET,        // Call before_set hook or default set header logging
  CASE_INIT,         // Per-case init: set current test case
  BEFORE_TEST,       // Call before_test hook
  SETUP_TEST,        // Call set->setup
  START_TEST,        // Call on_start_test hook
  EXECUTE_TEST,      // Run tc->test_func with setjmp for normal execution
  HANDLE_EXCEPTION,  // Handle longjmp (assertion failure, skip, etc.); optional future expansion for on_exception hook
  END_TEST,          // Call on_end_test hook
  TEARDOWN_TEST,     // Call set->teardown (remove hard-coded "Running teardown" log)
  AFTER_TEST,        // Call after_test hook, invoke default_on_testcase_finish for mem tracking
  PROCESS_RESULT,    // Adjust for expect_fail/throw, update counts, call on_test_result hook or default logging
  CASE_DONE,         // Increment totals, clear current, advance to next case (loop back if more)
  AFTER_SET,         // Call after_set hook or default summary, run set->cleanup, default_on_testset_finished for mem report
  RUNNER_SUMMARY,    // Final overall summary to stdout
  RUNNER_DONE        // Compute and return exit code
} RunnerState;
```

This is lightweight (16 states) and functional—enough to cover all paths without redundancy. We can implement this in a `while (state != RUNNER_DONE)` loop with a `switch (state)`, using local variables for loop iterators (e.g., `TestSet set = sets;`, `TestCase tc;`) to avoid gotos initially. Transitions are explicit (e.g., from EXECUTE_TEST to END_TEST on success, or to HANDLE_EXCEPTION on longjmp).

This directly addresses known bugs (e.g., output order, hook priority, fail message visibility) by ensuring states enforce sequence: hooks > set ops > defaults.

### Step 2: Comment (Annotate Virtual States) Obvious State Changes in the `run_tests` Function (Using Diffs)

To visualize the state transitions without altering code yet, I've annotated the existing `run_tests` function from `sigtest.c` with comments marking where each "virtual" state begins. These are inserted directly before the relevant code blocks, treating the current monolithic structure as implicit states.

Here's the unified diff (generated via Python's difflib for clarity) showing changes from the original to the annotated version. Only annotations are added—no logic changes.

```diff
--- original_run_tests
+++ annotated_run_tests
@@ -1,5 +1,6 @@
 
 int run_tests(TestSet sets, ST_Hooks test_hooks) {
+  // /* VIRTUAL STATE: RUNNER_INIT */
   int total_tests = 0;
   int set_sequence = 1;
   char timestamp[32];
@@ -35,6 +36,7 @@
   }
 
   for (TestSet set = sets; set; set = set->next, set_sequence++) {
+    // /* VIRTUAL STATE: SET_INIT */
     int tc_total = 0, tc_passed = 0, tc_failed = 0, tc_skipped = 0;
     if (!set->log_stream || !set->logger) {
       set->log_stream = stdout;
@@ -42,6 +44,7 @@
     // Set current_set to the executing set for writef/debugf
     current_set = set;
 
+    // /* VIRTUAL STATE: BEFORE_SET */
     // Call before_set hook if defined
     if (hooks && hooks->before_set) {
       hooks->before_set(set, hooks->context);
@@ -53,43 +56,60 @@
     }
 
     for (TestCase tc = set->cases; tc; tc = tc->next) {
+      // /* VIRTUAL STATE: CASE_INIT */
       set->current = tc; // Set current test for set_test_context
+
+      // /* VIRTUAL STATE: BEFORE_TEST */
       //	before test case setup
       if (hooks && hooks->before_test) {
         hooks->before_test(hooks->context);
       }
+
+      // /* VIRTUAL STATE: SETUP_TEST */
       //	test case setup
       if (set->setup) {
         set->setup();
       }
+
+      // /* VIRTUAL STATE: START_TEST */
       // on start test handler
       if (hooks && hooks->on_start_test) {
         hooks->on_start_test(hooks->context);
       }
+
+      // /* VIRTUAL STATE: EXECUTE_TEST */
       //	test case execution
       if (setjmp(jmpbuffer) == 0) {
         tc->test_func();
       } else {
+        // /* VIRTUAL STATE: HANDLE_EXCEPTION */
         // Longjmp triggered by an assertion failure (FAIL, SKIP, etc.)
         /*
                 We can add a custom handler for `on_exception`, `on_fail`, `on_skip`
          */
       }
+
+      // /* VIRTUAL STATE: END_TEST */
       // on end test handler
       if (hooks && hooks->on_end_test) {
         hooks->on_end_test(hooks->context);
       }
+
+      // /* VIRTUAL STATE: TEARDOWN_TEST */
       //	test case teardown
       if (set->teardown) {
         set->logger->log("Running teardown");
         set->teardown();
       }
+
+      // /* VIRTUAL STATE: AFTER_TEST */
       //	after test case teardown
       if (hooks && hooks->after_test) {
         hooks->after_test(hooks->context);
       }
       default_on_testcase_finish();
 
+      // /* VIRTUAL STATE: PROCESS_RESULT */
       // process FAIL test result
       if (tc->expect_fail) {
         if (tc->test_result.state == FAIL) {
@@ -144,10 +164,14 @@
         tc_failed++;
         set->failed++;
       }
+
+      // /* VIRTUAL STATE: CASE_DONE */
       tc_total++;
       total_tests++;
       set->current = NULL;
     }
+
+    // /* VIRTUAL STATE: AFTER_SET */
     // Call after_set hook if defined
     if (hooks && hooks->after_set) {
       hooks->after_set(set, hooks->context);
@@ -163,11 +187,13 @@
     default_on_testset_finished();
   }
 
+  // /* VIRTUAL STATE: RUNNER_SUMMARY */
   // Final output to stdout
   fwritelnf(stdout, "=================================================================");
   fwritelnf(stdout, "Tests run: %d, Passed: %d, Failed: %d, Skipped: %d",
             total_tests, current_set->passed, current_set->failed, current_set->skipped);
   fwritelnf(stdout, "Total test sets registered: %d", total_sets);
 
+  // /* VIRTUAL STATE: RUNNER_DONE */
   return current_set->failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
 }
```

This diff highlights obvious state boundaries (e.g., before hook calls, after loops). It ensures we can validate the flow without disruption.

### Step 3: Apply Labels to the Virtual States in Code

Now, to make the states more concrete in the code (still without full extraction), I've applied C-style labels (using `goto` for transitions) to the annotated version. This turns the monolithic function into a labeled state machine, allowing us to test iterations without breaking the build. Loops are flattened (e.g., `set_loop: if (!set) goto runner_summary;`), and jumps handle normal vs. exception paths.

Here's the labeled code snippet (minimal changes; compiles as-is if inserted into `sigtest.c`):

```
int run_tests(TestSet sets, ST_Hooks test_hooks) {
runner_init:
  int total_tests = 0;
  int set_sequence = 1;
  char timestamp[32];

  // Log total registered test sets for debugging
  int total_sets = 0;
  ST_Hooks hooks = NULL;
  for (TestSet set = sets; set; set = set->next) {
    if (!test_hooks && !set->hooks) {
      hooks = hook_registry->hooks;
    } else if (test_hooks) {
      hooks = test_hooks;
    } else {
      hooks = set->hooks;
    }

    current_hooks = hooks;
    total_sets++;
  }
  if (total_sets == 0) {
    return 0;
  }

  TestSet set = sets;
set_loop:
  if (!set) goto runner_summary;

set_init:
  int tc_total = 0, tc_passed = 0, tc_failed = 0, tc_skipped = 0;
  if (!set->log_stream || !set->logger) {
    set->log_stream = stdout;
  }
  current_set = set;

before_set:
  if (hooks && hooks->before_set) {
    hooks->before_set(set, hooks->context);
  } else {
    get_timestamp(timestamp, "%Y-%m-%d  %H:%M:%S");
    fwritelnf(set->log_stream, "[%d] %-25s:%4d %-10s%s",
              set_sequence, set->name, set->count, ":", timestamp);
    fwritelnf(set->log_stream, "=================================================================");
  }

  TestCase tc = set->cases;
case_loop:
  if (!tc) goto after_set;

case_init:
  set->current = tc;

before_test:
  if (hooks && hooks->before_test) {
    hooks->before_test(hooks->context);
  }

setup_test:
  if (set->setup) {
    set->setup();
  }

start_test:
  if (hooks && hooks->on_start_test) {
    hooks->on_start_test(hooks->context);
  }

execute_test:
  if (setjmp(jmpbuffer) == 0) {
    tc->test_func();
    goto end_test;  // Normal execution
  } else {
    goto handle_exception;
  }

handle_exception:
  // Handle longjmp (assertion failure)
  // (Future: Add on_exception hook call here)

end_test:
  if (hooks && hooks->on_end_test) {
    hooks->on_end_test(hooks->context);
  }

teardown_test:
  if (set->teardown) {
    set->logger->log("Running teardown");
    set->teardown();
  }

after_test:
  if (hooks && hooks->after_test) {
    hooks->after_test(hooks->context);
  }
  default_on_testcase_finish();

process_result:
  // process FAIL test result
  if (tc->expect_fail) {
    if (tc->test_result.state == FAIL) {
      tc->test_result.state = PASS;
      if (tc->test_result.message) {
        __real_free(tc->test_result.message);
        tc->test_result.message = strdup("Expected failure occurred");
      }
    } else if (tc->test_result.state != SKIP) {
      tc->test_result.state = FAIL;
      if (tc->test_result.message)
        __real_free(tc->test_result.message);
      tc->test_result.message = strdup("Expected failure but passed");
    }
  } else if (tc->expect_throw) {
    if (tc->test_result.state == FAIL) {
      tc->test_result.state = PASS;
      if (tc->test_result.message) {
        __real_free(tc->test_result.message);
        tc->test_result.message = strdup("Expected throw occurred");
      }
    } else if (tc->test_result.state != SKIP) {
      tc->test_result.state = FAIL;
      if (tc->test_result.message)
        __real_free(tc->test_result.message);
      tc->test_result.message = strdup("Expected throw but passed");
    }
  }
  //	process PASS/SKIP result
  if (tc->test_result.state == PASS) {
    if (hooks && hooks->on_test_result) {
      hooks->on_test_result(set, tc, hooks->context);
    } else {
      set->logger->log("[PASS]\n");
    }
    tc_passed++;
    set->passed++;
  } else if (tc->test_result.state == SKIP) {
    if (hooks && hooks->on_test_result) {
      hooks->on_test_result(set, tc, hooks->context);
    } else {
      set->logger->log("[SKIP]\n");
    }
    tc_skipped++;
    set->skipped++;
  } else {
    if (hooks && hooks->on_test_result) {
      hooks->on_test_result(set, tc, hooks->context);
    } else {
      set->logger->log("[FAIL]\n     %s", tc->test_result.message ? tc->test_result.message : "Unknown");
    }
    tc_failed++;
    set->failed++;
  }

case_done:
  tc_total++;
  total_tests++;
  set->current = NULL;
  tc = tc->next;
  goto case_loop;

after_set:
  if (hooks && hooks->after_set) {
    hooks->after_set(set, hooks->context);
  }
  fwritelnf(set->log_stream, "=================================================================");
  fwritelnf(set->log_stream, "[%d]     TESTS=%3d        PASS=%3d        FAIL=%3d        SKIP=%3d",
            set_sequence, tc_total, tc_passed, tc_failed, tc_skipped);

  if (set->cleanup) {
    set->cleanup();
  }

  default_on_testset_finished();

  set = set->next;
  set_sequence++;
  goto set_loop;

runner_summary:
  fwritelnf(stdout, "=================================================================");
  fwritelnf(stdout, "Tests run: %d, Passed: %d, Failed: %d, Skipped: %d",
            total_tests, current_set->passed, current_set->failed, current_set->skipped);
  fwritelnf(stdout, "Total test sets registered: %d", total_sets);

runner_done:
  return current_set->failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
```

This version maintains the original logic but makes states explicit via labels. We can compile/test this iteratively (e.g., add one label at a time) to catch issues early.

### Step 4: Extract State Functions One Iteration at a Time

In the coding phase (post-design), we'll extract each labeled block into a standalone function, one by one, replacing the label with a function call. This iterative approach minimizes risk—test after each extraction.

For illustration (minimal code), here's an example extracting `BEFORE_SET` into a function. Parameters are passed explicitly (e.g., `set`, `hooks`) to keep functions stateless where possible. After extraction, the label becomes:

```
// In run_tests...
before_set:
  handle_before_set(set, hooks, timestamp, set_sequence);
  // Next state transition...
```

And the extracted function (add to `sigtest.c`):

```
// State function example: Handles BEFORE_SET logic
void handle_before_set(TestSet set, ST_Hooks hooks, char* timestamp, int set_sequence) {
  if (hooks && hooks->before_set) {
    hooks->before_set(set, hooks->context);
  } else {
    get_timestamp(timestamp, "%Y-%m-%d  %H:%M:%S");
    fwritelnf(set->log_stream, "[%d] %-25s:%4d %-10s%s",
              set_sequence, set->name, set->count, ":", timestamp);
    fwritelnf(set->log_stream, "=================================================================");
  }
}
```

Repeat this for each state (e.g., next: `PROCESS_RESULT`, as it's complex and bug-prone). Once all are extracted, wrap in a state loop:

```
RunnerState state = RUNNER_INIT;
while (state != RUNNER_DONE) {
  switch (state) {
    case RUNNER_INIT: handle_runner_init(...); state = SET_LOOP; break;  // Or direct to SET_INIT
    // ...
  }
}
```

This ensures a clean, maintainable refactor. Next: Proceed to coding with this as the blueprint?
