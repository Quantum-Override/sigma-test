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
 * File: junit_hooks.h
 * Description: Header file for JUnit output hooks for SigmaTest
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
#pragma once

#include "sigtest.h"

struct JunitExtraData {
   char timestamp[32];
   char hostname[256];
   ts_time start_time;
   ts_time test_start;
   int total_tests;
   int failures;
   int skipped;
   TsInfo set;
};

struct JunitHookContext {
   struct {
      int count;
      int verbose;
      ts_time start;
      ts_time end;
      RunnerState state;
      Logger logger;
   } info;
   object data;
};

extern struct st_hooks_s junit_hooks;

void junit_before_set(const TsInfo set, tc_context *context);
void junit_after_set(const TsInfo set, tc_context *context);
void junit_on_test_result(const TsInfo set, tc_context *context);