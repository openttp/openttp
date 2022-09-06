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
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "sbfread.h"

#define APP_AUTHORS "Michael Wouters"
#define APP_NAME    "sbf2rnx"
#define APP_VERSION "0.1"

#define MAXSTR 1024

#define TRUE 1
#define FALSE 0

#define BLANK16 "                "

#define MAX_PRN irMAXPRN // according to sviddef.h

/* globals */

int debugOn = TRUE;


// Candidates for moving to another file
void 
GPS_to_date(unsigned int tow, 
					unsigned int fullWN,
					struct tm *tmGPS)
{
	
	time_t tGPS= 315964800 + tow + fullWN*7*86400; 
	gmtime_r(&tGPS,tmGPS);
}

// Convert SN in dbHz to RINEX indicator
int 
rinex_sn(
	double snraw
)
{
	// MIN(MAX(INT(snraw/6),1),9)
	int sn = snraw/6;
	if (sn < 1) sn =1;
	if (sn > 9) sn =9;
	return sn;
}

char flagBuf[3];
char *
rinex_format_flags(
	int  lli,
	int  sn)
{
	if (lli == -1 && sn == -1)
		strcpy(flagBuf,"  ");
	else if (lli == -1)
		sprintf(flagBuf," %1d",sn);
	else if (sn == -1)
		sprintf(flagBuf,"%1d ",lli);
	return flagBuf;
}

static void
sbf2rnx_print_help()
{
	printf("\n%s version %s\n",APP_NAME,APP_VERSION);
	printf("Usage: %s [-hvdi:o:] sbffile\n",APP_NAME);
	printf("-h print this help message\n");
	printf("-v print version\n");
	printf("-d enable debugging\n");
	
	printf("-i INTERVAL interval in the RINEX file (default is 30 s)\n");
	printf("-o FILE     set the file name for RINEX output\n");
}

