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
// Modification history
//

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
     
#include <sstream>
#include <cstring>

#include "Debug.h"
#include "Client.h"
#include "Server.h"

#define BUFSIZE 8192

extern ostream* debugStream;


//
// Public methods
//

Client::Client(Server *s,int fd)
{
	ostringstream ss;
	ss << fd;
	threadID = ss.str();
	
	socketfd=fd;
	channelMask=0;
	server=s;
}

Client::~Client()
{
	close(socketfd);
}

void Client::sendData(vector< int >&data)
{
	DBGMSG(debugStream,"sending data:" << data.size());
	
	string msg;
	for (unsigned int i=0;i<data.size();i+=4){
		ostringstream ss;
		ss << data.at(i) << " " << data.at(i+1) << " " << data.at(i+2) << " " << data.at(i+3) << endl;
		msg += ss.str();
	}
	DBGMSG(debugStream,msg);

	write(socketfd,msg.c_str(),strlen(msg.c_str())); // FIXME more robust
	
}

void Client::doWork()
{
	int nread;
	char msgbuf[BUFSIZE];
	while (!stopRequested){
		while ((nread=recv(socketfd,msgbuf,BUFSIZE-1,0))>0){ // nread ==0 -> shutdown
			msgbuf[nread]=0; // terminate that string
			// Get the channel mask
			DBGMSG(debugStream,threadID << ":" << msgbuf);
			// Expected input is of the form mask=N
		}
		if (0==nread){
			DBGMSG(debugStream,"connection " << threadID << " closed by client");
			stopRequested=true;
			close(socketfd);
			server->notifyClose(this); 
		}
		else if (-1==nread){
			DBGMSG(debugStream,"recv error: " << threadID << " closed");
			stopRequested=true;
			close(socketfd);
			server->notifyClose(this);
		}
	}
	// if stop has been requested by Server, then we don't notify it
}


