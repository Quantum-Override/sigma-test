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
    // MemCheck.reset();
}

static void test_noleak(void)
{
    MemCheck.enable();
    void *ptr = malloc(100);
    Assert.isNotNull(ptr, "Allocation should succeed");
    free(ptr);
    MemCheck.disable(); // disable to avoid false positive
    // MemCheck.reset();
}

//  verify size of memory leak detection
static void test_leak_size(void)
{
    MemCheck.enable();
    void *ptr = malloc(250); // leak!

    Assert.isNotNull(ptr, "Allocation should succeed");
    // MemCheck.disable(); // disable to trigger leak report

    int leaked_blocks = MemCheck.leakedBlocks();
    long leaked_bytes = MemCheck.leakedBytes();

    int expected_leaks = 1;
    long expected_bytes = 250L;

    Assert.areEqual(&expected_leaks, &leaked_blocks, INT, "Expected %d leaked block, got %d", expected_leaks, leaked_blocks);
    Assert.areEqual(&expected_bytes, &leaked_bytes, LONG, "Expected %ld leaked bytes, got %ld", expected_bytes, leaked_bytes);
    // MemCheck.reset();
}

// Register test cases
__attribute__((constructor)) void memcheck_detection_tests(void)
{

    testset("Memory Check Tests", config_testset, NULL);
    writelnf("Test Source: %s", __FILE__);

    testcase("Test Memory Leak Size", test_leak_size);
    testcase("Test Memory Leak", test_leak);
    testcase("Test Memory Leak Detection", test_noleak);
}