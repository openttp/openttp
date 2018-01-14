#!/usr/bin/perl

#
#
# The MIT License (MIT)
#
# Copyright (c) 2016  Louis Marais, Michael J. Wouters
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# nv08extract.pl

use warnings;
use strict;

# Extract data from  NV08C log files and present it in human readable
# format.
#
# Based on restextract.pl
#
# Version 1.0 start date: 2015-06-24
# Version 1.1 start date: 2015-09-03
# Author: Louis Marais
# Last modification date: 2018-01-14
#
# 2018-01-14 MJW Fix up wot was broken. Wot was old is new again.
#								 Version 1.2.0

use POSIX;
use Getopt::Std;
use Math::Trig;
use Switch;
use NV08C::DecodeMsg;
use TFLibrary;

use vars qw($opt_h $opt_v $opt_c $opt_d $opt_m $opt_t $opt_o $opt_s $opt_n $opt_w $opt_f $opt_T $opt_p $opt_a $opt_P $opt_e $opt_l $opt_i $opt_g $opt_r $opt_G $opt_u $opt_z); # $opt_F);

# Declare variables, required because of 'use strict'
my($DEBUG,$mjd,$home,$raw,$configFile,$infile,$zipfile,$zipit,$input,$save,$s,$data);
my($first,$msgID,$msg,$dts,$tstamp,$saw,$ret,$nv08id,$sats,$gpssats,$glosats,$lastMsg,$hdop,$vdop,$tdop,$ephemeris);
my($gpsint,$glonassint,$gpsutc,$glonassutc,$gpsglonass,$dataflag);
my($alpha0,$alpha1,$alpha2,$alpha3,$beta0,$beta1,$beta2,$beta3,$reliable,$rs);
my($A1,$A0,$tot,$WNt,$deltatls,$WNlsf,$DN,$deltatlsf,$relyGPS,$NA,$tauc,$relyGLONASS,$rs1,$rs2);
my($tom,$wn,$rxtscor,$numsats,@raw); # $gpsutc,$glonassutc, <-- already defined above
my($i); 
my($antX,$antY,$antZ,$rmsantX,$rmsantY,$rmsantZ,$modeflag);
my($satsys,$satno,@ephemeris,$satstr);
my($msgH,$msgF);
my ($AUTHORS,$VERSION);
my (%Init);

$AUTHORS = "Louis Marais,Michael Wouters";
$VERSION = "1.2.0";

$0=~s#.*/##;

$home=$ENV{HOME};
$configFile="$home/etc/gpscv.conf";

