#!/usr/bin/perl -w

#
#
# The MIT License (MIT)
#
# Copyright (c) 2015  R. Bruce Warrington
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

# jnslog.pl - Perl script to configure Javad GPS Rx and download data

# Modification history:
# 25. 4.00	1.0.0	RBW First version
# 15.12.00					RBW Completing some left-out bits
# 19.12.00					RBW First functioning version
# 22. 2.01        	RBW Fixed check for existing jnslog process
# 13. 3.01	1.1.0 	RBW Fixed serial bug (failed to clear all unused flags)
#  6. 6.01					RBW Pass PID to lockport to get this right (can be parent)
#  7. 6.01	1.1.1		RBW Don't require (lat,long,h); use TFMakeHash/tolower
# 16. 1.02					RBW Always reset on startup (unless -r)
# 17. 1.02					PTF Inserted 5 second delay after sending reset command
#  7. 3.02					RBW Query and record Rx ID to each rawdata file
#  8. 1.03	1.2.0		RBW Optional fixed position mode, with XYZ from cctf_header
#  2.12.08	1.3.0		MJW Read receiver setup from a file
#												Delay between commands added (present in other versions of jnslog)
#												Delayed 1 pps synchronization on start and periodic 1 pps synchronization therafter
# 											Restart of receiver on empty SI message
# 2016-01-25 2.0.0	MJW Imported into OpenTTP. Renamed from tclog to jnslog.pl
# 2016-04-27 2.0.1	MJW Testing. Some fixups due to changes elsewhere in OpenTTP
# 2018-01-12 2.0.2  MJW Removde 'fixed position' and replaced with 'positioning mode'
#

# TO DO:
# Enforce checksum checking for incoming messages
# Better message parsing: count hex characters instead of pattern matching
# Fix syntax glitches if message stream running already

use Getopt::Std;
use TFLibrary;
use POSIX;
use vars qw($opt_d $opt_h $opt_r $opt_v);

$VERSION="2.0.2";
$AUTHORS="Bruce Warrington, Michael Wouters";

$home=$ENV{HOME};
$configFile="$home/etc/gpscv.conf";

$0=~s#.*/##;

