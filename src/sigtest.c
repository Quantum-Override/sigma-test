/*
 * SigmaTest
 * Copyright (c) 2025 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ----------------------------------------------
 * File: sigtest.c
 * Description: Source file for SigmaTest core interfaces and implementations
 */
#include "sigtest.h"
#include "fuzzing.h"
#include "internal/logging.h"
#include "internal/runner_states.h"
#include <assert.h>
#include <float.h> //	for FLT_EPSILON && DBL_EPSILON
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // 	for jmp_buf and related functions
#include <strings.h>

#define SIGMATEST_VERSION "1.00.1-pre"

// Global test set "registry"
TestSet test_sets = NULL;
static TestSet current_set = NULL;

// Static buffer for jump
static jmp_buf jmpbuffer;

// Forward declaration for DebugLogger
extern const st_logger_s DebugLogger;

//	Fail messages
#define MESSAGE_TRUE_FAIL "Expected true, but was false"
#define MESSAGE_FALSE_FAIL "Expected false, but was true"
#define MESSAGE_EQUAL_FAIL "Expected %s, but was %s"
#define EXPECT_FAIL_FAIL "Expected test to fail but it passed"
#define EXPECT_THROW_FAIL "Expected test to throw but it didn't"
// For dynamic test state annotation
const char *TEST_STATES[] = {
    "PASS",
    "FAIL",
    "SKIP",
    NULL,
};
// For dynamic log level annotation
static const char *DBG_LEVELS[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "FATAL",
    NULL,
};
//	system clock structures
#define SYS_CLOCK SYS_clock_gettime
#define CLOCK_MONOTONIC 1

static ST_Hooks current_hooks = {0};
static atomic_size_t global_allocs = 0;
static atomic_size_t global_frees = 0;
static int inside_test = 0;
static int set_started = 0;
static TestCase current_tc = NULL;
size_t _sigtest_alloc_count = 0;
size_t _sigtest_free_count = 0;

int sys_gettime(ts_time *ts) {
   return clock_gettime(CLOCK_MONOTONIC, ts);
}
double get_elapsed_ms(ts_time *start, ts_time *end) {
   return (end->tv_nsec - start->tv_nsec) / 1000.0;
}
//	internal logger declarations
// internal clean up
static void default_on_testcase_finish(void);
static void default_on_testset_finished(void);
// hooks registry
static HookRegistry *hook_registry = NULL;

/*
 * Test case structure
 * Encapsulates the name of the test and the test case function pointer
 */
typedef struct st_case_s {
   struct {
      string name;
      struct {
         TestState state;
         string message;
      } result;
      int has_next;
   } info;
   TestFunc test_func; /* Test function pointer */
   // encapsulate fuzzy test function pointer
   union {
      TestFunc test;  /* Regular test function */
      FuzzyFunc fuzz; /* Fuzzy test function */
   } func;
   int expect_fail;    /* Expect failure flag */
   int expect_throw;   /* Expect throw flag */
   int is_fuzz;        /* Is fuzz test flag */
   FuzzType fuzz_type; /* Fuzz input type */
   TestCase next;      /* Pointer to the next test case */
   int ran_no_newline;
   int had_debug;
   int running_len;
} st_case_s;
/**
 * @brief Test set structure for global setup and cleanup
 */
typedef struct st_set_s {
   struct {
      string name;    /* Test set name */
      TcInfo tc_info; /* Current test case info */
      int count;      /* Number of test cases */
      int passed;     /* Number of passed test cases */
      int failed;     /* Number of failed test cases */
      int skipped;    /* Number of skipped test cases */
   } info;
   CleanupFunc cleanup; /* Test set cleanup function */
   CaseOp setup;        /* Test case setup function */
   CaseOp teardown;     /* Test case teardown function */
   FILE *log_stream;    /* Log stream for the test set */
   TestCase cases;      /* Pointer to the test cases */
   TestCase tail;       /* Pointer to the last test case */
   TestCase current;    /* Current test case */
   TestSet next;        /* Pointer to the next test set */
   ST_Hooks hooks;      /* Hooks for the test set */
   Logger logger;       /* Logger for the test set */
} st_set_s;

/*
 * hook_ctx_t - shared hook context structure
 *
 * This structure is used as the generic `context` pointer passed into hook
 * callbacks. Hooks may cast the `context` to `hook_ctx_t *` to read or write
 * runtime state shared across hooks for the current test execution.
 *
 * Fields:
 *  - count:     reference count / nesting counter used by before/after hooks
 *  - verbose:   verbosity flag
 *  - start,end: timestamps recorded around a test execution
 *  - state:     current runner state
 *  - ran_no_newline: flag set when the "Running:" line was printed without newline
 *  - had_debug:      flag set when debug/log messages were emitted inside a test
 *  - running_len:    length of the printed "Running:" prefix used for inline result padding
 */
typedef struct hook_ctx_s {
   int count;
   int verbose;
   ts_time start;
   ts_time end;
   RunnerState state;
   int ran_no_newline;
   int had_debug;
   int running_len;
} hook_ctx_t;

//	Implementations for internal helpers
/**
 * Formats the current time into a buffer using the specified format
 */
void get_timestamp(char *buffer, const char *format) {
   time_t now = time(NULL);
   strftime(buffer, 32, format, localtime(&now));
}
// format write message to stream
void fwritef(FILE *, const char *, ...);
// print an '=' separator of given width
static void print_sep(FILE *stream, int width) {
   if (!stream)
      stream = stdout;
   for (int i = 0; i < width; ++i)
      fputc('=', stream);
   fputc('\n', stream);
   fflush(stream);
}
// Initialize hooks with the given name/label
ST_Hooks init_hooks(const char *name) {
   // Check if the name is NULL or empty
   if (!name || !*name) {
      fwritelnf(stderr, "Error: Hook name cannot be NULL or empty");
      return NULL; // Invalid name
   }
   //	look first for named hooks in registry
   for (HookRegistry *entry = hook_registry; entry; entry = entry->next) {
      if (entry->hooks->name && strcmp(entry->hooks->name, name) == 0) {
         return entry->hooks;
      }
   }
   // or create a new one
   ST_Hooks hooks = __real_malloc(sizeof(struct st_hooks_s));
   if (!hooks) {
      fwritelnf(stderr, "Error: Failed to allocate memory for hooks");
      return NULL; // Memory allocation failed
   }
   hooks->name = strdup(name);
   if (!hooks->name) {
      fwritelnf(stderr, "Error: Failed to duplicate hook name");
      __real_free(hooks);
      return NULL; // Memory allocation failed
   }
   *hooks = (struct st_hooks_s){
       .name = strdup(name),
       .before_set = NULL,
       .after_set = NULL,
       .before_test = NULL,
       .after_test = NULL,
       .on_start_test = NULL,
       .on_end_test = NULL,
       .on_error = NULL,
       .on_test_result = NULL,
       .on_memory_alloc = NULL,
       .on_memory_free = NULL,
       .context = NULL,
   };

   return hooks;
}

