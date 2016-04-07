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


// Sys.h
//
// This header file is included at the top of every source file,
// before any other header file.
// It is intended to add defines that are needed globally and
// to work around Operating System dependend incompatibilities.

// EXAMPLE: If you use autoconf you can add the following here.
// #ifdef HAVE_CONFIG_H
// #include "config.h"
// #endif

// EXAMPLE: You could add stuff like this here too
// (Otherwise add -DCWDEBUG to your CFLAGS).
// #if defined(WANTSDEBUGGING) && defined(HAVE_LIBCWD_BLAHBLAH)
// #define CWDEBUG
// #endif

// The following is the libcwd related mandatory part.
// It must be included before any system header file is included!

#ifdef CWDEBUG
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <libcwd/sys.h>
#endif
