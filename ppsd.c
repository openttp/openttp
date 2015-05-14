/* 
 * ppsd - a daemon that produces ticks on the parallel port
 *
 * Usage: ppsd [-d] [-h] [-v] [-o delay ]
 *	-d turn debugging on
 *  -h print help
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

#define PPSD_VERSION "2.0.0"
#define PID_FILE _PATH_VARRUN "ppsd.pid"
#define PPSD_CONFIG "/usr/local/etc/ppsd.conf"

#define TRUE 1
#define FALSE 0

// #define USE_PARALLEL_PORT

#define USE_SIO8186x

/* Globals */
static int debugOn=0;

#ifdef USE_PARALLEL_PORT

#define PPBASE 0x378 /* base address of the parallel port */
  
#endif


#ifdef USE_SIO8186x

#define MASTERREG   0x2E
#define SLAVEREG    0x4E
#define CHIPID81865 0x0704
#define CHIPID81866 0x1010
#define VENID81865  0x1934

#define RegLDN      0x07
#define RegGPIO_LDN 0x06

#define RegGPIO5_OER 0xA0  /* GPIO port 5 output enable register, write 0x0F for 50~53 OP and 54~57 IP */
#define RegGPIO5_ODR 0xA1  /* GPIO port 5 output data register */
#define RegGPIO5_PSR 0xA2  /* GPIO port 5 pin status register */

#define RegGPIO0_OER 0xF0  /* GPIO port 0 output enable register, write 0x0F for 50~53 OP and 54~57 IP */
#define RegGPIO0_ODR 0xF1  /* GPIO port 0 output data register */
#define RegGPIO0_PSR 0xF2  /* GPIO port 0 pin status register */

unsigned char entryKey[2] = {0x87,0x87}; /* SIO Unlock key write twice */
static unsigned char reg_sio,reg_data; 

static unsigned char 
LpcReadIndirectByte(
  unsigned char index)
{
    outb(index,reg_sio);
    return inb(reg_data);
}

static void 
LpcWriteIndirectByte(
  unsigned char index,
  unsigned char data)
{
    outb(index,reg_sio);
    outb(data,reg_data);
}

static void 
SIOF8186x_select_LDN_GPIO()
{
	LpcWriteIndirectByte(RegLDN, RegGPIO_LDN);
}

static void 
SIOF8186x_unlock() 
{
	int i;
  ioperm(reg_sio,2,1);
	for (i=0; i<2; i++)                /* write twice to unlock */
       outb(entryKey[i], reg_sio);
}

static void 
SIOF8186x_close() 
{
   LpcWriteIndirectByte(reg_sio,0xaa);  
   ioperm(reg_sio,2,0);
}

static int 
SIOF8186x_check_ID() 
{
  unsigned int chipid, vendorid;
  //Read SIO Chip ID and Vendor ID
  chipid = LpcReadIndirectByte(0x20);
  chipid = (chipid<<8)|LpcReadIndirectByte(0x21);
  if (chipid != CHIPID81865 && chipid != CHIPID81866)
    return 1;

  if (debugOn) fprintf(stderr,"Chip ID : 0x%04x\n", chipid);
  vendorid = LpcReadIndirectByte(0x23);
  vendorid = (vendorid<<8)|LpcReadIndirectByte(0x24);
  if (debugOn) fprintf(stderr,"Vendor ID : 0x%04x\n", vendorid);
  return 0;
}

static int
SIOF8186x_init()
{
	reg_sio = MASTERREG;
	reg_data = reg_sio + 1;
	SIOF8186x_unlock();
	if (SIOF8186x_check_ID()){
		reg_sio = SLAVEREG;
		reg_data = reg_sio + 1;
		SIOF8186x_unlock();
		if (SIOF8186x_check_ID()){ // FIXME
				fprintf(stderr,"Init fail(1) .. not a F81865 or F81866\n");
				return FALSE;
		}
	}
	return TRUE;
}

#endif



typedef struct _Tppsd
{
	uid_t uid;
	int offset;
} Tppsd;

static void PPSDPrintHelp();
static void PPSDReadConfig(Tppsd *ppsd,const char *fname);


static void
PPSDPrintHelp()
{
	printf("\nppsd version %s\n",PPSD_VERSION);
	printf("Usage: ppsd [-hvd] [-o delay]\n");
	printf("-h print this help message\n");
	printf("-v print version\n");
	printf("-d enable debugging\n");
	printf("-o output delay in us\n");
}

static void
PPSDReadConfig(
	Tppsd *ppsd,
	const char *fname
	)
{
	FILE *fd;
	if ((fd=fopen(fname,"r")))
	{
		if (1 != fscanf(fd,"%i",&(ppsd->offset)))
		{
			fprintf(stderr,"Error in config file %s\n",fname);
			exit(1);
		}
		fclose(fd);
	}
	else
	{
		fprintf(stderr,"Can't open %s\n",fname);
		exit(1);
	}
	
}

