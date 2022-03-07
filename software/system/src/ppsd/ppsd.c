/* 
 * ppsd - a daemon that produces 1 pps ticks on a DIO port
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Michael J. Wouters
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 * Usage: ppsd [-dhvs] [-o delay ]
 *	-d turn debugging on
 *  -h print help
 *  -s use soft timing
 *  -v print version number
 *  -o delay offset for pps in us
 *
 * Requires a configuration file /usr/local/etc/ppsd.conf
 * The file has one entry, the pps offset in 1 ms
 *
 * Modification history
 * 24-07-2000 MJW First version 1.0
 * 19-04-2002 MJW Modified to produce 1 pps with specified delay (v1.1)
 * 23-04-2002 MJW ppsd now picks up delay from a configuration file.
 *                This is mainly to keep the delay in one place, where
 *                it can be referenced by whatever software needs it.(v1.1.1)
 * 22-11-2002 MJW Bug. Incorrect operation for offsets such that busy loop
 *								starts in the next second .... (v1.1.2)
 * 11-05-2015 MJW Fixups forchanges in Linux ... (v1.2.0)
 * 12-05-2015 MJW Support for SIO81865/66
 * 22-03-2018 MJW Fixups for time steps (v2.0.1)
 * 2022-03-04 MJW Soft timing mode, better configuration
 * 
 */
 	
/* Compile with gcc -O -Wall -o ppsd ppsd.c */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <string.h>
#include <paths.h>
#include <syslog.h>
#include <signal.h>
#include <dirent.h>

#include <time.h>
//#include <asm/io.h>
#include <sys/io.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <configurator.h>

#define APP_NAME "ppsd"
#define APP_VERSION "3.0.0"
#define PID_FILE _PATH_VARRUN "ppsd.pid"
#define DEFAULT_CONFIG "/usr/local/etc/ppsd.conf"
#define DEFAULT_LOG_DIR	 "/home/ntp-admin/logs/"

#define BUFLEN 1024
#define PRETTIFIER "*********************************************"

typedef int BOOL;
#define TRUE 1
#define FALSE 0

/* Globals */
static int debugOn=0;

#ifdef USE_PARALLEL_PORT

#define PPBASE 0x378 /* base address of the parallel port */
  
#endif


#ifdef USE_SIO8186x


#define CHIP_ID_81865 0x0704
#define CHIP_ID_81866 0x1010
#define VENDOR_ID_8186X    0x1934

#define MASTER_REG   0x2E /* the INDEX port */
#define SLAVE_REG    0x4E /* the DATA port  */
#define LDN_REG      0x07
#define GPIO_LDN_REG 0x06

/* This is for the 81865 */
#define GPIO0_OER 0xF0  /* GPIO port 0 output enable register, write 0x0F for 50~53 OP and 54~57 IP */
#define GPIO0_ODR 0xF1  /* GPIO port 0 output data register */
#define GPIO0_PSR 0xF2  /* GPIO port 0 pin status register */

//#define RegGPIO5_OER 0xA0  /* GPIO port 5 output enable register, write 0x0F for 50~53 OP and 54~57 IP */
//#define RegGPIO5_ODR 0xA1  /* GPIO port 5 output data register */
//#define RegGPIO5_PSR 0xA2  /* GPIO port 5 pin status register */

//#define GPIO0_OER 0xF0  /* GPIO port 0 output enable register, write 0x0F for 50~53 OP and 54~57 IP */
//#define GPIO0_ODR 0xF1  /* GPIO port 0 output data register */
//#define GPIO0_PSR 0xF2  /* GPIO port 0 pin status register */


/* This is for the 81866 */
#define GPIO8_OER 0x88           /* GPIO port 8 output enable register */
#define GPIO8_ODR (GPIO8_OER+1)  /* output data register */
#define GPIO8_PSR (GPIO8_OER+2)  /* pin status register */


