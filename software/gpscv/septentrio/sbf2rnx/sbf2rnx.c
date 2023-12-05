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
 */


/* Notes
 * Tested on PolaRx2, PolaRx4, PolaRx5, mosaicT
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
#define APP_VERSION "0.3.0"

#define MAXSTR 4097 // this is accomodates longest file path on Linux 

#define TRUE  1
#define FALSE 0

#define BLANK16 "                "

#define MAX_PRN irMAXPRN // according to sviddef.h

#define RINEX_MAJOR_VER 3
#define RINEX_MINOR_VER 4

#define PLL_LOCK_FUDGE 10

#define ICD_PI 3.1415926535898
static const double GPSURA[] = {2,2.8,4,5.7,8,11.3,16,32,64,128,256,512,1024,2048,4096,0.0};

/* globals */

int debugOn = FALSE;

typedef struct
{
	int valid;
	char * serialNumber;  // all packets
	char * FWVersion;  
	char * GNSSFWVersion; // packet version >= 2
	char * productName;   // packet version >= 3
}Receiver_t;


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
rinex_get_header
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

void
rinex_write_obs_header
(
	FILE *fobs,
	FILE *rnxObsHeaderFile,
	Receiver_t *rx,
	char *user,
	int rxSignals[],
	int obsInterval,
	int firstObsTOW,int firstObsWN,
	int lastObsTOW,int lastObsWN
)
{
	
	char buf[81],tmp[81];
	time_t tnow = time(NULL);
	struct tm *tgmt = gmtime(&tnow);
	
	fprintf(fobs,"%9s%11s%-20s%c%-19s%-20s\n",rinex_make_ver(RINEX_MAJOR_VER,RINEX_MINOR_VER),"","O",'M',"","RINEX VERSION / TYPE");
	
	snprintf(buf,81,"%04d%02d%02d %02d%02d%02d UTC",tgmt->tm_year+1900,tgmt->tm_mon+1,tgmt->tm_mday,
					 tgmt->tm_hour,tgmt->tm_min,tgmt->tm_sec);
	snprintf(tmp,21,"%s-%s",APP_NAME,APP_VERSION);
	fprintf(fobs,"%-20s%-20s%-20s%-20s\n",tmp,user,buf,"PGM / RUN BY / DATE");
	
	char *txt=NULL;
	
	//
	if (rnxObsHeaderFile){
		if ((txt = rinex_get_header(rnxObsHeaderFile,"MARKER NAME")))
			fprintf(fobs,"%s",txt);
	}
	if (!txt)
		fprintf(fobs,"%-60s%-20s\n","UNKNOWN","MARKER NAME");
	
	//
	if (rnxObsHeaderFile){
		if ((txt = rinex_get_header(rnxObsHeaderFile,"MARKER NUMBER")))
			fprintf(fobs,"%s",txt);
	}
	if (!txt)
		fprintf(fobs,"%-60s%-20s\n","UNKNOWN","MARKER NUMBER");
	
	//
	if (rnxObsHeaderFile){
		if ((txt = rinex_get_header(rnxObsHeaderFile,"MARKER TYPE")))
			fprintf(fobs,"%s",txt);
	}
	if (!txt)
		fprintf(fobs,"%-60s%-20s\n","UNKNOWN","MARKER TYPE");
	
	//
	if (rnxObsHeaderFile){
		if ((txt = rinex_get_header(rnxObsHeaderFile,"OBSERVER / AGENCY")))
			fprintf(fobs,"%s",txt);
	}
	if (!txt)
		fprintf(fobs,"%-20s%-40s%-20s\n","UNKNOWN","UNKNOWN","OBSERVER / AGENCY");
	
	//
	if (rnxObsHeaderFile){
		if ((txt = rinex_get_header(rnxObsHeaderFile,"REC # / TYPE / VERS")))
			fprintf(fobs,"%s",txt);
	}
	if (!txt){
		if (rx->valid){
			fprintf(fobs,"%-20s%-20s%-20s%-20s\n",rx->serialNumber,rx->productName,rx->FWVersion,"REC # / TYPE / VERS");
		}
		else{
			fprintf(fobs,"%-20s%-20s%-20s%-20s\n","UNKNOWN","UNKNOWN","UNKNOWN","REC # / TYPE / VERS");
		}
	}

	//
	if (rnxObsHeaderFile){
		if ((txt = rinex_get_header(rnxObsHeaderFile,"ANT # / TYPE")))
			fprintf(fobs,"%s",txt);
	}
	if (!txt)
		fprintf(fobs,"%-20s%-20s%-20s%-20s\n","UNKNOWN","UNKNOWN"," ","ANT # / TYPE");
	
	//
	if (rnxObsHeaderFile){
		if ((txt = rinex_get_header(rnxObsHeaderFile,"APPROX POSITION XYZ")))
			fprintf(fobs,"%s",txt);
	}
	if (!txt)
		fprintf(fobs,"%14.4lf%14.4lf%14.4lf%-18s%-20s\n",0.0,0.0,0.0," ","APPROX POSITION XYZ");
	
	//
	if (rnxObsHeaderFile){
		if ((txt = rinex_get_header(rnxObsHeaderFile,"ANTENNA: DELTA H/E/N")))
			fprintf(fobs,"%s",txt);
	}
	if (!txt)
		fprintf(fobs,"%14.4lf%14.4lf%14.4lf%-18s%-20s\n",0.0,0.0,0.0," ","ANTENNA: DELTA H/E/N");
	
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
		strcat(codes,"C2L L2L ");
	}
	if (rxSignals[SIG_GPSL5]){// FIXME not sure about observation names
		nObs += 2;
		strcat(codes,"C5Q L5Q ");
	}
	
	if (nObs > 13){ 
		// FIXME
	}
	else{
		fprintf(fobs,"%1s  %3d%-54s%-20s\n","G",nObs,codes,"SYS / # / OBS TYPES");
	}
	
	fprintf(fobs,"SEPTENTRIO RECEIVERS OUTPUT ALIGNED CARRIER PHASES.         COMMENT\n");             
	fprintf(fobs,"NO FURTHER PHASE SHIFT APPLIED IN THE RINEX ENCODER.        COMMENT\n");             
	if (rxSignals[SIG_GPSL1CA]){fprintf(fobs,"G L1C                                                       SYS / PHASE SHIFT\n");}   
	if (rxSignals[SIG_GPSL1P]) {fprintf(fobs,"G L2W                                                       SYS / PHASE SHIFT\n");}  
	if (rxSignals[SIG_GPSL2C]) {fprintf(fobs,"G L2L  0.00000                                              SYS / PHASE SHIFT\n");}
	if (rxSignals[SIG_GPSL5])  {fprintf(fobs,"G L5Q  0.00000                                              SYS / PHASE SHIFT\n");}
	
	fprintf(fobs,"%10.3lf%-50s%-20s\n",(double) obsInterval," ","INTERVAL");
	
	struct tm tmGPS;
	
	GPS_to_date(firstObsTOW/1000,firstObsWN,&tmGPS); 
	fprintf(fobs,"%6d%6d%6d%6d%6d%13.7lf%-5s%3s%-9s%-20s\n",
					tmGPS.tm_year+1900,tmGPS.tm_mon+1,tmGPS.tm_mday,tmGPS.tm_hour,tmGPS.tm_min,
					(double) (tmGPS.tm_sec + (firstObsTOW % 1000)/1000.0),
					" ", "GPS"," ","TIME OF FIRST OBS");
	
	
	GPS_to_date(lastObsTOW/1000,lastObsWN,&tmGPS); 
	fprintf(fobs,"%6d%6d%6d%6d%6d%13.7lf%-5s%3s%-9s%-20s\n",
					tmGPS.tm_year+1900,tmGPS.tm_mon+1,tmGPS.tm_mday,tmGPS.tm_hour,tmGPS.tm_min,
					(double) (tmGPS.tm_sec + (firstObsTOW % 1000)/1000.0),
					" ", "GPS"," ","TIME OF LAST OBS");
	
	fprintf(fobs,"%60s%-20s\n","","END OF HEADER");
}

