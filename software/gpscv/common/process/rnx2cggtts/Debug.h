//
//
// The MIT License (MIT)
//
// Copyright (c) 2023  Michael J. Wouters
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

#ifndef __DEBUG_H_
#define __DEBUG_H_

extern int verbosity;
extern bool shortDebugMessage;

#define INFO    1
#define WARNING 2
#define TRACE 3

// NB null stream indicates debugging off
// May see IDE grumbles about DBGMSG having too many arguments because DEBUG is defined at compile time
#ifdef DEBUG
	#define DBGMSG( os, v, msg ) \
  if (NULL != os && v<=verbosity) \
		{if (shortDebugMessage)\
			(*os) <<  __FUNCTION__ << "() "<< msg << std::endl; \
		else\
			(*os) << __FILE__ << "(" << __LINE__ << ") " << __FUNCTION__ << "() " << msg << std::endl;}
#else
	#define DBGMSG( os, msg ) 
#endif

#endif

