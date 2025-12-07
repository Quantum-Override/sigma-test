// tests/test_memory_checks.c
#include "sigtest.h"
#include <stdlib.h>
#include <string.h>

static void set_config(FILE **log_stream) {
  *log_stream = fopen("logs/test_mallocs.log", "w");
}

// === Freebie Memory Allocations Tracking (always on) ===
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

__attribute__((constructor)) void init_memory_tests(void) {
  // we could add a feature later to throw on leaks, but for now just log them
  testset("Memory Checks Suite", set_config, NULL);

  // Basic always-on checks
  testcase("Basic: Global leak detection", test_basic_global_leak_detection);
  testcase("Basic: Clean run", test_basic_clean_run);

  /*
     Reports at the end of each test set ...

      ===== Memory Allocations Report =================================
        WWARNING: MEMORY LEAK — 1 unfreed allocation(s)
          Total mallocs:                1
          Total frees:                  0
  */
}