### Enhanced Documentation for Step 1: Determine Our States

Based on the existing `run_tests` implementation in `sigtest.c`, the hook definitions in `sigtest.h`, and the initial proposal in `CR-01-122025 Runner Refactoring.md`, I've refined the state enum to ensure it's minimal yet comprehensive. The goal is to capture the exact flow without unnecessary explosion—focusing on logical phases where decisions, hooks, or actions occur. I've avoided over-fragmentation (e.g., no sub-states for minor conditionals like counter resets unless they impact hooks or defaults).

Key refinements:
- Consolidated some sub-phases (e.g., combined "CASE_DONE" into the end of "PROCESS_RESULT" loop logic to reduce count).
- Explicitly noted loop points (e.g., SET_LOOP, CASE_LOOP) as pseudo-states for iteration control in the state machine.
- Total states: Reduced to 14 (from 16) by merging minor transitions.
- Integrated hook firing points directly into state descriptions.
- Documented defaults and overrides: Defaults are fallback behaviors (e.g., logging) that execute only if the corresponding hook is NULL. Overrides occur via CLI-provided `test_hooks` (highest priority), then per-set `set->hooks`, then global defaults from `hook_registry`.
- Memory hooks (`on_memory_alloc`, `on_memory_free`) are indeed fired outside `run_tests`—specifically in the wrapper functions (`__wrap_malloc`, etc.) in `memcheck_hooks.c` (implied by `memwrap.h`). They are triggered on every allocation/free via `current_hooks` (set globally in `run_tests` before the set loop). As per your note, these are out-of-scope for this refactor but noted for completeness: They fire asynchronously during test execution (e.g., inside `EXECUTE_TEST`) if hooks are set.

This documentation serves as the blueprint for the enum in `sigtest.h` (or an internal header). It ensures the state machine enforces:
- Hook priority: CLI `test_hooks` > `set->hooks` > defaults.
- Sequence: Hooks always precede/follow core actions (e.g., setup/teardown).
- Bug fixes: Strict ordering prevents issues like misplaced output (e.g., fail messages always visible via `on_test_result` or default logging in `PROCESS_RESULT`).
- Extensibility: Easy to add future hooks (e.g., `on_exception` in `HANDLE_EXCEPTION`).

#### Proposed Enum for RunnerState
Add this to `sigtest.h` (below existing enums):
```
typedef enum {
  RUNNER_INIT,       // Outer init: Count sets, resolve global hooks, early exit if no sets.
  SET_LOOP,          // Check/advance to next set; transition to RUNNER_SUMMARY if done.
  SET_INIT,          // Per-set init: Reset counters, set log_stream/current_set, resolve per-set hooks.
  BEFORE_SET,        // Fire before_set or default header logging.
  CASE_LOOP,         // Check/advance to next case; transition to AFTER_SET if done.
  CASE_INIT,         // Per-case init: Set current test case.
  BEFORE_TEST,       // Fire before_test.
  SETUP_TEST,        // Run set->setup.
  START_TEST,        // Fire on_start_test.
  EXECUTE_TEST,      // Run tc->test_func with setjmp (normal path to END_TEST).
  HANDLE_EXCEPTION,  // Handle longjmp (fail/skip); future spot for on_exception/on_error.
  END_TEST,          // Fire on_end_test.
  TEARDOWN_TEST,     // Run set->teardown (remove hard-coded log; use logger if needed).
  AFTER_TEST,        // Fire after_test, call default_on_testcase_finish (mem tracking).
  PROCESS_RESULT,    // Adjust for expect_fail/throw, update counts, fire on_test_result or default logging, then loop back to CASE_LOOP.
  AFTER_SET,         // Fire after_set or default summary, run set->cleanup, call default_on_testset_finished (mem report).
  RUNNER_SUMMARY,    // Final overall summary to stdout.
  RUNNER_DONE        // Compute/return exit code based on failures.
} RunnerState;
```
- **Why this count?** 14 states balance granularity (for hook placement) with simplicity. Loops are explicit pseudo-states to flatten nested for-loops in the state machine (e.g., `while (state != RUNNER_DONE) { switch(state) { ... } }`).
- **Transitions:** Explicit in the machine (e.g., from EXECUTE_TEST: success → END_TEST, longjmp → HANDLE_EXCEPTION). No implicit jumps.

#### Detailed State Breakdown
For each state, I've documented:
- **Purpose:** What happens here.
- **Hooks Fired:** Which hooks (from `ST_Hooks`), when they fire, and priority.
- **Default Behaviors:** Fallback if hook is NULL (e.g., logging). These are **not** overridden if a hook is provided—hooks fully replace defaults unless the hook implementation calls them explicitly.
- **Overrides/Notes:** How CLI/set defaults interact; any bug fixes or future extensions.
- **Code Mapping:** Rough line refs from `sigtest.c` (based on provided snippet).

