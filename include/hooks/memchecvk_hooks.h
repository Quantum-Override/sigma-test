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
 * File: memcheck_hooks.h
 * Description: Header file for memory check hooks for Sigma-Test
 *    - Provides memory allocation tracking and leak detection
 *    - Integrates with Sigma-Test's test framework
 */
#pragma once

#include "sigtest.h"

typedef struct {
  void *ptr;
  size_t size;
  char **symbols;
  int frames;
} Leak;

struct MemCheckContext {
  int enabled;
  TestSet set; // ← current test set
  Leak *leaks;
  size_t count;
  size_t cap;
  size_t current_bytes;
  size_t peak_bytes;
  int backtraces_enabled;
  int verbose;
};

extern struct st_hooks_s memcheck_hooks;

typedef struct IMemCheck {
  void (*init)(int verbose);      // Initialize MemCheck with verbosity
  void (*enable)(void);           // Enable memory checking
  void (*disable)(void);          // Disable memory checking
  int (*isEnabled)(void);         // Check if memory checking is enabled
  int (*leakedBlocks)(void);      // Get the number of leaked memory blocks
  size_t (*leakedBytes)(void);    // Get the total number of leaked bytes
  size_t (*peakBytes)(void);      // Get the peak memory usage in bytes
  void (*enableBacktraces)(int);  // Enable/disable backtrace capture
  void (*printHistogram)(FILE *); // Print memory allocation history to the specified stream
  void (*reset)(void);            // Reset memory tracking data
} IMemCheck;

extern const IMemCheck MemCheck;