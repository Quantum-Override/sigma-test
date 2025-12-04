/*	sigtest.h
	Header for the sigma test assert interface

	David Boarman
	2024-09-01

	SIGTEST_VERSION "0.03.01"
*/
#ifndef SIGTEST_H
#define SIGTEST_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
// -----
#include <unistd.h>
#include <sys/syscall.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

struct sigtest_case_s;
struct sigtest_set_s;
struct sigtest_hooks_s;
struct sigtest_logger_s;

typedef void *object;
typedef char *string;
typedef struct timespec ts_time;

typedef struct sigtest_case_s *TestCase;
typedef struct sigtest_set_s *TestSet;
typedef struct sigtest_hooks_s *SigtestHooks;
typedef struct sigtest_logger_s *Logger;

typedef void (*TestFunc)(void);		 // Test function pointer
typedef void (*CaseOp)(void);		 // Test case operation function pointer - setup/teardown
typedef void (*ConfigFunc)(FILE **); // Test set config function pointer
typedef void (*CleanupFunc)(void);	 // Test set cleanup function pointer
// typedef void (*OutputFunc)(const TestSet, const TestCase, const object); // Output formatting function
typedef void (*SetOp)(const TestSet, object); // Test set operation function pointer

// Output debug levels
typedef enum
{
	DBG_DEBUG,	 // Debug level logging
	DBG_INFO,	 // Info level logging
	DBG_WARNING, // Warning level logging
	DBG_ERROR,	 // Error (non-fatal) level logging
	DBG_FATAL,	 // Fatal error level logging
} DebugLevel;

extern TestSet test_sets; // Global test set registry

/**
 * @brief Type info enums
 */
typedef enum
{
	INT,
	FLOAT,
	DOUBLE,
	CHAR,
	PTR,
	STRING,
	// Add more types as needed
} AssertType;
/**
 * @brief Test state result
 */
typedef enum
{
	PASS,
	FAIL,
	SKIP
} TestState;

/**
 * @brief Assert interface structure with function pointers
 */
typedef struct IAssert
{
	/**
	 * @brief Asserts the given condition is TRUE
	 * @param  condition :the condition to check
	 * @param  fmt :the format message to display if assertion fails
	 */
	void (*isTrue)(int, const string, ...);
	/**
	 * @brief Asserts the given condition is FALSE
	 * @param  condition :the condition to check
	 * @param  fmt :the format message to display if assertion fails
	 */
	void (*isFalse)(int, const string, ...);
	/**
	 * @brief Asserts that a pointer is NULL
	 * @param  ptr :the pointer to check
	 * @param  fmt :the format message to display if assertion fails
	 */
	void (*isNull)(object, const string, ...);
	/**
	 * @brief Asserts that a pointer is not NULL
	 * @param  ptr :the pointer to check
	 * @param  fmt :the format message to display if assertion fails
	 */
	void (*isNotNull)(object, const string, ...);
	/**
	 * @brief Asserts that two values are equal.
	 * @param expected :expected value.
	 * @param actual :actual value to compare.
	 * @param type :the value types
	 * @param fmt :format message to display if assertion fails.
	 */
	void (*areEqual)(object, object, AssertType, const string, ...);
	/**
	 * @brief Asserts that two values are not equal.
	 * @param expected :expected value.
	 * @param actual :actual value to compare.
	 * @param type :the value types
	 * @param fmt :format message to display if assertion fails.
	 */
	void (*areNotEqual)(object, object, AssertType, const string, ...);
	/**
	 * @brief Asserts that a float value is within a specified tolerance.
	 * @param value :the float value to check.
	 * @param min :the minimum tolerance value.
	 * @param max :the maximum tolerance value.
	 * @param fmt :format message to display if assertion fails.
	 */
	void (*floatWithin)(float, float, float, const string, ...);
	/**
	 * @brief Asserts that two strings are equal.
	 * @param expected :expected string.
	 * @param actual :actual string to compare.
	 * @param case_sensitive :case sensitivity flag.
	 * @param fmt :format message to display if assertion fails.
	 */
	void (*stringEqual)(string, string, int, const string, ...);
	/**
	 * @brief Assert throws an exception
	 * @param fmt :the format message to display if assertion fails
	 */
	void (*throw)(const string, ...);
	/**
	 * @brief Fails a testcase immediately and logs the message
	 * @param fmt :the format message to display
	 */
	void (*fail)(const string, ...);
	/**
	 * @brief Skips the testcase setting the state as skipped and logs the message
	 * @param fmt :the format message to display
	 */
	void (*skip)(const string, ...);
} IAssert;

/**
 * @brief Global instance of the IAssert interface for use in tests
 */
extern const IAssert Assert;

/**
 * @brief Test case structure
 * @detail Encapsulates the name of the test and the test case function pointer
 */
typedef struct sigtest_case_s
{
	string name;
	TestFunc test_func; /* Test function pointer */
	int expect_fail;	/* Expect failure flag */
	int expect_throw;	/* Expect throw flag */
	struct
	{
		TestState state;
		string message;
	} test_result;
	TestCase next; /* Pointer to the next test case */
} sigtest_case_s;