void 
rinex_write_nav_gps
(
	FILE *fnav,
 	GPSNav_1_0_t *gpsNav
)
{
	
	int nRolloverWeeks = (gpsNav->Eph.WNc/1024)*1024; // this is full WN for the message transmission time
	
	// FIXME the resolution of full WN uses transmission time, which might be after t_OC
	// Maybe detect this using the difference WNc and WNt?
	struct tm tmGPS;
	GPS_to_date(gpsNav->Eph.t_oc,gpsNav->Eph.WNt_oc + nRolloverWeeks,&tmGPS); 
	
	fprintf(fnav,"G%02d %4d %02d %02d %02d %02d %02d%19.12E%19.12E%19.12E\n",gpsNav->Eph.PRN,
		tmGPS.tm_year+1900,tmGPS.tm_mon+1,tmGPS.tm_mday,tmGPS.tm_hour,tmGPS.tm_min,tmGPS.tm_sec,
		gpsNav->Eph.a_f0,gpsNav->Eph.a_f1,gpsNav->Eph.a_f2);
	
	fprintf(fnav,"%4s%19.12E%19.12E%19.12E%19.12E\n"," ", // broadcast orbit 1
		(double) gpsNav->Eph.IODE2,gpsNav->Eph.C_rs,gpsNav->Eph.DEL_N*ICD_PI,gpsNav->Eph.M_0*ICD_PI);

	fprintf(fnav,"%4s%19.12E%19.12E%19.12E%19.12E\n"," ", // broadcast orbit 2
		gpsNav->Eph.C_uc,gpsNav->Eph.e,gpsNav->Eph.C_us,gpsNav->Eph.SQRT_A);

	fprintf(fnav,"%4s%19.12E%19.12E%19.12E%19.12E\n"," ", // broadcast orbit 3
		(double) gpsNav->Eph.t_oe,gpsNav->Eph.C_ic,gpsNav->Eph.OMEGA_0*ICD_PI,gpsNav->Eph.C_is);

	fprintf(fnav,"%4s%19.12E%19.12E%19.12E%19.12E\n"," ", // broadcast orbit 4
		gpsNav->Eph.i_0*ICD_PI,gpsNav->Eph.C_rc,gpsNav->Eph.omega*ICD_PI,gpsNav->Eph.OMEGADOT*ICD_PI);

	fprintf(fnav,"%4s%19.12E%19.12E%19.12E%19.12E\n"," ", // broadcast orbit 5
		gpsNav->Eph.IDOT*ICD_PI,1.0,(double) gpsNav->Eph.WNt_oe + nRolloverWeeks,0.0); // FIXME check behaviour near rollover

	fprintf(fnav,"%4s%19.12E%19.12E%19.12E%19.12E\n"," ", // broadcast orbit 6
		GPSURA[gpsNav->Eph.URA],(double) gpsNav->Eph.health,gpsNav->Eph.T_gd,(double) gpsNav->Eph.IODC);
	
	fprintf(fnav,"%4s%19.12E%19.12E\n"," ",(double) gpsNav->Eph.TOW/1000.0,4.0); // broadcast orbit 7 (TOW is transmission time of message)
}