unsigned char entryKey[2] = {0x87,0x87}; /* SIO Unlock key write twice */

static unsigned char INDEX_PORT,DATA_PORT;
static unsigned char GPIO_OER,GPIO_ODR,GPIO_PSR;


static unsigned char 
F8186X_read(
  unsigned char index)
{
    outb(index,INDEX_PORT);
    return inb(DATA_PORT);
}

static void 
F8186X_write(
  unsigned char index,
  unsigned char data)
{
    outb(index,INDEX_PORT);
    outb(data,DATA_PORT);
}

static void 
F8186X_select_LDN_GPIO()
{
	F8186X_write(LDN_REG, GPIO_LDN_REG);
}

static void 
F8186X_unlock() 
{
	
  ioperm(INDEX_PORT,2,1); /* get permission to write to the ports*/
	/* write twice to unlock/enable configuration */
	outb(0x87, INDEX_PORT);
	outb(0x87, INDEX_PORT);
}

static void 
F8186X_close()
{
   F8186X_write(INDEX_PORT,0xaa);  /* unlock/disable configuration */
   ioperm(INDEX_PORT,2,0); /* release write permissions */
}

static int 
F8186X_check()
{
  unsigned int chipID, vendorID;
  
	//Verify Chip ID and Vendor ID
  chipID = F8186X_read(0x20);
  chipID = (chipID<<8) | F8186X_read(0x21);
	
	if (chipID == CHIP_ID_81865){
		GPIO_OER = GPIO0_OER;
		GPIO_ODR = GPIO0_ODR;
		GPIO_PSR = GPIO0_PSR;
		if (debugOn) fprintf(stderr,"Using GPIO0 for 81865\n");
	}
	else if (chipID == CHIP_ID_81866){
		GPIO_OER = GPIO8_OER;
		GPIO_ODR = GPIO8_ODR;
		GPIO_PSR = GPIO8_PSR;
		if (debugOn) fprintf(stderr,"Using GPIO8 for 81866\n");
	}
	else{
		return FALSE;
	}
		
  if (debugOn) {
		fprintf(stderr,"Chip ID : 0x%04x\n", chipID);
  
		vendorID = F8186X_read(0x23);
		vendorID = (vendorID << 8) | F8186X_read(0x24);
		fprintf(stderr,"Vendor ID : 0x%04x\n", vendorID);
	}

	return TRUE;
}


// FIXME logic is wrong!
static int
F8186X_init()
{
	INDEX_PORT = MASTER_REG;
	DATA_PORT = INDEX_PORT + 1;
	F8186X_unlock();
	if (!F8186X_check()){
		INDEX_PORT = SLAVE_REG;
		DATA_PORT = INDEX_PORT + 1;
		F8186X_unlock();
		if (!F8186X_check()){ // FIXME
				fprintf(stderr,"Init fail(1) .. not a F81865 or F81866\n");
				return FALSE;
		}
	}
	return TRUE;
}



// static int
// SIOF8186x_init()
// {
// 	reg_sio = MASTER_REG;
// 	reg_data = reg_sio + 1;
// 	SIOF8186x_unlock();
// 	if (SIOF8186x_check_ID()){
// 		reg_sio = SLAVE_REG;
// 		reg_data = reg_sio + 1;
// 		SIOF8186x_unlock();
// 		if (SIOF8186x_check_ID()){ // FIXME
// 				fprintf(stderr,"Init fail(1) .. not a F81865 or F81866\n");
// 				return FALSE;
// 		}
// 	}
// 	return TRUE;
//}

#endif


typedef struct _Tppsd
{
	uid_t uid;
	int offset;
	
	FILE *logFile;       /* current data log file */
	char *logPath;       /* path for log files  */
	
	BOOL hardTiming;      /* hard/soft timing */
	
	int MJD; 
	char *statusFileName; /* status log file name */
	char *configurationFile;
	
	char *hostname;
	
} Tppsd;

