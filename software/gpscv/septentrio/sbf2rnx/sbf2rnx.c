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
#define APP_VERSION "0.1.0"

#define MAXSTR 4097 // this is accomodates longest file path on Linux 

#define TRUE  1
#define FALSE 0

#define BLANK16 "                "

#define MAX_PRN irMAXPRN // according to sviddef.h

#define RINEX_MAJOR_VER 3
#define RINEX_MINOR_VER 4


/* globals */

int debugOn = FALSE;


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
	else
		sprintf(flagBuf,"%1d%1d",lli,sn);
	return flagBuf;
}


char verBuf[17];
char *
rinex_make_ver(
	int majorVer,
	int minorVer)
{
    sprintf(verBuf,"%i.%02d",majorVer,minorVer);
    return verBuf;
}

char hdrBuf[83]; // 80 + CRLF + null terminator
char *
rinex_header_get
(
	FILE *rnxObsHeaderFile,
	char *token
)
{
	rewind(rnxObsHeaderFile);
	while (fgets(hdrBuf,83,rnxObsHeaderFile)){
		if (strstr(hdrBuf,token))
			return hdrBuf;
	}
	return NULL;
}


static void
sbf2rnx_print_help()
{
	printf("\n%s version %s\n",APP_NAME,APP_VERSION);
	printf("Usage: %s [-hvdi:o:] sbffile\n",APP_NAME);
	printf("-h print this help message\n");
	printf("-v print version\n");
	printf("-d enable debugging\n");
	
	printf("-H FILE  use FILE as template for the RINEX observation file header\n");
	printf("-i INTERVAL interval in the RINEX file (default is 30 s)\n");
	printf("-o FILE     set the file name for RINEX output\n");
}

static int
sbf2rnx_set_lli(
	int lockTime,
	int obsInterval)
{
	if (lockTime/1000 < obsInterval)
		return 1;
	return 0; // OK
}

static int
sbf2rnx_get_int_opt
(
	char *optName,
	char *optArg,
	int  * val
)
{
	if (optArg){
		if (1==sscanf(optarg,"%i",val))
			return 1;
		else
			fprintf(stderr,"Error in argument to option %s\n",optName);
	}
	else{
		fprintf(stderr,"Missing argument to option %s\n",optName);
	}
	return 0;
}

