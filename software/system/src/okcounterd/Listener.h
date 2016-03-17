//
//
//

#ifndef __LISTENER_H_
#define __LISTENER_H_

#include "Thread.h"

class Listener:Thread
{
	public:
	
		Listener(int,int);
		virtual ~Listener();
		

	protected:
		
		virtual void doWork();
		
	private:
		
		int port;
		int channelMask;
		
};
#endif