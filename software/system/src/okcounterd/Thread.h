//
//
// The MIT License (MIT)
//
// Copyright (c) 2014  Michael J. Wouters
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

#ifndef __THREAD_H_
#define __THREAD_H_

#include <cstdio>
#include <iostream>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

using namespace::std;

class Thread
{
	public:
		Thread()
				: stopRequested(false),running(false)
		{
			thread=NULL;
			pthread_mutex_init(&mutex,0);
			// default stack size is 8MB FIXME what about ARM ?
			// seems a bit too big ... reduce to 1 MB
			stackSize=PTHREAD_STACK_MIN*64;
		}

		virtual ~Thread()
		{
			pthread_mutex_destroy(&mutex);
		}
		
		bool isRunning(){return running;}
		
		// Create the thread and start work
		virtual void go() 
		{
			assert(!thread);
			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setstacksize(&attr, (size_t) stackSize);
			thread = new pthread_t;
			int ret =  pthread_create(thread, &attr, &workerCallback, (void *) this);
			if(ret != 0) {
				cerr << "Error: pthread_create() failed" << endl;
				exit(EXIT_FAILURE);
			}
			running=true;
		}

		virtual void stop() 
		{
			assert(thread);
			stopRequested = true;
			running=false;
			pthread_join(*thread,NULL);
		}

	protected:
			
		virtual void doWork(){};
		volatile bool stopRequested;
		volatile bool running;
		
		string threadID;
		int stackSize;
		
		pthread_t *thread;
		pthread_mutex_t mutex;
		
		static void * workerCallback(void *ptr)
		{
			Thread *pInstance = static_cast<Thread *>(ptr);
			pInstance->doWork();
			return NULL;
		}
};

#endif
