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
# Last modification date: 2015-09-03

use POSIX;
use Getopt::Std;
use Math::Trig;
use Switch;

use vars qw($opt_d $opt_m $opt_t $opt_o $opt_s $opt_n $opt_w $opt_f $opt_T $opt_p $opt_a $opt_P $opt_e $opt_l $opt_i $opt_g $opt_r $opt_G $opt_u $opt_z); # $opt_F);

# Declare variables, required because of 'use strict'
my($DEBUG,$mjd,$home,$raw,$infile,$zipfile,$zipit,$input,$save,$s,$data);
my($first,$msgID,$msg,$dts,$tstamp,$saw,$ret,$nv08id,$sats,$glosats,$lastMsg,$hdop,$vdop,$tdop);
my($gpsint,$glonassint,$gpsutc,$glonassutc,$gpsglonass,$dataflag);
my($alpha0,$alpha1,$alpha2,$alpha3,$beta0,$beta1,$beta2,$beta3,$reliable,$rs);
my($A1,$A0,$tot,$WNt,$deltatls,$WNlsf,$DN,$deltatlsf,$relyGPS,$NA,$tauc,$relyGLONASS,$rs1,$rs2);
my($tom,$wn,$rxtscor,$numsats,@raw); # $gpsutc,$glonassutc, <-- already defined above
my($i); 
my($antX,$antY,$antZ,$rmsantX,$rmsantY,$rmsantZ,$modeflag);
my($satsys,$satno,@ephemeris,$satstr);
my($msgH,$msgF);

if (!getopts('dm:tosnwfTpaPeligrGuz')){
  print "Usage: $0 [-m mjd] [-d] [-t] [-o] [-s] [-n] [-w] [-f] [-T] [-p] [-a] [-P] [-e] [-l] [-z]\n";
  print "          [-i] [-g] [-r] [-G] [-u] [-z]\n"; # [-F file]\n";
  print "  -m mjd  MJD\n";
  print "  -t      extract Time, Date and Time Zone offset\n";
  print "  -o      extract Receiver Operating Parameters\n";
  print "  -s      extract Visible Satellites\n";
  print "  -n      extract Number of Satellites and DOP\n";
  print "  -w      extract Software Version, Device ID and Number of Channels\n";
  print "  -f      extract 1PPS 'sawtooth' correction\n";
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
#  print "  -F file extract data for a specific file\n";
  exit;
}

if ($opt_m){$mjd=$opt_m;}else{$mjd=int(time()/86400) + 40587;}

$home=$ENV{HOME};
$raw="$home/raw";

$infile="$raw/$mjd.rx";
$zipfile=$infile.".gz";
$zipit=0;
if (-e $zipfile){
  `gunzip $zipfile`;
  $zipit=1;
}


