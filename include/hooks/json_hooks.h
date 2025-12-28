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
 * File: json_hooks.h
 * Description: Header file for JSON output hooks for SigmaTest
 */
#pragma once

#include "sigtest.h"

struct JsonHookContext {
   struct {
      int count;
      int verbose;
      ts_time start;
      ts_time end;
      RunnerState state;
      Logger logger;
   } info;
   object data;
   TsInfo set;
};

extern struct st_hooks_s json_hooks;

void json_before_set(const TsInfo set, tc_context *context);
void json_after_set(const TsInfo set, tc_context *context);
void json_before_test(tc_context *context);
void json_after_test(tc_context *context);
void json_on_start_test(tc_context *context);
void json_on_end_test(tc_context *context);
void json_on_error(const char *message, tc_context *context);
void json_on_test_result(const TsInfo set, tc_context *context);