static void
sbf2rnx_print_help()
{
	printf("\n%s version %s\n",APP_NAME,APP_VERSION);
	printf("Usage: %s [-hvdi:o:] [sbffile]\n",APP_NAME);
	printf("-h print this help message\n");
	printf("-v print version\n");
	printf("-d enable debugging\n");
	printf("-f FILE     name of SBF file (for compatibility with sbf2rin)\n");
	printf("-H FILE     use FILE as template for the RINEX observation file header\n");
	printf("-i INTERVAL interval in the RINEX file (default is 30 s)\n");
	printf("-n TYPE     set output types.\n");
	printf("            O=observation (default)\n");
	printf("            P=mixed navigation\n");
	printf("-o FILE     set the file name for RINEX output\n");
	printf("-r VERSION  set the RINEX version (only '3' at present)\n");
	printf("-x SYSTEMS  exclude the specified GNSS systems (from both observation and navigation files). \n");
	printf("             G=GPS, R=GLONASS, E=GALILEO, C=BEIDOU, S=SBAS, J=QZSS, I=IRNSS\n");
}

static int
sbf2rnx_set_lli(
	int lockTime,
	int obsInterval)
{
	if (lockTime/1000 < obsInterval + PLL_LOCK_FUDGE)
		return 1;
	return 0; // OK
}

