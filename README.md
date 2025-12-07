# Sigma Test Framework

A lightweight unit testing framework for C with a focus on simplicity and extensibility.

## Features

- Simple assertion interface  
- Test case setup/teardown  
- Test set configuration/cleanup  
- Exception handling via longjmp  
- Flexible logging system  
- Support for expected failures  
- Type-safe value comparisons  
- Automatic test registration  

## Installation

1. Copy `sigtest.h` and `sigtest.c` into your project  
2. Include `sigtest.h` in your test files  
3. Link `sigtest.c` with your test executable  

### **Sigma Test’s Approach:**  
🔹 **Function-pointer-based assertions** (`Assert.areEqual()`, `Assert.isTrue()`)  
🔹 **Type-safe comparisons** (no `void*` abuse)  
🔹 **Clear failure messages** (with optional formatting)  

**Feature Comparisons:**  
| Framework       | Assertion Syntax                              | Compile-Time Type Safety | Custom Failure Messages | Built-in Memory Leak Detection | Zero-Overhead When Clean |
|-----------------|-----------------------------------------------|---------------------------|--------------------------|--------------------------------|---------------------------|
| **SigmaTest**   | `Assert.areEqual(&expected, &actual, INT, "Values differ")` | Yes (type-tagged, no macros) | Yes (printf-style)       | Yes (global, always-on)        | Yes                       |
| **Unity**       | `TEST_ASSERT_EQUAL_INT(expected, actual)`     | Yes                       | No (fixed messages)      | No                             | Yes                       |
| **Check**       | `ck_assert_int_eq(expected, actual)`          | Yes                       | No (fixed messages)      | No                             | Yes                       |
| **CMocka**      | `assert_int_equal(expected, actual)`          | Yes                       | Limited                  | No                             | Yes                       |
| **GoogleTest**  | `EXPECT_EQ(expected, actual)`                 | Partial (templates)       | Yes                      | No (C++ only)                  | No (heavy runtime)        |

### Why SigmaTest Wins

- **Type-safe, macro-free assertions** — no varargs surprises, no silent truncation.
- **Full custom messages** — you write what you mean, not what the framework allows.
- **Memory leaks are not tolerated** — every test run ends with a mandatory allocation report. Pass your tests but leak memory? You will be told. No Valgrind required.
- **Zero runtime overhead when clean** — only two atomic counters and a destructor. No setup/teardown bloat.
- **Pure C99** — no C++, no exceptions, no RTTI, no templates. Works on embedded, kernels, and freestanding environments.

We don’t ask permission to tell you the truth.  
We don’t hide leaks behind optional tools.  
We don’t make you choose between safety and simplicity.

## Usage  
Intuitive without compromise ... SigmaTest is C testing — **done right**. 

### Basic Test Structure

```c
#include "sigtest.h"

void test_example(void) 
{
    int result = 1 + 1;
    int expected = 2;
    Assert.areEqual(&expected, &result, INT, "1 + 1 should equal 2");
}

__attribute__((constructor)) 
void register_tests(void) 
{
    testcase("example test", test_example);
}
```

### Assertions

Available assertions:

```c
// Asserts the given condition is TRUE
Assert.isTrue(condition, "optional message");

// Asserts the given condition is FALSE
Assert.isFalse(condition, "optional message");

// Asserts that a pointer is NULL
Assert.isNull(ptr, "optional message");

// Asserts that a pointer is not NULL
Assert.isNotNull(ptr, "optional message");

// Asserts that two values are equal
Assert.areEqual(&expected, &actual, type, "optional message");

// Asserts that two values are not equal
Assert.areNotEqual(&expected, &actual, type, "optional message");

// Asserts that a float value is within a specified tolerance
Assert.floatWithin(value, min, max, "optional message");

// Asserts that two strings are equal
Assert.stringEqual(expected, actual, case_sensitive, "optional message");

// Assert throws an exception (fails the test)
Assert.throw("failure message");

// Fails a testcase immediately and logs the message
Assert.fail("failure message");

// Skips the testcase and logs the message
Assert.skip("skip message");
```

Supported types for `areEqual`:  
- `INT`  
- `LONG`  
- `FLOAT`  
- `DOUBLE`  
- `CHAR`  
- `STRING`  
- `PTR`  

#### Test Fixtures

