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
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ----------------------------------------------
 * File: fuzzing.h
 * Description: Header file for SigmaTest fuzzing test case definitions and interfaces
 */
#pragma once

#include "core.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Fuzz input types
 */
typedef enum {
   FUZZ_INT,
   FUZZ_SIZE_T,
   FUZZ_FLOAT,
   FUZZ_BYTE,
   // Add more fuzz input types as needed
} FuzzType;

typedef void (*FuzzyFunc)(void *); // Fuzzy test function pointer

/**
 * @brief Registers a fuzzy test case
 * @param  name :the test name
 * @param  func :the fuzzy test function
 * @param  type :the fuzz input type
 */
void fuzz_testcase(string name, FuzzyFunc func, FuzzType type);

// Fuzzy data sets
static const int fuzz_int_values[] = {
    INT_MIN, INT_MIN + 1,
    -1, 0, 1,
    INT_MAX - 1, INT_MAX};
static const size_t fuzz_int_count = sizeof(fuzz_int_values) / sizeof(fuzz_int_values[0]);

static const size_t fuzz_size_t_values[] = {
    0, 1,
    SIZE_MAX / 2,
    SIZE_MAX - 1,
    SIZE_MAX};
static const size_t fuzz_size_t_count = sizeof(fuzz_size_t_values) / sizeof(fuzz_size_t_values[0]);

static const float fuzz_float_values[] = {
    -INFINITY,
    -FLT_MAX,
    -1.0f,
    -0.0f,
    0.0f,
    1.0f,
    FLT_MAX,
    INFINITY,
    NAN,
    1.17549435e-38f, // smallest positive normal float
    -1.17549435e-38f};
static const size_t fuzz_float_count = sizeof(fuzz_float_values) / sizeof(fuzz_float_values[0]);

static const signed char fuzz_byte_values[] = {
    SCHAR_MIN, -1, 0, 1, SCHAR_MAX};
static const size_t fuzz_byte_count = sizeof(fuzz_byte_values) / sizeof(fuzz_byte_values[0]);