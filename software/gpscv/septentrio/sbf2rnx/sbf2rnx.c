/* sbf2rnx - SBF to RINEX converter
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Michael J. Wouters
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
 * Usage: sbf2rnx [-dhv]
 *	-d turn debugging on
 *  -h print help
 *  -v print version number
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "sbfread.h"

#define APP_AUTHORS "Michael Wouters"
#define APP_NAME    "sbf2rnx"
#define APP_VERSION "0.1"

#define MAXSTR 1024

#define TRUE 1
#define FALSE 0

/* globals */

int debugOn = TRUE;

static void
sbf2rnx_print_help()
{
	printf("\n%s version %s\n",APP_NAME,APP_VERSION);
	printf("Usage: %s [-hvdi:o:] sbffile\n",APP_NAME);
	printf("-h print this help message\n");
	printf("-v print version\n");
	printf("-d enable debugging\n");
	
	printf("-i INTERVAL interval in the RINEX file\n");
	printf("-o FILE     set the file name for RINEX output\n");
}

int
main(
	int argc,
	char **argv)
{
	
	int c;
	char *rnxFileName;
	char *sbfLogFileName;
	
	int rnxInterval = 30;
	
	/* Process the command line options */
	while ((c=getopt(argc,argv,"dhvi:o:")) != -1){
		switch(c){
			case 'd':	/* enable debugging */
				printf("Debugging is ON\n");
				debugOn=TRUE;
				break;
			case 'h': /* print help */
				sbf2rnx_print_help();
				return EXIT_SUCCESS;
				break;
			case 'o': /* set output file name */
				rnxFileName = strndup(optarg,MAXSTR-1);
				break;
			case 'v': /* print version */
				printf("\n %s version %s\n",APP_NAME,APP_VERSION);
				printf(" Written by %s\n",APP_AUTHORS);
				printf(" This ain't no stinkin' Perl script!\n");
				return EXIT_SUCCESS;
				break;
			case '?': /* WTF */
				sbf2rnx_print_help();
				return EXIT_FAILURE;
				break;
		}
	}
 
	if (optind >= argc){
		sbf2rnx_print_help();
		return EXIT_FAILURE;
	}
	
	sbfLogFileName = strdup(argv[1]);
	FILE *fd = fopen(sbfLogFileName,"r");
	if (!fd){
		fprintf(stderr,"Unable to open %s\n",sbfLogFileName);
		return EXIT_FAILURE;
	}
	fclose(fd);
	
	SBFData_t   SBFData;
	uint8_t     SBFBlock[MAX_SBFSIZE];
	MeasEpoch_t MeasEpoch;
	
	InitializeSBFDecoding(sbfLogFileName, &SBFData); /* initialize the data containers that will be used to decode the SBF blocks */

	while (GetNextBlock(&SBFData, SBFBlock, BLOCKNUMBER_ALL, BLOCKNUMBER_ALL,START_POS_CURRENT | END_POS_AFTER_BLOCK) == 0){
		/* SBFBlock contains an SBF block as read from the file.  All
			blocks are sent to sbfread_MeasCollectAndDecode().  The
			function collects measurement-related blocks (MeasEpoch,
			MeasExtra, Meas3Ranges, Meas3Doppler, etc...), and returns true
			when a complete measurement epoch is available.  The decoded
			measurement epoch containing all observables from all
			satellites is provided in the MeasEpoch structure. */
		if (sbfread_MeasCollectAndDecode(&SBFData, SBFBlock, &MeasEpoch, SBFREAD_ALLMEAS_ENABLED)){
			int i,tow;
			tow = MeasEpoch.TOW_ms;
			if (tow/1000 % rnxInterval == 0){
				printf("TOW:%d\n", tow/1000);
				for (i = 0; i < MeasEpoch.nbrElements; i++){ /* go through all satellites */
					uint32_t SigIdx;
					printf("%3d ", MeasEpoch.channelData[i].PRN);
					for (SigIdx = 0; SigIdx < MAX_NR_OF_SIGNALS_PER_SATELLITE; SigIdx++){
						const MeasSet_t* const MeasSet = &(MeasEpoch.channelData[i].measSet[0][SigIdx]);
						if (MeasSet->flags != 0){
							SignalType_t SignalType = (SignalType_t)MeasSet->signalType;
								if (SignalType == SIG_GPSL1CA || SignalType == SIG_GLOL1CA || SignalType == SIG_GALE1BC || SignalType == SIG_BDSB1I){
									printf("(%.3f) ", MeasSet->PR_m);
								}
						}
					}
					printf("\n");
				}
			}
		}
	}
	
 return EXIT_SUCCESS;
 
}