// cleanup test runner
static void cleanup_test_runner(void) {
   if (!test_sets)
      return; // Already cleaned up

   TestSet set = test_sets;
   while (set) {
      TestSet next = set->next;
      // __real_free test cases
      TestCase tc = set->cases;
      while (tc) {
         TestCase next_tc = tc->next;
         __real_free(tc->info.name);
         if (tc->info.result.message)
            __real_free(tc->info.result.message);

         __real_free(tc);
         tc = next_tc;
      }

      // __real_free test set
      __real_free(set->info.name);
      if (set->log_stream != stdout && set->log_stream) {
         fclose(set->log_stream);
         set->log_stream = NULL;
      }

      __real_free(set);
      set = next;
   }
   /* do not free logger pointer here - logger may point to static data */

   // Reset the test set registry
   test_sets = NULL;
   current_set = NULL;
}
//	generate formatted message
static string format_msg(const string fmt, va_list args) {
   static char msg_buffer[256];
   vsnprintf(msg_buffer, sizeof(msg_buffer), fmt ? fmt : "", args);

   return msg_buffer;
}
// get defined message or default message
// format_message was inlined into specific assert handlers; removed unused helper
// generate message for assertEquals
static string gen_equals_fail_msg(object expected, object actual, AssertType type, const string fmt, va_list args) {
   static char msg_buffer[256];
   char exp_str[20], act_str[20];

   switch (type) {
   case INT:
      snprintf(exp_str, sizeof(exp_str), "%d", *(int *)expected);
      snprintf(act_str, sizeof(act_str), "%d", *(int *)actual);

      break;
   case FLOAT:
      snprintf(exp_str, sizeof(exp_str), "%.5f", *(float *)expected);
      snprintf(act_str, sizeof(act_str), "%.5f", *(float *)actual);

      break;
   case DOUBLE:
      snprintf(exp_str, sizeof(exp_str), "%.5f", *(double *)expected);
      snprintf(act_str, sizeof(act_str), "%.5f", *(double *)actual);

      break;
   case CHAR:
      snprintf(exp_str, sizeof(exp_str), "%c", *(char *)expected);
      snprintf(act_str, sizeof(act_str), "%c", *(char *)actual);

      break;
   case STRING:
      strncpy(exp_str, (string)expected, sizeof(exp_str) - 1);
      exp_str[sizeof(exp_str) - 1] = '\0';
      strncpy(act_str, (string)actual, sizeof(act_str) - 1);
      act_str[sizeof(act_str) - 1] = '\0';

      break;
   case PTR:
      snprintf(exp_str, sizeof(exp_str), "%p", expected);
      snprintf(act_str, sizeof(act_str), "%p", actual);

      break;
   default:
      return "Unsupported type for comparison";
   }

   string user_msg = fmt ? format_msg(fmt, args) : "";
   snprintf(msg_buffer, sizeof(msg_buffer), MESSAGE_EQUAL_FAIL, exp_str, act_str);
   if (user_msg[0] != '\0') {
      strncat(msg_buffer, "\n    - ", sizeof(msg_buffer) - strlen(msg_buffer) - 1);
      strncat(msg_buffer, user_msg, sizeof(msg_buffer) - strlen(msg_buffer) - 1);
   }
   return msg_buffer;
}
// format fuzz value for logging
static void format_fuzz_value(FuzzType type, const void *ptr, char *buf, size_t bufsize) {
   switch (type) {
   case FUZZ_INT:
      snprintf(buf, bufsize, "%d", *(const int *)ptr);
      break;
   case FUZZ_SIZE_T:
      snprintf(buf, bufsize, "%zu", *(const size_t *)ptr);
      break;
   case FUZZ_FLOAT: {
      float f = *(const float *)ptr;
      if (isnan(f))
         snprintf(buf, bufsize, "NAN");
      else if (isinf(f))
         snprintf(buf, bufsize, "%cINFINITY", f < 0 ? '-' : '+');
      else
         snprintf(buf, bufsize, "%.9g", f);
      break;
   }
   case FUZZ_BYTE:
      snprintf(buf, bufsize, "%d", (int)*(const signed char *)ptr);
      break;
   }
}

void set_test_context(TestState result, const string message) {
   if (current_set && current_set->current) {
      current_set->current->info.result.state = result;
      if (current_set->current->info.result.message) {
         __real_free(current_set->current->info.result.message);
      }
      current_set->current->info.result.message = message ? strdup(message) : NULL;
      if (result != PASS) {
         // Stop assertions for this test
         longjmp(jmpbuffer, 1);
      }
   }
}

void test_context(object ctx) {
   struct ctx {
      FILE *log_stream;
   } *context = ctx;

   if (current_set) {
      context->log_stream = current_set->log_stream;
   } else {
      context->log_stream = stdout;
   }
}

//	Implementations for CLI interface
const char *st_version(void) {
   return SIGMATEST_VERSION;
}

