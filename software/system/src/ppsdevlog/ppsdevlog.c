//
//
// The MIT License (MIT)
//
// Copyright (c) 2019  Michael J. Wouters
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
//


#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <configurator.h>

#include <sys/timepps.h>

#define APP_NAME "ppsdevlog"
#define APP_VERSION "0.1.2"
#define LAST_MODIFIED ""

#define DEFAULT_CONFIG			"/usr/local/etc/ppsdevlog.conf" 
#define DEFAULT_STATUS_FILE	"/home/ntpadmin/logs/ppsdevlog.status"
#define DEFAULT_LOG_DIR			"/home/ntpadmin/logs/"
#define DEFAULT_LOCK     		"/home/ntpadmin/logs/ppsdevlog.lock"
#define DEFAULT_DEV 				"/dev/pps0"

#define TSLEEP  300000       /* sleep to sleep between polls of the PPS device */
#define DEFAULT_TIMEOUT     60 /* if no data received, bomb out */

#define BUFLEN 1024
#define PRETTIFIER "*********************************************"

typedef int BOOL;
#define FALSE 0
#define TRUE 1

typedef struct
{
	char *configurationFile;
	
	char *devName;
	int  devfd;            /* device file descriptor */
	pps_handle_t devHandle;
	int reqCaps;
	int availCaps;
	
	FILE *logFile;       /* current data log file */
	char *logPath;       /* path for log files  */
	
	int logyday;      /* yday of current log file */
	char *statusFileName;      /* status log file name */
	
	char * lockFileName;
	
	struct timespec lastpps; /* this is tracked so that duplicates can be filtered out */
	unsigned long   seq; /* pps counter */
	
	int MJD; /* to track day rollover */
	
}ppsdevlog;


static volatile bool quit = false;


static int debugOn=0;
char timestr[64];

static char *
timestamp(time_t tt,int showDate)
{
	struct tm *ts;
	ts=gmtime(&tt);
	if (TRUE==showDate)
		strftime(timestr,64,"%F %T",ts);
	else
		strftime(timestr,64,"%T",ts);
	return timestr;
}

static void 
sighandler_exit(
	int signum) {

	quit = true;
}

static void
ppsdevlog_log_status(
	ppsdevlog *pp,
	char *msg
										)
{
	FILE *statusFile;
	
	if ((statusFile = fopen(pp->statusFileName,"a"))){
		fprintf(statusFile,"%s %s\n",timestamp(time(NULL),TRUE),msg);
		fclose(statusFile);
	}
}

static void 
ppsdevlog_help()
{
	
	printf("\n%s version %s\n",APP_NAME,APP_VERSION);
	printf("Usage: %s [-hvd]\n",APP_NAME);
	printf("-c <file> use the configuration file <file>\n");
	printf("-h print this help message\n");
	printf("-v print version\n");
	printf("-d enable debugging\n");
}

static void
ppsdevlog_init(
	ppsdevlog *pp
)
{
	struct timeval tv;
	
	pp->reqCaps = PPS_CAPTURECLEAR;
	pp->availCaps = 0;
	pp->configurationFile=strdup(DEFAULT_CONFIG);
	
	pp->lockFileName=strdup(DEFAULT_LOCK);
	pp->logFile=NULL;
	pp->statusFileName=strdup(DEFAULT_STATUS_FILE);
	pp->logPath=strdup(DEFAULT_LOG_DIR);
	pp->devName=strdup(DEFAULT_DEV);
	pp->MJD=-1;
	gettimeofday(&tv,NULL);
	pp->lastpps.tv_nsec=0;
	pp->lastpps.tv_sec=tv.tv_sec; /* set this so that we can track an initial timeout on the 1 pps */
}

static int
ppsdevlog_set_config(
		ListEntry *last,
		const char *section,
		const char *token,
		char **val,
		BOOL *ok,
		BOOL required)
{
	char *stmp;
	if (list_get_string(last,section,token,&stmp)){
		// ok is not set
		if (NULL != *val)
			free(*val);
		*val=strdup(stmp);
	}
	else{
		int err = config_file_get_last_error(NULL,0);
		if (err==TokenNotFound && required){
			fprintf(stderr,"Missing entry for %s::%s\n",section,token);
			*ok = FALSE;
			return FALSE;
		}
		else if (err==TokenNotFound){
			// ok is not false
			return FALSE;
		}
		else if (err==ParseFailed){
			fprintf(stderr,"Syntax error %s::%s\n",section,token);
			*ok = FALSE;
			return FALSE;
		}
	}
	return true;
}

