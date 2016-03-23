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
#                

# Format of the configuration file
# TARGET  EXECUTABLE  LOCKFILE

use POSIX;
use Getopt::Std;
use vars qw($opt_d $opt_h $opt_v);

$AUTHOR="Michael Wouters";
$VERSION="0.1";

my $home=$ENV{HOME};

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

if (-d "$home/logs") {$logpath="$home/logs";} else {die "$logpath missing"}; 
if (-d "$home/etc")  {$configpath="$home/etc";} else {die "$configpath missing";};

my $configfile ="$configpath/kickstart.conf";
my $checkpath="$logpath/kickstart.";
my $logfile=  "$logpath/kickstart.log";
my $outfile;

my @targets;
 
if (!(-e $configfile)){
	print STDERR "No configuration file found\n";
	exit;
}

open(IN,"<$configfile");
$ntargets=0;
while ($line=<IN>){
	if ($line=~ /^\s*#/) {next;}
	chomp $line;
	$targets[$ntargets]=[split ' ',$line];
	if ($#{$targets[$ntargets]} != 2){
		print STDERR "Syntax error in $configfile - need 3 entries\n"; 
		print STDERR "Entry is ($line)\n";
		exit;
	}
	$ntargets++;
}
close IN;
                                                                                
for ($i=0;$i<$ntargets;$i++){

	$target=$targets[$i][0];
	$lockfile=$targets[$i][2];
	$running=0;
	# Strip any file extension from the target
	$shorttarget = $target;
	$shorttarget =~ s/\.\w*$//;
		
	Debug("Testing $target for lock $lockfile");
	if (-e $lockfile){
		Debug("Lockfile $lockfile found\n");
		open(IN,"<$lockfile");
		@targetinfo = split ' ',<IN>;
		close IN;
		$running = kill 0,$targetinfo[1];
	}	
	
	Debug("Process is ".($running ? "" : "not")." running");
	$checkfile=$checkpath . $shorttarget. ".check";
	if ($running)
		{`/bin/touch $checkfile`}
	else{
		@_=gmtime(time);
		$message=sprintf("%02d/%02d/%02d %02d:%02d:%02d $target restarted",
						$_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0]);
		if (-e $checkfile){
			@_=stat($checkfile);
			@_=gmtime($_[9]);
			$message.=sprintf(" (last OK check %02d/%02d/%02d %02d:%02d:%02d)",
								$_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0]);
		}
		$message.="\n";
		if (open LOG,">>$logfile") {print LOG $message; close LOG}
		else {print "! Could not open log file $logfile\n",$message}
		
		
		$outfile="$logpath/$shorttarget.log";
		if (open LOG,">>$outfile") {print LOG $message; close LOG}
		else {print "! Could not open log file $outfile\n",$message}
		`nohup $targets[$i][1] >>$outfile 2>&1 &`;
		Debug("Restarted\n");
	}
	
}

#-----------------------------------------------------------------------

sub ShowHelp{
	print "Usage: $0 [OPTION] ...\n";
  print "\t-d turn on debugging\n";
  print "\t-h show this help\n";
  print "\t-v print version\n";
}

# -------------------------------------------------------------------------
sub Debug
{
	if ($opt_d){
		printf STDERR "$_[0]\n";
	}
}
