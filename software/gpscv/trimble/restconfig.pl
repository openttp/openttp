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

#
# Reconfigures the Resolution XXX serial port
# Tested for:
# 	Resolution T
#		Resolution SMTx
#		

use Time::HiRes qw( gettimeofday);
use TFLibrary;
use POSIX;
use Getopt::Std;
use POSIX qw(strftime);
use vars  qw($tmask $opt_r);

$DEBUG=0;
$VERSION="2.0";

$0=~s#.*/##;

$home=$ENV{HOME};
if (-d "$home/etc")  {$configpath="$home/etc";}  else 
	{$configpath="$home/Parameter_Files";} # backwards compatibility

if (-d "$home/logs")  {$logpath="$home/logs";} else 
	{$logpath="$home/Log_Files";} # backwards compatibility

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

# Check for an existing lock file
$lockfile = $logpath."/rx.lock";
if (-e $lockfile){
	open(LCK,"<$lockfile");
	$pid = <LCK>;
	chomp $pid;
	if (-e "/proc/$pid"){
		printf STDERR "Process $pid already running\n";
		exit;
	}
	close LCK;
}

&Initialise($configFile);

print "Configuring the serial port for 115200, no parity\n";
print "Note! Default serial port settings for the Resolution XXX are assumed:\n";
print " 9600 baud, odd parity, 8 data bits, 1 stop bit.\n";

# Open the serial port
$rxmask="";
if ($localMajorVersion == 1){
	$port=$Init{"gps port"};
}
elsif($localMajorVersion == 2){
	$port=$Init{"receiver:port"};
}
$port="/dev/$port" unless $port=~m#/#; # try to fix up the port name

unless (`/usr/local/bin/lockport $port $0`==1) {
	printf "! Could not obtain lock on $port. Exiting.\n";
	exit;
}

$rx=&TFConnectSerial($port,
		(ispeed=>B9600,ospeed=>B9600,iflag=>IGNBRK,
			oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|PARENB|PARODD|CLOCAL));
			
# Set up a mask which specifies this port for select polling later on
vec($rxmask,fileno $rx,1)=1;
  
sleep 3;

print "Configuring ...\n";

&SendCommand("\xBC\x00\x0b\x0b\x03\x00\x00\x00\x02\x02\x00");

sleep 3;

print "Closing the serial port ...\n";
close $rx;
sleep 3;

# Reopen the serial port 
print "Reopening the serial port at new speed\n";

$rxmask="";
$rx=&TFConnectSerial($port,
      (ispeed=>0010002,ospeed=>0010002,iflag=>IGNBRK,
       oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|CLOCAL));

# Set up a mask which specifies this port for select polling later on
vec($rxmask,fileno $rx,1)=1;

print "Writing settings to FLASH\n";
&SendCommand("\x8E\x26");
# This will cause a reset
sleep 3;

print "Done! New port settings are 115200 baud, no parity\n";

# Tidy up time

`/usr/local/bin/lockport -r $port`;

# end of main 

#----------------------------------------------------------------------------
sub Initialise 
{
  my $name=shift;
  my ($err);
  my @required=();
  if ($localMajorVersion==1){
		@required=( "gps port");
		%Init=&TFMakeHash($name,(tolower=>1));
	}
	elsif ($localMajorVersion==2){
		@required=( "receiver:port");
		%Init=&TFMakeHash2($name,(tolower=>1));
	}
 
	$err=0;
	foreach (@required){
		unless (defined $Init{$_}){
			print STDERR "! No value for $_ given in $name\n";
			$err=1;
		}
	}
	exit if $err;
}


#----------------------------------------------------------------------------
sub Debug
{
	$now = strftime "%H:%M:%S",gmtime;
	if ($DEBUG) {print "$now $_[0] \n";}
}

#----------------------------------------------------------------------------
sub SendCommand
{
  my $cmd=shift;
  $cmd=~s/\x10/\x10\x10/g;		# 'stuff' (double) DLE in data
  print $rx "\x10".$cmd."\x10\x03";	# DLE at start, DLE/ETX at end
} # SendCommand