```c
void setup() {
    // Runs before each test
}

void teardown() {
    // Runs after each test
}

__attribute__((constructor))
void register_fixtures() {
    setup_testcase(setup);
    teardown_testcase(teardown);
}
```

#### Test Configuration

```c
void config(FILE** log_stream) {
    *log_stream = fopen("tests.log", "w");
}

void cleanup() {
    // Global cleanup
}

__attribute__((constructor)) 
void register_config() {
    testset("my test suite", config, cleanup);
}
```

#### Expected Failures

```c
void failing_test() {
    Assert.isTrue(0, "This test should fail");
}

__attribute__((constructor))
void register_failing() {
    fail_testcase("expected failure", failing_test);
}
```

#### Expected Exceptions

```c
void throwing_test() {
    Assert.throw("This should throw");
}

__attribute__((constructor)) 
void register_throwing() {
    testcase_throws("expected throw", throwing_test);
}
```

### Building and Running

1. Compile your tests with the framework:  
   ```sh
   gcc -o tests sigtest.c your_tests.c

   // or ...
   gcc -o tests your_test.c -lstest
   ```

2. Run the test executable:  
   ```sh
   ./tests
   ```

### Output Example

```
Test Source: test/test_asserts.c
[1] asserts_set              :  13 :         2025-12-06  19:16:27
=================================================================
Running: Assert Is Null                          0.491 us  [PASS]
Running: Assert Is Not Null                      0.397 us  [PASS]
Running: Assert Int Not Equal                    0.210 us  [PASS]
Running: Assert Float Not Equal                  0.102 us  [PASS]
Running: Assert Strings Not Comparable           0.466 us  [PASS]
Running: Assert Fail Test Case                   0.180 us  [PASS]
Running: Assert Skip Test Case                   0.165 us  [SKIP]
=================================================================
[1]     TESTS= 13        PASS= 12        FAIL=  0        SKIP=  1
```  

**NOTE**: Yes, those times are real and they're accurate. Essentially, you are looking at the test framework's overhead. Even when expecting a throw, the overhead is negligible.

### Memory Allocation Tracking — Always On. No Escape.

SigmaTest includes **built-in, zero-config memory leak detection** — because in C, **memory safety is not optional**.

Add: `WRAP_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=calloc -Wl,--wrap=realloc` to your linker flags ... then every `malloc`, `calloc`, `realloc`, and `free` in your tests and test dependencies are tracked — globally and automatically.

At the end of every test run, you get a **Memory Allocations Report**:

```
===== Memory Allocations Report =================================
WARNING: MEMORY LEAK — 1 unfreed allocation(s)
  Total mallocs:                5
  Total frees:                  4
=================================================================
```

- **No setup required** (other than LDFLAGS) 
- **No hooks needed**  
- **No Valgrind**  
- **No excuses**

If your test passes but leaks memory — **you will know**.

This is not a "nice-to-have".  
This is **C done right**.

> **Note**: This is a global, always-on tracker. It reports total allocations across all test cases. Per-test isolation and advanced features (backtraces, peak memory, histograms) are coming in the full `MemCheck` hook (deferred for v1.0.0). But even without it — **your leaks are dead on arrival**.

Welcome to the future of C testing.

## Best Practices

1. Keep tests small and focused  
2. Use descriptive test names  
3. Test both success and failure cases  
4. Clean up resources in teardown  
5. Group related tests into test sets  
6. Use the `fail_testcase` wrapper for negative tests  
7. Prefer specific assertions over generic `isTrue`  

## Advanced Features

### Custom Logging  
Override the default logging by providing a different `FILE*` in your config function.

### Debug Output  
Use `debugf()` for additional debug information that only appears when tests fail.

### Floating Point Comparisons  
The framework uses FLT_EPSILON/DBL_EPSILON for floating point comparisons to handle precision issues.

## Limitations

- Fixed maximum number of tests (100 by default)  
- No built-in test discovery  
- No parallel test execution  
- Basic reporting format  

## Contributing  
Contributions are welcome! Please open an issue or pull request for any bugs or feature requests.

## License  
[MIT License](LICENSE)

--- 

Bug fixes:
- 12-04-2025: defaulted `log_stream` to `stdout` if test set config function not called
    Prior to fix, if the config function was not called, the `log_stream` was `null` so would _`seg-fault`_. The options were to _not_ log anything if `log_stream` was left null; or, set to `stdout`

---

I am currently working on expanding capabilities. I am open to feature requests as long as there is a value add for the general public.
