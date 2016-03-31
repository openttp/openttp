#!/usr/bin/perl -w

#
# The MIT License (MIT)
#
# Copyright (c) 2015 Michael J. Wouters
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

# look for unprocessed raw data files and process them
# reprocess should normally be run only by cron; running manually can disturb
# reprocessing already scheduled
# RBW 10.11.99

# Modification history:
#	   RBW  Use TFLibrary to build initialisation hash
#  2002-04-02 RBW  Allow up to MAXATTEMPTS to reprocess
#  ???        MJW Path fixups for new directory structure
#  2015-03-30 MJW Fixups to work with new configuration file. Renamed to reprocess.pl
#

use TFLibrary;
use POSIX;
use Getopt::Std;
use vars qw($opt_d $opt_h $opt_v);

$VERSION="1.0";
$AUTHOR="Bruce Warrington, Michael Wouters";

# some parameters
$MAXAGE=14;	# give up after two weeks
$MAXPERRUN=3;	# only catch up this many per run
$MAXATTEMPTS=2;	# have a couple of attempts
$TIMEGAP=60;	# minutes between catch-up process runs

# Check command line
if ($0=~m#^(.*/)#) {$path=$1} else {$path="./"}	# read path info
$0=~s#.*/##;					# then strip it

if (!(getopts('dhv')) || $opt_h){
	&ShowHelp();
	exit;
}

if ($opt_v){
	print "$0 version $VERSION\n";
	print "Written by $AUTHOR\n";
	exit;
}

$home=$ENV{HOME};
if (-d "$home/etc")  {
	$configPath="$home/etc";
}
else 
{	
	ErrorExit("No ~/etc directory found!\n");
} 

if (-d "$home/logs")  {
	$logPath="$home/logs";
} 
else{
	ErrorExit("No ~/logs directory found!\n");
}

$configFile=$configPath."/gpscv.conf";
if (defined $opt_c){
	$configFile=$opt_c;
}


if (!(-e $configFile)){
	ErrorExit("A configuration file was not found!\n");
}

%Init = &TFMakeHash2($configFile,(tolower=>1));

# Check we got the info we need from the config file
@check=("paths:counter data","paths:receiver data","counter:file extension",);
foreach (@check) {
  $tag=$_;
  $tag=~tr/A-Z/a-z/;	
  unless (defined $Init{$tag}) {ErrorExit("No entry for $_ found in $configFile")}
}

$tmpPath=$home."/tmp/";
if (defined($Init{"paths:tmp"})){
	$tmpPath=TFMakeAbsolutePath($Init{"paths:tmp"},$home);
}

$logFile="$logPath/process.log";
$batchFile=$tmpPath."reprocess";

#  find raw data files which do not have a corresponding CCTF output
$gpsDataPath=TFMakeAbsolutePath($Init{"paths:receiver data"},$home);
$gpsExt = ".rx";
if (defined ($Init{"receiver:file extension"})){
	$gpsExt = $Init{"receiver:file extension"};
	if (!($gpsExt =~ /^\./)){
		$gpsExt = ".".$gpsExt;
	}
}

$ticDataPath=TFMakeAbsolutePath($Init{"paths:counter data"},$home);
$ticExt = ".tic";
if (defined ($Init{"counter:file extension"})){
	$ticExt = $Init{"counter:file extension"};
	if (!($ticExt =~ /^\./)){
		$ticExt = ".".$ticExt;
	}
}

$cggttsPath = $home."/cggtts/";
#if (defined $Init{"cggtts"}){ # FIXME this is complicated
#}
$cggttsExt = ".cctf";# FIXME this is complicated

$mjd=int(time()/86400)+40587;

$fileSpec=$ticDataPath."*".$ticExt."*";
@files=glob $fileSpec;
for ($i=0;$i<=$#files;$i++) {
  if ($files[$i]=~/(.*)(\d{5})$ticExt/) {
    $rxData=$gpsDataPath.$2.$gpsExt;
    Debug("Checking $files[$i] $rxData");
    $cggttsData=$cggttsPath.$2.$cggttsExt; # FIXME BIPM naming convention etc ?
    if ((-e $rxData || -e $rxData.".gz") && !(-e $cggttsData) && 
      ($2<$mjd) && ($2>($mjd-$MAXAGE))) 
      {push @MJD,$2}
  }
}
exit unless @MJD;

# look in the process log to see if we already tried to generate the
# missing days; drop any we've already tried to process before
open IN,$logFile or die "Could not open process log $logFile: $!";
while (<IN>) {
  next unless /^(\d{5})/;
  for ($i=0; $i<=$#MJD; $i++) {
    next unless $1==$MJD[$i];
    $count{$1}=0 unless defined $count{$1};
    if (++$count{$1}>=$MAXATTEMPTS) {
      splice @MJD,$i,1;		# drop this MJD, as we already tried it
      last;
    }
  }
  exit unless @MJD;		# that is, if we dropped them all
}
close IN;

@MJD=sort @MJD;
splice @MJD,$MAXPERRUN;		# keep only the first $MAXPERRUN elements
if (open LOG,">>$logFile") {
  @_=gmtime(time());
  printf LOG "# reprocess: scheduling @MJD (%02d/%02d/%02d %02d:%02d)\n",
    $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1];
  close LOG;
}

$gap=5;
foreach $mjd (@MJD) {
  next unless open OUT,">$batchFile$mjd";
 # print OUT "nice $ProcessPath/process $mjd 2>>$LogPath/errorlog\n";
  print OUT "rm -f $batchFile$mjd\n";	# tidy up by deleting batch file
  close OUT;
 # `at -f $batchFile$mjd now +$gap minutes 2>>/dev/null`;
  $gap+=$TIMEGAP;
}

# end of reprocess

#-----------------------------------------------------------------------

sub ShowHelp{
	print "Usage: $0 [OPTION] ...\n";
	print "\t-d debug\n";
  print "\t-h show this help\n";
  print "\t-v print version\n";
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