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

#include <sys/types.h>         
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>

#include <iostream>
#include <sstream>

#include "Client.h"
#include "Debug.h"
#include "OKCounterD.h"
#include "Server.h"

#define BUFSIZE 8096

extern ostream* debugStream;

//
// Public
//

Server::Server(OKCounterD *a,int p)
{
	threadID = "server";
	app = a;
	port=p; 
	if (!init())
		exit(EXIT_FAILURE);
}

Server::~Server()
{
	close(listenfd);
	for (unsigned int i=0;i<clients.size();i++)
		clients.at(i)->stop();
}

void Server::notifyClose(Client *c)
{
	for (unsigned int i=0;i<clients.size();i++){
		if (clients.at(i) == c){
			DBGMSG(debugStream,"closed client");
			clients.erase(clients.begin()+i);
			delete c; 
		}
	}
}

void Server::sendData(vector<int> &data){
	for (unsigned int i=0;i<clients.size();i++){
		clients.at(i)->sendData(data);
	}
}

//
// Protected
//

void Server::doWork()
{
	// SIGPIPE is delivered if the client closes the socket before we've finished replying
	// This kills the process
	
	DBGMSG(debugStream,"working");
	
	sigset_t blocked;
	sigemptyset(&blocked);
	sigaddset(&blocked,SIGPIPE);
	pthread_sigmask(SIG_BLOCK,&blocked,NULL);
	
	int socketfd;
	socklen_t len;
	struct sockaddr_in cli_addr; 
	bzero(&cli_addr,sizeof(cli_addr));

	fd_set rfds;
  struct timeval tv;
  int retval;
	
	while (!stopRequested) 
	{
	
		tv.tv_sec=5;
		tv.tv_usec=0;
		
		FD_ZERO(&rfds);
		FD_SET(listenfd, &rfds);

		retval = select(listenfd+1, &rfds, NULL,NULL, &tv); // note number of file descriptors!
	
		if (retval) 
		{
			len= sizeof(cli_addr);
			if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &len)) < 0)
				app->log("ERROR in accept()");
			else
			{
				processRequest(socketfd);
			}
		}
	}
	
	pthread_exit(NULL);
}


//
// Private
//

bool Server::init()
{
	struct sockaddr_in serv_addr; 
	bzero(&serv_addr,sizeof(serv_addr));
	
	// Set up the socket
	#ifdef LINUX_PRE_2_6_27
	listenfd = socket(AF_INET, SOCK_STREAM ,0);
	if (listenfd >=0){
		fcntl(listenfd, F_SETFL, O_NONBLOCK);
	}
	else{
	#else
	if((listenfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK,0)) <0){
	#endif
		app->log("ERROR in socket()");
		return false;
	}
	
	if(port < 0 || port >60000){
		app->log("ERROR invalid port number (try 1->60000)");
		return false;
	}
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);
	
	if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0){
		app->log("ERROR in bind()");
		return false;
	}
	
	if( listen(listenfd,64) <0){
		app->log("ERROR in listen()");
		return false;
	}
	
	ostringstream ss;
	ss << "listening on port " << port;
	app->log(ss.str());
	
	return true;
	
}

bool Server::processRequest(int socketfd)
{
	// Two requests:
	// LISTEN to counter readings
	// CONFIGURE the counter
	// QUERY the counter configuration
	
	long ret;
	static char buffer[BUFSIZE+1]; 

	ret =read(socketfd,buffer,BUFSIZE); 	// read request in one go 
	if(ret == 0 || ret == -1) {	// read failure
		//app->log("failed to read client request");
		return false;
	}
	if(ret > 0 && ret < BUFSIZE)	// return code is valid chars 
		buffer[ret]=0;		// terminate the buffer 
	else buffer[0]=0;
	
	if (NULL != strstr(buffer,"CONFIGURE") ){
		DBGMSG(debugStream,"received " << buffer);
		if (NULL != strstr(buffer,"PPSSOURCE")){
			int src;
			sscanf(buffer,"%*s%*s%i",&src);
			app->setOutputPPSSource(src);
		}
		else if (NULL != strstr(buffer,"GPIO")){
			int en;
			sscanf(buffer,"%*s%*s%i",&en);
			app->setGPIOEnable((en==1));
		}
		else{
			DBGMSG(debugStream,"unknown command");
		}
		
		// done so close the connection
		if (-1==shutdown(socketfd,SHUT_RDWR)){
			app->log("ERROR in shutdown()");
		}
		if (-1==close(socketfd)){
			app->log("ERROR in close()");
		}
	}
	else if (NULL != strstr(buffer,"QUERY CONFIGURATION") ){
		DBGMSG(debugStream,"received " << buffer);
		string cfg = app->getConfiguration();
		writeBuffer(cfg,socketfd);
		sleep(1); // wait a bit
		// done so close the connection
		if (-1==shutdown(socketfd,SHUT_RDWR)){
			app->log("ERROR in shutdown()");
		}
		if (-1==close(socketfd)){
			app->log("ERROR in close()");
		}
	}
	else if(NULL != strstr(buffer,"LISTEN") ){ 
		DBGMSG(debugStream,"received " << buffer);
		Client *client = new Client(this,socketfd);
		clients.push_back(client);
		client->go();
	}
	return true;
}

void Server::writeBuffer(string msg,int socketfd){
	
	int nw,nEINTR=0;
	int nleft = msg.size();
	const char* pbuf=msg.c_str();
	
	while (nleft > 0){
		if (( nw = write(socketfd,pbuf,nleft)) <= 0){
			if (errno==EINTR){ // interrupted by signal
				nw=0;
				nEINTR++;
				if (10==nEINTR){
					DBGMSG(debugStream, "too many interrrupts - write() aborted");
					return;
				}
			}
			else{
				DBGMSG(debugStream, strerror(errno));
				return;
			}
		}
		DBGMSG(debugStream, "wrote " << nw);
		nleft -=nw;
		pbuf += nw;
	}	
}

	
