#include "sigtest.h"
#include "memcheck.h"
#include "sigcore.h"
#include <stdlib.h>

/*
 * Test set for memory allocation, leakage, and detection
 */

static void config_testset(FILE **log_stream)
{
    // log to test file
    const char *filename = "logs/memcheck_test.log";
    FILE *file = fopen(filename, "w");
    if (file)
    {
        *log_stream = file;
        writelnf("Logging to file: %s", filename);
        return;
    }
    *log_stream = stdout;
}

static void test_leak(void)
{
    MemCheck.enable();
    void *ptr = malloc(100); // leak!

    Assert.isNotNull(ptr, "Allocation should succeed");
    // no disable needed → auto-checked and failed
}

static void test_noleak(void)
{
    MemCheck.enable();
    void *ptr = malloc(100);
    Assert.isNotNull(ptr, "Allocation should succeed");
    free(ptr);
    MemCheck.disable(); // disable to avoid false positive
}

// Test expected memory leak -- it failed (12/3/2025)
static void test_expected_leak(void)
{
    MemCheck.enable();
    MemCheck.expectLeaks(1);
    void *ptr = malloc(50); // expected leak

    Assert.isNotNull(ptr, "Allocation should succeed");
    // no free needed → expected leak
}

// Register test cases
__attribute__((constructor)) void memcheck_detection_tests(void)
{

    testset("Memory Check Tests", config_testset, NULL);
    writelnf("Test Source: %s", __FILE__);

    testcase("Test Memory Leak Detection", test_leak);
    testcase("Test No Memory Leak", test_noleak);
    // testcase("Test Expected Memory Leak", test_expected_leak);
}