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
 * File: sigtest_cli.h
 * Description: Header file for Sigma-Test CLI definitions and interfaces
 */
#ifndef SIGTEST_CLI_H
#define SIGTEST_CLI_H

#include "sigtest.h"
#include <stdio.h>

#define MAX_TEMPLATE_LEN 64

// Output log levels
typedef enum
{
   LOG_NONE,    // No logging
   LOG_MINIMAL, // Minimal logging
   LOG_VERBOSE, // Verbose logging
} LogLevel;

// CLI state structure
typedef struct
{
   enum
   {
      START,
      TEST_SRC,
      DONE,
      ERROR,
      IGNORE,
   } state;
   enum
   {
      DEFAULT,
      SIMPLE,
      VERSION,
   } mode;
   const char *test_src;
   int no_clean;
   LogLevel log_level;
   DebugLevel debug_level;
} CliState;

/**
 * @brief Debug logging function
 * @param stream :the output stream to write to
 * @param log_level :the log level
 * @param debug_level :the debug level
 * @param fmt :the format message to display
 * @param ... :the variable arguments for the format message
 */
void fdebugf(FILE *, LogLevel, DebugLevel, const char *, ...);

#endif // SIGTEST_CLI_H