static void ppsd_print_help();
static void ppsd_init(Tppsd *ppsd);
static void ppsd_load_config(Tppsd *ppsd);
static int  ppsd_set_config_str(ListEntry *last,const char *section,const char *token,char **val,BOOL *ok,BOOL required);
static int  ppsd_set_config_int(ListEntry *last,const char *section,const char *token,int   *val,BOOL *ok,BOOL required);
static int  ppsd_set_config_bool(ListEntry *last,const char *section,const char *token,BOOL  *val,BOOL *ok,BOOL required);

static void
ppsd_print_help()
{
	printf("\nppsd version %s\n",APP_VERSION);
	printf("Usage: ppsd [-hvds] [-o delay] [-c config]\n");
	printf("-h print this help message\n");
	printf("-v print version\n");
	printf("-d enable debugging\n");
	printf("-s soft timing (default is hard)\n");
	printf("-o output delay in us\n");
	printf("-c <config> set configuration file\n");
}

static void
ppsd_init(
	Tppsd *pp
)
{
	char hn[BUFLEN];
	
	pp->MJD=-1;
	pp->offset=1000;
	pp->hardTiming = FALSE;
	
	pp->configurationFile=strdup(DEFAULT_CONFIG);
	
	pp->logFile=NULL;
	pp->logPath=strdup(DEFAULT_LOG_DIR);
	
	pp->hostname=strdup("localhost");
	gethostname(hn,BUFLEN);
	pp->hostname = strdup(hn); /* this will not usually be the FQDN - specify that via config file */
	
	
}

static int
ppsd_set_config_str(
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
	return TRUE;
}


