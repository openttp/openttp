/**
 * \file ssngetop.h
 *
 * \brief  A declaration of "ssngetopt()", Septentrio's own implementation
 *         of the Posix "getopt()" function. Should conform to IEEE Std
 *         1003.1-2001, but not all functionalities are implemented.
 *
 *   The implementation has the following limitations:
 *     1. It is a pure 8-bit character implementation, it doesn't
 *        support locales, multibytes or wide characters at all.
 *     2. "opterr" is supposed set to 0 by the application, i.e. no
 *        message is output to "stderr" in case of error.
 *     3. No extention is supported. (No GNU extention, no '-W'
 *        option...)
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

#ifndef SSNGETOP_H
#define SSNGETOP_H 1

#include "ssntypes.h"

#ifdef __cplusplus
extern "C" {
#endif

extern FW_EXPORT char* ssn_optarg;

extern FW_EXPORT int ssn_optind;
extern FW_EXPORT int ssn_opterr;
extern FW_EXPORT int ssn_optopt;

int FW_EXPORT ssn_getopt(int argc, char* const argv[], const char* optstring);

#ifdef __cplusplus
}
#endif

#endif
/* End of "ssngetop.h" */

