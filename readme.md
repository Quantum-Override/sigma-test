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

**Comparison:**  
| Framework       | Assertion Style | Type Safety | Custom Messages |  
|----------------|----------------|------------|----------------|  
| **Sigma Test** | `Assert.areEqual(&exp, &act, INT, "msg")` | ✅ Yes | ✅ Yes |  
| **Unity**      | `TEST_ASSERT_EQUAL_INT(exp, act)` | ✅ Yes | ❌ No |  
| **Check**      | `ck_assert_int_eq(exp, act)` | ✅ Yes | ❌ No |  

## Usage

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
Assert.isTrue(condition, "optional message");
Assert.isFalse(condition, "optional message");
Assert.areEqual(&expected, &actual, type, "optional message");
Assert.throw("failure message");
```

Supported types for `areEqual`:  
- `INT`  
- `FLOAT`  
- `DOUBLE`  
- `CHAR`  
- `STRING`  
- `PTR`  

### Test Fixtures

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

### Test Configuration

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

### Expected Failures

```c
void failing_test() {
    Assert.isTrue(0, "This test should fail");
}

__attribute__((constructor))
void register_failing() {
    fail_testcase("expected failure", failing_test);
}
```

### Expected Exceptions

```c
void throwing_test() {
    Assert.throw("This should throw");
}

__attribute__((constructor)) 
void register_throwing() {
    testcase_throws("expected throw", throwing_test);
}
```

## Building and Running

1. Compile your tests with the framework:  
   ```sh
   gcc -o tests sigtest.c your_tests.c
   ```

2. Run the test executable:  
   ```sh
   ./tests
   ```

## Output Example

```
Starting test set: logging test set, registered 4 tests
Running test: add [PASS]
Running test: divide [PASS]
Running test: divide_by_zero_fails [PASS]
Running test: divide_by_zero_throws [PASS]
Tests run: 4, Passed: 4, Failed: 0
```

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
Override the default logging by providing a different FILE* in your config function.

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
