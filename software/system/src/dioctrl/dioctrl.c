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
#define APP_VERSION "0.2"
#define APP_AUTHORS "Michael Wouters"

#define TRUE 1
#define FALSE 0

/* Globals */
static int debugOn=0;

#ifdef USE_PARALLEL_PORT

#define PPBASE 0x378 /* base address of the parallel port */
  
#endif

#ifdef USE_SIO8186x /* Fintek Super I/O chip */

#define CHIP_ID_81865    0x0704
#define CHIP_ID_81866    0x1010
#define VENDOR_ID_8186X  0x1934

#define MASTER_REG       0x2e
#define SLAVE_REG        0x4e
#define LDN_REG          0x07
#define GPIO_LDN_REG     0x06

/* This is for the 81865 */
#define GPIO0_OER 0xF0  /* GPIO port 0 output enable register, write 0x0F for 50~53 OP and 54~57 IP */
#define GPIO0_ODR 0xF1  /* GPIO port 0 output data register */
#define GPIO0_PSR 0xF2  /* GPIO port 0 pin status register */

/* This is for the 81866 */
#define GPIO8_OER 0x88           /* GPIO port 8 output enable register */
#define GPIO8_ODR (GPIO8_OER+1)  /* output data register */
#define GPIO8_PSR (GPIO8_OER+1)  /* pin status register */


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
	
  ioperm(INDEX_PORT,2,1); /* get permission to write */
	/* write twice to unlock */
	outb(0x87, INDEX_PORT);
	outb(0x87, INDEX_PORT);
}

static void 
F8186X_close()
{
   F8186X_write(INDEX_PORT,0xaa);  /* unlock */
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
dioctrl_get_int(char *str)
{
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
	if (!F8186X_init())
		exit(EXIT_FAILURE);
	
	F8186X_select_LDN_GPIO();
	currVal = F8186X_read(GPIO_PSR);
	if (debugOn)
		printf("old port value = 0x%02x\n", currVal);
	newVal= (currVal & ~bitmask) | (val & bitmask);
	F8186X_write(GPIO_ODR, newVal);
	currVal = F8186X_read(GPIO_PSR);
	if (debugOn)
		printf("new port value = 0x%02x\n", currVal);
	F8186X_close();
	
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
