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

# runrinexobstc.pl
# Runs rinexobstc, adds header based on gpscv.conf and can be used to process missed files
#
# 2016-08-17 MJW First version. A minimal script
# 2018-02-13 MJW Specify a configuration file as a command line option

use POSIX;
use Getopt::Std;
use TFLibrary;

use vars qw($opt_a $opt_c $opt_d $opt_h $opt_v $opt_x);

$VERSION="0.1.1";
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

if (!(getopts('a:c:dhvx')) || $opt_h){
	&ShowHelp();
	exit;
}

if ($opt_v){
	print "$0 version $VERSION\n";
	print "Written by $AUTHORS\n";
	exit;
}

$configFile=$configPath."/gpscv.conf";

if (defined $opt_c){
  $configFile=$opt_c;
}

if (!(-e $configFile)){
	ErrorExit("The configuration file $configFile was not found!\n");
}

$maxAge = $MAX_AGE;
if (defined $opt_a){
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

$tmpPath = '/tmp/';
if (defined $Init{"paths:tmp"} ){
	$tmpPath = TFMakeAbsolutePath($Init{"paths:tmp"},$home);
}

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
		AddRINEXHeader($rnxOut);
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
			AddRINEXHeader($rnxOut);
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
	print "\t-c <file> set configuration file\n";
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

# -------------------------------------------------------------------------
sub AddRINEXHeader
{
	# Checked alll required field present
	my $rnxin = $_[0];
	@tobs=GetFirstRINEXObservation($rnxin);
	Debug("Adding RINEX header");
	
	my $rnxout = "$tmpPath/rinex.tmp";
	open(OUT,">$rnxout");
	printf OUT "%9s%11s%-20s%1s%-19s%-20s\n","2.11","","O","G","","RINEX VERSION / TYPE"; # GPS only!
	my @gmt = gmtime(time); 
	my $datestr = sprintf "%04d%02d%02d %02d%02d%02d UTC",$gmt[5]+1900,$gmt[4]+1,$gmt[3],$gmt[2],$gmt[1],$gmt[0];
	printf OUT "%-20s%-20s%-20s%-20s\n","rinexobstc v2.2",$Init{"rinex:agency"},$datestr,"PGM / RUN BY / DATE";
	printf OUT "%-60s%-20s\n",$Init{"antenna:marker name"},"MARKER NAME";
	printf OUT "%-20s%40s%-20s\n",$Init{"antenna:marker number"},"","MARKER NUMBER"; # not required
	printf OUT "%-20s%-40s%-20s\n",$Init{"rinex:observer"},$Init{"rinex:agency"},"OBSERVER / AGENCY";
	my $sn ="xxxxxx";
	my $ver="xxxxxx";
	printf OUT "%-20s%-20s%-20s%-20s\n",$sn,$Init{"receiver:model"},$ver,"REC # / TYPE / VERS";
	my $x=$Init{"antenna:x"};
	$x =~ s/[a-zA-Z]//g;
	my $y=$Init{"antenna:y"};
	$y =~ s/[a-zA-Z]//g;
	my $z=$Init{"antenna:z"};
	$z =~ s/[a-zA-Z]//g;
	printf OUT "%-20s%-20s%-20s%-20s\n",$Init{"antenna:antenna number"},$Init{"antenna:antenna type"}," ","ANT # / TYPE";
	printf OUT "%14.4lf%14.4lf%14.4lf%-18s%-20s\n",$x,$y,$z," ","APPROX POSITION XYZ";
	printf OUT "%14.4lf%14.4lf%14.4lf%-18s%-20s\n",$Init{"antenna:delta h"},$Init{"antenna:delta e"},$Init{"antenna:delta n"}," ","ANTENNA: DELTA H/E/N";
	printf OUT "%6d%6d%6d%42s%-20s\n",1,1,0,"","WAVELENGTH FACT L1/2";
	printf OUT "%6d%6s%6s%6s%6s%6s%6s%6s%6s%6s%-20s\n",5,"L1","L2","C1","P1","P2","","","","","# / TYPES OF OBSERV";
	printf OUT "%6d%6d%6d%6d%6d%13.7lf%-5s%3s%-9s%-20s\n",$tobs[0]+2000,$tobs[1],$tobs[2],$tobs[3],$tobs[4],$tobs[5]," ", "GPS"," ","TIME OF FIRST OBS";
	# printf OUT "%6d%54s%-20s\n",0," ","LEAP SECONDS"; # optional, so leave it out
	printf OUT "%60s%-20s\n","","END OF HEADER";
	close OUT;
	
	`cat $rnxin >> $rnxout`;
	`mv $rnxout $rnxin`;
	
}

# -------------------------------------------------------------------------
sub GetFirstRINEXObservation
{
	open (IN,"<$_[0]");
	while ($l=<IN>){
		if ($l =~/^\s*(\d+)\s*(\d+)\s*(\d+)\s*(\d+)\s*(\d+)\s*(\d+\.\d+)/){
			return ($1,$2,$3,$4,$5,$6);
		}
	}
	close(IN);
}