1. **RUNNER_INIT**
   - **Purpose:** Global setup: Initialize totals (total_tests=0, set_sequence=1), count total_sets, resolve base hooks if no CLI/set-specific ones, early return if no sets.
   - **Hooks Fired:** None (pre-hook resolution).
   - **Default Behaviors:** Loop over sets to count and set `current_hooks` (global for mem hooks).
   - **Overrides/Notes:** Hooks resolved here: Prioritize CLI `test_hooks` > per-set `set->hooks` > `hook_registry->hooks` (defaults). Mem hooks become active after this. Addresses hook priority bug.
   - **Code Mapping:** Lines ~1-35 (int total_tests... if (total_sets == 0) return 0;).

2. **SET_LOOP**
   - **Purpose:** Iteration control: Check if more sets (set != NULL), advance set/set_sequence, transition to SET_INIT or RUNNER_SUMMARY.
   - **Hooks Fired:** None.
   - **Default Behaviors:** None (pure control).
   - **Overrides/Notes:** Flattens the outer for-loop. No overrides.
   - **Code Mapping:** Implicit in for (TestSet set = sets; set; set = set->next...).

3. **SET_INIT**
   - **Purpose:** Per-set prep: Reset local counters (tc_total, passed, etc.), set log_stream (fallback stdout), set current_set for logging/asserts.
   - **Hooks Fired:** None (post-resolution if per-set hooks differ).
   - **Default Behaviors:** Fallback log_stream/logger to stdout if unset.
   - **Overrides/Notes:** Re-resolves hooks if per-set specific (though usually done in RUNNER_INIT). Ensures logger availability for defaults.
   - **Code Mapping:** Lines ~36-43 (int tc_total=0... current_set = set;).

4. **BEFORE_SET**
   - **Purpose:** Pre-set hook or header.
   - **Hooks Fired:** `before_set(set, context)` – Fires if defined.
   - **Default Behaviors:** Log set header with timestamp if hook NULL (e.g., "[1] SetName:4 :2025-12-21 12:00:00" + separator).
   - **Overrides/Notes:** Hook replaces default logging entirely. Use for custom set intros. Fixes output order bug by ensuring this precedes case loop.
   - **Code Mapping:** Lines ~44-53 (if (hooks && hooks->before_set)... fwritelnf(...)).

5. **CASE_LOOP**
   - **Purpose:** Iteration control: Check if more cases (tc != NULL), advance tc, transition to CASE_INIT or AFTER_SET.
   - **Hooks Fired:** None.
   - **Default Behaviors:** None (pure control).
   - **Overrides/Notes:** Flattens inner for-loop. No overrides.
   - **Code Mapping:** Implicit in for (TestCase tc = set->cases; tc; tc = tc->next).

6. **CASE_INIT**
   - **Purpose:** Per-case prep: Set current test for assertions (set->current = tc).
   - **Hooks Fired:** None.
   - **Default Behaviors:** None.
   - **Overrides/Notes:** Critical for set_test_context() in assertions. No overrides.
   - **Code Mapping:** Line ~56 (set->current = tc;).

7. **BEFORE_TEST**
   - **Purpose:** Pre-case hook.
   - **Hooks Fired:** `before_test(context)` – Fires if defined.
   - **Default Behaviors:** None (pure hook point).
   - **Overrides/Notes:** Hook for custom pre-test setup (e.g., reset mocks). Defaults to no-op.
   - **Code Mapping:** Lines ~58-60 (if (hooks && hooks->before_test)...).

8. **SETUP_TEST**
   - **Purpose:** Run user-defined set-wide setup.
   - **Hooks Fired:** None.
   - **Default Behaviors:** Call set->setup() if defined; else no-op.
   - **Overrides/Notes:** Not a hook—user-provided via setup_testcase(). No overrides; runs after BEFORE_TEST.
   - **Code Mapping:** Lines ~62-64 (if (set->setup)...).

9. **START_TEST**
   - **Purpose:** Test start hook.
   - **Hooks Fired:** `on_start_test(context)` – Fires if defined.
   - **Default Behaviors:** None (pure hook point).
   - **Overrides/Notes:** For logging test start (e.g., defaults could be added here in future). Runs before execution.
   - **Code Mapping:** Lines ~66-68 (if (hooks && hooks->on_start_test)...).

