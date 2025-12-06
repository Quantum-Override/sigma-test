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
 * File: json_hooks.c
 * Description: Source file for JSON output hooks for Sigma-Test
 */
#include "hooks/json_hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
   Test hooks for custom (JSON) output formatting.asm

   David Boarman
 */

extern double get_elapsed_ms(ts_time *, ts_time *);
extern int sys_gettime(ts_time *);

struct st_hooks_s json_hooks = {
    .name = "json",
    .before_set = json_before_set,
    .after_set = json_after_set,
    .before_test = json_before_test,
    .after_test = json_after_test,
    .on_start_test = json_on_start_test,
    .on_end_test = json_on_end_test,
    .on_error = json_on_error,
    .on_test_result = json_on_test_result,
    .context = NULL,
};

void json_before_set(const TestSet set, object context) {
  struct JsonHookContext *ctx = context;
  ctx->set = set; // Store set for use in other hooks

  char timestamp[32];
  get_timestamp(timestamp, "%Y-%m-%d %H:%M:%S");
  set->logger->log("{\n");
  set->logger->log("  \"test_set\": \"%s\",\n", set->name);
  set->logger->log("  \"timestamp\": \"%s\",\n", timestamp);
  set->logger->log("  \"tests\": [\n");
}
void json_after_set(const TestSet set, object context) {
  set->logger->log("  ],\n");
  set->logger->log("  \"summary\": {\n");
  set->logger->log("    \"total\": %d,\n", set->count);
  set->logger->log("    \"passed\": %d,\n", set->passed);
  set->logger->log("    \"failed\": %d,\n", set->failed);
  set->logger->log("    \"skipped\": %d\n", set->skipped);
  set->logger->log("  }\n");
  set->logger->log("}\n");
}
void json_before_test(object context) {
  // Placeholder for any setup before each test
}
void json_after_test(object context) {
  // Placeholder for any cleanup after each test
}
void json_on_start_test(object context) {
  struct JsonHookContext *ctx = context;

  ctx->end.tv_sec = 0;
  ctx->end.tv_nsec = 0;

  if (sys_gettime(&ctx->start) == -1) {
    fwritelnf(stderr, "Error: Failed to get system start time");
    exit(EXIT_FAILURE);
  }

  if (ctx->verbose && ctx->set) {
    ctx->set->logger->log("    \"start_test\": \"%s\",\n", ctx->set->current->name);
  }
}
void json_on_end_test(object context) {
  struct JsonHookContext *ctx = context;

  if (sys_gettime(&ctx->end) == -1) {
    fwritelnf(stderr, "Error: Failed to get system end time");
    exit(EXIT_FAILURE);
  }

  if (ctx->verbose && ctx->set) {
    ctx->set->logger->log("    \"end_test\": \"%s\",\n", ctx->set->current->name);
  }
}
void json_on_error(const char *message, object context) {
  struct JsonHookContext *ctx = context;

  if (ctx->verbose && ctx->set) {
    char escaped[512];
    char *dst = escaped;
    for (const char *src = message; *src && dst < escaped + sizeof(escaped) - 2; src++) {
      if (*src == '"')
        *dst++ = '\\';
      *dst++ = *src;
    }
    *dst = '\0';
    ctx->set->logger->log("    \"error\": \"%s\",\n", escaped);
  }
}
void json_on_test_result(const TestSet set, const TestCase tc, object context) {
  struct JsonHookContext *ctx = context;

  // get test state label
  const char *status = NULL;
  switch (tc->test_result.state) {
  case PASS:
    status = "PASS";
    break;
  case FAIL:
    status = "FAIL";
    break;
  case SKIP:
    status = "SKIP";
    break;
  default:
    status = "UNKNOWN";
    break;
  }

  // Output test result in JSON format
  double elapsed_ms = get_elapsed_ms(&ctx->start, &ctx->end);
  char duration_str[16];
  if (elapsed_ms < 0.0001)
    snprintf(duration_str, sizeof(duration_str), "< 0.1");
  else
    snprintf(duration_str, sizeof(duration_str), "%.3f", elapsed_ms * 1000.0);

  char message[256];
  snprintf(message, sizeof(message), "%s", tc->test_result.message ? tc->test_result.message : "");
  char escaped_message[512];
  char *dst = escaped_message;
  for (const char *src = message; *src && dst < escaped_message + sizeof(escaped_message) - 2; src++) {
    if (*src == '"')
      *dst++ = '\\';
    *dst++ = *src;
  }
  *dst = '\0';

  set->logger->log("    {\n");
  set->logger->log("      \"test\": \"%s\",\n", tc->name);
  set->logger->log("      \"status\": \"%s\",\n", status);
  set->logger->log("      \"duration_us\": \"%s\",\n", duration_str);
  set->logger->log("      \"message\": \"%s\"\n", escaped_message);
  set->logger->log("    }%s\n", tc->next ? "," : "");
}