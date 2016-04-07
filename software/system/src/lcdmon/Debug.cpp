//
//
// The MIT License (MIT)
//
// Copyright (c) 2016  Michael J. Wouters
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// Modification history
//



#ifdef CWDEBUG

#include "Sys.h"
#include "Debug.h"

#include <pthread.h>


// This lock is only used for libcwd so far, so put this here.
extern pthread_mutex_t cout_mutex;

namespace lcdmonitor {		// >
	namespace debug {		//  >--> This part must match DEBUGCHANNELS, see debug.h
		namespace channels {	// >

			namespace dc {

        // Add new debug channels here.
				channel_ct trace("TRACE");
				channel_ct benchmark("BENCHMARK");
				channel_ct fixme("FIXME");
      } // namespace dc

    } // namespace DEBUGCHANNELS

// Initialize debugging code from new threads.
void init_thread(void)
{
  // Everything below needs to be repeated at the start of every
  // thread function, because every thread starts in a completely
  // reset state with all debug channels off etc.

#if LIBCWD_THREAD_SAFE		// For the non-threaded case this is set by the rcfile.
  // Turn on all debug channels by default.
  ForAllDebugChannels(while(!debugChannel.is_on()) debugChannel.on());
  // Turn off specific debug channels.
	Debug(dc::trace.off());
	Debug(dc::benchmark.off());
	Debug(dc::fixme.off());
	Debug(dc::warning.off());
  Debug(dc::bfd.off());
  Debug(dc::malloc.off());
#endif

	Debug(dc::bfd.off());
	Debug(dc::malloc.off());
	//Debug(dc::warning.off());
  // Turn on debug output.
  // Only turn on debug output when the environment variable SUPPRESS_DEBUG_OUTPUT is not set.
  Debug(if (getenv("SUPPRESS_DEBUG_OUTPUT") == NULL) libcw_do.on());
#if LIBCWD_THREAD_SAFE
  Debug(libcw_do.set_ostream(&std::cout, &cout_mutex));

  // Set the thread id in the margin.
  char margin[12];
  sprintf(margin, "%-10lu ", pthread_self());
  Debug(libcw_do.margin().assign(margin, 11));
#else
  Debug(libcw_do.set_ostream(&std::cout));
#endif

  // Write a list of all existing debug channels to the default debug device.
  Debug(list_channels_on(libcw_do));
}

// Initialize debugging code from main().
void init(void)
{
  // You want this, unless you mix streams output with C output.
  // Read  http://gcc.gnu.org/onlinedocs/libstdc++/27_io/howto.html#8 for an explanation.
  // We can't use it, because other code uses printf to write to the console.
#if 0
  Debug(set_invisible_on());
  std::ios::sync_with_stdio(false);	// Cause "memory leaks" ([w]cin, [w]cout and [w]cerr filebuf allocations).
  Debug(set_invisible_off());
#endif

  // This will warn you when you are using header files that do not belong to the
  // shared libcwd object that you linked with.
  Debug(check_configuration());

#if CWDEBUG_ALLOC
  // Remove all current (pre- main) allocations from the Allocated Memory Overview.
  libcwd::make_all_allocations_invisible_except(NULL);
#endif

  Debug(read_rcfile());

  init_thread();
}

  } // namespace debug
} // namespace screenhack

#endif // CWDEBUG
