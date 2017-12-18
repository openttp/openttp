
// The MIT License (MIT)
//
// Copyright (c) 2017  Michael J. Wouters
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
// For benchmarking the system time call
// gcc -o bmsystime bmsystime.c -lrt
// (linking to librt only necessary on older platforms see manpage)
//

#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#if _POSIX_C_SOURCE >= 199309L
	#include <time.h>
	#define HAVE_CLOCK_GETTIME
#endif

#define NSAMPLES 1000000

#define APP_NAME     "bmsystime"
#define APP_VERSION  "0.1"

static void ShowHelp()
{
	printf("%s version %s\n",APP_NAME,APP_VERSION);
	printf("Usage:\n");
	printf("-h           show this help\n");
	printf("-g           use gettimeofday (overrriding clock_gettiime\n");
	printf("-n <samples> set number of samples (default %i)\n",NSAMPLES);
	printf("-v           display version\n");
}

int main(int argc,char **argv){
	
	struct sched_param	sched;
	int i,c;
	
	int nsamples=NSAMPLES;
	int usegettimeofday=1;
	
	time_t *sec;
	time_t *usec;
	struct timeval todsample;
	
	#ifdef HAVE_CLOCK_GETTIME
	long *nsec;
	struct timespec tspec;
	usegettimeofday=0;
	#endif
	
	while ((c=getopt(argc,argv,"ghn:v")) != -1)
	{
		switch(c)
		{
			case 'g':
				usegettimeofday=1;
				break;
			case 'h': 
				ShowHelp();
				return EXIT_SUCCESS;
				break;
			case 'n':
				if (optarg){
					if (1==sscanf(optarg,"%i",&nsamples)){
						if ( nsamples < 0){
							fprintf(stderr,"Error in argument to option -n\n");
							return 0;
						}
					}
					else{
						fprintf(stderr,"Error in argument to option -o\n");
						return EXIT_FAILURE;
					}
				}
				break;
			case 'v':
				printf("%s version %s\n",APP_NAME,APP_VERSION);
				printf("This ain't no stinkin' Perl script!\n");
				return EXIT_SUCCESS;
				break;
		}
	}
	
	
	// Are we root ? 
	if (getuid()){
		fprintf(stderr, "Error! Must be run as root\n");
		exit(EXIT_FAILURE);
	}
	
	sec = malloc(nsamples*sizeof(time_t));
	
	if (1==usegettimeofday){
		usec = malloc(nsamples*sizeof(time_t));
		fprintf(stderr,"Using gettimeofday()\n");
	}
	else{
		#ifdef HAVE_CLOCK_GETTIME
		nsec = malloc(nsamples*sizeof(long));
		fprintf(stderr,"Using clock_gettime()\n");
		#endif
	}
	
	// Become a low priority real-time process
	sched.sched_priority = sched_get_priority_min(SCHED_FIFO);
	if ( sched_setscheduler(0, SCHED_FIFO, &sched) == -1 ){
		fprintf(stderr,"Exiting: %s\n",strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	// Lock memory so we don't get paged out, causing jitter 
	if ( mlockall(MCL_FUTURE|MCL_CURRENT) == -1 ){
		fprintf(stderr,"Exiting: %s\n",strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	
	for (i=0;i<nsamples;++i){
		
		if (1==usegettimeofday){
			gettimeofday(&todsample,NULL);
			usec[i]=todsample.tv_usec;
			sec[i]=todsample.tv_sec;
		}
		else{
			#ifdef HAVE_CLOCK_GETTIME
			clock_gettime(CLOCK_REALTIME,&tspec);
			nsec[i]=tspec.tv_nsec;
			sec[i]= tspec.tv_sec;
			#endif
		}
	}
	
	
	for (i=0;i<nsamples;++i){
		if (usegettimeofday==1){
			printf("%d %d\n",(int) sec[i],(int) usec[i]);
		}
		else{
			#ifdef HAVE_CLOCK_GETTIME
			printf("%d %d\n",(int) sec[i],(int) nsec[i]);
			#endif
		}
	}
	
	return EXIT_SUCCESS;
}