/**
 * @brief Logger structure for test set logging
 */
typedef struct sigtest_logger_s
{
	void (*log)(const char *, ...);				  /* Logging function pointer */
	void (*debug)(DebugLevel, const char *, ...); /* Debug logging function pointer */
} sigtest_logger_s;

/**
 * @brief Test set structure for global setup and cleanup
 */
typedef struct sigtest_set_s
{
	string name;		 /* Test set name */
	CleanupFunc cleanup; /* Test set cleanup function */
	CaseOp setup;		 /* Test case setup function */
	CaseOp teardown;	 /* Test case teardown function */
	FILE *log_stream;	 /* Log stream for the test set */
	TestCase cases;		 /* Pointer to the test cases */
	TestCase tail;		 /* Pointer to the last test case */
	int count;			 /* Number of test cases */
	int passed;			 /* Number of passed test cases */
	int failed;			 /* Number of failed test cases */
	int skipped;		 /* Number of skipped test cases */
	TestCase current;	 /* Current test case */
	TestSet next;		 /* Pointer to the next test set */
	SigtestHooks hooks;	 /* Hooks for the test set */
	Logger logger;		 /* Logger for the test set */
} sigtest_set_s;

/**
 * @brief Retrieve the SigmaTest version
 */
const char *
sigtest_version(void);
/**
 * @brief Registers a new test into the test array
 * @param  name :the test name
 * @param  func :the test function
 */
void testcase(string name, void (*func)(void));
/**
 * @brief Registers a testcase with fail expectation
 * @param  name :the test name
 * @param  func :the test function
 */
void fail_testcase(string name, void (*func)(void));
/**
 * @brief Registers a test case with expectation to throw
 * @param  name :the test name
 * @param  func :the test function
 */
void testcase_throws(string name, void (*func)(void));
/**
 * @brief Registers the test case setup function
 * @param  setup :the test case setup function
 */
void setup_testcase(void (*setup)(void));
/**
 * @brief Registers the test case teardown function
 * @param  teardown :the test case teardown function
 */
void teardown_testcase(void (*teardown)(void));
/**
 * @brief Registers the test set config & cleanup function
 * @param  config :the test set config function
 * @param  cleanup :the test set cleanup function
 */
void testset(string name, void (*config)(FILE **), void (*cleanup)(void));
/**
 * @brief Register test hooks
 * @param hooks :the test set hooks
 */
void register_hooks(SigtestHooks);

/**
 * @brief Writes a formatted message to the current test set's log stream
 * @param fmt :the format message to display
 * @param ... :the variable arguments for the format message
 */
void writef(const char *, ...);
/**
 * @brief Writes a formatted message with newline to the current test set's log stream
 * @param fmt :the format message to display
 * @param ... :the variable arguments for the format message
 */
void writelnf(const char *, ...);
/**
 * @brief Writes a formatted message to the specified stream
 * @param stream :the output stream to write to
 * @param fmt :the format message to display
 * @param ... :the variable arguments for the format message
 */
void fwritef(FILE *, const char *, ...);
/**
 * @brief Writes a formatted message with newline to the specified stream
 * @param stream :the output stream to write to
 * @param fmt :the format message to display
 * @param ... :the variable arguments for the format message
 */
void fwritelnf(FILE *, const char *, ...);

/**
 * @brief Formats the current time into a buffer using the specified format
 * @param buffer :output buffer for the timestamp (at least 32 chars)
 * @param format :strftime format string (e.g., "%Y-%m-%dT%H:%M:%S")
 */
void get_timestamp(char *, const char *);

/**
 * @brief Test hooks structure
 */
typedef struct sigtest_hooks_s
{
	const char *name;											   // Hooks label
	void (*before_set)(const TestSet, object);					   // Called before each test set
	void (*after_set)(const TestSet, object);					   // Called after each test set
	void (*before_test)(object);								   // Called before each test case
	void (*after_test)(object);									   // Called after each test case
	void (*on_start_test)(object);								   // Callback at the start of a test
	void (*on_end_test)(object);								   // Callback at the end of a test
	void (*on_error)(const char *, object);						   // Callback on error
	void (*on_test_result)(const TestSet, const TestCase, object); // Callback on test result
	void *context;												   // User-defined data
} sigtest_hooks_s;
/**
 * @brief Test hooks registry
 */
typedef struct hook_registry_s
{
	SigtestHooks hooks;
	struct hook_registry_s *next;
} HookRegistry;
/**
 * @brief Initialize default hooks with the specified output format
 * @param name :the desitred output format
 * @return pointer to the initialized SigtestHooks
 */
SigtestHooks init_hooks(const char *);

/**
 * @brief Registers a test set with the given name
 * @param  sets :the test sets under test
 * @param  hooks :the test runner hooks
 * @return 0 if all tests pass, 1 if any test fails
 */
int run_tests(TestSet, SigtestHooks);

#endif // SIGTEST_H