static int
ppsd_set_config_int(
		ListEntry *last,
		const char *section,
		const char *token,
		int *val,
		BOOL *ok,
		BOOL required)
{
	int itmp;
	if (list_get_int(last,section,token,&itmp)){
		*val=itmp;
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
	return TRUE;
}

static int
ppsd_set_config_bool(
		ListEntry *last,
		const char *section,
		const char *token,
		BOOL *val,
		BOOL *ok,
		BOOL required)
{
	char *stmp;
	if (list_get_string(last,section,token,&stmp)){
		lowercase(stmp);
		if ((0==strcmp(stmp,"off")) || (0==strcmp(stmp,"no")) || (0==strcmp(stmp,"false"))|| (0==strcmp(stmp,"0"))){
			*val = FALSE;
		}
		else if ((0==strcmp(stmp,"on")) || (0==strcmp(stmp,"yes")) || (0==strcmp(stmp,"true")) ||(0==strcmp(stmp,"1"))){
			*val = TRUE;
		}
		else{
			fprintf(stderr,"Syntax error %s::%s\n",section,token);
			return FALSE;
		}
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
	return TRUE;
}

static void
ppsd_load_config(
	Tppsd *pp
	)
{
	int configOK;
	
	ListEntry *last;
	if (!configfile_parse_as_list(&last,pp->configurationFile)){
		fprintf(stderr,"Unable to open the configuration file %s - exiting\n",pp->configurationFile);
		exit(EXIT_FAILURE);
	}
	
	ppsd_set_config_str(last,"main","log path",&(pp->logPath),&configOK,FALSE);
	ppsd_set_config_str(last,"main","host name",&(pp->hostname),&configOK,FALSE);
	ppsd_set_config_int(last,"main","delay",&(pp->offset),&configOK,FALSE);
	ppsd_set_config_bool(last,"main","hard timing",&(pp->hardTiming),&configOK,FALSE);
}

static int
ppsd_open_log(
	Tppsd *pp,
	int mjd
									)
{
	char buf[BUFLEN];
	struct stat sbuf;
	
	/* Open log file for recording time stamps when using soft timing*/
	if (NULL != pp->logFile) fclose(pp->logFile);
	
	sprintf(buf,"%s/%i.%s.ppsd",pp->logPath,mjd,pp->hostname);

	if (debugOn)
		fprintf(stderr,"Opening %s\n",buf);
	
	/* If the file exists then open it for appending */
	if ((0==stat(buf,&sbuf))){
		pp->logFile= fopen(buf,"a");
		return TRUE;
	}
	 
	if (!(pp->logFile = fopen(buf,"a"))){
		fprintf(stderr,"Unable to open %s\n",buf);
		exit(EXIT_FAILURE);
	}
	else{
		fprintf(pp->logFile,"# %s system 1 pps offset log, %s %s\n",pp->hostname,APP_NAME,APP_VERSION);
		fprintf(pp->logFile,"# Delay = %d\n",pp->offset);
		fflush(pp->logFile);
		return TRUE;
	}
}

static void
ppsd_log(
	Tppsd *pp,
	struct timeval *tv)
{
	struct tm *ts;
	char timestr[64];
	ts=gmtime((&(tv->tv_sec)));
	strftime(timestr,63,"%H:%M:%S",ts);
	fprintf(pp->logFile,"%s %i\n",timestr,(int) tv->tv_usec);
	fflush(pp->logFile);
}

/* 
 * 
 */

int
main(
	int argc,
	char **argv)
{

	Tppsd ppsd;
	int c;
	FILE *str;
	pid_t pid;
	
	int offset;
	int opts=FALSE,opto=FALSE,optc=FALSE;
	char *configurationFile=NULL;
	
	int tstart;
	struct sched_param	sched;
	long twait;
	struct timeval tv;
	long tloop;
	
#ifdef USE_PARALLEL_PORT
	int j;
#endif
	
#ifdef USE_SIO8186x
	unsigned char dout;
	unsigned char datagpio0;
	unsigned char bitmask;strdup(
#endif
	
	ppsd_init(&ppsd);
	
	/* Process the command line options */
	while ((c=getopt(argc,argv,"dhsvo:c:")) != -1){
		switch(c)
		{
			case 'c':
				configurationFile = strdup(optarg);
				optc=TRUE;
				break;
			case 'd':	/* enable debugging */
				printf("Debugging is ON\n");
				debugOn=TRUE;
				break;
			case 'h': /* print help */
				ppsd_print_help();
				return EXIT_SUCCESS;;
				break;
			case 's': /* set soft timing */
				opts = TRUE;
				break;
			case 'o': /* output delay */
				if (optarg){
					if (1==sscanf(optarg,"%i",&offset)){
						if (abs(offset) > 999999){
							fprintf(stderr,"Error in argument to option -o\n");
							fprintf(stderr,"Absolute value of delay must be less than 999999\n");
							return EXIT_FAILURE;
						}
						else{
							opto=TRUE;
						}
					}
					else
					{
						fprintf(stderr,"Error in argument to option -o\n");
						return EXIT_FAILURE;
					}
				}
				else
				{
					fprintf(stderr,"Missing argument to option -o\n");
					return EXIT_FAILURE;
				}
				break;
			case 'v': /* print version */
				printf("\n ppsd version %s\n",APP_VERSION);
#ifdef USE_PARALLEL_PORT
				printf("Compiled for the parallel port\n");
#endif
#ifdef USE_SIO8186x
				printf("Compiled for the SIOF8186x\n");
#endif
				printf("This ain't no stinkin' Perl script!\n");
				return EXIT_SUCCESS;
				break;
		}
	}
	
	if (optc){
		free(ppsd.configurationFile);
		ppsd.configurationFile = configurationFile;
	}
	
	ppsd_load_config(&ppsd);
	
	/* Command line options override */
	if (opto){
		ppsd.offset = offset;
	}
	
	if (opts){
		ppsd.hardTiming = FALSE;
	}
	
	if (debugOn){
		fprintf(stderr,"Hard timing is %s\n",(ppsd.hardTiming?"ON":"OFF"));
	}
	
	/* Check whether the daemon is already running and exit if it is */ 
	if (!access(PID_FILE, R_OK)){
		if ((str = fopen(PID_FILE, "r"))){
			if (1 != fscanf(str, "%d", &pid)){
				fprintf(stderr, "ppsd: failed to read the PID lock\n");
				exit(1);	
			}
			fclose(str);
			
			if (!kill(pid, 0) || errno == EPERM){
				fprintf(stderr,
					"A ppsd is already running as process %d\n"
					"If it is no longer running, remove %s\n",
					pid, PID_FILE);
    			exit(1);
			}
		}
	}
	
	/* Are we root ? */
	if ((ppsd.uid = getuid())){
		fprintf(stderr, "ppsd: must be run as root\n");
		exit(1);
	}
	
	/* Set up system logging */
	/* We log to stderr if debugging is on and the console if it is */
	/* available, using our pid. We are an "other system daemon" */
	openlog("ppsd",(debugOn ? LOG_PERROR : 0) | LOG_PID | LOG_CONS, LOG_DAEMON);
	
	/* If we are not debugging then we become a daemon */
	if (!debugOn){
  	if ((pid = fork())){ /* pid=0 is child, otherwise we are the parent */
			if ((str = fopen(PID_FILE, "w"))){
				fprintf(str, "%d\n", pid);
				fclose(str);
			}
		exit(0); /* and the parent must die */
		}
	
		/* If we got here then we are the child */
		if (pid < 0){ /* there was an error - pid should be 0 */
			syslog(LOG_INFO, "fork() failed: %m");
			unlink(PID_FILE);
			exit(1);
		}

		/* Become session leader */
		if (setsid() < 0){
		  syslog(LOG_INFO, "setsid() failed: %m");
		  unlink(PID_FILE);
		  exit(1);
		}
		
		/* Change working directory to /. This is to avoid the wd being   
		 * on a mounted file system, which cannot then be unmounted during
		 * a shutdown because daemons will persist to the last CPU cycle  
		 */
		
		if (-1 == chdir("/")){
			/* Do nothing */
		}
		
		/* Close unneeded file descriptors which may have been inherited 
		 * from tha parent. In this case we close stdin,stdout and stderr.
		 */
		 
		close(0);
		close(1);
		close(2);
		
		/* Clear the file mode creation mask */
		umask(0);
		
		
	}

	syslog(LOG_INFO,"ppsd version %s started with 1 pps offset %i us",
		APP_VERSION, ppsd.offset);

	
	/* For convenience, make negative offset positive */
	
	if (ppsd.offset < 0)
		ppsd.offset += 1000000;

#ifdef USE_SIO8186x
	if (!F8186X_init())
		exit(EXIT_FAILURE);
#endif
	
#ifdef USE_SIO8186x
	F8186X_select_LDN_GPIO();
	datagpio0 = F8186X_read(GPIO_PSR);
	if (debugOn){
		printf("Current GPIO0 value: 0x%02x\n", datagpio0);
	}
#endif
	 
#ifdef USE_PARALLEL_PORT
	/* Get permission to access the parallel port */
	/* ioperm OK because BASE < 0x3ff */
	ioperm(PPBASE,1,1);
	outb(0x00,PPBASE);
#endif 

	/* Become a low priority real-time process */
	sched.sched_priority = sched_get_priority_min(SCHED_FIFO);
	if ( sched_setscheduler(0, SCHED_FIFO, &sched) == -1 )
	{
		syslog(LOG_ERR,"%s",strerror(errno));
		syslog(LOG_ERR,"exiting");
		unlink(PID_FILE);
		exit(EXIT_FAILURE);
	}
	/* Lock memory so we don't get paged out, causing jitter */
	if ( mlockall(MCL_FUTURE|MCL_CURRENT) == -1 )
	{
		syslog(LOG_ERR,"%s",strerror(errno));
		syslog(LOG_ERR,"exiting");
		unlink(PID_FILE);
		exit(EXIT_FAILURE);
	}
	
	/* Main loop
	 * Here, we get the time and sleep till just before the tick.
	 * ppsd then goes into a busy loop waiting for the tick.
	 *
	 */

	
#ifdef USE_SIO8186x
	bitmask = 0x08; /* GPIO 3 is used for PPS output */
	bitmask = 0x80; /* FIXME but on the newest system we need to do this */
	dout= (datagpio0 & ~bitmask) | (0x0 & bitmask); /* set pps low */
	F8186X_write(GPIO_ODR, dout);
#endif
	
	for (;;){
	
		/* Get the time */
		gettimeofday(&tv,NULL);
		tstart=tv.tv_sec;
		twait= ppsd.offset+1000000-tv.tv_usec;
		// if (debugOn) fprintf(stderr,"twait %i\n",(int) twait);
		
		if (ppsd.hardTiming){
			/* We have to go into the busy loop 20 ms before the tick 
			* to have a decent chance of getting and keeping the time
			* slice.
			*/
			if (twait > 20000) 
				twait-=20000;
		}
		
		/* Wait till just before the tick */
		usleep(twait);
		
		if (ppsd.hardTiming){
			/* Loop till we see the tick */
			gettimeofday(&tv,NULL);
			
			/* Are we too late ? If we are then we will immediately output a pulse*/
			if ((tv.tv_sec > tstart && tv.tv_usec > ppsd.offset))
				continue;
				
			/* How many us to go ? */
			if (debugOn)
				fprintf(stderr,"%i us to go\n",(int)
					((tstart+1 -tv.tv_sec)*1000000	+ ppsd.offset - tv.tv_usec));
			tloop = (tstart+1 -tv.tv_sec)*1000000	+ ppsd.offset - tv.tv_usec;
			/* Guard against the system time going backwards while we are looping by restricting tloop */
			while ( tloop > 0 && tloop < 20000 ){
				gettimeofday(&tv,NULL);
				tloop = (tstart+1 -tv.tv_sec)*1000000	+ ppsd.offset - tv.tv_usec;
			}
		}
		else{
			gettimeofday(&tv,NULL); /* BEF timestamp */
		}
		
#ifdef USE_PARALLEL_PORT
		outb(0xff,PPBASE); /* We go high    */ 
		for (j=1;j<=10;j++)/* We wait a bit */
			inb(PPBASE);
		outb(0x00,PPBASE); /* We go low     */
#endif 
		
#ifdef USE_SIO8186x
		/* Read the current values so we don't tread on anyone's toes */
		/* This creates a bit of a delay but this gives the best chance of not stomping on something */
		datagpio0 = F8186X_read(GPIO_PSR);
		dout= (datagpio0 & ~bitmask) | (0xff & bitmask); /* set pps high */
		F8186X_write(GPIO_ODR, dout);
		dout= (datagpio0 & ~bitmask) | (0x0 & bitmask); /* set pps low */
		F8186X_write(GPIO_ODR, dout);
#endif
		
		if (!ppsd.hardTiming){/* log the wakeup time */
			
			time_t tt = tv.tv_sec;
			//if ( pp.lastpps.tv_nsec> NSEC_PER_SEC / 2)
			//	tt++;
			int mjd = tt/86400 + 40587;
		
			if (mjd != ppsd.MJD){
				if (ppsd_open_log(&ppsd,mjd))
					ppsd.MJD=mjd;
			}
			ppsd_log(&ppsd,&tv);
		}
		
		if (debugOn){
			fprintf(stderr,"BEF %i\n",(int) tv.tv_usec);
			gettimeofday(&tv,NULL);
			fprintf(stderr,"AFT %i\n",(int) tv.tv_usec);
		}
	}	

#ifdef USE_SIO8186x
	F8186X_close();
#endif
	syslog(LOG_INFO,"stopped");	
	closelog();	
	return EXIT_SUCCESS;
}
