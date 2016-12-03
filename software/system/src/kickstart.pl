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

# kickstart.pl
# Restarts processes listed in the file kickstart.conf
# Should be run periodically as a cron job
#
# Shamelessly cut&pasted from check_rx and siblings
# 17-07-2002 MJW first version
# 02-08-2002 MJW pedantic filter run over code
#                processes with /usr/bin/perl -w now recognised
# 30-06-2009 MJW Path fixups
# 23-03-2016 MJW Pipe stdout & stderr from a process to its own file, rather than a common file.
#                Rip the guts out of this. Multiple problems - checking for processes with the same name,
#                different behaviour of ps across O/Ss, long process names, imperfect detection of different startup methods ....
#								 Demand use of a lock and check whether the process is running via this.
# 30-03-2016 MJW New configuration file format. Use the 'standard' .conf format
#

use POSIX;
use Getopt::Std;
use TFLibrary;
use vars qw($opt_c $opt_d $opt_h $opt_v);

$AUTHOR="Michael Wouters";
$VERSION="1.0";

my $home=$ENV{HOME};

# Check command line
if ($0=~m#^(.*/)#) {$path=$1} else {$path="./"}	# read path info
$0=~s#.*/##;					# then strip it

if (!(getopts('c:dhv')) || $opt_h){
	&ShowHelp();
	exit;
}

if ($opt_v){
	print "$0 version $VERSION\n";
	print "Written by $AUTHOR\n";
	exit;
}

$configPath="";
if (-d "$home/logs") {$logPath="$home/logs";} else {die "$logPath missing"}; 
if (-d "$home/etc")  {$configPath="$home/etc";} 

my $logFile=  "$logPath/kickstart.log";

$configFile=$configPath."/kickstart.conf";
if (defined $opt_c){
	$configFile=$opt_c;
}

if (!(-e $configFile)){
	ErrorExit("A configuration file was not found!\n");
}

%Init = &TFMakeHash2($configFile,(tolower=>1));

# Check we got the info we need from the config file
@check=("targets");
foreach (@check) {
  $tag=$_;
  $tag=~tr/A-Z/a-z/;	
  unless (defined $Init{$tag}) {ErrorExit("No entry for $_ found in $configFile")}
}

my @targets=split /,/,$Init{"targets"};
for ($i=0;$i<=$#targets;$i++){ # trim whitespace
	$targets[$i] =~ s/^\s+//;
	$targets[$i] =~ s/\s+$//;
}

for ($i=0;$i<=$#targets;$i++){

	$target=$Init{"$targets[$i]:target"};
	$lockFile=TFMakeAbsoluteFilePath($Init{"$targets[$i]:lock file"},$home,$logPath);
	# since there can be command line arguments we need to extract the first part of the command
	@cmdargs = split /\s+/,$Init{"$targets[$i]:command"};
	if ($#cmdargs>0){
		$cmd=TFMakeAbsoluteFilePath($cmdargs[0],$home,"$home/bin");
		for ($j=1;$j<=$#cmdargs;$j++){ # reassemble the command
			$cmd .= " ".$cmdargs[$j]; 
		}
	}
	else{
		$cmd=TFMakeAbsoluteFilePath($Init{"$targets[$i]:command"},$home,"$home/bin");
	}
	$running=0;
		
	Debug("Testing $target for lock $lockFile");
	if (-e $lockFile){
		Debug("lock file $lockFile found\n");
		open(IN,"<$lockFile");
		@lockInfo = split ' ',<IN>;
		close IN;
		$running = kill 0,$lockInfo[1];
	}	
	
	Debug("Process is ".($running ? "" : "not")." running");
	$checkFile=$logPath . "/"."kickstart.".$target. ".check";
	if ($running)
		{`/bin/touch $checkFile`}
	else{
		@_=gmtime(time);
		$message=sprintf("%02d/%02d/%02d %02d:%02d:%02d $target restarted",
						$_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0]);
		if (-e $checkFile){
			@_=stat($checkFile);
			@_=gmtime($_[9]);
			$message.=sprintf(" (last OK check %02d/%02d/%02d %02d:%02d:%02d)",
								$_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0]);
		}
		$message.="\n";
		if (open LOG,">>$logFile") {print LOG $message; close LOG}
		else {print "! Could not open log file $logFile\n",$message}
		
		$outFile="$logPath/$target.log"; # stdout/ & stderr from target
		if (open LOG,">>$outFile") {print LOG $message; close LOG}
		else {print "! Could not open log file $outFile\n",$message}
		`nohup $cmd >>$outFile 2>&1 &`;
		Debug("Restarted using $cmd\n");
	}
}

#-----------------------------------------------------------------------

sub ShowHelp{
	print "Usage: $0 [OPTION] ...\n";
	print "\t-c <file> specify an alternate configuration file\n";
  print "\t-d turn on debugging\n";
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
