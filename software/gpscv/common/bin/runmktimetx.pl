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

# 2016-05-02 MJW First version. A minimal script
#

use POSIX;
use Getopt::Std;
use TFLibrary;

use vars qw($opt_d $opt_h $opt_v $opt_x);

$VERSION="0.1.0";
$AUTHORS="Michael Wouters";

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

if (!(getopts('dhvx')) || $opt_h){
	&ShowHelp();
	exit;
}

if ($opt_v){
	print "$0 version $VERSION\n";
	print "Written by $AUTHORS\n";
	exit;
}

$mjdPrev =  int(time()/86400 + 40587-1); # yesterday
$mjdStart = $mjdStop= $mjdPrev;

if ($#ARGV == 1){
	$mjdStart = $ARGV[0];
	$mjdStop  = $ARGV[1];
}
elsif ($#ARGV != -1){
	ShowHelp();
	exit;
}

if (!$opt_x){
	for ($mjd=$mjdStart;$mjd <= $mjdStop;$mjd++){
		Debug("Processing $mjd");
		`$binPath/mktimetx -m $mjd`;
	}
}
else{
	%Init = &TFMakeHash2($configFile,(tolower=>1));
	# Check we got the info we need from the config file
	@check=("cggtts:create","cggtts:outputs","cggtts:naming convention",
		"counter:file extension","paths:counter data",
		"receiver:file extension","paths:receiver data"
		);
	foreach (@check) {
		$tag=$_;
		$tag=~tr/A-Z/a-z/;	
		unless (defined $Init{$tag}) {ErrorExit("No entry for $_ found in $configFile")}
	}
	
	$ctrExt = $Init{"counter:file extension"};
	if (!($ctrExt =~ /^\./)){ # do we need a period ?
		$ctrExt = ".".$ctrExt;
	}
	$ctrDataPath = TFMakeAbsolutePath($Init{"paths:counter data"},$home);

	
	if (!($Init{"receiver:file extension"} =~ /^\./)){ # do we need a period ?
		$rxExt = ".".$Init{"receiver:file extension"};
	}
	$rxDataPath = TFMakeAbsolutePath($Init{"paths:receiver data"},$home);
	
	if (lc($Init{"cggtts:create"}) eq 'yes'){
		# Just search within the last 7 days for missed processing

		for ($m=$mjdPrev-7;$m<=$mjdPrev;$m++){
			Debug("Checking $m");
			
			# If there is a missing raw data file, then just skip
			$f = $ctrDataPath.$m.$ctrExt;
			if (!(-e $f || -e "$f.gz")){
				Debug("$f is missing");
				next;
			}
			$f = $rxDataPath.$m.$rxExt;
			if (!(-e $f || -e "$f.gz")){
				Debug("$f is missing");
				next;
			}
			
			# Look for missing CGGTTS files
			@outputs=split /,/,$Init{"cggtts:outputs"};
			foreach (@outputs)
			{
				$output = lc $_;
				$output=~ s/^\s+//;
				$output=~ s/\s+$//;
				
				Debug("\tchecking CGGTTS output $output");
				$cggttsPath = TFMakeAbsolutePath($Init{"$output:path"},$home);
				if (lc($Init{"cggtts:naming convention"}) eq "plain"){
					$cggttsFile = $cggttsPath.$m.".cctf";
				}
				elsif (lc($Init{"cggtts:naming convention"}) eq "bipm"){
					
					$c=lc $Init{"$output:constellation"};
					if ( $c eq 'gps'){
						$constellation='G';
					}
					elsif ($c eq 'glonass'){
						$constellation='R';
					}
					$ext = sprintf "%2d.%03d",int ($m/1000),$m - (int $m/1000)*1000;
					$cggttsFile = $cggttsPath.$constellation."M".$Init{"cggtts:lab id"}.$Init{"cggtts:receiver id"}.$ext;
				}
				Debug("\tchecking $cggttsFile");
				if (!(-e $cggttsFile)){ # only attempt to regenerate missing files 
					Debug("\t-->missing");
				}
				else{
					Debug("\t-->exists");
				}
			}
			
			# TODO look for missing RINEX files
		}
	} 
}


# End of main program

#-----------------------------------------------------------------------
sub ShowHelp
{
	print "Usage: $0 [OPTION] ... [startMJD] [stopMJD]\n";
	print "\t-d debug\n";
  print "\t-h show this help\n";
	print "\t-x run missed processing\n";
  print "\t-v print version\n";
	print "\nIf an MJD range is not specified, the previous day is processed\n";
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