static void
ppsdevlog_load_config(
	ppsdevlog *pp
)
{
	
	int configOK;
	
	ListEntry *last;
	if (!configfile_parse_as_list(&last,pp->configurationFile)){
		fprintf(stderr,"Unable to open the configuration file %s - exiting\n",pp->configurationFile);
		exit(EXIT_FAILURE);
	}
	
	ppsdevlog_set_config(last,"main","device",&(pp->devName),&configOK,FALSE);
	ppsdevlog_set_config(last,"main","lock file",&(pp->lockFileName),&configOK,FALSE);
	ppsdevlog_set_config(last,"main","log path",&(pp->logPath),&configOK,FALSE);
	ppsdevlog_set_config(last,"main","status file",&(pp->statusFileName),&configOK,TRUE);
}

static void
ppsdevlog_make_lock(
	ppsdevlog *pp){

	FILE *fd;
	pid_t pid;
	
	/* Check whether the daemon is already running and exit if it is */
	if (!access(pp->lockFileName, R_OK)){
		if ((fd = fopen(pp->lockFileName, "r"))){
			if (fscanf(fd, "%*s %d", &pid) != 1){
				fprintf(stderr,"Empty lock file\n");
				exit(EXIT_FAILURE);
			}
			fclose(fd);
			if (!kill(pid, 0) || errno == EPERM){
				fprintf(stderr,
					"A ppslogdev is already running as process %d\n"
					"If it is no longer running, remove %s\n",
					  pid,pp->lockFileName);
				exit(EXIT_FAILURE);
			}
			else{
				unlink(pp->lockFileName);
			}
		}
	}
	
	if ((fd = fopen(pp->lockFileName, "w"))){ /* write a new lock file */
		fprintf(fd,"ppslogdev %d\n",getpid());
		fclose(fd);
	}
}

static int
ppsdevlog_open_log(
	ppsdevlog *pp,
	int mjd
									)
{
	char buf[BUFLEN],hn[BUFLEN];
	time_t tt;
	struct tm *gmt;
	struct stat sbuf;
	
	
	/* Open log file for recording time stamps */
	if (NULL != pp->logFile) fclose(pp->logFile);
	
	gethostname(hn,BUFLEN);
	sprintf(buf,"%s/%i.%s",pp->logPath,mjd,hn);

	if (debugOn)
		fprintf(stderr,"Opening %s\n",buf);
	
	/* If the file exists then open it for appending */
	if ((0==stat(buf,&sbuf))){
		pp->logFile= fopen(buf,"a");
		return 1;
	}
	 
	if (!(pp->logFile = fopen(buf,"a")))
		return FALSE;
	else{
		gmt=gmtime(&tt);
		pp->logyday=gmt->tm_yday;
		fprintf(pp->logFile,"# %s system 1 pps log, %s %s\n",hn,APP_NAME,APP_VERSION);
		fprintf(pp->logFile,"# Delay = 0\n");
		fflush(pp->logFile);
		return TRUE;
	}
}

int 
ppsdevlog_open_source(
	ppsdevlog *pp)
{
	pps_params_t params;
	int ret;

	if (debugOn)
		fprintf(stderr,"Trying PPS source %s\n", pp->devName);

	/* Check that there is a device */
	if ((ret = open(pp->devName, O_RDWR)) < 0){
		ppsdevlog_log_status(pp,"Unable to open device");
		return ret;
	}

	if ((ret = time_pps_create(ret, &(pp->devHandle))) < 0){
		ppsdevlog_log_status(pp,"Cannot create a PPS source from device ");
		return -1;
	}
	if (debugOn)
		fprintf(stderr,"Found PPS source %s\n", pp->devName);

	/* Get the device capabilities */
	if ((ret = time_pps_getcap(pp->devHandle, &(pp->availCaps))) < 0){
		ppsdevlog_log_status(pp,"Cannot get device capabilities");
		return -1;
	}
	
	/* Check that we got the desired mode */
	if ((pp->availCaps & pp->reqCaps) == pp->reqCaps) {
		/* Get current parameters */
		ret = time_pps_getparams(pp->devHandle, &params);
		if (ret < 0) {
			ppsdevlog_log_status(pp,"Cannot get parameters");
			return -1;
		}
		params.mode |= pp->reqCaps;
		ret = time_pps_setparams(pp->devHandle, &params); /* this requires write permission for the device */
		if (ret < 0) {
			ppsdevlog_log_status(pp,"Cannot set the parameters");
			return -1;
		}
	} else {
		ppsdevlog_log_status(pp,"The selected mode is not supported");
		return -1;
	}

	fflush(stdout);
	return TRUE;
}



