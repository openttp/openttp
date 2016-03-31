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
#  4. 4.02 RBW  Allow up to MaxAttempts to reprocess
# 
use TFLibrary;


# some parameters
$MaxAge=14;	# give up after two weeks
$MaxPerRun=3;	# only catch up this many per run
$MaxAttempts=2;	# have a couple of attempts
$TimeGap=60;	# minutes between catch-up process runs

$home=$ENV{HOME};
if (-d "$home/logs") {$LogPath="$home/logs";}    else {$LogPath="$home/Log_Files";}
if (-d "$home/etc")  {$ConfigPath="$home/etc";}  else {$ConfigPath="$home/Parameter_Files";}
if (-d "$home/cctf")  {$cctfPath="$home/cctf";}  else {$cctfPath="$home/cctf_data";}
$ProcessPath="$ENV{HOME}/process";

$InitFile="$ConfigPath/cctf.setup";
$LogFile="$LogPath/process.log";
$BatchFile="/tmp/reprocess";

# read setup information from parameter file
%Init=&TFMakeHash($InitFile,(tolower=>1));
# while (($key,$val)=each %Init) {print "$key -> $val\n"}

#  find raw data files which do not have a corresponding CCTF output
$mjd=int(time()/86400)+40587;
$fileSpec=$Init{"data path"}."*".$Init{"counter data extension"}."*";
@files=glob $fileSpec;
foreach (@files) {
  if (/(.*)(\d{5})$Init{"counter data extension"}/) {
    $rxData=$1.$2.$Init{"gps data extension"};
    $cctfData="$cctfPath/$2.cctf";
    if ((-e $rxData || -e $rxData.".gz") && !(-e $cctfData) && 
      ($2<$mjd) && ($2>($mjd-$MaxAge))) 
      {push @MJD,$2}
  }
}
exit unless @MJD;

# look in the process log to see if we already tried to generate the
# missing days; drop any we've already tried to process before
open IN,$LogFile or die "Could not open process log $LogFile: $!";
while (<IN>) {
  next unless /^(\d{5})/;
  for ($i=0; $i<=$#MJD; $i++) {
    next unless $1==$MJD[$i];
    $count{$1}=0 unless defined $count{$1};
    if (++$count{$1}>=$MaxAttempts) {
      splice @MJD,$i,1;		# drop this MJD, as we already tried it
      last;
    }
  }
  exit unless @MJD;		# that is, if we dropped them all
}
close IN;

@MJD=sort @MJD;
splice @MJD,$MaxPerRun;		# keep only the first $MaxPerRun elements
if (open LOG,">>$LogFile") {
  @_=gmtime(time());
  printf LOG "# reprocess: scheduling @MJD (%02d/%02d/%02d %02d:%02d)\n",
    $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1];
  close LOG;
}

$gap=5;
foreach $mjd (@MJD) {
  next unless open OUT,">$BatchFile$mjd";
  print OUT "nice $ProcessPath/process $mjd 2>>$LogPath/errorlog\n";
  print OUT "rm -f $BatchFile$mjd\n";	# tidy up by deleting batch file
  close OUT;
  `at -f $BatchFile$mjd now +$gap minutes 2>>/dev/null`;
  $gap+=$TimeGap;
}

# end of reprocess
