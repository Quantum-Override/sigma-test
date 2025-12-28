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
 * File: json_hooks.c
 * Description: Source file for JSON output hooks for SigmaTest
 */
#include "hooks/json_hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
   Test hooks for custom (JSON) output formatting.asm

   David Boarman
 */

void json_on_set_summary(const TsInfo set, tc_context *context, st_summary *summary);

extern double get_elapsed_ms(ts_time *, ts_time *);
extern int sys_gettime(ts_time *);
extern size_t _sigtest_alloc_count;
extern size_t _sigtest_free_count;

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
    .on_set_summary = json_on_set_summary,
    .context = NULL,
};

void json_before_set(const TsInfo set, tc_context *context) {
   struct JsonHookContext *ctx = (struct JsonHookContext *)context;
   ctx->set = set; // Store set for use in other hooks

   char timestamp[32];
   get_timestamp(timestamp, "%Y-%m-%d %H:%M:%S");
   context->info.logger->log("{\n");
   context->info.logger->log("  \"test_set\": \"%s\",\n", set->name);
   context->info.logger->log("  \"timestamp\": \"%s\",\n", timestamp);
   context->info.logger->log("  \"tests\": [\n");
}
void json_after_set(const TsInfo set, tc_context *context) {
   context->info.logger->log("  ],\n");
   context->info.logger->log("  \"summary\": {\n");
   context->info.logger->log("    \"total\": %d,\n", set->count);
   context->info.logger->log("    \"passed\": %d,\n", set->passed);
   context->info.logger->log("    \"failed\": %d,\n", set->failed);
   context->info.logger->log("    \"skipped\": %d,\n", set->skipped);
   context->info.logger->log("    \"total_mallocs\": %zu,\n", _sigtest_alloc_count);
   context->info.logger->log("    \"total_frees\": %zu\n", _sigtest_free_count);
   context->info.logger->log("  }\n");
   context->info.logger->log("}\n");
}
void json_before_test(tc_context *context) {
   // Placeholder for any setup before each test
}
void json_after_test(tc_context *context) {
   // Placeholder for any cleanup after each test
}
void json_on_start_test(tc_context *context) {
   struct JsonHookContext *ctx = (struct JsonHookContext *)context;

   ctx->info.end.tv_sec = 0;
   ctx->info.end.tv_nsec = 0;

   if (sys_gettime(&ctx->info.start) == -1) {
      DebugLogger.flog(stderr, "Error: Failed to get system start time");
      exit(EXIT_FAILURE);
   }

   if (ctx->info.verbose && ctx->set) {
      context->info.logger->log("    \"start_test\": \"%s\",\n", ctx->set->tc_info->name);
   }
}
void json_on_end_test(tc_context *context) {
   struct JsonHookContext *ctx = (struct JsonHookContext *)context;

   if (sys_gettime(&ctx->info.end) == -1) {
      DebugLogger.flog(stderr, "Error: Failed to get system end time");
      exit(EXIT_FAILURE);
   }

   if (ctx->info.verbose && ctx->set) {
      context->info.logger->log("    \"end_test\": \"%s\",\n", ctx->set->tc_info->name);
   }
}
void json_on_error(const char *message, tc_context *context) {
   struct JsonHookContext *ctx = (struct JsonHookContext *)context;

   if (ctx->info.verbose && ctx->set) {
      char escaped[512];
      char *dst = escaped;
      for (const char *src = message; *src && dst < escaped + sizeof(escaped) - 2; src++) {
         if (*src == '"')
            *dst++ = '\\';
         *dst++ = *src;
      }
      *dst = '\0';
      context->info.logger->log("    \"error\": \"%s\",\n", escaped);
   }
}
void json_on_test_result(const TsInfo set, tc_context *context) {
   struct JsonHookContext *ctx = (struct JsonHookContext *)context;

   // get test state label
   const char *status = NULL;
   switch (set->tc_info->result.state) {
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
   double elapsed_ms = get_elapsed_ms(&ctx->info.start, &ctx->info.end);

   char message[256];
   snprintf(message, sizeof(message), "%s", set->tc_info->result.message ? set->tc_info->result.message : "");
   char escaped_message[512];
   char *dst = escaped_message;
   for (const char *src = message; *src && dst < escaped_message + sizeof(escaped_message) - 2; src++) {
      if (*src == '"') {
         *dst++ = '\\';
         *dst++ = '"';
      } else if (*src == '\n') {
         *dst++ = '\\';
         *dst++ = 'n';
      } else {
         *dst++ = *src;
      }
   }
   *dst = '\0';

   context->info.logger->log("    {\n");
   context->info.logger->log("      \"test\": \"%s\",\n", set->tc_info->name);
   context->info.logger->log("      \"status\": \"%s\",\n", status);
   context->info.logger->log("      \"duration_us\": %.3f,\n", elapsed_ms * 1000.0);
   context->info.logger->log("      \"message\": \"%s\"\n", escaped_message);
   context->info.logger->log("    }%s\n", set->tc_info->has_next ? "," : "");
}

void json_on_set_summary(const TsInfo set, tc_context *context, st_summary *summary) {
   (void)set; // unused
   (void)context; // unused
   (void)summary; // unused
   // Summary is handled in JSON output in after_set
}