#if 1 // Implementations for assertions (public interface)
// Asserts the condition is TRUE
static void assert_is_true(int condition, const string fmt, ...) {
   va_list args;
   va_start(args, fmt);
   if (!condition) {
      string user_msg = fmt ? format_msg(fmt, args) : "";
      if (user_msg && user_msg[0] != '\0') {
         static char buf[256];
         snprintf(buf, sizeof(buf), "%s\n    - %s", MESSAGE_TRUE_FAIL, user_msg);
         set_test_context(FAIL, buf);
      } else {
         set_test_context(FAIL, MESSAGE_TRUE_FAIL);
      }
   } else {
      set_test_context(PASS, NULL);
   }

   va_end(args);
}
// Asserts the condition is FALSE
static void assert_is_false(int condition, const string fmt, ...) {
   va_list args;
   va_start(args, fmt);
   if (condition) {
      string user_msg = fmt ? format_msg(fmt, args) : "";
      if (user_msg && user_msg[0] != '\0') {
         static char buf[256];
         snprintf(buf, sizeof(buf), "%s\n    - %s", MESSAGE_FALSE_FAIL, user_msg);
         set_test_context(FAIL, buf);
      } else {
         set_test_context(FAIL, MESSAGE_FALSE_FAIL);
      }
   } else {
      set_test_context(PASS, NULL);
   }

   va_end(args);
}
// Asserts the pointer is NULL
static void assert_is_null(object ptr, const string fmt, ...) {
   va_list args;
   va_start(args, fmt);
   if (ptr != NULL) {
      string user_msg = fmt ? format_msg(fmt, args) : "";
      if (user_msg && user_msg[0] != '\0') {
         static char buf[256];
         snprintf(buf, sizeof(buf), "%s\n    - %s", "Pointer is not NULL", user_msg);
         set_test_context(FAIL, buf);
      } else {
         set_test_context(FAIL, "Pointer is not NULL");
      }
   } else {
      set_test_context(PASS, NULL);
   }

   va_end(args);
}
// Asserts the pointer is not NULL
static void assert_is_not_null(object ptr, const string fmt, ...) {
   va_list args;
   va_start(args, fmt);
   if (ptr == NULL) {
      string user_msg = fmt ? format_msg(fmt, args) : "";
      if (user_msg && user_msg[0] != '\0') {
         static char buf[256];
         snprintf(buf, sizeof(buf), "%s\n    - %s", "Pointer is NULL", user_msg);
         set_test_context(FAIL, buf);
      } else {
         set_test_context(FAIL, "Pointer is NULL");
      }
   } else {
      set_test_context(PASS, NULL);
   }

   va_end(args);
}
// Asserts two values are equal
static void assert_are_equal(object expected, object actual, AssertType type, const string fmt, ...) {
   va_list args;
   va_start(args, fmt);

   // char messageBuffer[256];
   enum {
      PASS,
      FAIL
   } result = PASS;
   string failMessage = NULL;

   switch (type) {
   case INT:
      if (*(int *)expected != *(int *)actual) {
         failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
         result = FAIL;
      }

      break;
   case LONG:
      if (*(long *)expected != *(long *)actual) {
         failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
         result = FAIL;
      }

      break;
   case FLOAT:
      if (fabs(*(float *)expected - *(float *)actual) > FLT_EPSILON) {
         failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
         result = FAIL;
      }

      break;
   case DOUBLE:
      if (fabs(*(double *)expected - *(double *)actual) > DBL_EPSILON) {
         failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
         result = FAIL;
      }

      break;
   case CHAR:
      if (*(char *)expected != *(char *)actual) {
         failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
         result = FAIL;
      }

      break;
   case PTR:
      if (expected != actual) {
         failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
         result = FAIL;
      }

      break;
   case STRING:
      failMessage = "Use Assert.stringEqual for string comparison";
      result = FAIL;

      break;
   default:

      failMessage = "Unsupported type for comparison";
      result = FAIL;

      break;
      // Add cases for other types as needed
   }

   // if (result == FAIL)
   // {
   // 	debugf("Assertion failed: %s", failMessage); /* Log failure */
   // }

   set_test_context(result, failMessage);

   va_end(args);
}
// Asserts two values are not equal
static void assert_are_not_equal(object expected, object actual, AssertType type, const string fmt, ...) {
   va_list args;
   va_start(args, fmt);

   // char messageBuffer[256];
   enum {
      PASS,
      FAIL
   } result = PASS;
   string failMessage = NULL;

   switch (type) {
   case INT:
      if (*(int *)expected == *(int *)actual) {
         failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
         result = FAIL;
      }

      break;
   case FLOAT:
      if (fabs(*(float *)expected - *(float *)actual) <= FLT_EPSILON) {
         failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
         result = FAIL;
      }

      break;
   case DOUBLE:
      if (fabs(*(double *)expected - *(double *)actual) <= DBL_EPSILON) {
         failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
         result = FAIL;
      }

      break;
   case CHAR:
      if (*(char *)expected == *(char *)actual) {
         failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
         result = FAIL;
      }

      break;
   case PTR:
      if (expected == actual) {
         failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
         result = FAIL;
      }

      break;
   case STRING:
      failMessage = "Use Assert.stringEqual for string comparison";
      result = FAIL;

      break;
   default:

      failMessage = "Unsupported type for comparison";
      result = FAIL;

      break; // Add cases for other types as needed
   }

   // if (result == FAIL)
   // {
   // 	debugf("Assertion failed: %s", failMessage); /* Log failure */
   // }

   set_test_context(result, failMessage);

   va_end(args);
}
// Asserts that a float value is within a specified tolerance
static void assert_float_within(float value, float min, float max, const string fmt, ...) {
   va_list args;
   va_start(args, fmt);

   if (value < min || value > max) {
      string user_msg = fmt ? format_msg(fmt, args) : "";
      if (user_msg && user_msg[0] != '\0') {
         static char buf[256];
         snprintf(buf, sizeof(buf), "%s\n    - %s", "Value out of range", user_msg);
         set_test_context(FAIL, buf);
      } else {
         set_test_context(FAIL, "Value out of range");
      }
   } else {
      set_test_context(PASS, NULL);
   }

   va_end(args);
}
// Asserts that two strings are equal with respect to case sensitivity
static void assert_string_equal(string expected, string actual, int case_sensitive, const string fmt, ...) {
   va_list args;
   va_start(args, fmt);

   int equal = case_sensitive ? strcmp(expected, actual) == 0 : strcasecmp(expected, actual) == 0;
   if (!equal) {
      string failMessage = gen_equals_fail_msg(expected, actual, STRING, fmt, args);
      // debugf("Assertion failed: %s", failMessage);
      set_test_context(FAIL, failMessage);
   } else {
      set_test_context(PASS, NULL);
   }

   va_end(args);
}
// Assert throws
static void assert_throw(const string fmt, ...) {
   va_list args;
   va_start(args, fmt);

   string user_msg = fmt ? format_msg(fmt, args) : "";
   if (user_msg && user_msg[0] != '\0') {
      static char buf[256];
      snprintf(buf, sizeof(buf), "%s\n    - %s", "Explicit throw triggered", user_msg);
      set_test_context(FAIL, buf);
   } else {
      set_test_context(FAIL, "Explicit throw triggered");
   }

   va_end(args);
}
// Assert fail
static void assert_fail(const string fmt, ...) {
   va_list args;
   va_start(args, fmt);

   string user_msg = fmt ? format_msg(fmt, args) : "";
   if (user_msg && user_msg[0] != '\0') {
      static char buf[256];
      snprintf(buf, sizeof(buf), "%s\n    - %s", "Explicit failure triggered", user_msg);
      set_test_context(FAIL, buf);
   } else {
      set_test_context(FAIL, "Explicit failure triggered");
   }

   va_end(args);
}
// Assert skip
static void assert_skip(const string fmt, ...) {
   va_list args;
   va_start(args, fmt);

   string user_msg = fmt ? format_msg(fmt, args) : "";
   if (user_msg && user_msg[0] != '\0') {
      static char buf[256];
      snprintf(buf, sizeof(buf), "%s\n    - %s", "Testcase skipped", user_msg);
      set_test_context(SKIP, buf);
   } else {
      set_test_context(SKIP, "Testcase skipped");
   }

   va_end(args);
}
#endif

#if 1 // Region: Memory wrappers
void *__wrap_malloc(size_t s) {
   void *p = __real_malloc(s);
   if (p) {
      atomic_fetch_add(&global_allocs, 1);
      if (current_hooks && current_hooks->on_memory_alloc) {
         current_hooks->on_memory_alloc(s, p, current_hooks->context);
      }
   }
   return p;
}
void __wrap_free(void *p) {
   if (p) {
      atomic_fetch_add(&global_frees, 1);
      if (current_hooks && current_hooks->on_memory_free) {
         current_hooks->on_memory_free(p, current_hooks->context);
      }
   }
   __real_free(p);
}
#endif

