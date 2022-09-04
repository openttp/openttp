/*
 * "ssngetop.c"
 *
 *      A definition of "ssngetopt()", Septentrio's own (partial)
 *      implementation of the Posix "getopt()" function. Should
 *      conform to IEEE Std 1003.1-2001, but not all functionalities
 *      are implemented.
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

#include "ssngetop.h"

#if !defined(NULL)
#define NULL ((void*)0)
#endif

char* ssn_optarg = (char*)NULL;

int ssn_optind = 1;
int ssn_opterr = 1;
int ssn_optopt = 0;

/* "ssn_optgrpidx" tells how many option characters were already
 * processed, when options are grouped. It is the index of the next
 * option character in 'argv[ssn_optind]'. For example:
 *   The first call 'ssn_getopt( argc, argv, "f:sv" )'
 *   with 'argc=2' and 'argv = { "foo", "-vs", "-f", "data.txt" }'
 *   will return 'v' and set 'ssn_optgrpidx' to 2. The second
 *   identical call will return 's', set 'ssn_optgrpidx' back to 1
 *   and increment 'ssn_optind' to 2. */

static int ssn_optgrpidx = 1;

int ssn_getopt(int argc, char* const argv[], const char* optstring)
{
    int i;   /* Generic loop index */
    char c;  /* Current option */

    /* Sanity check on "ssn_optind" and "optstring" */
    if (ssn_optind < 1 || ssn_optind > argc
        || argv == NULL || optstring == NULL)
    {
        /* We don't know what to do in this case, because "getopt()"
         * defines no error. Let's return -1, i.e. "no more option".*/
        return -1;
    }

    /* assert( the string pointer "argv[ssn_optind]" is accessible ) */

    ssn_optarg = (char*)NULL;

    /* If there is no next argument at all, or if the next argument is
     * not an option, or if it is just a minus string, return -1 and
     * don't change "ssn_optind". */
    if (argv[ssn_optind] == NULL)
    {
        return -1;
    }

    if (argv[ssn_optind][0] != '-' || argv[ssn_optind][1] == '\0')
    {
        return -1;
    }

    /* assert( argv[ssn_optind][0] == '-' ) */
    /* assert( the option character "argv[ssn_optind][1]" is accessible ) */
    c = argv[ssn_optind][ssn_optgrpidx];

    /* If the next argument is "--", increment "ssn_optind" and return -1. */
    if (c == '-')
    {
        ssn_optind++;
        ssn_optgrpidx = 1;
        return -1;
    }

    /* assert( 'optstring' is an accessible string ) */

    /* For all character in 'optstring': */
    for (i = 0; optstring[i] != '\0' ; i++)
    {
        if (c == optstring[i])
        {
            /* The option is found in the option list. */
            /* Is it an option with an argument? */
            if (optstring[i + 1] == ':')
            {
                /* Set 'ssn_optarg' to point to the character that follows the
                 * current option character 'c'. Then increment
                 * "ssn_optind". */
                ssn_optarg = &(argv[ssn_optind][ssn_optgrpidx + 1]);
                ssn_optind++;
                ssn_optgrpidx = 1;

                /* No next character in the current "argv[]"
                 * => The option argument is in the full next "argv[]".
                 *    Let "ssn_optarg" point to the first character of the
                 *    next "argv[]", and increment "ssn_optind" once again. */
                if (*ssn_optarg == '\0')
                {
                    /* Is there a next "argv[]"? */
                    if (ssn_optind >= argc)
                    {
                        /* No. The last option requires an argument, but there is
                         * none. Set the wrong option char in "ssn_optopt" and
                         * return ':' or '?'. */
                        ssn_optopt = c;
                        ssn_optarg = (char*)NULL;
                        /* If the first character of 'optstring' is ':', return ':' for a
                         * missing argument, else return '?' (but '?' indicates also an
                         * unknown option). */
                        return (optstring[0] == ':') ? ':' : '?';
                    }

                    ssn_optarg = argv[ssn_optind];
                    ssn_optind++;
                    ssn_optgrpidx = 1;
                }

                return c;
            }
            else
            {
                /* No argument. */
                ssn_optarg = (char*)NULL;

                if (argv[ssn_optind][ssn_optgrpidx + 1] == '\0')
                {
                    /* No more grouped option character in this "argv[]". */
                    ssn_optind++;
                    ssn_optgrpidx = 1;
                }
                else
                {
                    ssn_optgrpidx++;
                }

                return c;
            }
        }
    }

    /* Option not found in 'optstring': set the wrong option char in
     * "ssn_optopt" and return '?'. Dont forget to increment
     * "ssn_optgrpidx" or "ssn_optind". */
    ssn_optopt = c;

    if (argv[ssn_optind][ssn_optgrpidx + 1] == '\0')
    {
        /* No more grouped option character in this "argv[]". */
        ssn_optind++;
        ssn_optgrpidx = 1;
    }
    else
    {
        ssn_optgrpidx++;
    }

    return '?';
}

/* End of "ssngetop.c" */
