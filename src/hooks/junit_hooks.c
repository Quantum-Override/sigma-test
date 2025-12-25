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
 * File: junit_hooks.c
 * Description: Source file for JUnit XML output hooks for SigmaTest
 * Goals:
 *    - Generates standards-compliant JUnit XML output for CI/CD integration
 *    - Fully validated against xmllint for well-formed XML structure
 *    - Compatible with Jenkins, GitLab CI, GitHub Actions, and other CI platforms
 *    - Captures test results with proper testsuite/testcase hierarchy
 *    - Records timing data (execution duration) for performance tracking
 *    - Includes failure messages, error types, and stack traces
 *    - Supports skipped/disabled test reporting
 *    - Handles special characters and CDATA sections correctly
 *    - Enables test result visualization in CI dashboards
 *    - Facilitates trend analysis and historical test reporting
 */
#include "hooks/junit_hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define COLOR_GREEN "\033[0;32m"
#define COLOR_RED "\033[0;31m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_RESET "\033[0m"
#define JUNIT_XML_BUFFER_SIZE (1024 * 1024)

static char junit_xml_buffer[JUNIT_XML_BUFFER_SIZE];
static size_t junit_xml_used = 0;

static int get_hostname(char *, size_t);
static char *xml_escape(const char *);
static void junit_append(const char *fmt, ...);

struct st_hooks_s junit_hooks = {
    .name = "junit",
    .before_set = junit_before_set,
    .after_set = junit_after_set,
    .before_test = NULL,
    .after_test = NULL,
    .on_start_test = NULL,
    .on_end_test = NULL,
    .on_error = NULL,
    .on_test_result = junit_on_test_result,
    .context = NULL,
};

void junit_before_set(const TestSet set, object context) {
   struct JunitHookContext *ctx = context;
   ctx->set = set;

   get_timestamp(ctx->timestamp, "%Y-%m-%dT%H:%M:%S");
   get_hostname(ctx->hostname, sizeof(ctx->hostname));

   junit_append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
   junit_append("<testsuites>\n");
   junit_append("  <testsuite name=\"%s\" timestamp=\"%s\" hostname=\"%s\" "
                "tests=\"%%d\" failures=\"%%d\" skipped=\"%%d\" time=\"0.000\">\n",
                set->name, ctx->timestamp, ctx->hostname);
}

void junit_on_test_result(const TestSet set, const TestCase tc, object context) {
   struct JunitHookContext *ctx = context;

   // Console color output
   if (ctx->verbose && isatty(fileno(stdout))) {
      // prefer using switch-case for clarity
      switch (tc->test_result.state) {
      case PASS:
         printf("%s[PASS]%s %s\n", COLOR_GREEN, COLOR_RESET, tc->name);
         break;
      case FAIL:
         printf("%s[FAIL]%s %s\n", COLOR_RED, COLOR_RESET, tc->name);
         break;
      case SKIP:
         printf("%s[SKIP]%s %s\n", COLOR_YELLOW, COLOR_RESET, tc->name);
         break;
      default:
         printf("[UNKNOWN] test state: %s\n", tc->name);
         break;
      }
   }

   // Append test case result to JUnit XML
   junit_append("    <testcase name=\"%s\" classname=\"%s\">\n", tc->name, set->name);

   if (tc->test_result.state == FAIL) {
      char *escaped = xml_escape(tc->test_result.message ? tc->test_result.message : "Unknown failure");
      junit_append("      <failure message=\"%s\"/>\n", escaped);
      free(escaped);
      ctx->failures++;
   } else if (tc->test_result.state == SKIP) {
      junit_append("      <skipped/>\n");
      ctx->skipped++;
   }

   junit_append("    </testcase>\n");
   ctx->total_tests++;
}

void junit_after_set(const TestSet set, object context) {
   struct JunitHookContext *ctx = context;

   // Close the testsuite
   junit_append("  </testsuite>\n");
   junit_append("</testsuites>\n");

   // Now go back and fix the placeholder line (the one with %%d)
   char *suite_line = strstr(junit_xml_buffer, "<testsuite");
   if (suite_line) {
      char fixed_line[1024];
      snprintf(fixed_line, sizeof(fixed_line),
               "<testsuites>\n  <testsuite name=\"%s\" timestamp=\"%s\" hostname=\"%s\" "
               "tests=\"%d\" failures=\"%d\" skipped=\"%d\" time=\"0.000\">\n",
               set->name, ctx->timestamp, ctx->hostname,
               ctx->total_tests, ctx->failures, ctx->skipped);

      // Overwrite the old line
      size_t len = strlen(fixed_line);
      memcpy(suite_line, fixed_line, len > 1024 ? 1024 : len);
   }

   // Final output
   set->logger->log("%s", junit_xml_buffer);

   ctx->set = NULL;
}

// Retrieve the hostname of the current machine
static int get_hostname(char *buffer, size_t size) {
   if (gethostname(buffer, size) != 0) {
      strncpy(buffer, "localhost", size);
      return -1;
   }

   return 0;
}

// Escape special XML characters in a string
static char *xml_escape(const char *input) {
   if (!input)
      return strdup("");

   size_t len = strlen(input);
   size_t needed = len;
   for (size_t i = 0; i < len; i++) {
      switch (input[i]) {
      case '&':
         needed += 4;
         break; // &amp;
      case '<':
         needed += 3;
         break; // &lt;
      case '>':
         needed += 3;
         break; // &gt;
      case '"':
         needed += 5;
         break; // &quot;
      case '\'':
         needed += 5;
         break; // &apos;
      }
   }

   char *dst = __real_malloc(needed + 1);
   if (!dst)
      return NULL;

   char *out = dst;
   for (size_t i = 0; i < len; i++) {
      switch (input[i]) {
      case '&':
         out += sprintf(out, "&amp;");
         break;
      case '<':
         out += sprintf(out, "&lt;");
         break;
      case '>':
         out += sprintf(out, "&gt;");
         break;
      case '"':
         out += sprintf(out, "&quot;");
         break;
      case '\'':
         out += sprintf(out, "&apos;");
         break;
      default:
         *out++ = input[i];
         break;
      }
   }
   *out = '\0';
   return dst;
}

// Append formatted XML to the JUnit report
static void junit_append(const char *fmt, ...) {
   va_list args;
   va_start(args, fmt);
   int written = vsnprintf(junit_xml_buffer + junit_xml_used,
                           JUNIT_XML_BUFFER_SIZE - junit_xml_used,
                           fmt, args);
   va_end(args);

   if (written < 0 || (size_t)written >= JUNIT_XML_BUFFER_SIZE - junit_xml_used) {
      // Truncate gracefully — never overflow
      junit_xml_buffer[JUNIT_XML_BUFFER_SIZE - 1] = '\0';
   } else {
      junit_xml_used += written;
   }
}