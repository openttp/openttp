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

# 2016-08-17 MJW First version. A minimal script
#

use POSIX;
use Getopt::Std;
use TFLibrary;

use vars qw($opt_a $opt_d $opt_h $opt_v $opt_x);

$VERSION="0.1.0";
$AUTHORS="Michael Wouters";
$MAX_AGE=7; # number of days to look backwards for missing files

# Check command line
if ($0=~m#^(.*/)#) {$path=$1} else {$path="./"}	# read path info
$0=~s#.*/##; # then strip it

$home=$ENV{HOME};
if (-d "$home/etc")  {
	$configPath="$home/etc";
}
else{	
	ErrorExit("No ~/etc directory found!\n");
} 

if (-d "$home/bin")  {
	$binPath="$home/bin";
}
else{	
	ErrorExit("No ~/bin directory found!\n");
} 

$configFile=$configPath."/gpscv.conf";
if (!(-e $configFile)){
	ErrorExit("The configuration file $configFile was not found!\n");
}

if (!(getopts('a:dhvx')) || $opt_h){
	&ShowHelp();
	exit;
}

if ($opt_v){
	print "$0 version $VERSION\n";
	print "Written by $AUTHORS\n";
	exit;
}

$opt_a = $MAX_AGE;
if ($opt_a){
	$maxAge=$opt_a;
}

$mjdPrev =  int(time()/86400 + 40587-1); # yesterday
$mjdStart = $mjdStop= $mjdPrev;

if ($#ARGV == 1){
	$mjdStart = $ARGV[0];
	$mjdStop  = $ARGV[1];
}
elsif ($#ARGV == 0){
	$mjdStart = $ARGV[0];
	$mjdStop=$mjdStart;
}
elsif ($#ARGV > 1){
	ShowHelp();
	exit;
}

%Init = &TFMakeHash2($configFile,(tolower=>1));
# Check we got the info we need from the config file
@check=("receiver:file extension","paths:receiver data","paths:rinex l1l2",
	"antenna:marker name");

foreach (@check) {
	$tag=$_;
	$tag=~tr/A-Z/a-z/;	
	unless (defined $Init{$tag}) {ErrorExit("No entry for $_ found in $configFile")}
}

if (!($Init{"receiver:manufacturer"} =~ /Javad/)){
	ErrorExit("$0 can only be used with Javad receivers"); 
}

if (!($Init{"receiver:file extension"} =~ /^\./)){ # do we need a period ?
	$rxExt = ".".$Init{"receiver:file extension"};
}
$rxDataPath = TFMakeAbsolutePath($Init{"paths:receiver data"},$home);
$rnxDataPath= TFMakeAbsolutePath($Init{"paths:rinex l1l2"},$home);

if (!$opt_x){
	for ($mjd=$mjdStart;$mjd <= $mjdStop;$mjd++){
		Debug("Processing $mjd");
		$recompress = 0;
		$rxin =   &MakeRxFileName($mjd);
		if (!(-e $rxin)){
			# may need to decompress
			$rxingz = $rxin.'.gz';
			if (!(-e $rxingz)){
				Debug("$rxin or $rxingz is missing - skipping");
				next;
			}
			$recompress = 1;
			Debug("Decompressing ...");
			`gunzip $rxingz`;
		}
		$rnxOut = &MakeRINEXFileName($mjd);
		Debug("Processing");
		`$binPath/rinexobstc $rxin > $rnxOut`;
		if ($recompress==1){
			Debug("Recompressing ...");
			`gzip $rxin`;
		}
	}
}
else{
	for ($mjd=$mjdPrev-$maxAge;$mjd<=$mjdPrev;$mjd++){
		Debug("Checking $mjd");
		
		# If there is a missing raw data file, then just skip
		$rxin =   &MakeRxFileName($mjd);
		if (!(-e $rxin || -e "$rxin.gz")){
			Debug("$rxin or $rxin.gz is missing - skipping");
			next;
		}
		# Look for missing files
		$rnxOut = &MakeRINEXFileName($mjd);
		$recompress=0;
		if (!(-e $rnxOut)){
			Debug("$rnxOut is missing");
			if (!(-e $rxin)){
				$rxingz = $rxin.'.gz';
				$recompress = 1;
				Debug("Decompressing ...");
				`gunzip $rxingz`;
			}
			$rnxOut = &MakeRINEXFileName($mjd);
			Debug("Processing");
			`$binPath/rinexobstc $rxin > $rnxOut`;
			if ($recompress==1){
				Debug("Recompressing ...");
				`gzip $rxin`;
			}
		}
		else{
			Debug("$rnxOut is OK");
		}
	} 
}

# End of main program

#-----------------------------------------------------------------------
sub MakeRxFileName
{
	my $mjd = $_[0];
	my $rxData  = $rxDataPath.$mjd.$rxExt;
	return $rxData;
}

#-----------------------------------------------------------------------
sub MakeRINEXFileName
{
	my $mjd = $_[0];
	my $rnx;
	
	# Convert MJD to YY and DOY
	my $t=($mjd-40587)*86400.0;
	my @tod = gmtime($t);
	my $yy = $tod[5]-(int($tod[5]/100))*100;
	my $doy = $tod[7]+1;
	
	$rnx = sprintf "%s%s%03d0.%02dO", $rnxDataPath,$Init{"antenna:marker name"},$doy,$yy;

	return $rnx;
}

#-----------------------------------------------------------------------
sub ShowHelp
{
	print "\nUsage: $0 [OPTION] ... [startMJD] [stopMJD]\n";
	print "\t-a <num> maximum age of files to look for when reprocessing\n";
	print "            missing files, in days (default $MAX_AGE)\n";
	print "\t-d        debug\n";
  print "\t-h        show this help\n";
	print "\t-x        run missed processing\n";
  print "\t-v        print version\n";
	print "\nIf an MJD range is not specified, the previous day is processed\n";
	print "The stop MJD is not required for single day processing\n\n";
}

#-----------------------------------------------------------------------
sub ErrorExit {
  my $message=shift;
  @_=gmtime(time());
  printf "%02d/%02d/%02d %02d:%02d:%02d $message\n",
    $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
  exit;
}

# -------------------------------------------------------------------------
sub Debug
{
	if ($opt_d){
		printf STDERR "$_[0]\n";
	}
}