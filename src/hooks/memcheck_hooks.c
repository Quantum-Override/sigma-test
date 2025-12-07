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
 * File: memcheck_hooks.c
 * Description: Source file for memory check hooks for Sigma-Test
 *    - Provides memory allocation tracking and leak detection
 *    - Integrates with Sigma-Test's test framework
 */
#include "hooks/memory_hooks.h"
#include "sigtest.h"
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TRACKED 100000
#define MAX_BT 32

//  forwards
static void memcheck_on_alloc(size_t, object, object);
static void memcheck_on_free(object, object);
static void memcheck_before_set(const TestSet, object);
static void memcheck_after_set(const TestSet, object);
static void memcheck_on_end_test(object);

// Hook registration
struct st_hooks_s memcheck_hooks = {
    .name = "memcheck",
    .before_set = memcheck_before_set,
    .after_set = memcheck_after_set,
    .on_end_test = memcheck_on_end_test,
    .on_memory_alloc = memcheck_on_alloc,
    .on_memory_free = memcheck_on_free,
    .on_test_result = NULL,
    .context = NULL,
};

// === Hook Callbacks (called from sigtest.c wrappers) ===
void memcheck_on_alloc(size_t size, void *ptr, object context) {
  struct MemCheckContext *ctx = context;
  if (!MemCheck.isEnabled() || !ctx || !ctx->set)
    return;

  // Grow buffer
  if (ctx->count >= ctx->cap) {
    ctx->cap = ctx->cap ? ctx->cap * 2 : 1024;
    ctx->leaks = __real_realloc(ctx->leaks, ctx->cap * sizeof(Leak));
  }

  // Store leak
  ctx->leaks[ctx->count].ptr = ptr;
  ctx->leaks[ctx->count].size = size;
  ctx->leaks[ctx->count].symbols = NULL;
  ctx->leaks[ctx->count].frames = 0;

  if (ctx->backtraces_enabled) {
    void *buffer[MAX_BT];
    int frames = backtrace(buffer, MAX_BT);
    ctx->leaks[ctx->count].symbols = backtrace_symbols(buffer, frames);
    ctx->leaks[ctx->count].frames = frames;
  }

  ctx->current_bytes += size;
  if (ctx->current_bytes > ctx->peak_bytes)
    ctx->peak_bytes = ctx->current_bytes;

  ctx->count++;
}

void memcheck_on_free(void *ptr, object context) {
  struct MemCheckContext *ctx = context;
  if (!MemCheck.isEnabled() || !ctx || !ctx->set || !ptr)
    return;

  for (size_t i = 0; i < ctx->count; i++) {
    if (ctx->leaks[i].ptr == ptr) {
      ctx->current_bytes -= ctx->leaks[i].size;
      if (ctx->leaks[i].symbols)
        free(ctx->leaks[i].symbols);
      // Remove by swap
      ctx->leaks[i] = ctx->leaks[--ctx->count];
      break;
    }
  }
}

// === Hook Lifecycle ===
static void memcheck_before_set(const TestSet set, object context) {
  writelnf("MemCheck (v0.0.1 Experimental) — enabled for '%s'", set->name);
  writelnf("=================================================================");

  struct MemCheckContext *ctx = context;
  ctx->set = set;
  ctx->count = 0;
  ctx->current_bytes = 0;
  ctx->peak_bytes = 0;
  ctx->backtraces_enabled = 0;
  ctx->verbose = 1;
}

static void memcheck_after_set(const TestSet set, object context) {
  struct MemCheckContext *ctx = context;
  ctx->set = NULL;

  // Free any leftover leaks array
  if (ctx->leaks) {
    for (size_t i = 0; i < ctx->count; i++)
      if (ctx->leaks[i].symbols)
        free(ctx->leaks[i].symbols);
    free(ctx->leaks);
    ctx->leaks = NULL;
    ctx->cap = 0;
  }
}

static void memcheck_on_end_test(object context) {
  struct MemCheckContext *ctx = context;
  if (!ctx || !ctx->set || ctx->count == 0)
    return;

  TestCase tc = ctx->set->current;
  tc->test_result.state = FAIL;

  fwritelnf(ctx->set->log_stream, "MemCheck: %zu leaked block(s) (%zu bytes)", ctx->count, ctx->current_bytes);

  // Feature 2: Backtrace (first leak only)
  if (ctx->backtraces_enabled && ctx->count > 0) {
    fprintf(ctx->set->log_stream, "\n--- MemCheck Leak Backtrace (first) ---\n");
    backtrace_symbols_fd((void *const *)ctx->leaks[0].symbols, ctx->leaks[0].frames, fileno(ctx->set->log_stream));
    fprintf(ctx->set->log_stream, "-----------------------------------------\n\n");
  }
  TestRunner.on_end_test(context);
}

