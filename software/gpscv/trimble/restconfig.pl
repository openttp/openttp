#!/usr/bin/perl -w

#
# The MIT License (MIT)
#
# Copyright (c) 2015  R. Bruce Warrington, Michael J. Wouters
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

#restconfig - Perl script to configure the serial port 


use Time::HiRes qw( gettimeofday);
use TFLibrary;
use POSIX;
use Getopt::Std;
use POSIX qw(strftime);
use vars  qw($tmask $opt_d $opt_h $opt_v);

$RESOLUTION_T=0;
#$RESOLUTION_SMT=1;
$RESOLUTION_SMT_360=2;

$0=~s#.*/##;

$home=$ENV{HOME};
if (-d "$home/etc")  {$configpath="$home/etc";}  else 
	{$configpath="$home/Parameter_Files";} # backwards compatibility

if (-d "$home/logs")  {$logpath="$home/logs";} else 
	{$logpath="$home/Log_Files";}
	
# More backwards compatibility fixups
if (-e "$configpath/cggtts.conf"){
	$configFile=$configpath."/cggtts.conf";
	$localMajorVersion=2;
}
elsif (-e "$configpath/cctf.setup"){
	$configFile="$configpath/cctf.setup";
	$localMajorVersion=1;
}
else{
	print STDERR "No configuration file found!\n";
	exit;
}

if( !(getopts('dhv')) || ($#ARGV>=1)) {
	select STDERR;
 	ShowHelp();
	exit;
}

if ($opt_v){
	print "$0 version $VERSION\n";
	exit;
}

if ($opt_h){
	ShowHelp();
	exit;
}

# Check for an existing lock file
$lockFile = $logpath."/rx.lock";
if (-e $lockFile){
	open(LCK,"<$lockFile");
	$pid = <LCK>;
	chomp $pid;
	if (-e "/proc/$pid"){
		printf STDERR "Process $pid already running\n";
		exit;
	}
	close LCK;
}

open(LCK,">$lockFile");
print LCK $$,"\n";
close LCK;

&Initialise(@ARGV==1? $ARGV[0] : $configFile);
$Init{version}="2.0";

$rxModel=$RESOLUTION_SMT_360;
if ($localMajorVersion == 2){
	if ($Init{"receiver:model"} eq "Resolution T"){
		$rxModel=$RESOLUTION_T;
	}
#	elsif ($Init{"receiver:model"} eq "Resolution SMT"){
#		$rxmodel=$RESOLUTION_SMT;
#	}
	elsif ($Init{"receiver:model"} eq "Resolution SMT 360"){
		$rxModel=$RESOLUTION_SMT_360;
	}
	else{
		print "Unknown receiver model: ".$Init{"receiver:model"}."\n";
		exit;
	}
}

Debug("Receiver model = " .($rxModel==$RESOLUTION_T?"Resolution T":"Resolution SMT 360"));

# Open the serial port to the receiver
$rxmask="";
$port=&GetConfig($localMajorVersion,"gps port","receiver:port");
$port="/dev/$port" unless $port=~m#/#;
Debug("Opening $port");

unless (`/usr/local/bin/lockport $port $0`==1) {
	print "! Could not obtain lock on $port. Exiting.\n";
	exit;
}

$rx=&TFConnectSerial($port,
		(ispeed=>0010002,ospeed=>0010002,iflag=>IGNBRK,
			oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|PARENB|PARODD|CLOCAL));
# Set up a mask which specifies this port for select polling later on
vec($rxmask,fileno $rx,1)=1;

if ($rxModel==$RESOLUTION_T){
	print "Note! Default serial port settings for the Resolution T are assumed:\n";
	print " 9600 baud, odd parity, 8 data bits, 1 stop bit.\n";
	$rx=&TFConnectSerial($port,
		(ispeed=>B9600,ospeed=>B9600,iflag=>IGNBRK,
			oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|PARENB|PARODD|CLOCAL));
}
elsif ($rxModel==$RESOLUTION_SMT_360){
		print "Note! Default serial port settings for the SMT 360 are assumed:\n";
		print " 115200 baud, odd parity, 8 data bits, 1 stop bit.\n";
		$rx=&TFConnectSerial($port,
		(ispeed=>0010002,ospeed=>0010002,iflag=>IGNBRK,
			oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|PARENB|PARODD|CLOCAL));
}


#Initialise parameters

print "Configuring the serial port for 115200, no parity .. wait a bit\n";

sleep 3;

print "Configuring ...\n";

&SendCommand("\xBC\x00\x0b\x0b\x03\x00\x00\x00\x02\x02\x00");
# port     
# input baud rate 
# output baud rate 
# data bits
# parity
# stop bits
# reserved
# input protocol
# output protocol
# reserved

sleep 3;

print "Closing the serial port ...\n";
close $rx;
sleep 3;

print "Reopening the serial port at new settings\n";

$rx=&TFConnectSerial($port,
      (ispeed=>0010002,ospeed=>0010002,iflag=>IGNBRK,
       oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|CLOCAL));

vec($rxmask,fileno $rx,1)=1;

print "Writing settings to FLASH\n";
&SendCommand("\x8E\x26");
# This will cause a reset
sleep 3;

print "Done! New port settings are 115200 baud, no parity\n";

# end of main 

#----------------------------------------------------------------------------
sub Initialise 
{
	my $name=shift;
	if ($localMajorVersion == 1){
		my @required=("gps port");
		%Init=&TFMakeHash($name,(tolower=>1));
	}
	elsif($localMajorVersion == 2){
		@required=("receiver:port","receiver:status file");
		%Init=&TFMakeHash2($name,(tolower=>1));
	}

	if (!%Init){
		print "Couldn't open $name\n";
		exit;
	}

	my ($tag,$err);
  
	# Check that all required information is present
	$err=0;
	foreach (@required) {
		unless (defined $Init{$_}) {
			print STDERR "! No value for $_ given in $name\n";
			$err=1;
		}
	}
	exit if $err;

}# Initialise

#-----------------------------------------------------------------------------
sub GetConfig()
{
	my($ver,$v1tag,$v2tag)=@_;
	if ($ver == 1){
		return $Init{$v1tag};
	}
	elsif ($ver==2){
		return $Init{$v2tag};
	}
}

#----------------------------------------------------------------------------
sub FixPath()
{
	my $path=$_[0];
	if (!($path=~/^\//)){
		$path =$ENV{HOME}."/".$path;
	}
	return $path;
}

#----------------------------------------------------------------------------
sub ShowHelp
{
	print "Usage: $0 [-h] [-d] [-v] [initfile]\n";
	print "  -d debug\n";
	print "  -h show this help\n";
	print "  -v show version\n";
	print "  The default initialisation file is $configFile\n";
}

#----------------------------------------------------------------------------
sub Debug
{
	$now = strftime "%H:%M:%S",gmtime;
	if ($opt_d) {print "$now $_[0] \n";}
}

#----------------------------------------------------------------------------

sub SendCommand
{
  my $cmd=shift;
  $cmd=~s/\x10/\x10\x10/g;		# 'stuff' (double) DLE in data
  print $rx "\x10".$cmd."\x10\x03";	# DLE at start, DLE/ETX at end
} # SendCommand
