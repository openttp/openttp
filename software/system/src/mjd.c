/*
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015  Stephen Quigg, Michael J. Wouters
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Description : date <--> mjd conversion program			     */
/*      Compile: cc -o mjd mjd.c -lm					     */
/*  NOTE date can be passed to mjd by the Unix utility 'date'. The format    */
/*       is 'date -u +" "%d" "%m" "%Y | mjd' (you may need a path for mjd)   */ 
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* 14 03 2000 v1.4   SQ		- Fix up bug in argc checking to prevent 
				  core dump.
				- Added -h and -v switches.
	 24-04-2008 v1.5.1 MJW  - minor compiler fixups
	            v1.6   MJW  - -t option for today's MJD
	 2016-07-10 v1.7   MJW  - -y option for DOY
*/	

#include <stdio.h>
#include <math.h>
#include <float.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define VERSION "1.7"

static void date_to_mjd(int day, int month, int year);
static void mjd_to_date(long mjd);
static void mjd_to_yday(long mjd);

char args[] = "-u +' '%d' '%m' '%Y";


int
main(
int argc,char *argv[])
{
  int day, month, year;
  int mjd;
  
  time_t tnow;
	struct tm *gmt;
	
  if(argc == 1){
    scanf("%d %d %d",& day, &month, &year);
    date_to_mjd(day, month, year);
    exit(EXIT_SUCCESS);
  }
	
  if(argc == 5 && strcmp(argv[1], "-d" ) == 0){
    day   = atoi(argv[2]);
    month = atoi(argv[3]);
    year  = atoi(argv[4]);
    date_to_mjd(day, month, year);
    printf("\n");
    exit(EXIT_SUCCESS);
  }
	
  if(argc == 3 && strcmp(argv[1], "-m" ) == 0){
    mjd = atoi(argv[2]);
    mjd_to_date(mjd);
    printf("\n");
    exit(EXIT_SUCCESS);
  }
 
  if(argc == 3 && strcmp(argv[1], "-y" ) == 0){
    mjd = atoi(argv[2]);
    mjd_to_yday(mjd);
    printf("\n");
    exit(EXIT_SUCCESS);
  }
  
  if(argc == 2 && strcmp(argv[1], "-h" ) == 0){
    printf("\nmjd version %s\n\n",VERSION);
    printf("usage: mjd [-d DD MM YYYY] [-m MJD] [-y MJD] [-h] [-v]\n\n");

    printf("Options:-\n\n");
    printf("\t-d\t: convert date to MJD\n");
    printf("\t-m\t: convert MJD to date\n");
    printf("\t-y\t: convert MJD to DOY year\n");
    printf("\t-t\t: show today's MJD\n");
    printf("\t-h\t: show this help\n");
    printf("\t-v\t: show version\n\n");
    printf("See also the man page for 'mjd'\n\n");
    exit(EXIT_SUCCESS);
  }
	
  if(argc == 2 && strcmp(argv[1], "-v" ) == 0)
  {
    printf("\nversion %s\n\n",VERSION);
    exit(EXIT_SUCCESS);
  }
	
  if(argc == 2 && strcmp(argv[1], "-t" ) == 0)
  {
		tnow = time(NULL);
		if (!(gmt=gmtime(&tnow)))
		{
			fprintf(stderr,"gmtime() failed!\n");
			return EXIT_FAILURE;
		}
		date_to_mjd(gmt->tm_mday, gmt->tm_mon+1,gmt->tm_year+1900);
    printf("\n");
    exit(EXIT_SUCCESS);
  }
	
  printf("mjd: invalid option or wrong number of parameters\n");
  printf("usage: mjd [-d DD MM YYYY] [-m MJD] [-y MJD] [-t] [-h] [-v]\n");
	
  return EXIT_FAILURE;
} 

void date_to_mjd(int day, int month, int year)
{
  int f, j_day;
  f = floor((14 - month)/12.0);
  j_day = floor(30.61*(month+1+f*12.0)) + day + floor(365.25*(year - f));
  j_day = j_day - floor((floor((year - f)/100.0)+1)*.75)+1720997 - 2400001;
  printf("%d",j_day); /* use "%ld\n" if type is 'long'        */
}

void mjd_to_date(long mjd)
{
  int  day, month, year;
  long julian;
  double y2;
  julian = mjd  + 2400001l;
  y2 = julian - 1721119.1+floor(0.75 * floor((julian - 1684594.75)/36524.25));
  day = floor(y2-floor(365.25*floor(y2/365.25))+122.2);
  year = floor(y2/365.25)+floor(day/429);
  month = floor(day/30.61)-1-floor(day/429)*12;
  day = day - floor(30.61*floor(day/30.61));
  printf("%2d %2d %4d", day, month,year);
}

void mjd_to_yday(long mjd){
  int  day, month, year;
  long julian;
  double y2;
  struct tm tmptm;
  
  julian = mjd  + 2400001l;
  y2 = julian - 1721119.1+floor(0.75 * floor((julian - 1684594.75)/36524.25));
  day = floor(y2-floor(365.25*floor(y2/365.25))+122.2);
  year = floor(y2/365.25)+floor(day/429);
  month = floor(day/30.61)-1-floor(day/429)*12;
  day = day - floor(30.61*floor(day/30.61));
  
  tmptm.tm_sec=tmptm.tm_min=tmptm.tm_hour=0;
  tmptm.tm_mday=day;
  tmptm.tm_mon=month-1;
  tmptm.tm_year=year-1900;
  tmptm.tm_isdst=0;
  
  mktime(&tmptm);
  
  printf("%d %d",tmptm.tm_yday+1,year);
}


/* ---------------------------------------------------------------------------

# Man page for mjd. convert to mjd.1 using
#  pod2man --center=" " -release=" " mjd.pod > /usr/man/man1/mjd.1
#

=head1 NAME

mjd - MJD (Modified Julian Day) to/from Date conversion.

=head1 SYNOPSIS

B<mjd> C<[ -d DATE][ -m MJD] [-y MJD] [-t] [-v]>

=head1 DESCRIPTION

B<mjd> converts a date to an MJD and vice versa. Output MJD's are truncated to 
5 digits. Input MJD's assume a leading "27".

=head1 OPTIONS

If no option is given, then mjd implicitly does a date-to-MJD conversion.
This is for compatibility with older versions of B<mjd> which had no options.
Input is via console or pipe (see EXAMPLES and NOTE).

=over 8

=item B<-d DATE>

Convert a date to an MJD.  Output is a 5-digit MJD.

=item B<-m MJD>

Convert an MJD to a date. Output is in form  DD  MM YYYY.

=item B<-y MJD>

Convert an MJD to day of year (1..366) and year. Output is in the form DOY YYYY.

=back

=head1 EXAMPLES

B<date  -u  +" %d  %m %Y" | mjd> returns the current mjd. See B<date> man page
for more information on date function.

B<mjd -d 1 1 1999> returns 51231.

B<mjd -m 51231> returns 1 1 1999.

=head1 NOTE

When no switch is used, the MJD is returned without a linefeed (for 
compatibility with earlier versions of mjd). If input is on the command line,
then output has a line-feed appended.

=head1 BUGS

No prompts are given in this version of mjd. No range checking is done on
input; type in nonsense and get nonsense back!  

Range is (27)00000 to (27)99999, ie 17 Nov 1858 to 31 Aug 2132!

=head1 SEE ALSO

date(1)

=cut

*/
