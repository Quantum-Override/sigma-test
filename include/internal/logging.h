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
 * File: logging.h
 * Description: Header file for logging functionality
 */
#pragma once

#include "../core.h"
#include <stdarg.h>
#include <stdio.h>

/**
 * @brief Writes a formatted message to the current test set's log stream
 * @param fmt :the format message to display
 * @param ... :the variable arguments for the format message
 */
void writef(const char *, ...);
/**
 * @brief Writes a formatted message with newline to the current test set's log
 * stream
 * @param fmt :the format message to display
 * @param ... :the variable arguments for the format message
 */
void writelnf(const char *, ...);
/**
 * @brief Writes a formatted message to the specified stream
 * @param stream :the output stream to write to
 * @param fmt :the format message to display
 * @param ... :the variable arguments for the format message
 */
void fwritef(FILE *, const char *, ...);
/**
 * @brief Writes a formatted message with newline to the specified stream
 * @param stream :the output stream to write to
 * @param fmt :the format message to display
 * @param ... :the variable arguments for the format message
 */
void fwritelnf(FILE *, const char *, ...);