// Assertion interface
const st_assert_i Assert = {
    .isTrue = assert_is_true,
    .isFalse = assert_is_false,
    .isNull = assert_is_null,
    .isNotNull = assert_is_not_null,
    .areEqual = assert_are_equal,
    .areNotEqual = assert_are_not_equal,
    .floatWithin = assert_float_within,
    .stringEqual = assert_string_equal,
    .throw = assert_throw,
    .fail = assert_fail,
    .skip = assert_skip,
};

// Register test set
void testset(string name, ConfigFunc config, CleanupFunc cleanup) {
   // ensure cleanup_test_runner is registered only once
   static int atexit_registered = 0;
   if (!atexit_registered) {
      if (atexit(cleanup_test_runner) != 0) {
         fwritelnf(stdout, "Failed to register `cleanup_test_runner` with atexit\n");
         exit(EXIT_FAILURE);
      }
      atexit_registered = 1;
   }

   TestSet set = __real_malloc(sizeof(struct st_set_s));
   if (!set) {
      fwritelnf(stdout, "Failed to allocate memory for test set\n");
      exit(EXIT_FAILURE);
   }
   set->info.name = strdup(name);
   set->cleanup = cleanup;
   set->setup = NULL;
   set->teardown = NULL;
   set->log_stream = stdout;
   set->cases = NULL;
   set->tail = NULL;
   set->info.count = 0;
   set->info.passed = 0;
   set->info.failed = 0;
   set->info.skipped = 0;
   set->current = NULL;
   set->next = test_sets;
   /* do not allocate a logger here; assign default logger during set_init */
   set->logger = NULL;

   // Execute config immediately if provided (open log stream first)
   if (config) {
      config(&set->log_stream);
      if (!set->log_stream) {
         set->log_stream = stdout; // Fallback to stdout if config fails
      }
      /* Do not write header here — runner's before_set will write it when executing */
   } else {
      // no config function we need to default log_stream to console output
      set->log_stream = stdout;
   }

   /* logger will be set in set_init (default to DebugLogger) */

   // Handle allocation failure after config
   if (!set->info.name) {
      if (set->log_stream != stdout && set->log_stream)
         fclose(set->log_stream);
      __real_free(set);
      writelnf("Failed to allocate memory for test set name\n");
      exit(EXIT_FAILURE);
   }

   test_sets = set;
   current_set = set;
}

static TestCase create_testcase(string);
// Register test to test registry
void testcase(string name, TestFunc func) {
   if (!current_set) {
      testset("default", NULL, NULL);
   }

   TestCase tc = create_testcase(name);
   tc->func.test = func;

   if (!current_set->cases) {
      current_set->cases = tc;
      current_set->tail = tc;
   } else {
      current_set->tail->next = tc;
      current_set->tail = tc;
   }

   current_set->info.count++;
}
// Register test to test registry with expectation to fail
void fail_testcase(string name, void (*func)(void)) {
   if (!current_set) {
      testset("default", NULL, NULL);
   }

   TestCase tc = create_testcase(name);
   tc->func.test = func;
   tc->expect_fail = TRUE;

   if (!current_set->cases) {
      current_set->cases = tc;
      current_set->tail = tc;
   } else {
      current_set->tail->next = tc;
      current_set->tail = tc;
   }

   current_set->info.count++;
}
// Register test to test registry with expectation to throw
void testcase_throws(string name, void (*func)(void)) {
   if (!current_set) {
      testset("default", NULL, NULL);
   }

   TestCase tc = create_testcase(name);
   tc->func.test = func;
   tc->expect_throw = TRUE;

   if (!current_set->cases) {
      current_set->cases = tc;
      current_set->tail = tc;
   } else {
      current_set->tail->next = tc;
      current_set->tail = tc;
   }

   current_set->info.count++;
}
// Register a fuzz test case
void fuzz_testcase(string name, FuzzyFunc func, FuzzType type) {
   if (!current_set) {
      testset("default", NULL, NULL);
   }

   TestCase tc = create_testcase(name);
   tc->is_fuzz = TRUE;
   tc->func.fuzz = func;
   tc->fuzz_type = type;

   if (!current_set->cases) {
      current_set->cases = tc;
      current_set->tail = tc;
   } else {
      current_set->tail->next = tc;
      current_set->tail = tc;
   }

   current_set->info.count++;
}

// Setup test case
void setup_testcase(CaseOp setup) {
   if (current_set) {
      current_set->setup = setup;
   }
}
// Teardown test case
void teardown_testcase(CaseOp teardown) {
   if (current_set) {
      current_set->teardown = teardown;
   }
}
// Create a test case with common defaults
static TestCase create_testcase(string name) {
   TestCase tc = __real_malloc(sizeof(struct st_case_s));
   if (!tc) {
      writef("Failed to allocate memory for test case `%s`\n", name);
      exit(EXIT_FAILURE);
   }
   memset(&tc->info, 0, sizeof(TcInfo));
   tc->info.name = strdup(name);
   if (!tc->info.name) {
      writef("Failed to allocate memory for test case name `%s`\n", name);
      __real_free(tc);
      exit(EXIT_FAILURE);
   }
   tc->info.result.state = PASS;
   tc->info.result.message = NULL;
   tc->info.has_next = FALSE;

   tc->is_fuzz = FALSE;
   tc->func.test = NULL;
   tc->expect_fail = FALSE;
   tc->expect_throw = FALSE;
   tc->next = NULL;
   tc->ran_no_newline = 0;
   tc->had_debug = 0;
   tc->running_len = 0;
   return tc;
}

// Register test hooks
void register_hooks(ST_Hooks hooks) {
   HookRegistry *entry = __real_malloc(sizeof(HookRegistry));
   if (!entry) {
      fwritelnf(stderr, "Error: Failed to allocate hook registry entry");
      return; // Memory allocation failed
   }

   entry->hooks = hooks;
   entry->next = hook_registry;
   hook_registry = entry;

   current_hooks = hooks;

   if (current_set && !current_set->hooks) {
      current_set->hooks = hooks;
   }
}
//	default test hooks
tc_context default_ctx = {{0, 0, {0, 0}, {0, 0}, RUNNER_IDLE, NULL}, NULL};

