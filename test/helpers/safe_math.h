#pragma once

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

/* Optional helpers for saturating integer arithmetic.
 * These are examples only — not part of SigmaCore API.
 * Use in your code or as reference for your own implementations.
 */

/**
 * @brief Safe addition for integers with saturation
 * @param a First operand
 * @param b Second operand
 * @return Sum of a and b, or INT_MAX/INT_MIN on overflow
 */
static inline int safe_add_int(int a, int b) {
   if (b > 0 && a > INT_MAX - b)
      return INT_MAX;
   if (b < 0 && a < INT_MIN - b)
      return INT_MIN;
   return a + b;
}
/**
 * @brief Safe subtraction for integers with saturation
 * @param a First operand
 * @param b Second operand
 * @return Difference of a and b, or INT_MAX/INT_MIN on overflow
 */
static inline int safe_sub_int(int a, int b) {
   if (b < 0 && a > INT_MAX + b)
      return INT_MAX;
   if (b > 0 && a < INT_MIN + b)
      return INT_MIN;
   return a - b;
}
/**
 * @brief Safe multiplication for integers with saturation
 * @param a First operand
 * @param b Second operand
 * @return Product of a and b, or INT_MAX/INT_MIN on overflow
 */
static inline int safe_mul_int(int a, int b) {
   if (a > 0 && b > 0 && a > INT_MAX / b)
      return INT_MAX;
   if (a > 0 && b < 0 && b < INT_MIN / a)
      return INT_MIN;
   if (a < 0 && b > 0 && a < INT_MIN / b)
      return INT_MIN;
   if (a < 0 && b < 0 && a < INT_MAX / b)
      return INT_MAX; // both negative, result positive
   return a * b;
}
/**
 * @brief Safe division for integers with zero-check
 * @param a Dividend
 * @param b Divisor
 * @return Quotient of a divided by b, or 0 if b is zero
 */
static inline int safe_div_int(int a, int b) {
   if (b == 0)
      return 0; // or INT_MAX, or assert — your policy
   return a / b;
}

/**
 * @brief Safe addition for size_t with saturation
 * @param a First operand
 * @param b Second operand
 * @return Sum of a and b, or SIZE_MAX on overflow
 */
static inline size_t safe_add_size_t(size_t a, size_t b) {
   if (a > SIZE_MAX - b)
      return SIZE_MAX;
   return a + b;
}