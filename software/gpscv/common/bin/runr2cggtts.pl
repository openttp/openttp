#!/usr/bin/perl -w

#
# The MIT License (MIT)
#
# Copyright (c) 2016 Michael J. Wouters
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

#
# Script to help with automatic processing of RINEX time-transfer files via rinex_cggtts_nmia
#
# Example usage
# runr2cggtts STATION MJD C1|P1|P2 RINEX_OBS_PATH RINEX_NAV_STATION RINEX_NAV_PATH paramCGGTTS file 
# ./runr2cggtts.pl SYDN 57606 C1 ../rinex-sydn brdc ./brdc SYDN.paramCGGTTS.dat output_path
# Modification history
# 2017-01-04 MJW insert current number of leap seconds in paramCGGTTS automatically
# 2017-01-17 MJW create directories needed for output

$STA=$ARGV[0];
$MJD=$ARGV[1];
$CODE=$ARGV[2];
$RNXOBSDIR=$ARGV[3];
$NAV=$ARGV[4];
$RNXNAVDIR=$ARGV[5];
$PARS=$ARGV[6];
$CGGTTSDIR=$ARGV[7];

if ($#ARGV != 7){
	print "Invalid arguments\n";
	print "Usage:\n";
	print "run2cggtts.pl station MJD C1|P1|P2 RINEX_obs_path RINEX_nav_station RINEX_nav_path paramCGGTTS_file cggtts_output_path\n";
	print "\nExample:\n"; 
	print "./runr2cggtts.pl SYDN 57606 C1 ../rinex-sydn brdc ./brdc SYDN.paramCGGTTS.dat sydn-ct\n\n";
	exit;
}

unless  ($CODE eq 'C1' || $CODE eq 'P1' || $CODE eq 'P2'){
	print STDERR "Invalid code $CODE\n";
	exit;
}

# Clean up from last run
@files = glob("*.tmp");
unlink @files;

# Convert MJD to YY and DOY
$t=($MJD-40587)*86400.0;
@tod = gmtime($t);
$yy1 = $tod[5]-(int($tod[5]/100))*100;
$doy1 = $tod[7]+1;

# and for the following day
$t=($MJD+1-40587)*86400.0;
@tod = gmtime($t);
$yy2 = $tod[5]-(int($tod[5]/100))*100;
$doy2 = $tod[7]+1;

print "Creating RINEX (single) observation files for $MJD (yy=$yy1 doy=$doy1)\n";

# Make RINEX file names
$rnx1=MakeRINEXObservationName($RNXOBSDIR,$STA,$yy1,$doy1,1);
$nav1=MakeRINEXNavigationName($RNXNAVDIR,$NAV,$yy1,$doy1,1);
$rnx2=MakeRINEXObservationName($RNXOBSDIR,$STA,$yy2,$doy2,0); #don't require next day
$nav2=MakeRINEXNavigationName($RNXNAVDIR,$NAV,$yy2,$doy2,0);

if (-e $PARS){
	if (!($PARS eq "paramCGGTTS.dat")){
		`cp $PARS paramCGGTTS.dat`;
	}
}
else{
	print "Can't find $PARS\n";
	exit;
}

# Got all needed files so proceed

# Get the current number of leap seconds
my $nLeap = 0;
# Try the RINEX navigation file.
# Use the number of leap seconds in $nav1
open(IN,"<$nav1");
while ($l=<IN>){
  if ($l=~/\s+(\d+)\s+LEAP\s+SECONDS/){
    print "Current leap seconds = $1\n";
    $nLeap=$1;
    last;
  }
}
close IN;
# Otherwise try the system leapseconds file
if ($nLeap==0){
  # TO DO
}

if ($nLeap>0){
  # Back up paramCGGTTS
  `cp -a $PARS $PARS.bak`;
  `cp -a $PARS $PARS.tmp`;
  open(IN,"<$PARS");
  open(OUT,">$PARS.tmp");
  while ($l=<IN>){
    if ($l=~/LEAP\s+SECOND/){
      print OUT $l;
      $l=<IN>;
      print "LS was $l, now $nLeap\n";
      print OUT $nLeap,"\n";
    }
    else{
      print OUT $l;
    }
  }
  close IN;
  close OUT;
  `cp $PARS.tmp $PARS`;
}

# Extract the appropriate single RINEX observation 

$rnxout1=sprintf("$STA%03d0.%02do",$doy1,$yy1);
print "Extracting $CODE from $rnx1\n";
`xrnxobs.pl $CODE $rnx1 > $rnxout1`;

$rnxout2=sprintf("$STA%03d0.%02do",$doy2,$yy2);
if (-e $rnx2){ # don't demand this
	print "Extracting $CODE from $rnx2\n";
	`xrnxobs.pl $CODE $rnx2 > $rnxout2`;
}

# Create the input file for rinex_cggtts
open(OUT,">inputFile.dat");

print OUT "FILE_RINEX_NAV\n";
print OUT $nav1,"\n";

print OUT "FILE_RINEX_NAV_P\n";
print OUT $nav2,"\n";

print OUT "FILE_RINEX_OBS\n"; 
print OUT $rnxout1,"\n";

print OUT "FILE_RINEX_OBS_P\n";
print OUT $rnxout2,"\n";

print OUT "FILE_CGGTTS_LOG\n"; 
print OUT "$MJD.cggtts.log.tmp\n";

print OUT "FILE_CGGTTS_OUT\n";
print OUT "$MJD.cctf\n";

print OUT "FILE_CGGTTS_MIX\n";
print OUT "$MJD.cggtts.mix.tmp\n";
 
print OUT "MODIFIED_JULIAN_DAY\n"; 
print OUT "$MJD\n";

close OUT;

`rinex_cggtts_nmia`;

if (!(-e $CGGTTSDIR)){
	`mkdir -p $CGGTTSDIR`;
}

if (-e "$MJD.cctf"){
	`cp $MJD.cctf $CGGTTSDIR/$MJD.cctf`;
}

`cp $PARS.bak $PARS`;

sub MakeRINEXObservationName
{
	my ($obsdir,$sta,$yy,$doy,$req) = @_;
	my $rnx;
	$rnx=sprintf("$obsdir/$sta%03d0.%02do",$doy,$yy);
	if (!(-e $rnx)){
		$rnx=sprintf("$obsdir/$sta%03d0.%02dO",$doy,$yy);
		if (!(-e $rnx) && ($req==1)){
			print STDERR "Can't find $rnx (or .o either)\n";
			exit;
		}
	}
	return $rnx;
}

sub MakeRINEXNavigationName
{
	my ($navdir,$nav,$yy,$doy,$req) = @_;
	my $rnx;
	$rnx=sprintf("$navdir/$nav%03d0.%02dn",$doy,$yy);
	if (!(-e $rnx)){
		$rnx=sprintf("$navdir/$nav%03d0.%02dN",$doy,$yy);
		if (!(-e $rnx) && ($req==1)){
			print STDERR "Can't find $rnx (or .n either)\n";
			exit;
		}
	}
	return $rnx;
}

