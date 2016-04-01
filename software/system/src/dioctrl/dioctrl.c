/* 
 * dioctrl - a utility for setting DIO bits on various output devices
 *
 * * The MIT License (MIT)
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
 * Usage: dioctrl [-d] [-h] [-v] bitmask value
 *	-d turn debugging on
 *  -h print help
 *  -v print version number
 *
 * Modification history
 * 
 */
 	

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
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

#define APP_NAME    "dioctrl"
#define APP_VERSION "0.1"
#define APP_AUTHORS "Michael Wouters"

#define TRUE 1
#define FALSE 0

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

static void
dioctrl_print_help()
{
	printf("\n%s version %s\n",APP_NAME,APP_VERSION);
	printf("Usage: %s [-hvd] -b  <mask> val(in hex)\n",APP_NAME);
	printf("-b <mask> bitmask in hex\n");
	printf("-h print this help message\n");
	printf("-v print version\n");
	printf("-d enable debugging\n");
}

static void
dioctrl_print_version()
{
	printf("\n %s version %s\n",APP_NAME,APP_VERSION);
#ifdef USE_PARALLEL_PORT
	printf("Compiled for the parallel port\n");
#endif
#ifdef USE_SIO8186x
	printf("Compiled for the SIOF8186x\n");
#endif
	printf("Written by %s\n",APP_AUTHORS);
	printf("This ain't no stinkin' Perl script!\n");
}

int 
dioctrl_get_int(char *str){
	char *endptr;
	
	errno =0;
	
	int val = strtol(str,&endptr,16);
	
	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0)) {
		perror("strtol");
		exit(EXIT_FAILURE);
	}
	
	if (*endptr != '\0'){
		fprintf(stderr,"Error! Bad parameter %s\n",str);
		exit(EXIT_FAILURE);
	}
	
	return val;
	
}
		
int
main(
	int argc,
	char **argv)
{

	int c;

	unsigned char currVal;
	unsigned char newVal;
	unsigned int bitmask=0xff;
	
	/* Process the command line options */
	while ((c=getopt(argc,argv,"b:dhv")) != -1)
	{
		switch(c)
		{
			case 'd':	/* enable debugging */
				printf("Debugging is ON\n");
				debugOn=TRUE;
				break;
			case 'b':
				bitmask=dioctrl_get_int(optarg);
				break;
			case 'h': /* print help */
				dioctrl_print_help();
				exit(EXIT_SUCCESS);
				break;
			case 'v':
				dioctrl_print_version();
				exit(EXIT_SUCCESS);
				break;
			default:
				dioctrl_print_help();
				exit(EXIT_FAILURE);
		}
	}
	
	if ((argc - optind != 1)){
		printf("\nError!: missing argument\n");
		dioctrl_print_help();
		exit(EXIT_FAILURE);
	}
	
	unsigned int val = dioctrl_get_int(argv[optind]);
	
	if (debugOn)
		printf("mask=0x%0x val=0x%0x (%i,%i)\n",bitmask,val,bitmask,val);
	
	/* Are we root ? */
	if ((getuid())){
		fprintf(stderr, "%s: must be run as root\n",APP_NAME);
		exit(1);
	}
	
#ifdef USE_SIO8186x
	if (!SIOF8186x_init())
		exit(EXIT_FAILURE);
	
	SIOF8186x_select_LDN_GPIO();
	currVal = LpcReadIndirectByte(RegGPIO0_PSR);
	if (debugOn)
		printf("current GPIO0 value = 0x%02x\n", currVal);
	newVal= (currVal & ~bitmask) | (val & bitmask);
	LpcWriteIndirectByte(RegGPIO0_ODR, newVal);
	currVal = LpcReadIndirectByte(RegGPIO0_PSR);
	if (debugOn)
		printf("new GPIO0 value = 0x%02x\n", currVal);
	SIOF8186x_close();
	
#endif
	
#ifdef USE_PARALLEL_PORT
	/* Get permission to access the parallel port */
	/* ioperm OK because BASE < 0x3ff */
	ioperm(PPBASE,1,1);
	currVal = inb(PPBASE);
	if (debugOn) printf("current value = 0x%02x\n",currVal);
	newVal= (currVal & ~bitmask) | (val & bitmask);
	outb(newVal,PPBASE);
	if (debugOn) printf("new value = 0x%02x\n",inb(PPBASE));
#endif 
	

	return EXIT_SUCCESS;
}