int 
ppsdevlog_read(
	ppsdevlog *pp)
{
	struct timespec timeout = { 3, 0 };
	struct timespec ppsrdg;
	
	pps_info_t infobuf;
	int ret;

	while (!quit){ /* loop here until we get a new reading or we get a SIGxxx */
		if (pp->availCaps & PPS_CANWAIT){ /* waits for the next event */
			ret = time_pps_fetch(pp->devHandle, PPS_TSFMT_TSPEC, &infobuf,
						&timeout);
		}
		else {
			usleep(TSLEEP);
			ret = time_pps_fetch(pp->devHandle, PPS_TSFMT_TSPEC, &infobuf,
						&timeout);
		}
	
		if (ret < 0 || quit) {
			if (errno == EINTR || quit) {
				if (debugOn)
					fprintf(stderr, "time_pps_fetch() EINTR\n");
				return -1;
			}
			if (debugOn)
				fprintf(stderr, "time_pps_fetch() error %d\n", ret);
			return -1;
		}
		
		ppsrdg.tv_sec=0;
		ppsrdg.tv_nsec=0;
		pp->seq=0;
		if (pp->reqCaps & PPS_CAPTUREASSERT) {
			ppsrdg = infobuf.assert_timestamp;
			pp->seq = infobuf.assert_sequence;
		} else if (pp->reqCaps & PPS_CAPTURECLEAR) {
			ppsrdg = infobuf.clear_timestamp;
			pp->seq = infobuf.clear_sequence;
		}
	
		if (ppsrdg.tv_sec != pp->lastpps.tv_sec || 
			(ppsrdg.tv_sec == pp->lastpps.tv_sec && ppsrdg.tv_nsec != pp->lastpps.tv_nsec) /* ignore duplicates */
		){
			pp->lastpps = ppsrdg;
			return 0;
		}
	}

	return 0;
}

#define NSEC_PER_SEC 1000000000L
static void
ppsdevlog_log(
	ppsdevlog *pp)
{
	if (debugOn)
		fprintf(stderr,"timestamp: %ld, sequence: %ld, offset: % 6ld\n", pp->lastpps.tv_sec, pp->seq, pp->lastpps.tv_nsec);
	int tsns= pp->lastpps.tv_nsec;
	time_t tt = pp->lastpps.tv_sec;
	if (tsns > NSEC_PER_SEC / 2){
		tsns -= NSEC_PER_SEC ;
		tt++;
	}
	
	fprintf(pp->logFile,"%s %i\n",timestamp(tt,FALSE),-tsns); /* Note the sign for the timestamp. Our convention is that the PPS is the reference */
	fflush(pp->logFile);
}


/*
 * 
 */

int main(
	int argc, 
	char *argv[])
{
	struct sigaction sigact;
	int opt,ret;
	time_t tt;
	int mjd;
	struct timeval tvnow;
	
	ppsdevlog pp;
	
	ppsdevlog_init(&pp);
	
	while ((opt=getopt(argc,argv,"c:dhv")) != EOF){
		switch (opt){
			case 'c': /* use alternate configuration file */
				if (optarg)
					pp.configurationFile = strdup(optarg);
				break;
			case 'd':/* enable debugging */
				fprintf(stderr,"Debugging to stderr is ON\n");
				debugOn=1;
				break;
			case 'h': /* print help */
				ppsdevlog_help();
				return 0;
				break;
			case 'v': /* print version */
				printf("\n%s version %s, last modified %s\n",
					APP_NAME,APP_VERSION, LAST_MODIFIED);
				printf("\nThis ain't no stinkin' Perl script!\n");
				return 0;
				break;
			default:
				break;
		}
	}
	
	ppsdevlog_load_config(&pp);
	
	ppsdevlog_make_lock(&pp);
	
	ppsdevlog_log_status(&pp,"started");
	
	if (FALSE == ppsdevlog_open_source(&pp)){
		ppsdevlog_log_status(&pp,"Failed to get pps device");
		exit(EXIT_FAILURE);
	}
	
	sigact.sa_handler = sighandler_exit;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT,  &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);

	
	/* Data capture loop */
	while (1) {
		
		ret = ppsdevlog_read(&pp);
		/* Rotate log ? */
		if (ret == 0){ /* use the timestamp in the PPS event */
			tt = pp.lastpps.tv_sec;
			if ( pp.lastpps.tv_nsec> NSEC_PER_SEC / 2)
				tt++;
			mjd = tt/86400 + 40587;
		}
		else /* we want an empty log if nothing is working */
			mjd = (int)(time(&tt)/86400 + 40587);
		
		if (mjd != pp.MJD){
			if (TRUE==ppsdevlog_open_log(&pp,mjd))
				pp.MJD=mjd;
		}
			
		if (ret == 0){		
			ppsdevlog_log(&pp);
		}
		
		if ((ret < 0 && errno == EINTR) || quit) {
			ret = 0;
			break;
		}
		
		if (ret < 0 && errno != ETIMEDOUT) /* exit on all other errors except a timeout since we tolerate a timeout for a while */
			break;
		
		gettimeofday(&tvnow,NULL);
		
		if (tvnow.tv_sec - pp.lastpps.tv_sec > DEFAULT_TIMEOUT){
			if (debugOn)
				fprintf(stderr,"timeout\n");
			break;
		}
	}
	
	time_pps_destroy(pp.devHandle);

	ppsdevlog_log_status(&pp,"stopped");

	return ret;
}

