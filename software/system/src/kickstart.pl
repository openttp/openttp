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
#
my $home=$ENV{HOME};

if (-d "$home/logs") {$logpath="$home/logs";}    else {$logpath="$home/Log_Files";}
if (-d "$home/etc")  {$configpath="$home/etc";}  else {$configpath="$home/Parameter_Files";}
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
$nprocs=0;
while ($line=<IN>){
	chop $line;
	$targets[$nprocs]=[split ' ',$line];
	$nprocs++;
}
close IN;
                                                                                
for ($i=0;$i<$nprocs;$i++){
	$target=$targets[$i][0];
	$its_there=0;
	open (PS, "ps -A |");
	while (<PS>){
		# Look for the target, but keep track of what occurs before it in the line
		next unless /([\w:\/]+)\s+([\w\/]*)$target/;
		# Got some process with target. Now split up the
		# process information --
		@tokens=split;
		for ($j=0;$j<=$#tokens;$j++){
			last if ($tokens[$j] =~/$target/);
		}
		# Found it if the token preceding the target is
		# CPU time or /usr/bin/perl -w
		last if $its_there=($tokens[$j-1]=~/[\d:]+/) ||
			($tokens[$j-2]=~/perl/);
		# Otherwise it's some other executable (pico, vi, less...)
	}
	close PS;
		
	$checkfile=$checkpath . $target. ".check";
	if ($its_there)
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
		$outfile="$logpath/$target.log";
		if (open LOG,">>$outfile") {print LOG $message; close LOG}
		else {print "! Could not open log file $outfile\n",$message}
		`nohup $targets[$i][1] >>$outfile 2>&1 &`;
	}
}