static void default_before_test(tc_context *ctx) {
   (void)*ctx; // unused
   default_ctx.info.count++;
}
static void append_to_buffer(tc_context *ctx, const char *str) {
   size_t len = strlen(str);
   if (ctx->buffer_used + len >= ctx->buffer_size) {
      ctx->buffer_size = (ctx->buffer_size + len + 1024) & ~1023; // round up to 1024
      ctx->output_buffer = __real_realloc(ctx->output_buffer, ctx->buffer_size);
   }
   memcpy(ctx->output_buffer + ctx->buffer_used, str, len);
   ctx->buffer_used += len;
}
static void default_on_start_test(tc_context *ctx) {
   if (sys_gettime(&ctx->test_start) == -1) {
      fwritelnf(stderr, "Error: Failed to get system start time");
      exit(EXIT_FAILURE);
   }
   // zero out the end time
   ctx->info.end = (ts_time){0, 0};

   if (!ctx->output_buffer) {
      ctx->buffer_size = 2048;
      ctx->output_buffer = __real_malloc(ctx->buffer_size);
   }
   ctx->buffer_used = 0;

   TestCase tc = current_set ? current_set->current : NULL;

   (void)set_started; /* header is printed in before_set */
   if (tc) {
      inside_test = 1;
      /* Print Running without newline so result can be appended on same line if no debug output */
      char running_buf[128];
      int len = snprintf(running_buf, sizeof(running_buf), "Running: %-*s", 40, tc->info.name);
      FILE *stream = (current_set && current_set->log_stream) ? current_set->log_stream : stdout;
      fwritef(stream, "%s", running_buf);
      /* record that we've written Running without newline */
      current_tc->ran_no_newline = 1;
      current_tc->had_debug = 0;
      current_tc->running_len = len;
   }
}
static void default_on_end_test(tc_context *ctx) {
   if (sys_gettime(&ctx->info.end) == -1) {
      fwritelnf(stderr, "Error: Failed to get system end time");
      exit(EXIT_FAILURE);
   }

   inside_test = 0;
   /* end time recorded; final result printing occurs in on_test_result after process_result */
}
static void default_after_test(tc_context *ctx) {
   (void)*ctx; // unused
   default_ctx.info.count--;
}
static void default_on_test_result(const TsInfo ts, tc_context *ctx) {
   if (!ts || !ts->tc_info)
      return;

   double elapsed = get_elapsed_ms(&ctx->test_start, &ctx->info.end);
   const char *state_str = TEST_STATES[ts->tc_info->result.state];

   double elapsed_us = elapsed * 1000.0;
   const char *unit = "µs";
   double display_time = elapsed_us;
   if (elapsed_us >= 1000.0) {
      unit = "ms";
      display_time = elapsed_us / 1000.0;
   }
   char result_buf[64];
   snprintf(result_buf, sizeof(result_buf), "%.3f %s [%s]", display_time, unit, state_str);

   if (ts->tc_info->result.state == FAIL && ts->tc_info->result.message) {
      /* append failure message indented */
      char msg_buf[512];
      snprintf(msg_buf, sizeof(msg_buf), "\n  - %s", ts->tc_info->result.message);
      append_to_buffer(ctx, msg_buf);
      current_tc->had_debug = 1;
   }
   if (!current_tc->had_debug && current_tc->ran_no_newline) {

      /* No debug output occurred; append result to the Running line. Use stored running_len. */
      int pad = 80 - (current_tc->running_len + (int)strlen(result_buf));
      if (pad < 0)
         pad = 0;
      /* append padding and result, then newline */
      char pad_str[81];
      memset(pad_str, ' ', pad);
      pad_str[pad] = '\0';
      append_to_buffer(ctx, pad_str);
      append_to_buffer(ctx, result_buf);
      append_to_buffer(ctx, "\n");
   } else {
      /* right-justify result at column 80 on its own line */
      int pad = 80 - (int)strlen(result_buf);
      if (pad < 0)
         pad = 0;
      char pad_str[81];
      memset(pad_str, ' ', pad);
      pad_str[pad] = '\0';
      append_to_buffer(ctx, "\n");
      append_to_buffer(ctx, pad_str);
      append_to_buffer(ctx, result_buf);
      append_to_buffer(ctx, "\n");
   }

   // Flush the buffer
   if (ctx->output_buffer && ctx->buffer_used > 0) {
      FILE *stream = (current_set && current_set->log_stream) ? current_set->log_stream : stdout;
      fwrite(ctx->output_buffer, 1, ctx->buffer_used, stream);
      fflush(stream);
   }

   /* reset flags for next test */
   current_tc->ran_no_newline = 0;
   current_tc->had_debug = 0;
   current_tc->running_len = 0;
}
static void default_on_debug_log(tc_context *ctx, DebugLevel level, const char *fmt, ...) {
   va_list args;
   va_start(args, fmt);
   char buf[2048];
   vsnprintf(buf, sizeof(buf), fmt, args);
   va_end(args);
   append_to_buffer(ctx, buf);
   current_tc->had_debug = 1;
}
static void default_on_error(const char *message, tc_context *ctx) {
   (void)*ctx;    // unused
   (void)message; // unused
   // Default error handling: do nothing
}
static void default_on_testcase_finish(void) {
   //  total up atomic allocation counts
   _sigtest_alloc_count += atomic_load(&global_allocs);
   _sigtest_free_count += atomic_load(&global_frees);
   // reset atomic allocation counters
   atomic_store(&global_allocs, 0);
   atomic_store(&global_frees, 0);
}
static void default_on_testset_finished(void) {
   //  get the atomic allocation counts
   size_t leaks = (_sigtest_alloc_count > _sigtest_free_count)
                      ? (_sigtest_alloc_count - _sigtest_free_count)
                      : 0;
   /* Print memory allocation report into the test log only (aggregate counts).
    * Final printing to stdout will be handled by runner_summary to ensure timestamp appears first. */
   if (current_set && current_set->log_stream) {
      char memhdr[128];
      snprintf(memhdr, sizeof(memhdr), "===== Memory Allocations Report ");
      int mpad = 80 - (int)strlen(memhdr);
      if (mpad < 0)
         mpad = 0;
      fprintf(current_set->log_stream, "\n%s", memhdr);
      for (int i = 0; i < mpad; ++i)
         fputc('=', current_set->log_stream);
      fputc('\n', current_set->log_stream);

      if (leaks > 0) {
         fwritelnf(current_set->log_stream, "WARNING: MEMORY LEAK — %zu unfreed allocation(s)", leaks);
      } else if (_sigtest_alloc_count > 0) {
         fwritelnf(current_set->log_stream, "Memory clean — all %zu allocations freed.", _sigtest_alloc_count);
      }

      fwritelnf(current_set->log_stream, "  Total mallocs:               %zu", _sigtest_alloc_count);
      fwritelnf(current_set->log_stream, "  Total frees:                 %zu", _sigtest_free_count);
   }

   set_started = 0;
}
static const st_hooks_s default_hooks = {
    .name = "default",
    .before_set = NULL,
    .after_set = NULL,
    .before_test = default_before_test,
    .on_start_test = default_on_start_test,
    .on_end_test = default_on_end_test,
    .after_test = default_after_test,
    .on_error = default_on_error,
    .on_test_result = default_on_test_result,
    .on_debug_log = default_on_debug_log,
    .context = &default_ctx,
};

//	 initialize on start up
__attribute__((constructor)) static void init_default_hooks(void) {
   HookRegistry *entry = __real_malloc(sizeof(HookRegistry));
   // if we don't have a valid hooks registry, we exit
   if (!entry) {
      fwritelnf(stderr, "Error: Failed to allocate hooks registry entry");
      exit(EXIT_FAILURE);
   }

   entry->hooks = (ST_Hooks)&default_hooks;
   entry->next = hook_registry;
   hook_registry = entry;
}

