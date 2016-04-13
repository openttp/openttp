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

# Logging script for Opal Kelly XEM6001, via okcounterd
# Derived from get_counter_data_prs10
#
# Modification history
# 2015-06-09 MJW Initial version 0.1
# 2016-03-21 MJW Remove requirement for configuration file
#

use POSIX;
use Errno;
use Getopt::Std;
use IO::Socket;
use TFLibrary;

use vars qw($opt_c $opt_d $opt_h $opt_v);

$VERSION="0.1.0";
$AUTHOR="Michael Wouters";

$alarmTimeout = 120; # SIGAARLM timout 

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

$home=$ENV{HOME};
if (-d "$home/etc")  {
	$configpath="$home/etc";
}
else 
{	
	ErrorExit("No ~/etc directory found!\n");
} 

if (-d "$home/logs")  {
	$logpath="$home/logs";
} 
else{
	ErrorExit("No ~/logs directory found!\n");
}

$configFile=$configpath."/gpscv.conf";
if (defined $opt_c){
	$configFile=$opt_c;
}

if (!(-e $configFile)){
	ErrorExit("A configuration file was not found!\n");
}

%Init = &TFMakeHash2($configFile,(tolower=>1));

# Check we got the info we need from the config file
@check=("counter:port","counter:okxem channel","paths:counter data","counter:file extension","counter:lock file");
foreach (@check) {
  $tag=$_;
  $tag=~tr/A-Z/a-z/;	
  unless (defined $Init{$tag}) {ErrorExit("No entry for $_ found in $configFile")}
}

# Check the lock file
$lockfile = TFMakeAbsoluteFilePath($Init{"counter:lock file"},$home,$logpath);

if (-e $lockfile){
	open(IN,"<$lockfile");
	@info = split ' ',<IN>;
	close IN;
	Debug("$info[0] $info[1]");
	$running = kill 0,$info[1];
	if ($running){
		ErrorExit("already running ($info[1])");
	}
	else{
		open(OUT,">$lockfile");
		print OUT "$0 $$\n"; 
		close OUT;
	}
}
else{
	open(OUT,">$lockfile");
	print OUT "$0 $$\n"; 
	close OUT;
}

$port = $Init{"counter:port"};
$chan = $Init{"counter:okxem channel"};

$headerGen="";
if (defined $Init{"counter:header generator"}){
	$headerGen = TFMakeAbsoluteFilePath($Init{"counter:header generator"},$home,"$home/bin");
}

$sock=new IO::Socket::INET(PeerAddr=>'localhost',PeerPort=>$port,Proto=>'tcp',);
unless ($sock) {ErrorExit("Could not create a socket at $port - okcounterd not running?");} 

# set up a timeout in case we get stuck 
$SIG{ALRM} = sub {close $sock; unlink $lockfile; ErrorExit("Timed out - exiting.")};
alarm $alarmTimeout;

# catch kill signal so we can log that we were killed
$SIG{TERM} = sub {close $sock;unlink $lockfile; ErrorExit("Received SIGTERM - exiting.")};

$sock->send("LISTEN"); # tell okcounterd we are just listening politely

$oldmjd=0;
$ctrext = $Init{"counter:file extension"};
if (!($ctrext =~ /^\./)){ # do we need a period ?
	$ctrext = ".".$ctrext;
}

$dataPath = TFMakeAbsolutePath($Init{"paths:counter data"},$home);

while (1) 
{
	# Get MJD and time
	$tt=time();
	$mjd=int($tt / 86400)+40587;
	($sec,$min,$hour)=gmtime($tt);

	# Check for new day and create new filename if necessary
	if ($mjd!=$oldmjd) {
		$oldmjd=$mjd;
		$file_out=$dataPath . $mjd . $ctrext;
		if (-x $headerGen){
			if (open OUT,">>$file_out"){
				@header = split /\n/,`$headerGen`;
				for ($i=0;$i<=$#header;$i++){
					print OUT "# $header[0]\n";
				}
				close OUT;
			}
		}
	}

	$msg = <$sock>;
	if (!(defined $msg)){ # connection may have closed in an unorderly way
		if ($!{EINTR}){ # interrupted system call is recoverable
			next;
		}
		else{
			close $sock;
			unlink $lockfile;
			ErrorExit("Remote connection closed - exiting.");
		}
	}
	alarm $alarmTimeout; # reset timeout since we have some data
	
	Debug($msg);
	@dfields = split ' ',$msg;
	if ($#dfields == 3){
		if ($chan == $dfields[0]){
			$time=sprintf("%02d:%02d:%02d",$hour,$min,$sec);
			$data = $dfields[3];
			if (open OUT,">>$file_out")
			{
				if ($data > 5.0E8) {$data-=1.0E9;}
				print OUT $time,"  ",$data*1.0E-9,"\n";
				close OUT;
			}
		}
	}
}

close $sock;
unlink $lockfile;

print "Unexpected termination\n";

# End of main program
#-----------------------------------------------------------------------

sub ShowHelp{
	print "Usage: $0 [OPTION] ...\n";
	print "\t-c <config> specify alternate configuration file\n";
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
	if ($opt_d)
	{
		($sec,$min,$hour)=gmtime(time());
		printf STDERR "%02d:%02d:%02d $_[0]\n",$hour,$min,$sec;
	}
}


