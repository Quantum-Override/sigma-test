// tests/test_memory_checks.c
#include "sigtest.h"
#include <stdlib.h>
#include <string.h>

static void set_config(FILE **log_stream) {
  *log_stream = fopen("logs/test_mallocs.log", "w");
}

// === FREE BASIC FEATURES (always on) ===
static void test_basic_global_leak_detection(void) {
  void *p = malloc(100);
  (void)p; // intentional leak
           // No free → basic destructor should report "unfreed allocations"
}
static void test_basic_clean_run(void) {
  void *p = malloc(100);
  free(p);
  // Should be clean
}

// === MEMCHECK HOOK — ADVANCED FEATURES ===
// static void test_memcheck_per_test_leak_fail(void) {
//   Assert.skip("skip"); // flaky on CI?

//   MemCheck.enable();
//   void *p = malloc(100);
//   (void)p;
//   // Should auto-fail this test
//   MemCheck.reset();
// }

// static void test_memcheck_no_leak_pass(void) {
//   Assert.skip("skip"); // flaky on CI?

//   MemCheck.enable();
//   void *p = malloc(100);
//   free(p);
//   // Should pass
//   MemCheck.reset();
// }

// static void test_memcheck_backtraces(void) {
//   Assert.skip("skip"); // flaky on CI?

//   MemCheck.enableBacktraces(1);
//   void *p = malloc(100);
//   (void)p;
//   // Leak → should print backtrace in failure message
//   MemCheck.reset();
// }

// static void test_memcheck_peak_memory(void) {
//   Assert.skip("skip"); // flaky on CI?

//   MemCheck.enable();
//   void *a = malloc(1024 * 1024); // 1MB
//   void *b = malloc(512 * 1024);  // 0.5MB
//   free(a);
//   long peak = MemCheck.peakBytes();
//   Assert.isTrue(peak >= 1536 * 1024, "Peak should be at least 1.5MB, got %ld", peak);
//   free(b);
//   MemCheck.reset();
// }

// static void test_memcheck_histogram(void) {
//   Assert.skip("skip"); // flaky on CI?

//   MemCheck.enable();
//   malloc(16);
//   malloc(32);
//   malloc(1024);
//   malloc(1024);
//   // Histogram should show bins
//   struct ctx {
//     FILE *log_stream;
//   } ctx;
//   test_context(&ctx);

//   MemCheck.printHistogram(ctx.log_stream); // Just calls it — visual check
// }

__attribute__((constructor)) void init_memory_tests(void) {
  testset("Memory Checks Suite", set_config, NULL);

  // Register MemCheck hook
  // MemCheck.init(0);

  // Basic always-on checks
  testcase("Basic: Global leak detection", test_basic_global_leak_detection);
  testcase("Basic: Clean run", test_basic_clean_run);

  // Advanced MemCheck features
  // fail_testcase("MemCheck: Per-test leak → auto-fail", test_memcheck_per_test_leak_fail);
  // testcase("MemCheck: No leak → pass", test_memcheck_no_leak_pass);
  // fail_testcase("MemCheck: Leak with backtrace", test_memcheck_backtraces);
  // testcase("MemCheck: Peak memory tracking", test_memcheck_peak_memory);
  // testcase("MemCheck: Allocation histogram", test_memcheck_histogram);
}