open (RXDATA,$infile) or die "Can't open data file $infile: $!\n";

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
			case "46" 
			{
				($ret,$dts) = decodeMsg46($msg);
				# ... but we will specifically show its content if requested
				if ($opt_t) {
					if($ret ne "") { print $dts." ".$ret."\n"; }
				}
			} 
			# Ionosphere parameters - partial response to F4h
			case "4A" 
			{
				if($opt_i)
				{
					($alpha0,$alpha1,$alpha2,$alpha3,$beta0,$beta1,$beta2,$beta3,$reliable) = decodeMsg4A($msg);
					$rs = ""; 
					if($reliable != 255){ $rs = " NOT"; }
					printf("%s:  Ionosphere parameters - they are%s reliable\n",$dts,$rs);
					printf("          Alpha[0..3]: %19.12e %19.12e %19.12e %19.12e\n",$alpha0,$alpha1,$alpha2,$alpha3);
					printf("           Beta[0..3]: %19.12e %19.12e %19.12e %19.12e\n",$beta0,$beta1,$beta2,$beta3);
				}
			} 
			# GPS, GLONASS and UTC time scale parameters - partial response to F4h
			case "4B" 
			{
				if($opt_g)
				{
					($A1,$A0,$tot,$WNt,$deltatls,$WNlsf,$DN,$deltatlsf,$relyGPS,$NA,$tauc,$relyGLONASS) = decodeMsg4B($msg);
					if(($relyGPS == 255) && ($relyGLONASS == 255))
					{
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
			case "50" 
			{
				if ($opt_P) 
				{
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
			case "60"
			{
				if ($opt_n)
				{
					($ret,$glosats,$hdop,$vdop) = decodeMsg60($msg);
					if($ret ne "") { print $dts." ".$ret."\n"; }
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
				if ($opt_w)
				{
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
					if($ret ne "") 
					{ 
						printf ("%s %0.0f ns\n",$dts,$saw); 
						if($saw > $sawmax) { $sawmax = $saw; $sawmaxtime = $dts; }
						if($saw < $sawmin) { $sawmin = $saw; $sawmintime = $dts; }
					}
				}
			} 
			# Time synchronisation opertaing mode - reponse to 1Dh
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
				if ($opt_a)
				{
					$ret = decodeMsgE7($msg);
					if($ret ne "") { print $dts." ".$ret."\n"; }
				}
			} 
			# Raw data - partial response to F4h 
			case "F5"
			{
				if ($opt_r)
				{
					($tom,$wn,$gpsutc,$glonassutc,$rxtscor,$numsats,@raw) = decodeMsgF5($msg);
					# Check to see if message decoded correctly, if not, the subroutine will
					# return invalid data.  
					if($numsats > 0)
					{
				
						# Get timestamp from $tom
						my($tods) = timeofdayfromgpstime($tom/1000,$wn,$mjd);
						# Show fixed data
						print "$dts  Raw data\n";
						printf("          [Time (formatted),Time(ms),WN]: %s %19.12e ms %5d\n",$tods,$tom,$wn);
						printf("          [GPS-UTC,GLO-UTC,Rx tcor     ]: %19.12e ms %19.12e ms %4d ms\n",,$gpsutc,$glonassutc,$rxtscor);
						# Show per satellite data
						print "                   GLO                                                                  Raw\n";
						print "          Sgnl Sat car  SNR    Carrier phase       Pseudo range      Doppler frequency Data\n";
						print "          Type  No  No dBHz       (cycles)            (ms)                 (Hz)        Flag Rsvd\n";
						#      hh:mm:ss   000 000 000  000 0.0000000000000e-00 0.0000000000000e-00 0.0000000000000e-00 000 000
						for($i=0;$i<$numsats;$i++)
						{
							my($glocarno);
							if(($raw[$i][0]) == 1) { $glocarno = sprintf("%3d",$raw[$i][2]); } else {$glocarno = "  -"; }
							printf ("           %3d %3d %3s  %3d %19.12e %19.12e %19.12e %3d %3d\n",$raw[$i][0],$raw[$i][1],$glocarno,$raw[$i][3],$raw[$i][4],$raw[$i][5],$raw[$i][6],$raw[$i][7],$raw[$i][8]);
#                  printf ("           %3d %3d %3d  %3d %19.12e %19.12e %19.12e %3d %3d\n",$raw[$i][0],$raw[$i][1],$raw[$i][2],$raw[$i][3],$raw[$i][4],$raw[$i][5],$raw[$i][6],$raw[$i][7],$raw[$i][8]);
							# Grab pseudoranges and put them in individual files
							my($fhp);
							my($flnm) = sprintf("%5d_%2d.ps",$mjd,$raw[$i][1]);
							$flnm =~ s/ /0/g;
							#print "\$flnm: $flnm\n"; 
							open($fhp,'>>',$flnm) or die "cannot open $flnm:$!\n"; 
							print $fhp "$tods\t",$raw[$i][5],"\n";
							close($fhp); 
						}
					#if(length($msg) == 81) { print "Finished with individual sats!"; }
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
				if ($opt_e)
				{
					($satsys,$satno,@ephemeris) = decodeMsgF7($msg);
					if(($satsys == 1) || ($satsys == 2))
					{ 
						if($satsys == 1) { $satstr = "GPS"; } else { $satstr = "GLONASS"; }
						print "$dts  Extended Satellite Ephemeris\n";
						printf("          Satellite: %s %d\n",$satstr,$satno);
						if($satsys == 1)  # We have ephemeris for a GPS satellite
						{
							printf("          [Crs,Dn,M0,Cuc    ]: %19.12e %19.12e %19.12e %19.12e\n",$ephemeris[0], $ephemeris[1], $ephemeris[2], $ephemeris[3]);
							printf("          [E,Cus,SqrtA,Toe  ]: %19.12e %19.12e %19.12e %19.12e\n",$ephemeris[4], $ephemeris[5], $ephemeris[6], $ephemeris[7]);
							printf("          [Cic,Omega0,Cis,I0]: %19.12e %19.12e %19.12e %19.12e\n",$ephemeris[8], $ephemeris[9], $ephemeris[10],$ephemeris[11]);
							printf("          [Crc,W,OmegaR,Ir  ]: %19.12e %19.12e %19.12e %19.12e\n",$ephemeris[12],$ephemeris[13],$ephemeris[14],$ephemeris[15]);
							printf("          [Tgd,Toc,Af2,Af1  ]: %19.12e %19.12e %19.12e %19.12e\n",$ephemeris[16],$ephemeris[17],$ephemeris[18],$ephemeris[19]);
							printf("          [Af0,URA,IODE,IODC]: %19.12e %19d %19d %19d\n",         $ephemeris[20],$ephemeris[21],$ephemeris[22],$ephemeris[23]);
							printf("          [CodeL2,L2Pflag,WN]: %19d %19d %19d\n",                 $ephemeris[24],$ephemeris[25],$ephemeris[26]);
						}
						else              # We have ephemeris for a GLONASS satellite
						{
							printf("          [Cno,X,Y,Z         ]: %3d %19.12e %19.12e %19.12e\n",    $ephemeris[0], $ephemeris[1], $ephemeris[2], $ephemeris[3]);
							printf("          [Vx,Vy,Vz          ]:     %19.12e %19.12e %19.12e\n",    $ephemeris[4], $ephemeris[5], $ephemeris[6]);
							printf("          [Ax,Ay,Az          ]:     %19.12e %19.12e %19.12e\n",    $ephemeris[7], $ephemeris[8], $ephemeris[9]);
							printf("          [tb,lambdan,taun,En]:     %19.12e %19.12e %19.12e %5d\n",$ephemeris[10],$ephemeris[11],$ephemeris[12],$ephemeris[13]);
						}
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

if ($opt_f)
{
  print "SAW max: $sawmax ns. $sawmaxtime\n";
  print "and min: $sawmin ns. $sawmintime\n";
}

if (1==$zipit) {`gzip $infile`;}

exit;

# ------ End of main ------------------------------------------------------------

# Subroutines

# ----------------------------------------------------------------------------

sub Debug
{
  my($now) = strftime "%H:%M:%S",gmtime;
  if ($opt_d) {print "$now $_[0] \n";}
} # Debug


#
sub decodeMsg46
{
}

sub decodeMsg4A
{
}

sub decodeMsg4B
{
}

sub decodeMsg50
{
}

sub decodeMsg51
{
}

sub decodeMsg52
{
}

sub decodeMsg60
{
}

sub decodeMsg61
{
	return ("","");
}

sub decodeMsg70
{
}

sub decodeMsg72
{
}

sub decodeMsg73
{
}

sub decodeMsg74
{
	my $m=$_[0];
	if (length($m) != (51*2)){
		return "";
	}
	my @data=unpack "a50",(pack "H*",$m);
	my $par1 = FP80toFP64( $data[0],0,10);
	my $par2 = FP80toFP64(substr $data[0],10,10);
	my $par3 = FP80toFP64(substr $data[0],20,10);
	my $par4 = FP80toFP64(substr $data[0],30,10);
	my $par5 = FP80toFP64(substr $data[0],40,10);
	return ($par1,$par2,$par3,$par4,$par5,1);
}

sub decodeMsg88
{
	my $m=$_[0];
	if (length($m) != (69*2)){
		return "";
	}
	my @data=unpack "d3",(pack "H*",$m);
	my $ret = sprintf "%lf %lf %lf",(2.0E7/180.0)*$data[0]*180/pi,(2.0E7/180.0)*$data[1]*180/pi,$data[2];
	return $ret;
}

sub decodeMsgE7
{
}

sub decodeMsgF5
{
}

sub decodeMsgF6
{
	my $m=$_[0];
	my @data=();
	if (length($m) != (49*2)){
		return @data;
	}
	@data=unpack "d6c",(pack "H*",$m);
	return @data;
}

sub decodeMsgF7
{
}


sub FP80toFP64
{
	my $buf = $_[0];
	my @b = unpack 'C' x length $buf, $buf; # convert from string to array of unsigned char
	# little - endian ....
	my $sign=1.0;
	if (($b[9] & 0x80) != 0x00){
		$sign=-1.0;
	}
	my $exponent = (($b[9] & 0x7f)<<8) + $b[8];
	my $mantissa=unpack "q",$buf; # mantissa is 64 bits - won't work on some platforms
	#Is this a normalized number ?
	my $normalizeCorrection;
	if (($mantissa & 0x8000000000000000) != 0x00){
		$normalizeCorrection = 1;
	}
	else{
		$normalizeCorrection = 0;
	}
	$mantissa &= 0x7FFFFFFFFFFFFFFF;
	
	return $sign * pow(2,$exponent - 16383) * ($normalizeCorrection + $mantissa/pow(2,63));
}


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
  # Lets pervent an endless loop
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