int
main(
	int argc,
	char **argv)
{
	
	int c;
	char rnxObsFileName[MAXSTR];
	char tmpRnxObsFileName[MAXSTR];
	char *rnxObsHeaderFileName = NULL;
	char *sbfLogFileName;
	
	int obsInterval = 30;
	strncpy(rnxObsFileName,"test.rnx",MAXSTR-1);
	strncpy(tmpRnxObsFileName,"test.rnx.tmp",MAXSTR-1);
	
	/* Process the command line options */

	while ((c=getopt(argc,argv,"dhH:vi:o:")) != -1){
		switch(c){
			case 'd':	// enable debugging 
				printf("Debugging is ON\n");
				debugOn=TRUE;
				break;
			case 'h': // print help 
				sbf2rnx_print_help();
				return EXIT_SUCCESS;
				break;
			case 'H': // use as template for RINEX obs header
				rnxObsHeaderFileName = strdup(optarg);
				break;
			case 'i': // observation interval
				if (!sbf2rnx_get_int_opt("-i",optarg,&obsInterval)){
					sbf2rnx_print_help();
					return EXIT_FAILURE;
				}
				break;
			case 'o': // set output file name 
				strncpy(rnxObsFileName,optarg,MAXSTR-1);
				strncpy(tmpRnxObsFileName,optarg,MAXSTR-1);
				strncat(tmpRnxObsFileName,".tmp",MAXSTR-1);
				break;
			case 'v': // print version 
				printf("\n %s version %s\n",APP_NAME,APP_VERSION);
				printf(" Written by %s\n",APP_AUTHORS);
				printf(" This ain't no stinkin' Perl script!\n");
				return EXIT_SUCCESS;
				break;
			case '?': // WTF 
				sbf2rnx_print_help();
				return EXIT_FAILURE;
				break;
		}
	}
 
	if (optind >= argc){
		fprintf(stderr,"Expected SBF file name!");
		sbf2rnx_print_help();
		return EXIT_FAILURE;
	}
	
	sbfLogFileName = strdup(argv[optind]);
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
	
	FILE *tmpObsFile = fopen(tmpRnxObsFileName,"w");
	if (!obsFile){
		fprintf(stderr,"Unable to open %s for writing\n",tmpRnxObsFileName);
		return EXIT_FAILURE;
	}
	
	FILE *rnxObsHeaderFile = NULL;
	if (rnxObsHeaderFileName){
	 if (!(rnxObsHeaderFile = fopen(rnxObsHeaderFileName,"r"))){
		fprintf(stderr,"Unable to open %s\n",rnxObsHeaderFileName);
		return EXIT_FAILURE;
	 }
	}
 
	int rxSignals[SIG_LAST+1]; // use this to track what's reported
	int s;
	for (s=0;s<=SIG_LAST;s++)
		rxSignals[s]=0;
	
	SBFData_t   SBFData;
	uint8_t     SBFBlock[MAX_SBFSIZE];
	MeasEpoch_t MeasEpoch;
	
	// Initial pass through the file to determine signals that are being tracked
	InitializeSBFDecoding(sbfLogFileName, &SBFData);

	while (GetNextBlock(&SBFData, SBFBlock, BLOCKNUMBER_ALL, BLOCKNUMBER_ALL,START_POS_CURRENT | END_POS_AFTER_BLOCK) == 0){
		if (sbfread_MeasCollectAndDecode(&SBFData, SBFBlock, &MeasEpoch, SBFREAD_ALLMEAS_ENABLED)){
			int i,tow;
			tow = MeasEpoch.TOW_ms;
			if (tow/1000 % obsInterval == 0){
				for (i=0; i<MeasEpoch.nbrElements; i++){
					uint32_t SigIdx;
					for (SigIdx=0; SigIdx<MAX_NR_OF_SIGNALS_PER_SATELLITE; SigIdx++){
						const MeasSet_t* const MeasSet = &(MeasEpoch.channelData[i].measSet[0][SigIdx]);
						if (MeasSet->flags != 0){
							int st = (SignalType_t)MeasSet->signalType;
							rxSignals[st] = 1;
						}
					}
				}
			}
		}
	}
	CloseSBFFile(&SBFData);
	
	InitializeSBFDecoding(sbfLogFileName, &SBFData);
	
	// To sort PRNs, use an array (index is SVID 1)  which contains the index of the corresponding measurement set
	int prnIDs[MAX_PRN+1];
	int firstObsTOW=-1,firstObsWN=-1,lastObsTOW=-1,lastObsWN=-1;
	
	while (GetNextBlock(&SBFData, SBFBlock, BLOCKNUMBER_ALL, BLOCKNUMBER_ALL,START_POS_CURRENT | END_POS_AFTER_BLOCK) == 0){
		memset((void *) prnIDs,-1,(MAX_PRN+1)*sizeof(int));
		
		if (sbfread_MeasCollectAndDecode(&SBFData, SBFBlock, &MeasEpoch, SBFREAD_ALLMEAS_ENABLED)){
		//if (0){
			int i,tow;
			tow = MeasEpoch.TOW_ms;
			if (tow/1000 % obsInterval == 0){
				
				// Sort the measurements into ascending PRN
				for (i = 0; i < MeasEpoch.nbrElements; i++){
					 const MeasChannel_t* const ChannelData = &(MeasEpoch.channelData[i]);
					 //fprintf(obsFile,"%d\n",ChannelData->PRN);
					 if agIsValid(ChannelData->PRN){
						 prnIDs[ChannelData->PRN] = i;
					 }
				}
				
				// Count sats for observation record
				int nSats = 0;
				for (i=1;i<=MAX_PRN;i++){
					int measIndex = prnIDs[i];
					if ( measIndex != -1){
						if (agIsGPS(i)) nSats += 1;
					}
				}
				
				// FIXME bail if nsats == 0 ?
				
				// Output header for the observation record
				struct tm tmGPS;
				GPS_to_date(MeasEpoch.TOW_ms/1000,MeasEpoch.WNc,&tmGPS); 
				
				if (firstObsWN == -1){ 
					firstObsTOW = MeasEpoch.TOW_ms;
					firstObsWN  = MeasEpoch.WNc;
				}
				
				lastObsTOW = MeasEpoch.TOW_ms;
				lastObsWN  = MeasEpoch.WNc;
					
				// Epoch flag currently set to 0 - could use this ...
				fprintf(tmpObsFile,"> %4d %2.2d %2.2d %2.2d %2.2d%11.7f  %1d%3d%6s%15.12lf\n",
					tmGPS.tm_year+1900,tmGPS.tm_mon+1,tmGPS.tm_mday,tmGPS.tm_hour,tmGPS.tm_min,tmGPS.tm_sec + (double) (MeasEpoch.TOW_ms % 1000)/1000.0,
					0,nSats," ",0.0);
				
				// Output observations 
				for (i=1;i<=MAX_PRN;i++){
					int measIndex = prnIDs[i];
					if ( measIndex != -1){
						if (agIsGPS(i)){
							fprintf(tmpObsFile,"G%02d",i);
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
											// discard carrier phases with half-cycle ambiguities 
											if ((measSet->flags & MEASFLAG_HALFCYCLEAMBIGUITY) == 0 && measSet->L_cycles != F64_NOTVALID){
												// FIXME truncate phase if it will overflow ! 
												snprintf(pL1P, 17,"%14.3lf%2s",measSet->L_cycles,
													rinex_format_flags(sbf2rnx_set_lli(measSet->PLLTimer_ms,obsInterval),sn)); // repeated for L1P to make sure we get it
											}
											//fprintf(stderr,"L1C %2d %2d %2d SV=%02d %d %d \n",tmGPS.tm_hour,tmGPS.tm_min,tmGPS.tm_sec,i,measSet->PLLTimer_ms,measSet->lockCount);
											break;
										case SIG_GPSL1P:  // GPS L1 P(Y) 
											if (measSet->PR_m != F64_NOTVALID){
												snprintf(cL1P, 17,"%14.3lf%2s",measSet->PR_m,rinex_format_flags(-1,sn));
											}
											// FIXME need this if GPSL1CA is masked out
											//if ((measSet->flags & MEASFLAG_HALFCYCLEAMBIGUITY) == 0 && measSet->L_cycles != F64_NOTVALID){
											//	snprintf(pL1P, 17,"%14.3lf%2s",measSet->L_cycles,rinex_format_flags(sbf2rnx_set_lli(measSet->PLLTimer_ms,obsInterval),sn));
											//}
											//fprintf(stderr,"L1P %2d %2d %2d SV=%02d %d %d \n",tmGPS.tm_hour,tmGPS.tm_min,tmGPS.tm_sec,i,measSet->PLLTimer_ms,measSet->lockCount);
											break;
										case SIG_GPSL2P:  // GPS L2 P(Y)
											if (measSet->PR_m != F64_NOTVALID){
												snprintf(cL2P, 17,"%14.3lf%2s",measSet->PR_m,rinex_format_flags(-1,sn));
											}
											if ((measSet->flags & MEASFLAG_HALFCYCLEAMBIGUITY) == 0 && measSet->L_cycles != F64_NOTVALID){
												snprintf(pL2P, 17,"%14.3lf%2s",measSet->L_cycles,
													rinex_format_flags(sbf2rnx_set_lli(measSet->PLLTimer_ms,obsInterval),sn));
											}
											break;
										case SIG_GPSL2C:  // GPS L2C
											if (measSet->PR_m != F64_NOTVALID){
												snprintf(cL2C, 17,"%14.3lf%2s",measSet->PR_m,rinex_format_flags(-1,sn));
											}
											if ((measSet->flags & MEASFLAG_HALFCYCLEAMBIGUITY) == 0 && measSet->L_cycles != F64_NOTVALID){
												snprintf(pL2C, 17,"%14.3lf%2s",measSet->L_cycles,
													rinex_format_flags(sbf2rnx_set_lli(measSet->PLLTimer_ms,obsInterval),sn));
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
							if (rxSignals[SIG_GPSL1CA])
								fprintf(tmpObsFile,"%s%s",cL1CA,pL1P);
							if (rxSignals[SIG_GPSL1P]){
								if (rxSignals[SIG_GPSL1CA]) // already have CP
									fprintf(tmpObsFile,"%s",cL1P);
								else
									fprintf(tmpObsFile,"%s%s",cL1P,pL1P);
							}
							if (rxSignals[SIG_GPSL2P]){
								fprintf(tmpObsFile,"%s%s",cL2P,pL2P);
							}
							if (rxSignals[SIG_GPSL2C]){
								fprintf(tmpObsFile,"%s%s",cL2C,pL2C);
							}
							fprintf(tmpObsFile,"\n");
						}
					}
				}
			}
		}
		else
		//else  switch (SBF_ID_TO_NUMBER(((VoidBlock_t*)SBFBlock)->ID)) 
		// 4002 GALNav
		// 4004 GLONav
		// 4007 PVTGeodetic
		// 4012 SatVisibility
		// 4031 GalUtc
		// 4032 GALGstGps
		// 4081 BDSNav
		// 4095 QZSNav
		// 5891 GPSNav
		// 5893 GPSIon
		// 5894 GPSUtc
		// 5896 GEONav
		{
			//fprintf(stderr,"%d\n",SBF_ID_TO_NUMBER(((VoidBlock_t*)SBFBlock)->ID));
			switch (SBF_ID_TO_NUMBER(((VoidBlock_t*)SBFBlock)->ID))
			{
				case sbfnr_GPSNav_1:
				{
					//GPSNav_1_0_t *gpsNav = (GPSNav_1_0_t *) SBFBlock;
					//fprintf(stderr,"%d %d %d %d\n",gpsNav->Eph.TOW/1000,gpsNav->Eph.PRN,gpsNav->Eph.IODE2,gpsNav->Eph.IODE3);
					break;
				}
				case sbfnr_GPSIon_1:
					break;
				case sbfnr_GPSUtc_1:
					break;
				case sbfnr_ReceiverSetup_1:
					break;
				default:
					break;
			}
		}
	}
	
	fclose(tmpObsFile);
	
	//
	// Create the observation file header
	// 
	
	char buf[81],tmp[81];
	FILE *fout = obsFile;
	
	fprintf(fout,"%9s%11s%-20s%c%-19s%-20s\n",rinex_make_ver(RINEX_MAJOR_VER,RINEX_MINOR_VER),"","O",'M',"","RINEX VERSION / TYPE");
	
	time_t tnow = time(NULL);
	struct tm *tgmt = gmtime(&tnow);
	snprintf(buf,81,"%04d%02d%02d %02d%02d%02d UTC",tgmt->tm_year+1900,tgmt->tm_mon+1,tgmt->tm_mday,
					 tgmt->tm_hour,tgmt->tm_min,tgmt->tm_sec);
	snprintf(tmp,21,"%s-%s",APP_NAME,APP_VERSION);
	fprintf(fout,"%-20s%-20s%-20s%-20s\n",tmp,"UNKNOWN",buf,"PGM / RUN BY / DATE");
	char *txt=NULL;
	
	//
	if (rnxObsHeaderFile){
		if ((txt = rinex_header_get(rnxObsHeaderFile,"MARKER NAME")))
			fprintf(fout,"%s",txt);
	}
	if (!txt)
		fprintf(fout,"%-60s%-20s\n","UNKNOWN","MARKER NAME");
	
	//
	if (rnxObsHeaderFile){
		if ((txt = rinex_header_get(rnxObsHeaderFile,"MARKER NUMBER")))
			fprintf(fout,"%s",txt);
	}
	if (!txt)
		fprintf(fout,"%-60s%-20s\n","UNKNOWN","MARKER NUMBER");
	
	//
	if (rnxObsHeaderFile){
		if ((txt = rinex_header_get(rnxObsHeaderFile,"MARKER TYPE")))
			fprintf(fout,"%s",txt);
	}
	if (!txt)
		fprintf(fout,"%-60s%-20s\n","UNKNOWN","MARKER TYPE");
	
	//
	if (rnxObsHeaderFile){
		if ((txt = rinex_header_get(rnxObsHeaderFile,"OBSERVER / AGENCY")))
			fprintf(fout,"%s",txt);
	}
	if (!txt)
		fprintf(fout,"%-20s%-40s%-20s\n","UNKNOWN","UNKNOWN","OBSERVER / AGENCY");
	
	//
	if (rnxObsHeaderFile){
		if ((txt = rinex_header_get(rnxObsHeaderFile,"REC # / TYPE / VERS")))
			fprintf(fout,"%s",txt);
	}
	if (!txt)
		fprintf(fout,"%-20s%-20s%-20s%-20s\n","UNKNOWN","UNKNOWN","UNKNOWN","REC # / TYPE / VERS");
	
	//
	if (rnxObsHeaderFile){
		if ((txt = rinex_header_get(rnxObsHeaderFile,"ANT # / TYPE")))
			fprintf(fout,"%s",txt);
	}
	if (!txt)
		fprintf(fout,"%-20s%-20s%-20s%-20s\n","UNKNOWN","UNKNOWN"," ","ANT # / TYPE");
	
	//
	if (rnxObsHeaderFile){
		if ((txt = rinex_header_get(rnxObsHeaderFile,"APPROX POSITION XYZ")))
			fprintf(fout,"%s",txt);
	}
	if (!txt)
		fprintf(fout,"%14.4lf%14.4lf%14.4lf%-18s%-20s\n",0.0,0.0,0.0," ","APPROX POSITION XYZ");
	
	//
	if (rnxObsHeaderFile){
		if ((txt = rinex_header_get(rnxObsHeaderFile,"ANTENNA: DELTA H/E/N")))
			fprintf(fout,"%s",txt);
	}
	if (!txt)
		fprintf(fout,"%14.4lf%14.4lf%14.4lf%-18s%-20s\n",0.0,0.0,0.0," ","ANTENNA: DELTA H/E/N");
	
	//
	
	int nObs =0;
	char codes[81];
	strcpy(codes," "); 
	if (rxSignals[SIG_GPSL1CA]){
		nObs+=2;
		strcat(codes,"C1C L1C ");
	}
	if (rxSignals[SIG_GPSL1P]){
		if (rxSignals[SIG_GPSL1CA]){
			nObs+=1;
			strcat(codes,"C1W ");
		}
		else{
			nObs+=2;
			strcat(codes,"C1W L1C ");
		}
	}
	if (rxSignals[SIG_GPSL2P]){
		nObs += 2;
		strcat(codes,"C2W L2W ");
	}
	if (rxSignals[SIG_GPSL2C]){
		nObs += 2;
		strcat(codes,"C2L L2L");
	}
	fprintf(fout,"%1s  %3d%-54s%-20s\n","G",nObs,codes,"SYS / # / OBS TYPES");
	
	fprintf(fout,"SEPTENTRIO RECEIVERS OUTPUT ALIGNED CARRIER PHASES.         COMMENT\n");             
	fprintf(fout,"NO FURTHER PHASE SHIFT APPLIED IN THE RINEX ENCODER.        COMMENT\n");             
	fprintf(fout,"G L1C                                                       SYS / PHASE SHIFT\n");   
	fprintf(fout,"G L2W                                                       SYS / PHASE SHIFT\n");   
	fprintf(fout,"G L2L  0.00000                                              SYS / PHASE SHIFT\n");
	
	
	fprintf(fout,"%10.3lf%-50s%-20s\n",(double) obsInterval," ","INTERVAL");
	
	struct tm tmGPS;	
	GPS_to_date(firstObsTOW/1000,firstObsWN,&tmGPS); 
	fprintf(fout,"%6d%6d%6d%6d%6d%13.7lf%-5s%3s%-9s%-20s\n",
					tmGPS.tm_year+1900,tmGPS.tm_mon+1,tmGPS.tm_mday,tmGPS.tm_hour,tmGPS.tm_min,
					(double) (tmGPS.tm_sec + (firstObsTOW % 1000)/1000.0),
					" ", "GPS"," ","TIME OF FIRST OBS");
	
	
	GPS_to_date(lastObsTOW/1000,lastObsWN,&tmGPS); 
	fprintf(fout,"%6d%6d%6d%6d%6d%13.7lf%-5s%3s%-9s%-20s\n",
					tmGPS.tm_year+1900,tmGPS.tm_mon+1,tmGPS.tm_mday,tmGPS.tm_hour,tmGPS.tm_min,
					(double) (tmGPS.tm_sec + (firstObsTOW % 1000)/1000.0),
					" ", "GPS"," ","TIME OF LAST OBS");
	
	fprintf(fout,"%60s%-20s\n","","END OF HEADER");
	

	// Append the observations
	tmpObsFile = fopen(tmpRnxObsFileName,"r");
	char bigBuf[MAXSTR];
	
	while (fgets(bigBuf,MAXSTR,tmpObsFile)){
		fprintf(obsFile,"%s",bigBuf);
	}
	fclose(tmpObsFile);
	fclose(fout);
	unlink(tmpRnxObsFileName);
	
	
	return EXIT_SUCCESS;
 
}