#ifdef SIGTEST_TEST
/*
        test executor entry point
*/
int main(void) {
   int retResult = run_tests(test_sets, current_hooks);
   cleanup_test_runner();

   return retResult;
}
#endif // SIGTEST_TEST

// Runner state handlers
static RunnerState runner_init(TestSet, ST_Hooks, int *, int *, ST_Hooks *);
static RunnerState set_loop(TestSet *, int *);
static RunnerState set_init(TestSet, int *, int *, int *, int *, TestSet *);
static RunnerState before_set(ST_Hooks, int, TestSet, char *);
static RunnerState case_loop(TestCase *);
static RunnerState case_init(TestCase, TestSet);
static RunnerState before_test(ST_Hooks);
static RunnerState setup_test(TestSet);
static RunnerState start_test(ST_Hooks);
static RunnerState execute_test(TestCase, jmp_buf);
static RunnerState execute_fuzzing(TestCase, jmp_buf, const void *, size_t, size_t);
static RunnerState end_test(ST_Hooks);
static RunnerState teardown_test(TestSet);
static RunnerState after_test(ST_Hooks);
static RunnerState process_result(TestCase, TestSet, ST_Hooks,
                                  int *, int *, int *, int *, int *);
static RunnerState after_set(ST_Hooks, TestSet,
                             int, int, int, int, int);
static void runner_summary(int, int, TestSet, ST_Hooks);
static int runner_done(TestSet);

