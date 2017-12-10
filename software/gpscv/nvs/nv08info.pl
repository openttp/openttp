#!/usr/bin/perl
# nv08info.pl 
use warnings;
use strict;

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

# Modification history:
# 2017-02-27 MJW First version
# 2017-12-11 MJW Configurable path for UUCP lock files. Version change to 1.0.1
#

use Time::HiRes qw(gettimeofday);
use POSIX;
use TFLibrary;
use vars qw($tmask  $opt_c $opt_d $opt_h $opt_r $opt_v);
use Switch;
use Getopt::Std;
use NV08C::DecodeMsg;

# declare variables - required because of 'use strict'
my($home,$configPath,$logPath,$lockFile,$port,$uucpLockPath,$VERSION,$rx,$rxStatus);
my($now,$mjd,$next,$then,$input,$save,$data,$killed,$tstart,$receiverTimeout);
my($lastMsg,$msg,$rxmask,$nfound,$first,$msgID,$ret,$nowstr,$sats);
my(%Init);
my($AUTHORS,$configFile,@info,@required);

$AUTHORS = "Louis Marais,Michael Wouters";
$VERSION = "1.0.1";

$0=~s#.*/##;

$home=$ENV{HOME};
$configFile="$home/etc/gpscv.conf";

if( !(getopts('c:dhrv')) || ($#ARGV>=1) || $opt_h) {
  ShowHelp();
  exit;
}

if ($opt_v){
  print "$0 version $VERSION\n";
  print "Written by $AUTHORS\n";
  exit;
}

if (!(-d "$home/etc")){
  ErrorExit("No ~/etc directory found!\n");
} 

if (-d "$home/logs"){
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

# Check the lock file
$lockFile = TFMakeAbsoluteFilePath($Init{"receiver:lock file"},$home,$lockPath);
if (!TFCreateProcessLock($lockFile)){
	ErrorExit("Process is already running\n");
}

&InitSerial();

# end of main 

#-----------------------------------------------------------------------------
sub ShowHelp
{
  print "Usage: $0 [OPTIONS] ..\n";
  print "  -c <file> set configuration file\n";
  print "  -d debug\n";
  print "  -h show this help\n";
  print "  -v show version\n";
  print "  The default configuration file is $configFile\n";
}

#-----------------------------------------------------------------------------
sub ErrorExit 
{
  my $message=shift;
  @_=gmtime(time());
  printf "%02d/%02d/%02d %02d:%02d:%02d $message\n",
    $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
  exit;
} # ErrorExit

#-----------------------------------------------------------------------------
sub Debug
{
  if ($opt_d){
    print strftime("%H:%M:%S",gmtime)." $_[0]\n";
  }
} # Debug

#----------------------------------------------------------------------------
sub Initialise 
{
  my $name=shift; # The name of the configuration file.
  
  # Define the parameters that MUST have values in the configuration file
  # note that all these values come in as lowercase, no matter how they are
  # written in the configuration file

  @required=( "paths:receiver data","receiver:file extension","receiver:port","receiver:status file",
    "receiver:pps offset","receiver:lock file");
  %Init=&TFMakeHash2($name,(tolower=>1));
  
  if (!%Init)
  {
    print "Couldn't open $name\n";
    exit;
  }
  
  my $err;
  
  # Check that all required information is present
  $err=0;
  foreach (@required) 
  {
    unless (defined $Init{$_}) 
    {
      print STDERR "! No value for $_ given in $name\n";
      $err=1;
    }
  }
  exit if $err;

 
} # Initialise

#----------------------------------------------------------------------------
sub InitSerial
{
		# Open the serial port to the receiver
	$rxmask = "";
	my($port) = $Init{"receiver:port"};
	$port="/dev/$port" unless $port=~m#/#;
  
	$uucpLockPath="/var/lock";
	if (defined $Init{"paths:uucp lock"}){
		$uucpLockPath = $Init{"paths:uucp lock"};
	}

  unless (`/usr/local/bin/lockport -d $uucpLockPath $port $0`==1) {
    printf "! Could not obtain lock on $port. Exiting.\n";
    exit;
  }
  
  # Open port to NV08C UART B (COM2) at 115200 baud, 8 data bits, 1 stop bit,
  # odd parity, no flow control (see paragraph 2, page 8 of 91 in BINR
  # protocol specification).
  # To decipher the flags, look in the termios documentation

  $rx = &TFConnectSerial($port,
        (ispeed=>0010002,ospeed=>0010002,iflag=>IGNBRK,
         oflag=>0,lflag=>0,cflag=>CS8|CREAD|PARENB|PARODD|HUPCL|CLOCAL));

  # Set up a mask which specifies this port for select polling later on
  vec($rxmask,fileno $rx,1)=1;

  print "> Port $port to NV08C (GPS) Rx is open\n";
  # Wait a bit
  sleep(1);
}

#----------------------------------------------------------------------------

sub sendCmd
{
  my($cmd) = shift;
  $cmd =~ s/\x10/\x10\x10/g;        # 'stuff' (double) DLE in data
  print $rx "\x10".$cmd."\x10\x03";	# DLE at start, DLE/ETX at end
  # Wait 100 ms
  select (undef,undef,undef,0.1);
} # SendCmd

#----------------------------------------------------------------------------

sub stripDLEandETX
{
  my($s) = @_;
  if(length($s) != 0){
    my($len)=length($s); 
    # strip off first character and last 2 characters
    $s = substr $s,1,$len-3; 
  }
  # return the answer
  return $s;
} # stripDLEandETX

#----------------------------------------------------------------------------