int
main(
	int argc,
	char **argv)
{

	Tppsd ppsd;
	char c;
	FILE *str;
	pid_t pid;
	
	int j,tstart;
	struct sched_param	sched;
	long twait;
	struct timeval tv;

#ifdef USE_SIO8186x
	unsigned char dout;
	unsigned char datagpio0;
#endif
	
	ppsd.offset=0;
	PPSDReadConfig(&ppsd,PPSD_CONFIG);
	
	/* Process the command line options */
	while ((c=getopt(argc,argv,"dhvo:")) != EOF)
	{
		switch(c)
		{
			case 'd':	/* enable debugging */
				printf("Debugging is ON\n");
				debugOn=TRUE;
				break;
			case 'h': /* print help */
				PPSDPrintHelp();
				return 0;
				break;
			case 'o': /* output delay */
				if (optarg)
				{
					if (1==sscanf(optarg,"%i",&(ppsd.offset)))
					{
						if (abs(ppsd.offset) > 999999)
						{
							fprintf(stderr,"Error in argument to option -o\n");
							fprintf(stderr,"Absolute value of delay must be less than "
									"999999\n");
							return 0;
						}
						else
						{
							
						}
					}
					else
					{
						fprintf(stderr,"Error in argument to option -o\n");
						return 0;
					}
				}
				else
				{
					fprintf(stderr,"Missing argument to option -o\n");
					return 0;
				}
				break;
			case 'v': /* print version */
				printf("\n ppsd version %s\n",PPSD_VERSION);
#ifdef USE_PARALLEL_PORT
				printf("Compiled for the parallel port\n");
#endif
#ifdef USE_SIO8186x
				printf("Compiled for the SIOF8186x\n");
#endif
				printf("This ain't no stinkin' Perl script!\n");
				return 0;
				break;
		}
	}
	
	/* Check whether the daemon is already running and exit if it is */ 
	if (!access(PID_FILE, R_OK)){
		if ((str = fopen(PID_FILE, "r"))){
			fscanf(str, "%d", &pid);
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
		chdir("/");
		
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
		PPSD_VERSION, ppsd.offset);

	
	/* For convenience, make negative offset positive */
	
	if (ppsd.offset < 0)
		ppsd.offset += 1000000;

#ifdef USE_SIO8186x
	if (!SIOF8186x_init())
		exit(EXIT_FAILURE);
#endif
	
#ifdef USE_SIO8186x
	SIOF8186x_select_LDN_GPIO();
	if (debugOn){
		printf("Reading GPIO0 port status\n");
		datagpio0 = LpcReadIndirectByte(RegGPIO0_PSR);
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
	dout=0x0;
	LpcWriteIndirectByte(RegGPIO0_ODR, dout);
#endif
	
	for (;;){
	
		/* Get the time */
		gettimeofday(&tv,NULL);
		tstart=tv.tv_sec;
		twait= ppsd.offset+1000000-tv.tv_usec;
		if (debugOn) fprintf(stderr,"twait %i\n",(int) twait);
		/* We have to go into the busy loop 20 ms before the tick 
		 * to have a decent chance of getting and keeping the time
		 * slice.
		 */
		if (twait > 20000) 
			twait-=20000;
	
		/* Wait till just before the tick */
		usleep(twait);
		/* Loop till we see the tick */
		gettimeofday(&tv,NULL);
		
		/* Are we too late ? If we are then we will immediately output a pulse*/
		if ((tv.tv_sec > tstart && tv.tv_usec > ppsd.offset))
			continue;
			
		/* How many us to go ? */
		if (debugOn)
			fprintf(stderr,"%i us to go\n",(int)
				((tstart+1 -tv.tv_sec)*1000000	+ ppsd.offset - tv.tv_usec));
		
		while ((tstart+1 -tv.tv_sec)*1000000	+ ppsd.offset - tv.tv_usec > 0)
			gettimeofday(&tv,NULL);
				
#ifdef USE_PARALLEL_PORT
		outb(0xff,PPBASE); /* We go high    */ 
		for (j=1;j<=10;j++)/* We wait a bit */
			inb(PPBASE);
		outb(0x00,PPBASE); /* We go low     */
#endif 
		
#ifdef USE_SIO8186x
		dout=0xff;
		LpcWriteIndirectByte(RegGPIO0_ODR, dout);
		dout=0x0;
		LpcWriteIndirectByte(RegGPIO0_ODR, dout);
#endif
		gettimeofday(&tv,NULL);
		if (debugOn)
			fprintf(stderr,"%i\n",(int) tv.tv_usec);
	}	

#ifdef USE_SIO8186x
	SIOF8186x_close();
#endif
	syslog(LOG_INFO,"stopped");	
	closelog();	
	return EXIT_SUCCESS;
}