int
main(
	int argc,
	char **argv)
{
	
	int c;
	char *rnxObsFileName;
	char *sbfLogFileName;
	
	int rnxInterval = 30;
	rnxObsFileName = strdup("test.rnx");
	
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
				free(rnxObsFileName);
				rnxObsFileName = strndup(optarg,MAXSTR-1);
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
		fprintf(stderr,"Unable to open %s for reading\n",sbfLogFileName);
		return EXIT_FAILURE;
	}
	fclose(fd);
	
	FILE *obsFile = fopen(rnxObsFileName,"w");
	if (!obsFile){
		fprintf(stderr,"Unable to open %s for writing\n",rnxObsFileName);
		return EXIT_FAILURE;
	}
	
	SBFData_t   SBFData;
	uint8_t     SBFBlock[MAX_SBFSIZE];
	MeasEpoch_t MeasEpoch;
	
	InitializeSBFDecoding(sbfLogFileName, &SBFData); /* initialize the data containers that will be used to decode the SBF blocks */

	//char buf[MEAS3_SAT_MAX][MAX_NR_OF_SIGNALS_PER_SATELLITE*16+3+1];

	// To sort PRNs, use an array (index is SVID 1)  which contains the index of the corresponding measurement set
	int prnIDs[MAX_PRN+1];
	
	while (GetNextBlock(&SBFData, SBFBlock, BLOCKNUMBER_ALL, BLOCKNUMBER_ALL,START_POS_CURRENT | END_POS_AFTER_BLOCK) == 0){
		memset((void *) prnIDs,-1,(MAX_PRN+1)*sizeof(int));
		
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
				
				// Sort the measurements into ascending PRN
				for (i = 0; i < MeasEpoch.nbrElements; i++){
					 const MeasChannel_t* const ChannelData = &(MeasEpoch.channelData[i]);
					 //fprintf(obsFile,"%d\n",ChannelData->PRN);
					 if agIsValid(ChannelData->PRN){
						 prnIDs[ChannelData->PRN] = i;
					 }
				}
				
				// Count sats
				int nSats = 0;
				for (i=1;i<=MAX_PRN;i++){
					int measIndex = prnIDs[i];
					if ( measIndex != -1){
						if (agIsGPS(i)) nSats += 1;
					}
				}
				
				// Header for the observation record
				struct tm tmGPS;
				GPS_to_date(MeasEpoch.TOW_ms/1000,MeasEpoch.WNc,&tmGPS); 
				
				// Epoch flag currently set to 0 - could use this ...
				fprintf(obsFile,"> %4d %2.2d %2.2d %2.2d %2.2d%11.7f  %1d%3d%6s%15.12lf\n",
					tmGPS.tm_year+1900,tmGPS.tm_mon+1,tmGPS.tm_mday,tmGPS.tm_hour,tmGPS.tm_min,tmGPS.tm_sec + (double) (MeasEpoch.TOW_ms % 1000)/1000.0,
					0,nSats," ",0.0);
								
				for (i=1;i<=MAX_PRN;i++){
					int measIndex = prnIDs[i];
					if ( measIndex != -1){
						if (agIsGPS(i)){
							fprintf(obsFile,"G%02d",i);
							uint32_t SigIdx;
							
							char cL1CA[17];strcpy(cL1CA,BLANK16);
							char cL1P[17];strcpy(cL1P,BLANK16);
							char pL1P[17];strcpy(pL1P,BLANK16);
							char cL2P[17];strcpy(cL2P,BLANK16);
							char pL2P[17];strcpy(pL2P,BLANK16);
							char cL2C[17];strcpy(cL2C,BLANK16);
							char pL2C[17];strcpy(pL2C,BLANK16);
							
							for (SigIdx = 0; SigIdx < MAX_NR_OF_SIGNALS_PER_SATELLITE; SigIdx++){
								const MeasSet_t* const measSet = &(MeasEpoch.channelData[measIndex].measSet[0][SigIdx]); /* main antenna */
								if (measSet->flags != 0){
									SignalType_t sig = (SignalType_t) measSet->signalType;
									int sn = -1;
									if (measSet->CN0_dBHz != F32_NOTVALID)
										sn = rinex_sn(measSet->CN0_dBHz);
									switch (sig){
										case SIG_GPSL1CA: // GPS L1 C/A
											if (measSet->PR_m != F64_NOTVALID){
												snprintf(cL1CA,17,"%14.3lf%2s",measSet->PR_m,rinex_format_flags(-1,sn));
											}
											/* discard carrier phases with half-cycle ambiguities */
											if ((measSet->flags & MEASFLAG_HALFCYCLEAMBIGUITY) == 0 && measSet->L_cycles != F64_NOTVALID){
												// FIXME truncate phase if it will overflow ! 
												snprintf(pL1P, 17,"%14.3lf%2s",measSet->L_cycles,rinex_format_flags(-1,sn)); // repeated for L1P to make sure we get it
											}
											break;
										case SIG_GPSL1P:  // GPS L1 P(Y) 
											if (measSet->PR_m != F64_NOTVALID){
												snprintf(cL1P, 17,"%14.3lf%2s",measSet->PR_m,rinex_format_flags(-1,sn));
											}
											// FIXME need this if GPSL1CA is masked out
											//if ((measSet->flags & MEASFLAG_HALFCYCLEAMBIGUITY) == 0 && measSet->L_cycles != F64_NOTVALID){
											//	snprintf(pL1P, 17,"%14.3lf%2s",measSet->L_cycles,rinex_format_flags(-1,sn));
											//}
											break;
										case SIG_GPSL2P:  // GPS L2 P(Y)
											if (measSet->PR_m != F64_NOTVALID){
												snprintf(cL2P, 17,"%14.3lf%2s",measSet->PR_m,rinex_format_flags(-1,sn));
											}
											if ((measSet->flags & MEASFLAG_HALFCYCLEAMBIGUITY) == 0 && measSet->L_cycles != F64_NOTVALID){
												snprintf(pL2P, 17,"%14.3lf%2s",measSet->L_cycles,rinex_format_flags(-1,sn));
											}
											break;
										case SIG_GPSL2C:  // GPS L2C
											if (measSet->PR_m != F64_NOTVALID){
												snprintf(cL2C, 17,"%14.3lf%2s",measSet->PR_m,rinex_format_flags(-1,sn));
											}
											if ((measSet->flags & MEASFLAG_HALFCYCLEAMBIGUITY) == 0 && measSet->L_cycles != F64_NOTVALID){
												snprintf(pL2C, 17,"%14.3lf%2s",measSet->L_cycles,rinex_format_flags(-1,sn));
											}
											break;
										case SIG_GPSL5:   // GPS L5
											break;
										case SIG_GPSL1C:  // GPS L1C
											break;
										default:
											break;
									}
								}
							}
							fprintf(obsFile,"%s%s%s%s%s%s%s\n",cL1CA,pL1P,cL1P,cL2P,pL2P,cL2C,pL2C);
						}
					}
				}
			}
		}
		
		//else  switch (SBF_ID_TO_NUMBER(((VoidBlock_t*)SBFBlock)->ID)) 
     // {
	}
	
 return EXIT_SUCCESS;
 
}
