/*
 * Sigma-Test
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
 * Description: Source file for Sigma-Test core interfaces and implementations
 */
#include "sigtest.h"
#include "internal/logging.h"
#include "internal/runner_states.h"
#include <assert.h>
#include <float.h> //	for FLT_EPSILON && DBL_EPSILON
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // 	for jmp_buf and related functions
#include <strings.h>

#define SIGMATEST_VERSION "1.00.1-pre1"

// Global test set "registry"
TestSet test_sets = NULL;
static TestSet current_set = NULL;

// Static buffer for jump
static jmp_buf jmpbuffer;

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
         __real_free(tc->name);
         if (tc->test_result.message)
            __real_free(tc->test_result.message);

         __real_free(tc);
         tc = next_tc;
      }

      // __real_free test set
      __real_free(set->name);
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

void set_test_context(TestState result, const string message) {
   if (current_set && current_set->current) {
      current_set->current->test_result.state = result;
      if (current_set->current->test_result.message) {
         __real_free(current_set->current->test_result.message);
      }
      current_set->current->test_result.message = message ? strdup(message) : NULL;
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

// Memory wrappers
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
   set->name = strdup(name);
   set->cleanup = cleanup;
   set->setup = NULL;
   set->teardown = NULL;
   set->log_stream = stdout;
   set->cases = NULL;
   set->tail = NULL;
   set->count = 0;
   set->passed = 0;
   set->failed = 0;
   set->skipped = 0;
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
   if (!set->name) {
      if (set->log_stream != stdout && set->log_stream)
         fclose(set->log_stream);
      __real_free(set);
      writelnf("Failed to allocate memory for test set name\n");
      exit(EXIT_FAILURE);
   }

   test_sets = set;
   current_set = set;
}
// Register test to test registry
void testcase(string name, TestFunc func) {
   if (!current_set) {
      testset("default", NULL, NULL);
   }

   TestCase tc = __real_malloc(sizeof(struct st_case_s));
   if (!tc) {
      writef("Failed to allocate memory for test case `%s`\n", name);
      exit(EXIT_FAILURE);
   }
   tc->name = strdup(name);
   if (!tc->name) {
      writef("Failed to allocate memory for test case name `%s`\n", name);
      __real_free(tc);

      exit(EXIT_FAILURE); // cleanup_test_sets will handle freeing
   }
   tc->test_func = func;
   tc->expect_fail = FALSE;
   tc->expect_throw = FALSE;
   tc->test_result.state = PASS;
   tc->test_result.message = NULL;
   tc->next = NULL;

   if (!current_set->cases) {
      current_set->cases = tc;
      current_set->tail = tc;
   } else {
      current_set->tail->next = tc;
      current_set->tail = tc;
   }

   current_set->count++;
}
// Register test to test registry with expectation to fail
void fail_testcase(string name, void (*func)(void)) {
   if (!current_set) {
      testset("default", NULL, NULL);
   }

   TestCase tc = __real_malloc(sizeof(struct st_case_s));
   if (!tc) {
      writef("Failed to allocate memory for test case `%s`\n", name);
      exit(EXIT_FAILURE);
   }
   tc->name = strdup(name);
   if (!tc->name) {
      writef("Failed to allocate memory for test case name `%s`\n", name);
      __real_free(tc);

      exit(EXIT_FAILURE); // cleanup_test_sets will handle freeing
   }
   tc->test_func = func;
   tc->expect_fail = TRUE;
   tc->expect_throw = FALSE;
   tc->test_result.state = PASS; // Set to PASS initially, evaluated in main
   tc->test_result.message = NULL;
   tc->next = NULL;

   if (!current_set->cases) {
      current_set->cases = tc;
      current_set->tail = tc;
   } else {
      current_set->tail->next = tc;
      current_set->tail = tc;
   }

   current_set->count++;
}
// Register test to test registry with expectation to throw
void testcase_throws(string name, void (*func)(void)) {
   if (!current_set) {
      testset("default", NULL, NULL);
   }

   TestCase tc = __real_malloc(sizeof(struct st_case_s));
   if (!tc) {
      writef("Failed to allocate memory for test case `%s`\n", name);
      exit(EXIT_FAILURE);
   }
   tc->name = strdup(name);
   if (!tc->name) {
      writef("Failed to allocate memory for test case name `%s`\n", name);
      __real_free(tc);

      exit(EXIT_FAILURE); // cleanup_test_sets will handle freeing
   }
   tc->test_func = func;
   tc->expect_fail = FALSE;
   tc->expect_throw = TRUE;
   tc->test_result.state = PASS; // Set to PASS initially, evaluated in main
   tc->test_result.message = NULL;
   tc->next = NULL;

   if (!current_set->cases) {
      current_set->cases = tc;
      current_set->tail = tc;
   } else {
      current_set->tail->next = tc;
      current_set->tail = tc;
   }

   current_set->count++;
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

   if (current_set && !current_set->hooks) {
      current_set->hooks = hooks;
   }
}
//	default test hooks
static void default_before_test(object context) {
   struct
   {
      int count;
      int verbose;
      ts_time start;
      ts_time end;
   } *ctx = context;

   ctx->count++;
}
static void default_on_start_test(object context) {
   struct
   {
      int count;
      int verbose;
      ts_time start;
      ts_time end;
   } *ctx = context;

   if (sys_gettime(&ctx->start) == -1) {
      fwritelnf(stderr, "Error: Failed to get system start time");
      exit(EXIT_FAILURE);
   }
   // zero out the end time
   ctx->end = (ts_time){0, 0};

   TestCase tc = current_set ? current_set->current : NULL;
   Logger logger = current_set ? current_set->logger : NULL;

   (void)set_started; /* header is printed in before_set */
   if (tc) {
      inside_test = 1;
      /* Print Running without newline so result can be appended on same line if no debug output */
      char running_buf[128];
      int len = snprintf(running_buf, sizeof(running_buf), "Running: %-*s", 40, tc->name);
      FILE *stream = (current_set && current_set->log_stream) ? current_set->log_stream : stdout;
      fwritef(stream, "%s", running_buf);
      /* record that we've written Running without newline */
      ((struct { int count; int verbose; ts_time start; ts_time end; RunnerState state; int ran_no_newline; int had_debug; int running_len; } *)context)->ran_no_newline = 1;
      ((struct { int count; int verbose; ts_time start; ts_time end; RunnerState state; int ran_no_newline; int had_debug; int running_len; } *)context)->had_debug = 0;
      ((struct { int count; int verbose; ts_time start; ts_time end; RunnerState state; int ran_no_newline; int had_debug; int running_len; } *)context)->running_len = len;
   }
}
static void default_on_end_test(object context) {
   struct
   {
      int count;
      int verbose;
      ts_time start;
      ts_time end;
   } *ctx = context;

   if (sys_gettime(&ctx->end) == -1) {
      fwritelnf(stderr, "Error: Failed to get system end time");
      exit(EXIT_FAILURE);
   }

   inside_test = 0;
   /* end time recorded; final result printing occurs in on_test_result after process_result */
}
static void default_after_test(object context) {
   struct
   {
      int count;
      int verbose;
      ts_time start;
      ts_time end;
      RunnerState state;
   } *ctx = context;

   ctx->count--;
}
static void default_on_test_result(const TestSet set, const TestCase tc, object context) {
   struct
   {
      int count;
      int verbose;
      ts_time start;
      ts_time end;
      RunnerState state;
      int ran_no_newline;
      int had_debug;
      int running_len;
   } *ctx = context;

   if (!set || !tc || !context)
      return;

   double elapsed = get_elapsed_ms(&ctx->start, &ctx->end);
   const char *state_str = TEST_STATES[tc->test_result.state];

   char result_buf[64];
   snprintf(result_buf, sizeof(result_buf), "%.3f µs [%s]", elapsed, state_str);

   if (!ctx->had_debug && ctx->ran_no_newline) {
      /* No debug output occurred; append result to the Running line. Use stored running_len. */
      int pad = 80 - (ctx->running_len + (int)strlen(result_buf));
      if (pad < 0)
         pad = 0;
      /* print padding and result, then newline */
      for (int i = 0; i < pad; ++i)
         fputc(' ', set->log_stream);
      fprintf(set->log_stream, "%s\n", result_buf);
   } else {
      if (tc->test_result.state == FAIL && tc->test_result.message) {
         /* print failure message indented */
         fwritelnf(set->log_stream, "  - %s", tc->test_result.message);
      }
      /* right-justify result at column 80 on its own line */
      int pad = 80 - (int)strlen(result_buf);
      if (pad < 0)
         pad = 0;
      fwritelnf(set->log_stream, "%*s%s", pad, "", result_buf);
   }

      /* reset flags for next test */
   ctx->ran_no_newline = 0;
   ctx->had_debug = 0;
   ctx->running_len = 0;
}
static void default_on_error(const char *message, object context) {
   struct {
      int count;
      int verbose;
      ts_time start;
      ts_time end;
      RunnerState state;
   } *ctx = context;

   if (ctx->verbose && current_set) {
      current_set->logger->log("Error in test [%s]: %s\n", current_set->current->name, message);
   }
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
static struct {
   int count;
   int verbose;
   ts_time start;
   ts_time end;
   RunnerState state;
   int ran_no_newline;
   int had_debug;
   int running_len;
} default_ctx = {0, 0, {0, 0}, {0, 0}, RUNNER_IDLE};
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
   int retResult = run_tests(test_sets, NULL);
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
static RunnerState end_test(ST_Hooks);
static RunnerState teardown_test(TestSet);
static RunnerState after_test(ST_Hooks);
static RunnerState process_result(TestCase, TestSet, ST_Hooks,
                                  int *, int *, int *, int *, int *);
static RunnerState after_set(ST_Hooks, TestSet,
                             int, int, int, int, int);
static void runner_summary(int, int, TestSet);
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
      default_ctx.state = state; // update default context state
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
         runner_summary(total_tests, total_sets, sets);
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
   if (hooks && hooks->before_set) {
      hooks->before_set(current, hooks->context);
   } else {
      get_timestamp(timestamp, "%Y-%m-%d  %H:%M:%S");
      char header[128];
      snprintf(header, sizeof(header), "[%d] %-25s : %4d : %20s",
               set_sequence, current->name, current->count, timestamp);
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
   set->current = tc; // Set current test for set_test_context
   return BEFORE_TEST;
}
static RunnerState before_test(ST_Hooks hooks) {
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
   if (hooks && hooks->on_start_test) {
      hooks->on_start_test(hooks->context);
   } else {
      default_hooks.on_start_test(hooks->context);
   }
   return EXECUTE_TEST;
}
static RunnerState execute_test(TestCase tc, jmp_buf jmpbuffer) {
   if (setjmp(jmpbuffer) == 0) {
      tc->test_func();
   } else {
      // handle exception
   }
   return END_TEST;
}
static RunnerState end_test(ST_Hooks hooks) {
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

   if (tc->test_result.state == PASS) {
      if (hooks && hooks->on_test_result) {
         hooks->on_test_result(set, tc, hooks->context);
      } else {
         set->logger->log("[PASS]\n");
      }
      (*tc_passed)++;
      set->passed++;
   } else if (tc->test_result.state == SKIP) {
      if (hooks && hooks->on_test_result) {
         hooks->on_test_result(set, tc, hooks->context);
      } else {
         set->logger->log("[SKIP]\n");
      }
      (*tc_skipped)++;
      set->skipped++;
   } else {
      if (hooks && hooks->on_test_result) {
         hooks->on_test_result(set, tc, hooks->context);
      } else {
         set->logger->log("[FAIL]\n     %s", tc->test_result.message ? tc->test_result.message : "Unknown");
      }
      (*tc_failed)++;
      set->failed++;
   }

   (*tc_total)++;
   (*total_tests)++;
   /* per-set summary is emitted in after_set where the set sequence is available */

   return CASE_LOOP;
}
static RunnerState after_set(ST_Hooks hooks, TestSet set,
                             int sequence, int tc_total, int tc_passed, int tc_failed, int tc_skipped) {
   if (hooks && hooks->after_set) {
      hooks->after_set(current_set, hooks->context);
   }

   /* Emit per-set summary (use provided sequence and counters) */
   print_sep(set->log_stream, 80);
   char stats[128];
   snprintf(stats, sizeof(stats), "[%d]     TESTS=%3d        PASS=%3d        FAIL=%3d        SKIP=%3d",
            sequence, tc_total, tc_passed, tc_failed, tc_skipped);
   fwritelnf(set->log_stream, "%s", stats);

   /* file-local separator */
   default_on_testset_finished();
   if (set->cleanup) {
      set->cleanup();
   }
   return SET_LOOP;
}
static void runner_summary(int total_tests, int total_sets, TestSet set) {
   char timestamp[32];
   get_timestamp(timestamp, "%Y-%m-%d %H:%M:%S");
   char hdr[128];
   snprintf(hdr, sizeof(hdr), "[%s]   Test Set:                    %s", timestamp, set->name);
   int hpad = 80 - (int)strlen(hdr);
   if (hpad < 0)
      hpad = 0;
   fwritelnf(stdout, "%s%*s", hdr, hpad, "");
   print_sep(stdout, 80);
   fwritelnf(stdout, "Tests run: %d, Passed: %d, Failed: %d, Skipped: %d",
             total_tests, set->passed, set->failed, set->skipped);
   fwritelnf(stdout, "Total test sets registered: %d", total_sets);
   /* Print aggregate malloc/free totals with adjusted alignment */
   fwritelnf(stdout, "Total mallocs:              %zu", _sigtest_alloc_count);
   fwritelnf(stdout, "Total frees:                %zu", _sigtest_free_count);
}
static int runner_done(TestSet set) {
   // Final cleanup if needed
   return set->failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

#if 1 // Region: Logging functions with formatted test ouput
static void flog_debug(DebugLevel level, FILE *stream, const char *fmt, ...) {
   va_list args;
   va_start(args, fmt);

   fprintf(stream, "[%s] ", DBG_LEVELS[level]);
   vfprintf(stream, fmt, args);
   fflush(stream);

   va_end(args);
}
// This function is used to write formatted messages to the log stream
void writef(const char *fmt, ...) {
   va_list args;
   va_start(args, fmt);

   FILE *stream = (current_set && current_set->log_stream) ? current_set->log_stream : stdout;
   /* If Running was printed without newline and this is the first debug output, break the line
    * so debug lines start on the next line and are indented. Also mark that debug occurred. */
   if (inside_test && default_ctx.ran_no_newline && strncmp(fmt, "Running:", 8) != 0 && strncmp(fmt, "[%d]", 4) != 0 && strncmp(fmt, "=", 1) != 0) {
      fputc('\n', stream);
      default_ctx.ran_no_newline = 0;
      default_ctx.had_debug = 1;
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
   if (inside_test && default_ctx.ran_no_newline && strncmp(fmt, "Running:", 8) != 0 && strncmp(fmt, "[%d]", 4) != 0 && strncmp(fmt, "=", 1) != 0) {
      fputc('\n', stream);
      default_ctx.ran_no_newline = 0;
      default_ctx.had_debug = 1;
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
const st_logger_i DebugLogger = {
    .log = writelnf,
    .flog = fwritelnf,
    .debug = flog_debug};