10. **EXECUTE_TEST**
    - **Purpose:** Run the test function with exception handling.
    - **Hooks Fired:** None (but mem hooks may fire inside tc->test_func()).
    - **Default Behaviors:** setjmp(jmpbuffer); tc->test_func(); (normal path).
    - **Overrides/Notes:** On success → END_TEST; on longjmp → HANDLE_EXCEPTION. Mem hooks fire here if allocations occur.
    - **Code Mapping:** Lines ~70-73 (if (setjmp... tc->test_func();).

11. **HANDLE_EXCEPTION**
    - **Purpose:** Process assertion failures/skips from longjmp.
    - **Hooks Fired:** None currently (future: `on_error(msg, context)` or `on_exception`).
    - **Default Behaviors:** Comment placeholder for custom handlers (e.g., on_fail, on_skip).
    - **Overrides/Notes:** Currently no-op beyond longjmp catch. Add `on_error` here for fail/skip logging. Fixes fail message visibility by ensuring flow continues to PROCESS_RESULT.
    - **Code Mapping:** Lines ~74-79 (} else { // Longjmp... }).

12. **END_TEST**
    - **Purpose:** Test end hook.
    - **Hooks Fired:** `on_end_test(context)` – Fires if defined.
    - **Default Behaviors:** None (pure hook point).
    - **Overrides/Notes:** For post-execution actions (e.g., metrics). Runs regardless of success/fail.
    - **Code Mapping:** Lines ~81-83 (if (hooks && hooks->on_end_test)...).

13. **TEARDOWN_TEST**
    - **Purpose:** Run user-defined set-wide teardown.
    - **Hooks Fired:** None.
    - **Default Behaviors:** Call set->teardown() if defined; log "Running teardown" via logger (remove this hard-code; make optional via config).
    - **Overrides/Notes:** Not a hook—user-provided via teardown_testcase(). Runs after END_TEST. Bug fix: Remove hard-coded log to avoid output clutter unless verbose.
    - **Code Mapping:** Lines ~85-88 (if (set->teardown)... set->logger->log(...)).

14. **AFTER_TEST**
    - **Purpose:** Post-case hook and mem tracking.
    - **Hooks Fired:** `after_test(context)` – Fires if defined.
    - **Default Behaviors:** Call default_on_testcase_finish() (mem leak check/report).
    - **Overrides/Notes:** Hook replaces nothing—defaults always run after hook (if present). Ensures mem tracking persists.
    - **Code Mapping:** Lines ~90-93 (if (hooks && hooks->after_test)... default_on_testcase_finish();).

15. **PROCESS_RESULT** (Note: Merged with former CASE_DONE for minimalism)
    - **Purpose:** Adjust results (expect_fail/throw), update counters (tc_total++, total_tests++), log result, clear current, loop back to CASE_LOOP.
    - **Hooks Fired:** `on_test_result(set, tc, context)` – Fires based on state (PASS/FAIL/SKIP).
    - **Default Behaviors:** Adjust state/message for expect_*; then log "[PASS]\n", "[SKIP]\n", or "[FAIL]\n     msg" via logger; update set/global counts.
    - **Overrides/Notes:** Hook replaces default logging. Ensures fail messages always shown (bug fix). Includes `on_error` potential if FAIL. Clears set->current = NULL.
    - **Code Mapping:** Lines ~95-164 (// process FAIL... tc_total++; total_tests++; set->current = NULL;).

16. **AFTER_SET**
    - **Purpose:** Post-set hook, summary, cleanup.
    - **Hooks Fired:** `after_set(set, context)` – Fires if defined.
    - **Default Behaviors:** Log set summary/separator if hook NULL; call set->cleanup() if defined; call default_on_testset_finished() (set-wide mem report).
    - **Overrides/Notes:** Hook replaces summary logging. Cleanup/defaults always run after. Bug fix: Ensures summary after all cases.
    - **Code Mapping:** Lines ~166-177 (if (hooks && hooks->after_set)... default_on_testset_finished();).

17. **RUNNER_SUMMARY**
    - **Purpose:** Final global output.
    - **Hooks Fired:** None (future: global after_all hook?).
    - **Default Behaviors:** Log overall stats to stdout (tests run, passed/failed/skipped, total_sets).
    - **Overrides/Notes:** No overrides—always runs. Use stdout explicitly (not set->log_stream).
    - **Code Mapping:** Lines ~179-183 (fwritelnf(stdout, "...").

18. **RUNNER_DONE**
    - **Purpose:** Exit computation.
    - **Hooks Fired:** None.
    - **Default Behaviors:** Return EXIT_FAILURE if any failures, else EXIT_SUCCESS.
    - **Overrides/Notes:** Based on global current_set->failed (bug: This uses last set's failed; fix to use aggregated total_failed?).
    - **Code Mapping:** Line ~185 (return current_set->failed > 0 ? ...).

#### Additional Notes on Hooks
- **Available Hooks (from ST_Hooks):** All listed above. Unused in code: `on_error` (add to HANDLE_EXCEPTION?), `on_memory_*` (out-of-scope, fired in wrappers).
- **When Fired:** As mapped per-state. Always conditional (if (hooks && hook_ptr)).
- **Defaults vs. Overrides:** Defaults (logging, mem funcs) execute only if hook NULL. No partial overrides—hooks must reimplement if needed. CLI `-s` mode passes NULL hooks, forcing defaults off where applicable.
- **Global Impacts:** `current_hooks` set in RUNNER_INIT/SET_INIT enables mem hooks throughout. `on_error` could be fired in assertions or HANDLE_EXCEPTION for better error handling.

This solidifies Step 1. Next: Proceed to Step 2 (annotate diffs) with these states? Or refine further?