// the actual test runner
int run_tests(TestSet sets, ST_Hooks test_hooks) {
   // VIRTUAL STATE: RUNNER_INIT
   int total_tests = 0;
   int set_sequence = 1;
   char timestamp[32];
   int total_sets = 0;
   ST_Hooks hooks = NULL;

   TestSet current_set_iter = sets;
   TestCase current_tc_iter = NULL;
   current_set = NULL;
   TestCase tc = NULL;

   int tc_total = 0;
   int tc_passed = 0;
   int tc_failed = 0;
   int tc_skipped = 0;

   RunnerState state = RUNNER_INIT;
   while (state != RUNNER_DONE) {
      default_ctx.info.state = state; // update default context state
      switch (state) {
      case RUNNER_INIT:
         state = runner_init(sets, test_hooks, &total_tests, &total_sets, &hooks);

         break;
      case SET_LOOP:
         state = set_loop(&current_set_iter, &set_sequence);
         if (state == RUNNER_SUMMARY) {
            break;
         }
         current_set = current_set_iter;
         state = SET_INIT;

         break;
      case SET_INIT:
         state = set_init(current_set_iter, &tc_total, &tc_passed, &tc_failed, &tc_skipped, &current_set);

         break;
      case BEFORE_SET:
         state = before_set(hooks, set_sequence, current_set, timestamp);
         current_tc_iter = current_set->cases;
         state = CASE_LOOP;

         break;
      case CASE_LOOP:
         state = case_loop(&current_tc_iter);

         break;
      case CASE_INIT:
         tc = current_tc_iter;
         state = case_init(tc, current_set);

         break;
      case BEFORE_TEST:
         state = before_test(hooks);

         break;
      case SETUP_TEST:
         state = setup_test(current_set);

         break;
      case START_TEST:
         state = start_test(hooks);

         break;
      case EXECUTE_TEST:
         state = execute_test(tc, jmpbuffer);

         break;
      case FUZZING_INIT:
         // FUZZ TEST EXECUTION
         const void *dataset;
         size_t count;
         size_t elem_size;

         switch (tc->fuzz_type) {
         case FUZZ_INT:
            dataset = fuzz_int_values;
            count = fuzz_int_count;
            elem_size = sizeof(int);

            break;
         case FUZZ_SIZE_T:
            dataset = fuzz_size_t_values;
            count = fuzz_size_t_count;
            elem_size = sizeof(size_t);

            break;
         case FUZZ_FLOAT:
            dataset = fuzz_float_values;
            count = fuzz_float_count;
            elem_size = sizeof(float);

            break;
         case FUZZ_BYTE:
            dataset = fuzz_byte_values;
            count = fuzz_byte_count;
            elem_size = sizeof(signed char);

            break;
         default:
            tc->info.result.state = FAIL;
            tc->info.result.message = strdup("Invalid FuzzType in fuzz test");
            return END_TEST; // or your next state
         }
         state = execute_fuzzing(tc, jmpbuffer, dataset, count, elem_size);

         break;
      case END_TEST:
         state = end_test(hooks);

         break;
      case TEARDOWN_TEST:
         /* Process result before teardown so the elapsed/status prints inside teardown scope */
         state = process_result(tc, current_set, hooks,
                                &tc_passed, &tc_failed, &tc_skipped,
                                &tc_total, &total_tests);
         /* advance iterator now (we will still run teardown for current test)
          * process_result returns CASE_LOOP normally but we force teardown next */
         current_tc_iter = current_tc_iter->next;
         state = teardown_test(current_set);

         break;
      case AFTER_TEST:
         state = after_test(hooks);
         default_on_testcase_finish();

         break;
      case PROCESS_RESULT:
         /* no-op: results are processed in TEARDOWN_TEST to ensure result prints before teardown
          * keep iterator advance as a fallback */
         if (current_tc_iter)
            current_tc_iter = current_tc_iter->next;
         state = CASE_LOOP;

         break;
      case AFTER_SET:
         state = after_set(hooks, current_set,
                           set_sequence, tc_total, tc_passed, tc_failed, tc_skipped);
         current_set_iter = current_set_iter->next;

         break;
      case RUNNER_SUMMARY:
         runner_summary(total_tests, total_sets, sets, test_hooks);
         state = RUNNER_DONE;

         break;
      case RUNNER_DONE:
         break;
      default:
         fwritelnf(stderr, "Error: Unknown runner state %d", state);
         state = RUNNER_DONE;
         break;
      }
   }

   return runner_done(sets);
}
// State handlers:
static RunnerState runner_init(TestSet sets, ST_Hooks test_hooks, int *total_tests_out, int *total_sets_out, ST_Hooks *hooks_out) {
   // NOTE: we are not counting tests here; present for consistency
   (void)total_tests_out;

   int total_sets = 0;
   ST_Hooks hooks = NULL;

   for (TestSet set = sets; set; set = set->next) {
      total_sets++;

      if (!test_hooks && !set->hooks) {
         if (hook_registry && hook_registry->hooks) {
            hooks = hook_registry->hooks;
         }
      } else if (test_hooks) {
         hooks = test_hooks;
      } else if (set->hooks) {
         hooks = set->hooks;
      }

      current_hooks = hooks;
      if (hooks) {
         if (!hooks->before_test)
            hooks->before_test = default_before_test;
         if (!hooks->after_test)
            hooks->after_test = default_after_test;
         if (!hooks->on_start_test)
            hooks->on_start_test = default_on_start_test;
         if (!hooks->on_end_test)
            hooks->on_end_test = default_on_end_test;
         if (!hooks->on_error)
            hooks->on_error = default_on_error;
         if (!hooks->on_test_result)
            hooks->on_test_result = default_on_test_result;
      }
   }

   *total_sets_out = total_sets;
   *hooks_out = hooks;

   return SET_LOOP;
}
static RunnerState set_loop(TestSet *set_iter, int *sequence_out) {
   if (!*set_iter) {
      return RUNNER_SUMMARY;
   }

   (*sequence_out)++;
   return SET_INIT;
}
static RunnerState set_init(TestSet current_set_iter, int *tc_total_out, int *tc_passed_out, int *tc_failed_out, int *tc_skipped_out, TestSet *current) {
   TestSet set = current_set_iter;
   *tc_total_out = 0;
   *tc_passed_out = 0;
   *tc_failed_out = 0;
   *tc_skipped_out = 0;

   if (!set->log_stream) {
      set->log_stream = stdout;
   }
   // Set current_set to the executing set for writef/debugf
   *current = set;
   (*current)->logger = (Logger)&DebugLogger;

   return BEFORE_SET;
}
static RunnerState before_set(ST_Hooks hooks, int set_sequence, TestSet current, char *timestamp) {
   hooks->context->info.logger = current->logger;
   if (hooks && hooks->before_set) {
      hooks->before_set((TsInfo)&current->info, hooks->context);
   } else {
      get_timestamp(timestamp, "%Y-%m-%d  %H:%M:%S");
      char header[128];
      snprintf(header, sizeof(header), "[%d] %-25s : %4d : %20s",
               set_sequence, current->info.name, current->info.count, timestamp);
      int pad = 80 - (int)strlen(header);
      if (pad < 0)
         pad = 0;
      fwritelnf(current->log_stream, "%s%*s", header, pad, "");
      print_sep(current->log_stream, 80);
   }

   return CASE_LOOP;
}
static RunnerState case_loop(TestCase *tc_iter) {
   if (!*tc_iter) {
      return AFTER_SET;
   }
   return CASE_INIT;
}
static RunnerState case_init(TestCase tc, TestSet set) {
   tc->info.has_next = (tc->next != NULL);
   set->current = tc; // Set current test for set_test_context
   return BEFORE_TEST;
}
static RunnerState before_test(ST_Hooks hooks) {
   hooks->context->info.logger = current_set->logger;
   if (hooks && hooks->before_test) {
      hooks->before_test(hooks->context);
   }
   return SETUP_TEST;
}
static RunnerState setup_test(TestSet set) {
   if (set->setup) {
      set->setup();
   }
   return START_TEST;
}
static RunnerState start_test(ST_Hooks hooks) {
   current_tc = current_set->current;
   hooks->context->info.logger = current_set->logger;
   if (hooks && hooks->on_start_test) {
      hooks->on_start_test(hooks->context);
   } else {
      default_on_start_test(hooks->context);
   }
   return EXECUTE_TEST;
}
static RunnerState execute_test(TestCase tc, jmp_buf jmpbuffer) {
   current_tc = tc;
   if (setjmp(jmpbuffer) == 0) {
      if (!tc->is_fuzz) {
         tc->func.test();
      } else {
         return FUZZING_INIT;
      }
   } else {
      // handle exception
   }
   return END_TEST;
}
static RunnerState execute_fuzzing(TestCase tc, jmp_buf jmpbuffer, const void *dataset, size_t count, size_t elem_size) {
   current_tc = tc;
   int failed_count = 0;
   char val_buf[64];

   for (size_t i = 0; i < count; ++i) {
      const void *input = (const char *)dataset + (i * elem_size);

      format_fuzz_value(tc->fuzz_type, input, val_buf, sizeof(val_buf));
      writef("value = %-10.3s", val_buf);

      if (setjmp(jmpbuffer) == 0) {
         tc->func.fuzz((void *)input);
         writelnf("Okay");
      } else {
         const char *msg = tc->info.result.message ? tc->info.result.message : "Unknown failure";
         writelnf("Failed:\n  - %s", msg);
         failed_count++;

         // Reset message for next iteration
         if (tc->info.result.message) {
            tc->info.result.message = NULL;
         }
      }
   }

   // Final result for the fuzz test case
   if (failed_count == 0) {
      tc->info.result.state = PASS;
      tc->info.result.message = NULL;
   } else {
      tc->info.result.state = FAIL;
      char summary[128];
      snprintf(summary, sizeof(summary),
               "%ld of %zu fuzz iterations passed", count - failed_count, count);
      tc->info.result.message = strdup(summary);
   }

   return END_TEST;
}
static RunnerState end_test(ST_Hooks hooks) {
   hooks->context->info.logger = current_set->logger;
   if (hooks && hooks->on_end_test) {
      hooks->on_end_test(hooks->context);
   }
   return TEARDOWN_TEST;
}
static RunnerState teardown_test(TestSet set) {
   if (set->teardown) {
      set->teardown();
   }
   return AFTER_TEST;
}
static RunnerState after_test(ST_Hooks hooks) {
   hooks->context->info.logger = current_set->logger;
   if (hooks && hooks->after_test) {
      hooks->after_test(hooks->context);
   }
   return CASE_LOOP;
}
static RunnerState process_result(TestCase tc, TestSet set, ST_Hooks hooks,
                                  int *tc_passed, int *tc_failed, int *tc_skipped,
                                  int *tc_total, int *total_tests) {
   // VIRTUAL STATE: PROCESS_RESULT
   if (tc->expect_fail) {
      if (tc->info.result.state == FAIL) {
         tc->info.result.state = PASS;
         if (tc->info.result.message) {
            __real_free(tc->info.result.message);
            tc->info.result.message = strdup("Expected failure occurred");
         }
      } else if (tc->info.result.state != SKIP) {
         tc->info.result.state = FAIL;
         if (tc->info.result.message)
            __real_free(tc->info.result.message);
         tc->info.result.message = strdup("Expected failure but passed");
      }
   } else if (tc->expect_throw) {
      if (tc->info.result.state == FAIL) {
         tc->info.result.state = PASS;
         if (tc->info.result.message) {
            __real_free(tc->info.result.message);
            tc->info.result.message = strdup("Expected throw occurred");
         }
      } else if (tc->info.result.state != SKIP) {
         tc->info.result.state = FAIL;
         if (tc->info.result.message)
            __real_free(tc->info.result.message);
         tc->info.result.message = strdup("Expected throw but passed");
      }
   }

   if (tc->info.result.state == PASS) {
      set->info.tc_info = (TcInfo)&tc->info;
      hooks->context->info.logger = current_set->logger;
      if (hooks && hooks->on_test_result) {
         hooks->on_test_result((TsInfo)&set->info, hooks->context);
      } else {
         set->logger->log("[PASS]\n");
      }
      (*tc_passed)++;
      set->info.passed++;
   } else if (tc->info.result.state == SKIP) {
      set->info.tc_info = (TcInfo)&tc->info;
      hooks->context->info.logger = current_set->logger;
      if (hooks && hooks->on_test_result) {
         hooks->on_test_result((TsInfo)&set->info, hooks->context);
      } else {
         set->logger->log("[SKIP]\n");
      }
      (*tc_skipped)++;
      set->info.skipped++;
   } else {
      set->info.tc_info = (TcInfo)&tc->info;
      hooks->context->info.logger = current_set->logger;
      if (hooks && hooks->on_test_result) {
         hooks->on_test_result((TsInfo)&set->info, hooks->context);
      } else {
         set->logger->log("[FAIL]\n     %s", tc->info.result.message ? tc->info.result.message : "Unknown");
      }
      (*tc_failed)++;
      set->info.failed++;
   }

   (*tc_total)++;
   (*total_tests)++;
   /* per-set summary is emitted in after_set where the set sequence is available */

   return CASE_LOOP;
}
static RunnerState after_set(ST_Hooks hooks, TestSet set,
                             int sequence, int tc_total, int tc_passed, int tc_failed, int tc_skipped) {
   hooks->context->info.logger = current_set->logger;
   if (hooks && hooks->after_set) {
      hooks->after_set((TsInfo)&current_set->info, hooks->context);
   }

   /* Emit per-set summary */
   st_summary summary = {
       .sequence = sequence,
       .tc_total = tc_total,
       .tc_passed = tc_passed,
       .tc_failed = tc_failed,
       .tc_skipped = tc_skipped,
       .total_mallocs = _sigtest_alloc_count,
       .total_frees = _sigtest_free_count};
   if (hooks && hooks->on_set_summary) {
      hooks->on_set_summary((TsInfo)&current_set->info, hooks->context, &summary);
   } else {
      print_sep(set->log_stream, 80);
      char stats[128];
      snprintf(stats, sizeof(stats), "[%d]     TESTS=%3d        PASS=%3d        FAIL=%3d        SKIP=%3d",
               sequence, tc_total, tc_passed, tc_failed, tc_skipped);
      fwritelnf(set->log_stream, "%s", stats);
   }

   /* file-local separator */
   if (false) {
      default_on_testset_finished();
   }
   if (set->cleanup) {
      set->cleanup();
   }
   return SET_LOOP;
}
static void runner_summary(int total_tests, int total_sets, TestSet set, ST_Hooks test_hooks) {
   char timestamp[32];
   get_timestamp(timestamp, "%Y-%m-%d %H:%M:%S");
   char hdr[128];
   snprintf(hdr, sizeof(hdr), "[%s]   Test Set:                    %s", timestamp, set->info.name);
   int hpad = 80 - (int)strlen(hdr);
   if (hpad < 0)
      hpad = 0;
   fwritelnf(stdout, "%s%*s", hdr, hpad, "");
   print_sep(stdout, 80);
   fwritelnf(stdout, "Tests run: %d, Passed: %d, Failed: %d, Skipped: %d",
             total_tests, set->info.passed, set->info.failed, set->info.skipped);
   fwritelnf(stdout, "Total test sets registered: %d", total_sets);
   /* Print aggregate malloc/free totals with adjusted alignment */
   fwritelnf(stdout, "Total mallocs:              %zu", _sigtest_alloc_count);
   fwritelnf(stdout, "Total frees:                %zu", _sigtest_free_count);
}
static int runner_done(TestSet set) {
   // Final cleanup if needed
   return set->info.failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

#if 1 // Region: Logging functions with formatted test ouput
static void flog_debug(DebugLevel level, FILE *stream, const char *fmt, ...) {
   if (current_set && current_set->hooks && current_set->hooks->on_debug_log) {
      va_list args;
      va_start(args, fmt);
      char buf[2048];
      vsnprintf(buf, sizeof(buf), fmt, args);
      va_end(args);
      current_set->hooks->on_debug_log(current_set->hooks->context, level, "[%s] %s", DBG_LEVELS[level], buf);
   } else {
      fprintf(stream, "[%s] ", DBG_LEVELS[level]);
      va_list args;
      va_start(args, fmt);
      vfprintf(stream, fmt, args);
      va_end(args);
      fflush(stream);
   }
}
// This function is used to write formatted messages to the log stream
void writef(const char *fmt, ...) {
   va_list args;
   va_start(args, fmt);

   FILE *stream = (current_set && current_set->log_stream) ? current_set->log_stream : stdout;
   /* If Running was printed without newline and this is the first debug output, break the line
    * so debug lines start on the next line and are indented. Also mark that debug occurred. */
   if (inside_test && current_tc->ran_no_newline && strncmp(fmt, "Running:", 8) != 0 && strncmp(fmt, "[%d]", 4) != 0 && strncmp(fmt, "=", 1) != 0) {
      fputc('\n', stream);
      current_tc->ran_no_newline = 0;
      current_tc->had_debug = 1;
   }
   if (inside_test && strncmp(fmt, "Running:", 8) != 0 && strncmp(fmt, "[%d]", 4) != 0 && strncmp(fmt, "=", 1) != 0) {
      fprintf(stream, "  - ");
   }
   vfprintf(stream, fmt, args);
   fflush(stream);

   va_end(args);
}
// This function is used to write formatted messages with a newline to the log stream
void writelnf(const char *fmt, ...) {
   va_list args;
   va_start(args, fmt);

   FILE *stream = (current_set && current_set->log_stream) ? current_set->log_stream : stdout;
   if (inside_test && current_tc->ran_no_newline && strncmp(fmt, "Running:", 8) != 0 && strncmp(fmt, "[%d]", 4) != 0 && strncmp(fmt, "=", 1) != 0) {
      fputc('\n', stream);
      current_tc->ran_no_newline = 0;
      current_tc->had_debug = 1;
   }
   if (inside_test && strncmp(fmt, "Running:", 8) != 0 && strncmp(fmt, "[%d]", 4) != 0 && strncmp(fmt, "=", 1) != 0) {
      fprintf(stream, "  - ");
   }
   vfprintf(stream, fmt, args);
   fprintf(stream, "\n");
   fflush(stream);

   va_end(args);
}
// This function is used to write formatted messages to the given stream
void fwritef(FILE *stream, const char *fmt, ...) {
   va_list args;
   va_start(args, fmt);

   stream = stream ? stream : stdout;
   vfprintf(stream, fmt, args);
   fflush(stream);

   va_end(args);
}
// This function is used to write formatted messages with a newline to the given stream
void fwritelnf(FILE *stream, const char *fmt, ...) {
   va_list args;
   va_start(args, fmt);

   stream = stream ? stream : stdout;
   vfprintf(stream, fmt, args);
   fprintf(stream, "\n");
   fflush(stream);

   va_end(args);
}
#endif

// Public global TestRunner interface
const sc_testrunner_i TestRunner = {
    .on_test_result = default_on_test_result,
    .on_start_test = default_on_start_test,
    .on_end_test = default_on_end_test,
    .before_test = default_before_test,
    .after_test = default_after_test,
};
// Public global DebugLogger interface
const st_logger_s DebugLogger = {
    .log = writelnf,
    .flog = fwritelnf,
    .debug = flog_debug};