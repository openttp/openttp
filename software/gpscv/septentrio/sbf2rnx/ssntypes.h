/**
 * \file ssntypes.h
 * \brief Declarations of Septentrio types and implementation of common C types which are not implemented by every compiler.
 *
 *  If your compiler does not support the standard C99 types from \p stdint.h
 *  and \p stdbool.h, please define them for your platform. \n
 *  This is already done for the Microsoft C/C++ compiler.
 *
 * Septentrio grants permission to use, copy, modify, and/or distribute
 * this software for any purpose with or without fee.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND SEPTENTRIO DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * SEPTENTRIO BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef SSNTYPES_H
#define SSNTYPES_H 1 /*  To avoid multiple inclusions. */

#ifdef SSN_DLL
# ifdef _WIN32
#  ifdef FW_MAKE_DLL
#   define FW_EXPORT __declspec(dllexport)
#  else
#   define FW_EXPORT __declspec(dllimport)
#  endif
# else
#  ifdef FW_MAKE_DLL
#   define FW_EXPORT __attribute__((visibility("default")))
#  else
#   define FW_EXPORT
#  endif
# endif
#else
# define FW_EXPORT
#endif

#if (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) || defined(__GNUC__) || defined(__ARMCC__)
#  include <stdint.h>
#  include <stdbool.h>
#  include <inttypes.h>
#else
#  ifdef _MSC_VER
#    include "mscssntypes.h"
#    ifndef _USE_MATH_DEFINES /* used to define M_PI and so*/
#      define _USE_MATH_DEFINES
#    endif
#  endif
#endif

// Helper to deprecate functions and methods
// See https://blog.samat.io/2017/02/27/Deprecating-functions-and-methods-in-Cplusplus/
// For C++14
#if (defined(__cplusplus) && (__cplusplus >= 201402L))
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(deprecated)
#define DEPRECATED(msg, func) [[deprecated(msg)]] func
#endif
#endif
// For everyone else
#else
#ifdef __GNUC__
#define DEPRECATED(msg, func) func __attribute__ ((deprecated(msg)))
#elif defined(_MSC_VER)
#define DEPRECATED(msg, func) __declspec(deprecated(msg)) func
#else
// not defined
#define DEPRECATED(msg, func)
#endif
#endif

// Helper to deprecate enum values
// For C++14
#if (defined(__cplusplus) && (__cplusplus >= 201402L))
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(deprecated)
#define DEPRECATED_ENUM(msg) [[deprecated(msg)]]
#endif
#endif
// For everyone else
#else
#if defined(__GNUC__) && (__GNUC__ >= 6)
#define DEPRECATED_ENUM(msg) __attribute__ ((deprecated(msg)))
#else
// not defined
#define DEPRECATED_ENUM(msg)
#endif
#endif

// Helper macro for static_cast in c++ and c-style cast in C
#ifdef __cplusplus
#define STATIC_CAST(type, toCast) (static_cast<type>(toCast))
#else
#define STATIC_CAST(type, toCast) ((type)(toCast))
#endif

#endif
/* End of "ifndef SSNTYPES_H" */