if( !(getopts('c:dhrv')) || ($#ARGV>=1) || $opt_h) {
  ShowHelp();
  exit;
}

if ($opt_v){
	print "$0 version $VERSION\n";
	print "Written by $AUTHORS\n";
	exit;
}

if (-d "$home/etc")  {
	$configPath="$home/etc";
}
else{	
	ErrorExit("No ~/etc directory found!\n");
} 

if (-d "$home/logs")  {
	$logPath="$home/logs";
} 
else{
	ErrorExit("No ~/logs directory found!\n");
}

if (defined $opt_c){
	$configFile=$opt_c;
}

if (!(-e $configFile)){
	ErrorExit("A configuration file was not found!\n");
}

&Initialise($configFile);

# Check for an existing lock file
$lockFile = TFMakeAbsoluteFilePath($Init{"receiver:lock file"},$home,$logPath);
if (!TFCreateProcessLock($lockFile)){
	ErrorExit("Process is already running\n");
}

$rxStatus = &TFMakeAbsoluteFilePath($Init{"receiver:status file"},$home,$logPath);
$Init{"paths:receiver data"}=TFMakeAbsolutePath($Init{"paths:receiver data"},$home);

if (!($Init{"receiver:file extension"} =~ /^\./)){ # do we need a period ?
	$Init{"receiver:file extension"} = ".".$Init{"receiver:file extension"};
}

$now=time();
$mjd=int($now/86400) + 40587;	# don't call &TFMJD(), for speed
&OpenDataFile($mjd,1);
$next=($mjd-40587+1)*86400;	# seconds at next MJD

# Set up periodic outputs - do this BEFORE ConfigureReceiver()
# because ConfigureReceiver() will enable NE message resending if it is on
# This is a workaround for a rare problem where the Rx decides to stop sending
# these messages
# Format of each record is
# (time of next resend,time between resends, command to send,enabel/disable)
$Timer{"UO"}=[0,3600,"%UO%em,/dev/ser/a,jps/UO:{3600,14400,0,2}",1];
$Timer{"IO"}=[0,3600,"%IO%em,/dev/ser/a,jps/IO:{3600,14400,0,2}",1];
$Timer{"GE"}=[0,3600,"%GE%em,/dev/ser/a,jps/GE:{1,3600,0,2}",1];
$Timer{"NE"}=[0,3600,"%NE%em,/dev/ser/a,jps/NE:{1,3600,0,2}",0]; # off by default
foreach $tag (keys %Timer) {$Timer{$tag}[0]=$now+$Timer{$tag}[1]}

&ConfigureReceiver();
&SendCommands(@rxlog,"RXID",1);

$ppsSync=0; # off by default
if (defined($Init{"receiver:pps synchronization"})) {$ppsSync=lc($Init{"receiver:pps synchronization"}) eq "yes";}
if ($ppsSync) {$nextSync=time()+$Init{"receiver:pps synchronization delay"};}

$receiverTimeout=600;
if (defined($Init{"receiver:timeout"})) {$receiverTimeout=$Init{"receiver:timeout"};}
$lastSIok=time(); # last time we saw satellites in the SI message

$then=0;
$input="";
$killed=0;
$SIG{TERM}=sub {$killed=1};

LOOP: while (!$killed) {
	# See if there is text waiting
	$nfound=select $tmask=$rxmask,undef,undef,0.2;
	next unless $nfound;
	if ($nfound<0) {$killed=1; last}
	# Read what's waiting, and add it on the end of $input
	sysread $rx,$input,$nfound,length $input;
	# Look for a message in what we've accumulated
	if ($input=~/(..)([\dA-F]{3})(.*)/) {
		
		$now=time();			# got one - tag the time
		
		if ($now>=$next) {
			# (this way is safer than just incrementing $mjd)
			$mjd=int($now/86400) + 40587;	
			&OpenDataFile($mjd,0);
			$next=($mjd-40587+1)*86400;	# seconds at next MJD
			&SendCommands(@rxlog,"RXID",1);
		}
		
		$id=$1;
		$expected=hex $2;
		$data=$3;
		$len=length $data;
		
		while ($len<$expected) {
			# look for the remainder of the message content
			$nfound=select $tmask=$rxmask,undef,undef,0.2;
			last unless $nfound;
			if ($nfound<0) {$killed=1; last LOOP}
			$len+=sysread $rx,$data,$nfound,length $data;
		}
		
		if ($len>$expected) {
			$input=substr $data,$expected;
			$data=substr $data,0,$expected;
		}
		else {
			$input="";			# clear for next message
		}
		
		# Check that we got all we expected, and that we want this message
		next unless ($len>=$expected) && ($responses=~/:$id:/);	
		# Skip empty confirmation messages
		next if ($id eq "RE") && ($data=~/%$/);

		# Parse notification messages as comments
		if ($data=~/^%(.+)%(.+)/) {
			print OUT "\@ $1 $2\n";
			next;
		}
    
		# Otherwise we have a data message
		if ($now>$then) {
			# update string version of time stamp
			@_=gmtime $now;
			$nowstr=sprintf "%02d:%02d:%02d",$_[2],$_[1],$_[0];
			$then=$now;
		}
    
		printf OUT "$id $nowstr %s\n",	# write to the output file
		($ascii=~/:$id:/? $data : unpack "H*",$data);
		
		# Output some status information so that other processes can use it
		# without having to parse the entire receiver data stream
		if ($id eq "NP"){
			open(STATUS,">$rxStatus");
			printf STATUS "$id $nowstr %s\n",$data;
			close(STATUS);
		}
		
		if ($id eq "SI"){
			# Just check the length of the SI message - must be more than 2 characters
			if (length($data) > 1) {$lastSIok=time();}
		}
	}
	else {
		# Search for message failed at all positions; to avoid wasting time on
		# repeat, discard all but final four characters (think about it)
		$input=substr $input,-4;
	}
  
	$now = time();
  
	# Do all our checks
	if ($now-$lastSIok > $receiverTimeout){ # no satellites for a while so bye bye
		@_=gmtime();
		$msg=sprintf "%02d/%02d/%02d %02d:%02d:%02d no satellites visible - exiting\n",
		$_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
		printf OUT "# ".$msg;
		close OUT;
		goto BYEBYE;	
	}
  
	if ($ppsSync){ # do we have to whack it with a 1 pps?
		if ($now >= $nextSync){
			@sync=("set,dev/event/a/in,on",    # sync with external 1pps too
				"set,dev/event/a/edge,rise",
				"set,dev/event/a/time,gps",
				"set,dev/event/a/tied,off",
				"set,dev/event/a/lock,on");
			&SendCommands(@sync,"sync",1); 
			$nextSync=int($now/86400)*86400+86405; # resync a bit after day rollover ad infinitum
		}
	}
  
	foreach $tag (keys %Timer){ # send any keepalives
		next if ($now<$Timer{$tag}[0] || !($Timer{$tag}[3]));
		$cmd=$Timer{$tag}[2];
		print $rx "$cmd\n";
		print OUT "# $cmd\n";
		$Timer{$tag}[0]+=$Timer{$tag}[1];
	}
  
}


BYEBYE:

if (-e $lockFile) {unlink $lockFile;}

@_=gmtime();
$msg=sprintf "%02d/%02d/%02d %02d:%02d:%02d $0 killed\n",
  $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
printf $msg;
printf OUT "# ".$msg;
close OUT;

# Before we exit, try to turn off the receiver messages
# First wait until the receiver isn't talking
$count=0;
while ($count<10) {
  $nfound=select $tmask=$rxmask,undef,undef,0.1;	# anything waiting?
  last if $nfound==0;
  sysread $rx,$_,$nfound;				# throw it away
  $count++;
}
foreach (1..3) {print $rx "dm\n"}			# disable messages

# end of main

# -----------------------------------------------------------------------------

sub ShowHelp
{
  print "Usage: $0 [-r] [-d] [-h] [-v] [configuration file]\n";
  print "  -d debug\n";
  print "  -h show this help\n";
  print "  -r suppress reset of receiver on startup\n";
  print "  -v show version\n";
  print "  The default configuration file is $configFile\n";
}

#-----------------------------------------------------------------------------

sub Initialise {
	my $name=shift;
	my @required=("receiver:pps offset", "receiver:elevation mask", "receiver:port","receiver:status file",
		"paths:receiver data", "receiver:file extension","receiver:configuration","receiver:lock file");
	my $err;
  
	%Init=&TFMakeHash2($name,(tolower=>1));
	$Init{file}=$name;

	# Check that all required information is present
	$err=0;
	foreach (@required) {
		unless (defined $Init{$_}) {
			print STDERR "! No value for $_ given in $name\n";
			$err=1;
		}
	}
	exit if $err;

	# For some reason, the constant B115200 is not picked up properly?
	# Following line comes from termbits.ph 
	# eval 'sub B115200 () {0010002;}' unless defined(&B115200);

	# Open the serial port to the receiver
	$port=$Init{"receiver:port"};
	unless (`/usr/local/bin/lockport -p $$ $port $0`==1) {
		printf "! Could not obtain lock on $port. Exiting.\n";
		exit;
	}
	$rx=&TFConnectSerial($port,
		(ispeed=>0010002,ospeed=>0010002,
		iflag=>IGNPAR,oflag=>0,lflag=>0,cflag=>CS8|CLOCAL|CREAD));

	# Set up a mask which specifies this port for select polling later on
	$rxmask="";
	vec($rxmask,fileno $rx,1)=1;

	# Flush receiver and wait a moment just in case
	print $rx "\n\n\n\n";
	sleep 1;
} # Initialise

#-----------------------------------------------------------------------------

sub GetFixedPositionCommands {
	my $cmd=0;
	my %pos=();
	my $msg;

	if ((defined $Init{"receiver:positioning mode"})){
		if ($Init{"receiver:positioning mode"} eq 'fixed'){
			$pos{X}=$Init{"antenna:X"};
			$pos{Y}=$Init{"antenna:Y"};
			$pos{Z}=$Init{"antenna:Z"};
			if ($pos{X} && $pos{Y} && $pos{Z}) {
				$cmd="set,ref/pos/gps/xyz,{W84,$pos{X},$pos{Y},$pos{Z}}";
				$cmd=~s/\+//g;
			}
		}
	}
 
	unless ($cmd) {
		$msg="Fixed XYZ position not set: setting static mode OFF";
		print OUT "# $msg\n";
		@_=gmtime;
		printf "%02d/%02d/%02d %02d:%02d:%02d $msg\n",
			$_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
	}
	
	return $cmd? ($cmd,"set,pos/clk/fixpos,on") : ("set,pos/clk/fixpos,off");
} # GetFixedPositionCommands

#-----------------------------------------------------------------------------

sub OpenDataFile {
	my $mjd=$_[0];
	my $name=$Init{"paths:receiver data"}."/".$mjd.$Init{"receiver:file extension"};
	my $old=(-e $name);

	open OUT,">>$name" or die "Could not write to $name";
	select OUT;
	$|=1;
	printf "# %s $0 (version $VERSION) %s\n",
		&TFTimeStamp(),($_[1]? "beginning" : "continuing");
		printf "# %s file $name\n",
		($old? "Appending to" : "Beginning new");
	printf "\@ MJD=%d\n",$mjd;
	select STDOUT;
} # OpenDataFile

#-----------------------------------------------------------------------------

sub SendCommands {
	my $log=pop;
	my $type=pop;

	printf OUT "# %s Sending $type commands\n",&TFTimeStamp() if $log;
	foreach (@_) {
		print OUT "# $_\n" if $log;
		print $rx "%$type%$_\n";
		select undef,undef,undef,0.01; # bit of a delay
	}
	printf OUT "# %s $type commands completed\n",&TFTimeStamp() if $log;
} # SendCommands

#-----------------------------------------------------------------------------

sub ConfigureReceiver {
	# Reset command sequence
	my @reset=(
		"init,/dev/nvm/a",
		"init,/par/",
		"set,/par/reset,yes"
	);

	# Receiver messages to log once at start
	my @msgs=(
		"MF",			# Message format
		"RD"			# Receiver date 
	);

	# Read the receiver setup file
	my $setup = $configPath."/".$Init{"receiver:configuration"};
	if (!(-e $setup)){
		print STDERR "Can't open $setup\n";
		exit; 
	}
	open (IN,"<$setup");
	my @init=();
	my @data=();
	while ($inp=<IN>){
		next if ($inp=~/^\s*#/); # skip comments
		if ($inp=~/^\s*set,/){    # setup commands
			$inp =~ s/#.*$//; # trim trailing comments
			chomp $inp; # and the newline
			$inp =~ s/^\s*(.*?)\s*$/$1/; # and whitespace
			# substitute configuration values where required
			if ($inp=~/(\$Init\{(.+)})/){
				$val = $Init{$2};
				$inp=~ s/(\$Init\{.+})/$val/;
			}
			push @init,$inp;
		}
		else{ # anything else has gotta be a message
			$inp =~ s/#.*$//; # Trim trailing comments
			chomp $inp; # and the newline
			$inp =~ s/^\s*(.*?)\s*$/$1/; # and whitespace
			push @data,$inp;
			# reconfigure & enable messages we are sending a keep-alive for
			if ($inp =~/^UO/){
				$Timer{"UO"}[3]=1;
				$Timer{"UO"}[2]=~ s/UO:\{.+}/$inp/;
			}
			elsif ($inp =~/^IO/){
				$Timer{"IO"}[3]=1;
				$Timer{"IO"}[2]=~ s/IO:\{.+}/$inp/;
			}
			if ($inp =~/^GE/){
				$Timer{"GE"}[3]=1;
				$Timer{"GE"}[2]=~ s/GE:\{.+}/$inp/;
			}
			if ($inp =~/^NE/){
				$Timer{"NE"}[3]=1;
				$Timer{"NE"}[2]=~ s/NE:\{.+}/$inp/;
			}
		}
	}
	close(IN);

	# commands to print setup info (NB NOT LOCAL: do not insert "my")
	@rxlog=(
		"print,rcv/"
	);

	unless ($opt_r) {
		&SendCommands(@reset,"reset",1);
		sleep 5;
	}

	# Add fixed position messages to the initialisation set if requested;
	# otherwise generates the command to turn off static mode
	unshift @init,&GetFixedPositionCommands();

	# add the message commands to the initialisation set
	$responses=":RE:";		# expected message identifiers
	foreach (@msgs) {
		push @init,"out,/dev/ser/a,jps/".$_;
		$responses=":$_$responses";	# add at beginning
	}
	foreach (@data) {
		push @init,"em,/dev/ser/a,jps/".$_;
		s/:.+//;			# delete any options to get two-char identifier
		s/RT/~~/;			# receiver time message has special identifier
		$responses.="$_:";		# add at end
	}
  
	&SendCommands(@init,"initialise",1);
	sleep 5;

	# Arrange the data message identifiers in approximate order of frequency,
	# in order to make the identifier matching a little faster
	$responses=~s/^:(.*)(RE:)(.*)/:$3$1$2ER:/;

	$ascii=":TR:RE:ER:TM:NP:GS:MF:"	# responses in ASCII
} # ConfigureReceiver

#-----------------------------------------------------------------------------

sub ErrorExit {
  my $message=shift;
  @_=gmtime(time());
  printf "%02d/%02d/%02d %02d:%02d:%02d $message\n",
    $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
  exit;
}
