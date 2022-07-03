
//
//
// The MIT License (MIT)
//
// Copyright (c) 2018  Michael J. Wouters
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
// THE SOFTWARE
//

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>

#define APP_NAME    "ntploadtest"
#define APP_VERSION "1.0"
#define APP_AUTHORS "Michael Wouters"

#define TRUE 1
#define FALSE 0

// Straight out of include/ntp.h 
typedef struct {
	uint8_t	li_vn_mode;	// peer leap indicator
	uint8_t	stratum;	// peer stratum
	uint8_t	ppoll;		// peer poll interval 
	char 	precision;	// peer clock precision 
	uint32_t	rootdelay;	// roundtrip delay to primary source */
	uint32_t	rootdisp;	// dispersion to primary source*/
	uint32_t	refid;		// reference id */
	uint32_t	reftime_s,reftime_f;	// last update time */
	uint32_t	org_s,org_f;		//originate time stamp */
	uint32_t	rec_s,rec_f;		// receive time stamp */
	uint32_t	xmt_s,xmt_f;		// transmit time stamp */
}ntp_pkt;

static int debugOn=0;

static void
print_help()
{
	printf("\n%s version %s\n",APP_NAME,APP_VERSION);
	printf("Usage: %s [-hvd] hostname\n",APP_NAME);
	printf("-h print this help message\n");
	printf("-v print version\n");
	printf("-r <rate> set poll rate (default 1/s)\n");
	printf("-t <time> set time to poll for (default forever)\n");
	printf("-d enable debugging\n");
}

static void
print_version()
{
	printf("\n %s version %s\n",APP_NAME,APP_VERSION);
	printf("Written by %s\n",APP_AUTHORS);
	printf("This ain't no stinkin' Perl script!\n");
}

static void 
error_exit( 
	char* msg)
{
	perror( msg ); 
	exit( EXIT_FAILURE );
}

int 
get_int(char *str){
	char *endptr;
	
	errno =0;
	
	int val = strtol(str,&endptr,10);
	
	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0)) {
		error_exit("strtol");
	}
	
	if (*endptr != '\0'){
		error_exit("Error! Bad parameter");
	}
	
	return val;
	
}

int main( 
	int argc, 
	char* argv[ ] )
{
	int c;
  int sockfd; 
  int portno = 123;
	char *hostname;
	
	int rate=1; // packets per second to send
	int runtime=-1; // time to send packets for, negative means forever
	int totalpkts=rate*runtime; // total packets to send
	int runForever = (totalpkts < 0);
	int tusleep;
	struct timeval tvstart,tvstop,tvstartpoll,tvstoppoll;
	int tupoll;
	int pktsize;
	int nleft,nw;
	char *pbuf;
	int nsent;
	// Process the command line options 
	while ((c=getopt(argc,argv,"dht:r:v")) != -1)
	{
		switch(c)
		{
			case 'd':	// enable debugging 
				printf("Debugging is ON\n");
				debugOn=TRUE;
				break;
			case 'h': // print help 
				print_help();
				exit(EXIT_SUCCESS);
				break;
			case 'r': // set poll rate
				rate = get_int(optarg);
				if (rate > 10000){
					error_exit("The rate is too high: 10 000 packets/s is the practical maximum"); 
				}
				break;
			case 't':
				runtime = get_int(optarg);
				break;
			case 'v':
				print_version();
				exit(EXIT_SUCCESS);
				break;
			default:
				print_help();
				exit(EXIT_FAILURE);
		}
	}
	
	if ((argc - optind != 1)){
		printf("\nError!: no hostname was specified\n");
		print_help();
		exit(EXIT_FAILURE);
	}
	
	totalpkts=rate*runtime; 
	runForever = (totalpkts < 0);
	hostname = argv[argc-1];
	
	// Construct a minimal NTP request packet
  ntp_pkt packet = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  memset( &packet, 0, sizeof( ntp_pkt ) );
  *( ( char * ) &packet + 0 ) = 0x1b; 

	// Open the socket
  struct sockaddr_in serv_addr; 
  struct hostent *server;      
  sockfd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP ); 
  if ( sockfd < 0 )
    error_exit( "ERROR opening socket" );

  server = gethostbyname( hostname );
  if ( server == NULL )
    error_exit( "ERROR, no such host" );
  bzero( ( char* ) &serv_addr, sizeof( serv_addr ) );

  serv_addr.sin_family = AF_INET;
  bcopy( ( char* )server->h_addr, ( char* ) &serv_addr.sin_addr.s_addr, server->h_length );
  serv_addr.sin_port = htons( portno );

  if ( connect( sockfd, ( struct sockaddr * ) &serv_addr, sizeof( serv_addr) ) < 0 )
    error_exit( "ERROR connecting" );
	
	tusleep = rint(1000000.0/rate);
	gettimeofday(&tvstart,NULL); // takes a few microseconds
	
	pktsize = sizeof( ntp_pkt );
	nsent=0;
	while (totalpkts > 0 || runForever){
		gettimeofday(&tvstartpoll,NULL); 
		nleft = pktsize;
		pbuf = (char *)(&packet);
		while (nleft > 0){
			if (( nw = write(sockfd,pbuf,nleft)) <= 0){
				fprintf(stderr,"write error\n");
				break;
			}
			nleft -=nw;
			pbuf += nw;
		}	
		totalpkts--;
		gettimeofday(&tvstoppoll,NULL);
		tupoll = (tvstoppoll.tv_sec - tvstartpoll.tv_sec)*1000000+(tvstoppoll.tv_usec - tvstartpoll.tv_usec);
		if (tupoll >= tusleep)
			tupoll=tusleep;
		struct timespec ts;
		ts.tv_sec=0;
		ts.tv_nsec = (tusleep-tupoll)*1000;
		nanosleep(&ts,NULL);
		nsent++;
	}
	gettimeofday(&tvstop,NULL); 
	printf("Sent %i packets in %g s %i\n",nsent,(tvstop.tv_sec - tvstart.tv_sec)+(tvstop.tv_usec - tvstart.tv_usec)/1.0E6,tupoll);
  return EXIT_SUCCESS;
}