static void
sbf2rnx_format_obs(
	char * code,
	char * phase,
	int sn,
	int obsInterval,
	const MeasSet_t* const measSet
)
{
	if (measSet->PR_m != F64_NOTVALID){
		snprintf(code, 17,"%14.3lf%2s",measSet->PR_m,rinex_format_flags(-1,sn));
	}
	if ((measSet->flags & MEASFLAG_HALFCYCLEAMBIGUITY) == 0 && measSet->L_cycles != F64_NOTVALID){
		snprintf(phase, 17,"%14.3lf%2s",measSet->L_cycles,
			rinex_format_flags(sbf2rnx_set_lli(measSet->PLLTimer_ms,obsInterval),sn));
		//if (measSet->PLLTimer_ms/1000 < obsInterval){
		//	fprintf(stderr,"%02d%02d%02d %d %d %s\n",tmGPS.tm_hour,tmGPS.tm_min,tmGPS.tm_sec,i,measSet->PLLTimer_ms/1000,
		//				rinex_format_flags(sbf2rnx_set_lli(measSet->PLLTimer_ms,obsInterval),sn));
		//}
	}
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


static void
rx_read_setup(
	void * SBFBlock,
	Receiver_t * rx)
{
	
	int rev = SBF_ID_TO_REV(((VoidBlock_t*)SBFBlock)->ID);
	fprintf(stderr,"ReceiverSetup! REV=%d\n",rev);
	switch (rev){
		case 0:
		{
			rx->valid=1;
			ReceiverSetup_1_0_t *rxsetup = (ReceiverSetup_1_0_t *) SBFBlock;
			rx->serialNumber = strdup(rxsetup->RxSerialNbr);
			rx->FWVersion = strdup(rxsetup->RxVersion);
			rx->GNSSFWVersion = strdup(rxsetup->RxVersion); // 
			rx->productName = strdup("UNKNOWN");
			break;
		}
		case 3:
		{
			rx->valid=1;
			ReceiverSetup_1_3_t *rxsetup = (ReceiverSetup_1_3_t *) SBFBlock;
			rx->serialNumber = strdup(rxsetup->RxSerialNbr);
			rx->FWVersion = strdup(rxsetup->RxVersion);
			rx->GNSSFWVersion = strdup(rxsetup->GNSSFWVersion);
			rx->productName = strdup(rxsetup->ProductName);
			//fprintf(stderr,"%s %s\n",rxsetup->ProductName,rxsetup->GNSSFWVersion);
			break;
		}
		default:
			break;
	}
}


int
main(
	int argc,
	char **argv)
{
	
	Receiver_t rx;
	
	int c;
	int rnxMajorVersion;
	int excludedGNSS;
	
	int makeObsFile= TRUE;
	char rnxObsFileName[MAXSTR];
	char tmpRnxObsFileName[MAXSTR];
	char *rnxObsHeaderFileName = NULL;
	
	int  makeNavFile = FALSE;
	char rnxNavFileName[MAXSTR];
	
	int optf = FALSE;
	char *sbfLogFileName;
	
	rx.valid = 0;
	
	int obsInterval = 30;
	
	strncpy(rnxObsFileName,"",MAXSTR-1);
	
	/* Process the command line options */

	while ((c=getopt(argc,argv,"df:hH:vi:n:o:R:x:")) != -1){
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
			case 'f': // use as template for RINEX obs header
				sbfLogFileName = strdup(optarg);
				optf=TRUE;
				break;
			case 'i': // observation interval
				if (!sbf2rnx_get_int_opt("-i",optarg,&obsInterval)){
					sbf2rnx_print_help();
					return EXIT_FAILURE;
				}
				break;
			
			case 'n':	// set ouput files
			{
				makeObsFile = FALSE; // override the default
				if (0==strlen(optarg)){
					fprintf(stderr,"Bad file spec in -n\n");
							sbf2rnx_print_help();
							return EXIT_FAILURE;
				}
				for (int index=0;index < strlen(optarg);index++){
					switch (optarg[index]){
						case 'O': makeObsFile = TRUE; break;
						case 'P': makeNavFile = TRUE;break;
						default:
							fprintf(stderr,"Bad file spec in -n\n");
							sbf2rnx_print_help();
							return EXIT_FAILURE;
							break;
					}
				}
				break;
			}
			case 'o': // set output file name 
				strncpy(rnxObsFileName,optarg,MAXSTR-1);
				strncpy(tmpRnxObsFileName,optarg,MAXSTR-1);
				strncat(tmpRnxObsFileName,".tmp",MAXSTR-1);
				break;
			case 'R': // RINEX version FIXME this is just for compatibility
				if (!sbf2rnx_get_int_opt("-r",optarg,&rnxMajorVersion)){
					sbf2rnx_print_help();
					return EXIT_FAILURE;
				}
				if (3 != rnxMajorVersion){
					fprintf(stderr,"Only version 3 RINEX is supported\n");
					return EXIT_FAILURE;
				}
				break;
			case 'v': // print version 
				printf("\n %s version %s\n",APP_NAME,APP_VERSION);
				printf(" Written by %s\n",APP_AUTHORS);
				printf(" This ain't no stinkin' Perl script!\n");
				return EXIT_SUCCESS;
				break;
			case 'x':
				excludedGNSS = 0; // FIXME does nothing 
				break;
			case '?': // WTF 
				sbf2rnx_print_help();
				return EXIT_FAILURE;
				break;
		}
	}
 
	if (!optf){// if no opt f, then expect name
		if (optind >= argc){
			fprintf(stderr,"Expected SBF file name!");
			sbf2rnx_print_help();
			return EXIT_FAILURE;
		}
	
		sbfLogFileName = strdup(argv[optind]);
	}

	FILE *fd = fopen(sbfLogFileName,"r");
	if (!fd){
		fprintf(stderr,"Unable to open %s for reading\n",sbfLogFileName);
		return EXIT_FAILURE;
	}
	fclose(fd);
	
	FILE *rnxObsHeaderFile = NULL;
	if (rnxObsHeaderFileName){
	 if (!(rnxObsHeaderFile = fopen(rnxObsHeaderFileName,"r"))){
		fprintf(stderr,"Unable to open %s\n",rnxObsHeaderFileName);
		return EXIT_FAILURE;
	 }
	}
 
  char *user = getenv("USER"); 
	if (NULL == user)
		user = strdup("UNKNOWN");
	if (strlen(user) > 20)
		user[20]=0; // possibly ugly
	
	int rxSignals[SIG_LAST+1]; // use this to track what's reported
	int s;
	for (s=0;s<=SIG_LAST;s++)
		rxSignals[s]=0;
	
	SBFData_t   SBFData;
	uint8_t     SBFBlock[MAX_SBFSIZE];
	MeasEpoch_t MeasEpoch;
	
	int gotGPSIono = FALSE;
	GPSIon_1_0_t gpsIono;
	int gotGPSUTC = FALSE;
	GPSUtc_1_0_t gpsUTC;
	
	// To stop compiler warnings ...
	memset((void *) &gpsIono,0,sizeof(GPSIon_1_0_t ));
	memset((void *) &gpsUTC,0,sizeof(GPSUtc_1_0_t ));
	
	// Initial pass through the file to determine signals that are being tracked
	// ionosphere and UTC
	InitializeSBFDecoding(sbfLogFileName, &SBFData);

	int firstObsTOW=-1,firstObsWN=-1,lastObsTOW=-1,lastObsWN=-1;
	
	while (GetNextBlock(&SBFData, SBFBlock, BLOCKNUMBER_ALL, BLOCKNUMBER_ALL,START_POS_CURRENT | END_POS_AFTER_BLOCK) == 0){
		if (sbfread_MeasCollectAndDecode(&SBFData, SBFBlock, &MeasEpoch, SBFREAD_ALLMEAS_ENABLED)){
			int i,tow;
			tow = MeasEpoch.TOW_ms;
			if (tow/1000 % obsInterval == 0){
				if (firstObsWN == -1){ 
					firstObsTOW = MeasEpoch.TOW_ms;
					firstObsWN  = MeasEpoch.WNc;
				}
				lastObsTOW = MeasEpoch.TOW_ms;
				lastObsWN  = MeasEpoch.WNc;
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
		else{
			//fprintf(stderr,"%d\n",SBF_ID_TO_NUMBER(((VoidBlock_t*)SBFBlock)->ID));
			switch (SBF_ID_TO_NUMBER(((VoidBlock_t*)SBFBlock)->ID))
			{
				case sbfnr_GPSIon_1: // parse NAV file stuff always
					if (!(gotGPSIono)){
						gpsIono = *((GPSIon_1_0_t *) SBFBlock);
						gotGPSIono = TRUE;
					}
					break;
				case sbfnr_GPSUtc_1:
					//if (!(gotGPSUTC)){
						gpsUTC = *((GPSUtc_1_0_t *) SBFBlock);
						gotGPSUTC = TRUE;
					//}
					break;
				case sbfnr_ReceiverSetup_1:
					rx_read_setup((void *) SBFBlock, &rx);
					break;
				default:
					break;
			}
		}
	}
	
	CloseSBFFile(&SBFData);
	
	// TODO Now that we have made a pass through the file, we can make suitable default file names
	
	struct tm tmGPS;	
	if (0==strlen(rnxObsFileName)){
		GPS_to_date(firstObsTOW/1000,firstObsWN,&tmGPS); 
		// sbf2rin defaults to V2 names so we dutifully follow
		sprintf(rnxObsFileName,"SEPT%03d0.%02dO",tmGPS.tm_yday+1,tmGPS.tm_year % 100);
		sprintf(rnxNavFileName,"SEPT%03d0.%02dP",tmGPS.tm_yday+1, tmGPS.tm_year % 100);
	}
	else{
		sprintf(rnxNavFileName,"SEPT%03d0.%02dP",tmGPS.tm_yday+1, tmGPS.tm_year % 100);
	}
	
	strncpy(tmpRnxObsFileName,"SEPT.rnx.tmp",MAXSTR-1); // FIXME unique name ?
	
	FILE *obsFile = NULL;
	FILE *tmpObsFile = NULL;
	if (makeObsFile){
		obsFile = fopen(rnxObsFileName,"w");
		if (!obsFile){
			fprintf(stderr,"Unable to open %s for writing\n",rnxObsFileName);
			return EXIT_FAILURE;
		}
		
		tmpObsFile = fopen(tmpRnxObsFileName,"w");
		if (!obsFile){
			fprintf(stderr,"Unable to open %s for writing\n",tmpRnxObsFileName);
			return EXIT_FAILURE;
		}
	}
	
	FILE *fnav = NULL;
	if (makeNavFile){
		fnav = fopen(rnxNavFileName,"w");
		if (!fnav){
			fprintf(stderr,"Unable to open %s for writing\n",rnxNavFileName);
			return EXIT_FAILURE;
		}
	}
	
	char buf[81],tmp[81];
	time_t tnow = time(NULL);
	struct tm *tgmt = gmtime(&tnow);
	
	// Write the navigation file header
	if (makeNavFile) {
		fprintf(fnav,"%9s%11s%-20s%-20s%-20s\n",rinex_make_ver(RINEX_MAJOR_VER,RINEX_MINOR_VER),"","N: GNSS NAV DATA","G: GPS","RINEX VERSION / TYPE");
		snprintf(buf,80,"%04d%02d%02d %02d%02d%02d UTC",tgmt->tm_year+1900,tgmt->tm_mon+1,tgmt->tm_mday,
						tgmt->tm_hour,tgmt->tm_min,tgmt->tm_sec);
		snprintf(tmp,21,"%s-%s",APP_NAME,APP_VERSION);
		fprintf(fnav,"%-20s%-20s%-20s%-20s\n",tmp,user,buf,"PGM / RUN BY / DATE");
		if (gotGPSIono){
			fprintf(fnav,"GPSA %12.4E%12.4E%12.4E%12.4E%7s%-20s\n",
							gpsIono.Ion.alpha_0,gpsIono.Ion.alpha_1,gpsIono.Ion.alpha_2,gpsIono.Ion.alpha_3,"","IONOSPHERIC CORR");
			fprintf(fnav,"GPSB %12.4E%12.4E%12.4E%12.4E%7s%-20s\n",
							gpsIono.Ion.beta_0,gpsIono.Ion.beta_1,gpsIono.Ion.beta_2,gpsIono.Ion.beta_3,"","IONOSPHERIC CORR");
		}
		if (gotGPSUTC){
			// gpsUTC.Utc.WN_t is truncated so fix it
			int nRolloverWeeks = (gpsUTC.Utc.WNc/256) * 256; // WNc current week number ? Note that WN_t is 8 bits hence divisor of 256
			fprintf(fnav,"GPUT %17.10E%16.9E%7d%5d %5s %2d %-20s\n",gpsUTC.Utc.A_0,gpsUTC.Utc.A_1,(int) gpsUTC.Utc.t_ot,gpsUTC.Utc.WN_t + nRolloverWeeks," ", 0,"TIME SYSTEM CORR");
			fprintf(fnav,"%6d%54s%-20s\n",gpsUTC.Utc.DEL_t_LS," ","LEAP SECONDS");
		}
		
		fprintf(fnav,"%60s%-20s\n"," ","END OF HEADER");
	}
	
	InitializeSBFDecoding(sbfLogFileName, &SBFData);
	
	// To sort PRNs, use an array (index is SVID 1)  which contains the index of the corresponding measurement set
	int prnIDs[MAX_PRN+1];
	
	while (GetNextBlock(&SBFData, SBFBlock, BLOCKNUMBER_ALL, BLOCKNUMBER_ALL,START_POS_CURRENT | END_POS_AFTER_BLOCK) == 0){
		memset((void *) prnIDs,-1,(MAX_PRN+1)*sizeof(int));
		
		// There are many kinds of measurement blocks and this is multiplied by the number of antennas
		// so it's easiest just to decode and then skip
		if (sbfread_MeasCollectAndDecode(&SBFData, SBFBlock, &MeasEpoch, SBFREAD_ALLMEAS_ENABLED)){
		//if (0){
			if (!makeObsFile){
				continue;
			}
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
					
				// Epoch flag currently set to 0 - could use this ...
				fprintf(tmpObsFile,"> %4d %2.2d %2.2d %2.2d %2.2d%11.7f  %1d%3d\n",
					tmGPS.tm_year+1900,tmGPS.tm_mon+1,tmGPS.tm_mday,tmGPS.tm_hour,tmGPS.tm_min,tmGPS.tm_sec + (double) (MeasEpoch.TOW_ms % 1000)/1000.0,
					0,nSats); // don't output receiver clock offset
				
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
							char cL5[17];strcpy(cL5,BLANK16);
							char pL5[17];strcpy(pL5,BLANK16);
							
							for (SigIdx = 0; SigIdx < MAX_NR_OF_SIGNALS_PER_SATELLITE; SigIdx++){
								const MeasSet_t* const measSet = &(MeasEpoch.channelData[measIndex].measSet[0][SigIdx]); /* main antenna */
								if (measSet->flags != 0){
									SignalType_t sig = (SignalType_t) measSet->signalType;
									int sn = -1;
									if (measSet->CN0_dBHz != F32_NOTVALID)
										sn = rinex_sn(measSet->CN0_dBHz);
									switch (sig){
										case SIG_GPSL1CA: // GPS L1 C/A
											sbf2rnx_format_obs(cL1CA,pL1P,sn,obsInterval,measSet);
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
											sbf2rnx_format_obs(cL2P,pL2P,sn,obsInterval,measSet);
											break;
										case SIG_GPSL2C:  // GPS L2C
											sbf2rnx_format_obs(cL2C,pL2C,sn,obsInterval,measSet);
											break;
										case SIG_GPSL5:   // GPS L5
											sbf2rnx_format_obs(cL5,pL5,sn,obsInterval,measSet);
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
							if (rxSignals[SIG_GPSL5]){
								fprintf(tmpObsFile,"%s%s",cL5,pL5);
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
		// 5902 ReceiverSetup
		{
			//fprintf(stderr,"%d\n",SBF_ID_TO_NUMBER(((VoidBlock_t*)SBFBlock)->ID));
			int rev = SBF_ID_TO_REV(((VoidBlock_t*)SBFBlock)->ID);
			switch (SBF_ID_TO_NUMBER(((VoidBlock_t*)SBFBlock)->ID))
			{
				case sbfnr_GPSNav_1:
				{
					if (makeNavFile){
						rinex_write_nav_gps(fnav,(GPSNav_1_0_t *) SBFBlock);
					}
					break;
				}
			
				default:
					break;
			}
		}
	}
	
	if (makeNavFile){
		fclose(fnav);
	}
	
	// Assemble the observation file
	if (makeObsFile){
		
		fclose(tmpObsFile);
		
		// Write the header
		rinex_write_obs_header(obsFile,rnxObsHeaderFile,&rx,user,rxSignals,obsInterval,firstObsTOW,firstObsWN,lastObsTOW,lastObsWN);

		// Append the observations
		tmpObsFile = fopen(tmpRnxObsFileName,"r");
		char bigBuf[MAXSTR];
		while (fgets(bigBuf,MAXSTR,tmpObsFile)){
			fprintf(obsFile,"%s",bigBuf);
		}
		
		fclose(tmpObsFile);
		fclose(obsFile);
		unlink(tmpRnxObsFileName);
	}
	
	return EXIT_SUCCESS;
 
}