// === MemCheck Interface ===
void memcheck_init(int verbose) {
  struct MemCheckContext *ctx = malloc(sizeof(struct MemCheckContext));
  if (!ctx) {
    fwritelnf(stderr, "MemCheck: Failed to allocate context");
    exit(EXIT_FAILURE);
  }
  ctx->enabled = 0;
  ctx->set = NULL;
  ctx->leaks = NULL;
  ctx->count = 0;
  ctx->cap = 0;
  ctx->current_bytes = 0;
  ctx->peak_bytes = 0;
  ctx->backtraces_enabled = 0;
  ctx->verbose = verbose;

  memcheck_hooks.context = ctx;
  memcheck_hooks.on_test_result = TestRunner.on_test_result;

  register_hooks((ST_Hooks)&memcheck_hooks);
}
void memcheck_enable(void) {
  struct MemCheckContext *ctx = memcheck_hooks.context;
  if (ctx)
    ctx->enabled = 1;
}
void memcheck_disable(void) {
  struct MemCheckContext *ctx = memcheck_hooks.context;
  if (ctx)
    ctx->enabled = 0; // Disable
}
int memcheck_isEnabled(void) {
  struct MemCheckContext *ctx = memcheck_hooks.context;
  return ctx && ctx->enabled;
}
int memcheck_leakedBlocks(void) {
  struct MemCheckContext *ctx = memcheck_hooks.context;
  return ctx ? ctx->count : 0;
}
size_t memcheck_leakedBytes(void) {
  struct MemCheckContext *ctx = memcheck_hooks.context;
  return ctx ? ctx->current_bytes : 0;
}
size_t memcheck_peakBytes(void) {
  struct MemCheckContext *ctx = memcheck_hooks.context;
  return ctx ? ctx->peak_bytes : 0;
}
void memcheck_enableBacktraces(int enable) {
  struct MemCheckContext *ctx = memcheck_hooks.context;
  if (ctx)
    ctx->backtraces_enabled = enable;
}
void memcheck_printHistogram(FILE *stream) {
  struct MemCheckContext *ctx = memcheck_hooks.context;
  if (!ctx || ctx->count == 0)
    return; // Nothing to print

  fprintf(stream, "MemCheck Allocation Histogram:\n");
  size_t bins[10] = {0};
  for (size_t i = 0; i < ctx->count; i++) {
    size_t size = ctx->leaks[i].size;
    if (size < 16)
      bins[0]++;
    else if (size < 32)
      bins[1]++;
    else if (size < 64)
      bins[2]++;
    else if (size < 128)
      bins[3]++;
    else if (size < 256)
      bins[4]++;
    else if (size < 512)
      bins[5]++;
    else if (size < 1024)
      bins[6]++;
    else if (size < 2048)
      bins[7]++;
    else if (size < 4096)
      bins[8]++;
    else
      bins[9]++;
  }
  fprintf(stream, "  <16B     : %zu\n", bins[0]);
  fprintf(stream, "  16-31B   : %zu\n", bins[1]);
  fprintf(stream, "  32-63B   : %zu\n", bins[2]);
  fprintf(stream, "  64-127B  : %zu\n", bins[3]);
  fprintf(stream, "  128-255B : %zu\n", bins[4]);
  fprintf(stream, "  256-511B : %zu\n", bins[5]);
  fprintf(stream, "  512-1023B: %zu\n ", bins[6]);
  fprintf(stream, "  1-2KB    : %zu\n", bins[7]);
  fprintf(stream, "  2-4KB    : %zu\n", bins[8]);
  fprintf(stream, "  >=4KB    : %zu\n", bins[9]);
}
void memcheck_reset(void) {
  struct MemCheckContext *ctx = memcheck_hooks.context;
  if (!ctx)
    return;
  // Free any existing leaks
  if (ctx->leaks) {
    for (size_t i = 0; i < ctx->count; i++)
      if (ctx->leaks[i].symbols)
        free(ctx->leaks[i].symbols);
    free(ctx->leaks);
    ctx->leaks = NULL;
    ctx->cap = 0;
  }
  ctx->count = 0;
  ctx->current_bytes = 0;
}

// MemCheck interface instance
const IMemCheck MemCheck = {
    .init = memcheck_init,
    .enable = memcheck_enable,
    .disable = memcheck_disable,
    .isEnabled = memcheck_isEnabled,
    .leakedBlocks = memcheck_leakedBlocks,
    .leakedBytes = memcheck_leakedBytes,
    .peakBytes = memcheck_peakBytes,
    .enableBacktraces = memcheck_enableBacktraces,
    .printHistogram = memcheck_printHistogram,
    .reset = memcheck_reset,
};