if (!getopts('dhc:vm:tosnwfTpaPeligrGuz') || ($#ARGV>=1) || $opt_h){
  ShowHelp();
  exit;
}

if ($opt_v){
  print "$0 version $VERSION\n";
  print "Written by $AUTHORS\n";
  exit;
}

if ($opt_m){$mjd=$opt_m;}else{$mjd=int(time()/86400) + 40587;}

if (defined $opt_c){
  $configFile=$opt_c;
}

if (!(-e $configFile)){
  ErrorExit("A configuration file was not found!\n");
}

&Initialise($configFile);

$Init{"paths:receiver data"}=TFMakeAbsolutePath($Init{"paths:receiver data"},$home);

if (!($Init{"receiver:file extension"} =~ /^\./)){ # do we need a period ?
  $Init{"receiver:file extension"} = ".".$Init{"receiver:file extension"};
}

$zipit=0;

if ($#ARGV == 0){ # a file has been explicitly specified
	$infile = $ARGV[0];
	if ($infile =~/\.gz$/){
		$zipfile = $infile;
		`gunzip $zipfile`;
		$zipit=1;
		$infile =~ s/\.gz//; # this is what we open
	}
}
else{
	$infile=$Init{"paths:receiver data"}.$mjd.$Init{"receiver:file extension"};
	$zipfile=$infile.".gz";
	if (-e $zipfile){
		`gunzip $zipfile`;
		$zipit=1;
	}
}

open (RXDATA,$infile) or die "Can't open the data file $infile: $!\n";

# Initialise variables that require it.
$input = "";
$ret = "";
$dts = "";
$save = "";

my($sawmax) = -999.9;
my($sawmin) = 999.9;
my($sawmaxtime,$sawmintime);
my ($hh,$mm,$ss,$tods);

while ($s = <RXDATA>) {
		chomp $s;
		($msgID,$tstamp,$msg) = split /\s+/,$s;
		if (defined $tstamp){
			($hh,$mm,$ss) = split /:/,$tstamp;
			if (defined $hh && defined $mm && defined $ss){
				$tods = $hh*3600+ $mm*60+$ss;
			}
		}
		switch ($msgID)
		{
			# Time, date and time zone message - response to 23h 
			case "46" {
				($ret,$dts) = decodeMsg46($msg);
				# ... but we will specifically show its content if requested
				if ($opt_t) {
					if($ret ne "") { print $dts." ".$ret."\n"; }
				}
			} 
			# Ionosphere parameters - partial response to F4h
			case "4A" {
				if($opt_i){
					($alpha0,$alpha1,$alpha2,$alpha3,$beta0,$beta1,$beta2,$beta3,$reliable) = decodeMsg4A($msg);
					$rs = ""; 
					if($reliable != 255){ $rs = " NOT"; }
					printf("%s:  Ionosphere parameters - they are%s reliable\n",$dts,$rs);
					printf("          Alpha[0..3]: %19.12e %19.12e %19.12e %19.12e\n",$alpha0,$alpha1,$alpha2,$alpha3);
					printf("           Beta[0..3]: %19.12e %19.12e %19.12e %19.12e\n",$beta0,$beta1,$beta2,$beta3);
				}
			} 
			# GPS, GLONASS and UTC time scale parameters - partial response to F4h
			case "4B" {
				if($opt_g){
					($A1,$A0,$tot,$WNt,$deltatls,$WNlsf,$DN,$deltatlsf,$relyGPS,$NA,$tauc,$relyGLONASS) = decodeMsg4B($msg);
					if(($relyGPS == 255) && ($relyGLONASS == 255)){
						#$rs1 = "";
						#if($relyGPS != 255){ $rs1 = " NOT"; }  
						#$rs2 = "";
						#if($relyGLONASS != 255){ $rs2 = " NOT"; }  
						print "$dts:  GLONASS and GPS Time scale parameters.\n";
						printf("                            [A1,A0,t_ot,WN_t]: %19.12e %19.12e %10d %10d\n",$A1,$A0,$tot,$WNt);
						printf("           [delta t_LS,WN_LSF,DN,delta t_LSF]: %19d %19d %10d %10d\n",$deltatls,$WNlsf,$DN,$deltatlsf);
						printf("                                  [N^A,tau_c]: %19d %19.12e\n",$NA,$tauc);
						#printf("           GPS time parameters are%s reliable; GLONASS time parameters are%s reliable.\n",$rs1,$rs2);
					}  
				}
			} 
			# Current port status - response to 0Bh
			case "50" {
				if ($opt_P) {
					$ret = decodeMsg50($msg);
					if($ret ne "" ) { print $dts." ".$ret."\n"; }
				}
			}
			# Receiver operating parameters - response to 0Dh
			case "51"  
			{
				if ($opt_o) 
				{
					$ret = decodeMsg51($msg);
					if($ret ne "") { print $dts." ".$ret."\n"; }
				}
			}
			# Visible satellites - response to 24h
			case "52"
			{
				if ($opt_s) 
				{
					($ret,$sats) = decodeMsg52($msg);
					if ($ret ne "") 
					{
						$ret = "\n".
										"                               Carrier\n".
										"                    Satellite frequency                      Signal\n".
										"Satellite    SVN     on-board    slot   Elevation  Azimuth  strength\n".
										" System     number    number    number  (degrees) (degrees)  (dBHz)\n\n".
										$ret;
					} else {
						$ret = "No satellites visible\n";
					}  
					print $dts." ".$ret."\n";
				}
			} 
			# Number of satellites used and DOP - reponse to 21h
			case "60"{
				if ($opt_n){
					($gpssats,$glosats,$hdop,$vdop) = decodeMsg60($msg);
					if ($ret ne ""){
						if ($opt_z){
							printf ("$tods %d %d %5.1f %5.1f\n",$gpssats,$glosats,$hdop,$vdop); 
						}
						else{
							printf ("%s GPS=%d GLN=%d HDOP=%5.1f VDOP=%5.1f\n",$dts,$gpssats,$glosats,$hdop,$vdop);
						}
					}
				}
			} 
			# DOP and RMS for calculated PVT - reponse to 31h
			case "61"
			{
				if ($opt_p)
				{
					($ret,$tdop) = decodeMsg61($msg);
					if($ret ne "") { print $ret."\n"; } # no timestamp printed, because message 88 is always associated with message 61
				}
			}
			# Software version - reponse to 1Bh
			case "70"
			{
				if ($opt_w){
					$ret = decodeMsg70($msg);
					if($ret ne "") { print $dts." ".$ret."\n"; }
				}
			} 
			# Time and frequency parameters - response to 1Fh
			case "72"
			{
				if ($opt_f)
				{
					($ret,$saw) = decodeMsg72($msg);
					if($ret ne "") { 
						if ($opt_z){
							printf("$tods $saw\n");
						}
						else{
							printf ("%s %0.0f ns\n",$dts,$saw); 
							if($saw > $sawmax) { $sawmax = $saw; $sawmaxtime = $dts; }
							if($saw < $sawmin) { $sawmin = $saw; $sawmintime = $dts; }
						}
					}
				}
			} 
			# Time synchronisation operating mode - reponse to 1Dh
			case "73"
			{
				if ($opt_T)
				{
					$ret = decodeMsg73($msg);
					if($ret ne "") { print $dts." ".$ret."\n"; }
				}
			} 
			# Time scale parameters - response to 1Eh   
			case "74"
			{
				if ($opt_l)
				{
					($gpsint,$glonassint,$gpsutc,$glonassutc,$gpsglonass,$dataflag) = decodeMsg74($msg);
					if($dataflag < 32)
					{
						printf("$tods %14.12f %14.12f %14.12f %14.12f %14.12f\n",$gpsint,$glonassint,$gpsutc,$glonassutc,$gpsglonass); 
					}
				}
			} 
			# PVT vector - reponse to 27h
			case "88"
			{
				if ($opt_p)
				{
					$ret = decodeMsg88($msg);
					if($ret ne "") { print $tstamp." ".$ret."\n"; }
				}
			} 
			# Additional operating parameters - response to B2h 
			case "E7"
			{
				if ($opt_a){
					$ret = decodeMsgE7($msg);
					if($ret ne "") { print $dts." ".$ret."\n"; }
				}
			} 
			# Raw data - partial response to F4h 
			case "F5"
			{
				if ($opt_r){
					$ret = decodeMsgF5($msg);
					# Check to see if message decoded correctly, if not, the subroutine will
					# return invalid data.  
					if($ret ne ""){
						print "$dts $ret";
					}
				}
			} 
			# Geocentric coordinates of antenna in WGS84 - partial response to F4h 
			case "F6"
			{
				if ($opt_G)
				{
					my($antX,$antY,$antZ,$rmsantX,$rmsantY,$rmsantZ,$modeflag) = decodeMsgF6($msg);
					if($modeflag <= 1)
					{ 
						my($mode) = "STATIC";
						if($modeflag == 1) { $mode = "DYNAMIC"; }
						if ($opt_z){
							printf ("$tods %19.12e %19.12e %19.12e %d\n",$antX,$antY,$antZ,$modeflag); 
						}
						else{
							print "$tstamp  Geocentric coordinates of antenna (WGS-84). Receiver mode is $mode.\n";
							printf("          [X(m),Y(m),Z(m)]: %19.12e %19.12e %19.12e\n",$antX,$antY,$antZ);
							printf("          [RMS errors (m)]: %19.12e %19.12e %19.12e\n",$rmsantX,$rmsantY,$rmsantZ);
						}
					}
				}
			} 
			# Extended ephemeris - partial response to F4h 
			case "F7"
			{
				if ($opt_e){
					($ephemeris) = decodeMsgF7($msg);
					if ($ephemeris ne ""){
						
						print "$dts $ephemeris";
					}
				}
			} 
			else
			{
				if ($opt_u)
				{
				}
				
			}
	}
}

close RXDATA;

if ($opt_f){
  print "SAW max: $sawmax ns. $sawmaxtime\n";
  print "and min: $sawmin ns. $sawmintime\n";
}

if (1==$zipit) {`gzip $infile`;}

exit;

# ------ End of main ------------------------------------------------------------

# Subroutines

# ----------------------------------------------------------------------------

sub ShowHelp
{
	print "Usage: $0 [OPTIONS] ... [filename]\n";
  print "\n"; 
  print "  -h      show this help\n";
  print "  -v      show version\n";
  print "  -c file use the specified configuration file\n";    
  print "  -m mjd  MJD\n";
  print "  -t      extract Time, Date and Time Zone offset\n";
  print "  -o      extract Receiver Operating Parameters\n";
  print "  -s      extract Visible Satellites\n";
  print "  -n      extract Number of Satellites and DOP\n";
  print "  -w      extract Software Version, Device ID and Number of Channels\n";
  print "  -f      extract 1 PPS 'sawtooth' correction\n";
  print "  -T      extract Time Synchronisation Operating Mode (ant cab delay, aver time)\n";
  print "  -p      extract PVT Vectors and associated quality factors (incl. TDOP)\n";
  print "  -a      extract Additional Operating Parameters\n";
  print "  -P      extract Port status messages\n";
  print "  -e      extract Satellite ephemeris\n";
  print "  -l      extract Time scale parameters\n";
  print "  -i      extract Ionosphere parameters\n";
  print "  -g      extract GPS, GLONASS and UTC time scale parameters\n";
  print "  -r      extract Raw data (pseudoranges, etc)\n";
  print "  -G      extract Geocentric antenna coordinates in WGS-84 system\n";
  print "  -u      extract Unknown message (garbage data)\n";
  print "  -z      less verbose output\n";
  print "  The default configuration file is $configFile\n";
}

#-----------------------------------------------------------------------------
sub ErrorExit 
{
  my $message=shift;
  @_=gmtime(time());
  printf "%02d/%02d/%02d %02d:%02d:%02d $message\n",
    $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
  exit;
} # ErrorExit

#----------------------------------------------------------------------------
sub Debug
{
  my($now) = strftime "%H:%M:%S",gmtime;
  if ($opt_d) {print "$now $_[0] \n";}
} # Debug

#----------------------------------------------------------------------------

sub Initialise 
{
  my $name=shift; # The name of the configuration file.
  
  # Define the parameters that MUST have values in the configuration file
  # note that all these values come in as lowercase, no matter how they are
  # written in the configuration file

  my @required=( "paths:receiver data","receiver:file extension");
  %Init=&TFMakeHash2($name,(tolower=>1));
  
  if (!%Init){
    print "Couldn't open $name\n";
    exit;
  }
  
  my $err;
  
  # Check that all required information is present
  $err=0;
  foreach (@required) {
    unless (defined $Init{$_}) {
      print STDERR "! No value for $_ given in $name\n";
      $err=1;
    }
  }
  exit if $err;

} # Initialise

#----------------------------------------------------------------------------

sub timeofdayfromgpstime  #($tom/1000,$wn);
{
  # Receive gps time of week (seconds) and gps week number, return a formatted
  # time string. Note that gps week number is modulo 1024 (as it should be).
  my($tow,$wn,$mjd) = (@_);
  my($startdate,$dt,$ss,$mm,$hh,$dy_mjd,$mn_mjd,$yr_mjd,$tsd,$tst,$ts,$dy,$mn,$yr);
  my($loops);
  # Set start date: GPS week 0 started on 1980-01-06.
  $startdate = mktime(0,0,0,6,1-1,80);
  # Get date from MJD (in unix time, seconds since 1970-01-01)
  $dt = ($mjd-40587)*86400;
  # Get day time year of MJD ($hh, $mm, $ss all 0):
  ($ss,$mm,$hh,$dy_mjd,$mn_mjd,$yr_mjd) = localtime($dt);
  
#  $tsd = sprintf("%4d-%2d-%2d",$yr_mjd+1900,$mn_mjd+1,$dy_mjd);
#  $tsd =~ s/ /0/g; 
#  $tst = sprintf("%2d:%2d:%12.9f",$hh,$mm,$ss);
#  $tst =~ s/ /0/g;
#  $ts = $tsd." ".$tst."\n";
#  print "$ts\n";

#  # Get date from result.
#  ($ss,$mm,$hh,$dy,$mn,$yr) = localtime($startdate);
#
#  $tsd = sprintf("%4d-%2d-%2d",$yr+1900,$mn+1,$dy);
#  $tsd =~ s/ /0/g; 
#  $tst = sprintf("%2d:%2d:%2d",$hh,$mm,$ss);
#  $tst =~ s/ /0/g;
#  $ts = $tsd." ".$tst."\n";
#  print "$ts\n";


  # Get date of observation
  $startdate += $wn * 7.0 * 86400 + $tow;
  $yr = 0;
  # Lets prevent an endless loop
  $loops = 0;
  while(($yr < $yr_mjd) && ($loops < 10))
  {
    # Get date from result.
    ($ss,$mm,$hh,$dy,$mn,$yr) = localtime($startdate);
    # Format the return string 
    $tsd = sprintf("%4d-%2d-%2d",$yr+1900,$mn+1,$dy);
    $tsd =~ s/ /0/g; 
    $tst = sprintf("%2d:%2d:%12.9f",$hh,$mm,$ss+($tow-int($tow)));
    $tst =~ s/ /0/g;
    $ts = $tsd." ".$tst;
    # Check if we need to add 1024 weeks
    # If it is way off, add 1024 weeks to the date, until the date is approximately
    # the same. This way the routine will work even if the weeks are not sent modulo
    # 1024...
    if($yr < $yr_mjd){ $startdate += 1024 * 7 * 86400; }
    $loops++;   
  }
  if($loops >= 10) { $ts = "0000-00-00 00:00:00.000000000"; }
  # Return the result
  return($ts);
  # Temporary empty return string
  #return ("soon ... ;-)");
} # timeofdayfromgpstime