#!/usr/bin/perl
# nv08log.pl 
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

# nv08log.pl
# based on nv08lograw.pl
#
# Version 1.0 start date: 2015-09-02 by Louis Marais
# Version 2.0 start date: approx March 2016 by Michael Wouters
#                         The Trimble SMT360 issue could not be resolved, so Michael
#                         started code to use the NV08 as the GPS timing receiver for
#                         the version 3 system.
# Last modification date: 2017-12-11
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# Modification record:
# ~~~~~~~~~~~~~~~~~~~~
#   Date               Done by         Notes
# ~~~~~~~~~~~~~~~~~  ~~~~~~~~~~~~~~~  ~~~~~~~
# approx March 2016  Michael Wouters  Modified to fit with way other scripts are done -
# to May 2016                         loads of changes. Some routines added.
#                                     Also went away from storing stuff as binary, now
#                                     hex-encoded.
#                                     'use strict' commented out.
# 2016-05-26         Louis Marais     Fixed issue with setting of mask angle in 
#     to                              'ConfigureReceiver' subroutine.
# 2016-06-27                          Set antenna cable delay to zero in 
#                                     'ConfigureReceiver' subroutine. Actual antenna
#                                     cable delay is handled in processing software.
#                                     'use strict' put back...
#                                     Added decoding of messages to generate a status
#                                     message so users can check on performance of 
#                                     receiver in real time.
# 2017-05-10         Louis Marais     Added lockStatusCheck directory, this is a RAM
#                                     disk. This is to make the SD card last longer.
#                                     status and lock files are stored here, because
#                                     they don't need to survive a reboot.
# 2017-08-10         Louis Marais     Using antenna delay command to implement pps
#                                     offset. This feature was missing from the program
# 2017-12-11         Michael Wouters  Configurable path for UUCP lock files
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#

# Load required libraries

use Time::HiRes qw(gettimeofday);
use POSIX;
use Math::Trig;
use TFLibrary;
use vars qw($tmask $opt_c $opt_d $opt_h $opt_r $opt_v);
use Switch;
use Getopt::Std;
use NV08C::DecodeMsg;

# declare variables - required because of 'use strict'
my($home,$configPath,$logPath,$lockFile,$uucpLockPath,$pid,$VERSION,$rx,$rxStatus,$port);
my($now,$mjd,$next,$then,$input,$save,$data,$killed,$tstart,$receiverTimeout);
my($lastMsg,$msg,$rxmask,$nfound,$first,$msgID,$ret,$nowstr,$sats);
my($glosats,$gpssats,$hdop,$vdop,$tdop,$nv08id,$dts,$saw);
my(%Init);
my($AUTHORS,$configFile,@info,@required);
my($lockPath,$statusPath);

$AUTHORS = "Louis Marais,Michael Wouters";
$VERSION = "2.0.2";

$0=~s#.*/##;

$home=$ENV{HOME};
$configFile="$home/etc/gpscv.conf";

if( !(getopts('c:dhrv')) || ($#ARGV>=1) || $opt_h){ 
  ShowHelp();
  exit;
}

if ($opt_v){
  print "$0 version $VERSION\n";
  print "Written by $AUTHORS\n";
  exit;
}

if (!(-d "$home/etc"))  {
  ErrorExit("No ~/etc directory found!\n");
} 

if (-d "$home/logs"){
  $logPath="$home/logs";
} 
else{
  ErrorExit("No ~/logs directory found!\n");
}

if (-d "$home/lockStatusCheck"){
  $lockPath="$home/lockStatusCheck";
  $statusPath="$home/lockStatusCheck";
}
else{
  ErrorExit("No ~/lockStatusCheck directory found!\n");
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

$Init{version}=$VERSION;
$rxStatus=$Init{"receiver:status file"};
$rxStatus=&TFMakeAbsoluteFilePath($rxStatus,$home,$statusPath);

$Init{"paths:receiver data"}=TFMakeAbsolutePath($Init{"paths:receiver data"},$home);

if (!($Init{"receiver:file extension"} =~ /^\./)){ # do we need a period ?
  $Init{"receiver:file extension"} = ".".$Init{"receiver:file extension"};
}

&InitSerial();

$now=time();
$mjd=int($now/86400) + 40587;	
OpenDataFile($mjd,1);
$next=($mjd-40587+1)*86400;	# seconds at next MJD
$then=0;

# Configure the NV08C for GPS operation only, set elevation mask, etc.
ConfigureReceiver();

$input = "";
$save = "";
$killed = 0;
# This intercepts the termination signal, and in this case sets
# the variable $killed = 1. This variable controls the main loop.
# Very clever!
$SIG{INT}  = sub { $killed = 1; };  # For 'Ctrl-C'
$SIG{TERM} = sub { $killed = 1; };  # For 'kill' commands, also those issued by OS
$tstart = time;
$tdop = 999.9;
$gpssats = 0;

$receiverTimeout=600;
if (defined($Init{"receiver timeout"})) {$receiverTimeout=$Init{"receiver timeout"};}
$lastMsg=time(); 

while (!$killed)
{
  # see if there is text waiting (every 100 ms)
  $nfound=select $tmask=$rxmask,undef,undef,0.1; # ... ,0.1;
  next unless $nfound;
  # For debugging:
  #print time(),": \$nfound: ",$nfound,"\n";
  # to prevent sysread attempting to read a negative length:
  if ($nfound < 0) { $killed = 1; last; }
  # Read until we have a complete message
  sysread $rx,$input,$nfound,length($input);    
  #if($DEBUG) {print hexStr($input);}
  #print "\$input: ",hexStr($input),"\n";
  # Check to see if we can find the end of a message
  if ($input=~/(\x10+)\x03/)
  {
    if ((length $1) & 1)
    {
      # ETX preceded by odd number of DLE: got the packet termination
      #$dle = substr $&,0; # <-- this does nothing now, in restlog it strips out first and last character
      $data = $save.$`.$&;  #$dle;
      #print "\$data: ",hexStr($data),"\n";
      # Note on special variables: $& is MATCHED string, $` is PREMATCH string
      # Is the first character a DLE? If not we may have stray bytes
      # transmitted...
      # Strip off first character until string is too short or DLE is found
      if (length($data) > 2)
      {  
        $first = 0x00;
        while ($first != 0x10) 
        {
          $first = ord(substr $data,0,1);
          # strip off first character if we have not found DLE, but
          # only if the string is long enough
          if (length($data) > 1) {if ($first != 0x10) { $data = substr $data,1; }}
          # Check length: if it is too short, force the exit   
          if (length($data) <= 3) { $first = 0x10; }
        }   
      }
      # check length to see if we were forced to exit
      if (length($data) <= 3) { $first = 0x11; }
      if ($first != 0x10)
      {
        printf "! Parse error - bad packet start char 0x%02X\n",$first;
        $input = $';
        # Special variable: $' is POSTMATCH string
      }
      else
      {
        # Save message to file -
        $now = time(); # got one - tag the time
        $mjd = int($now/86400) + 40587;
      
        if ($now>=$next)
        {
          # (this way is safer than just incrementing $mjd)
          $mjd=int($now/86400) + 40587;	# don't call &TFMJD(), for speed
          OpenDataFile($mjd,0); # New file
          # Run ConfigureReceiver without resetting the receiver to get
          # all the relevant messages for the start of the file
          $opt_r = 0;
          ConfigureReceiver();
          $next=($mjd-40587+1)*86400;	# seconds at next MJD
        }
        if ($now>$then)
        {
          # update string version of time stamp
          @_=gmtime $now;
          $nowstr=sprintf "%02d:%02d:%02d",$_[2],$_[1],$_[0];
          $then=$now;
        }
        # Strip off DLE at start and DLE ETX at end
        $data = stripDLEandETX ($data);
        # Strip out double DLE in message
        $data =~ s/\x10\x10/\x10/g;
        # Check message ID and call appropriate subroutine
        $msgID = substr $data,0,1;
        $msg = substr $data,1;      
        $lastMsg=$now;
        printf OUT "%02X $nowstr %s\n",(ord substr $data,0,1),(unpack "H*",$msg);
        switch ($msgID)
        {
          case "\x52"
          {
            ($ret,$sats) = decodeMsg52(unpack "H*",$msg);           
            #writeStatus($nv08id,$sats,$glosats,$hdop,$vdop);
            writeStatus($nv08id,$sats,$glosats,$gpssats,$hdop,$vdop,$tdop);
          }
          case "\x60"
          {
            ($ret,$gpssats,$glosats,$hdop,$vdop) = decodeMsg60(unpack "H*",$msg);
          }
          case "\x61"
          {
            ($ret,$tdop) = decodeMsg61(unpack "H*",$msg);
          }
          case "\x70"
          {
            $nv08id = decodeMsg70(unpack "H*",$msg);
          } 
          case "\x72"
          {
            # request message 46h (for date and time)
            sendCmd("\x23");
            # request message 74h (for time scale parameters)
            sendCmd("\x1E");
          }
        }
        # Clear out buffer, ready for next message
        $save = "";
      }
    }
    else
    {
      # ETX preceded by even number of DLE: DLEs "stuffed", this is packet data
      # Remove from $input for next search, but save it for later     
      $save = $save.$`.$&;
      # Note on special variables: $` is PREMATCH string, $& is MATCHed string
    }
    $input = $';	
    # Note on special variables: $' is POSTMATCH string
  }
  # The $lastMsg variable is updated when msg52h show that there are GPS / GLONASS 
  # satellites available.
  if (time()-$lastMsg > $receiverTimeout)
  {
    @_=gmtime();
    $msg=sprintf("%04d-%02d-%02d %02d:%02d:%02d no satellites visible - exiting\n",
                  $_[5]+1900,$_[4]+1,$_[3],$_[2],$_[1],$_[0]);
    printf OUT "# ".$msg;
    $killed = 1;
  }
}

BYEBYE:
if (-e $lockFile) {unlink $lockFile;}
`/usr/local/bin/lockport -r -d $uucpLockPath $port`;

@_=gmtime();
$msg=sprintf ("%04d-%02d-%02d %02d:%02d:%02d $0 killed\n",
              $_[5]+1900,$_[4]+1,$_[3],$_[2],$_[1],$_[0]);
printf $msg;
printf OUT "# ".$msg;
close OUT;

# end of main 

#-----------------------------------------------------------------------------
sub ShowHelp
{
  print "Usage: $0 [OPTIONS] ..\n";
  print "  -c <file> set configuration file\n";
  print "  -d debug\n";
  print "  -h show this help\n";
  print "  -r reset receiver on startup\n";
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
  
  if (!%Init){
    print "Couldn't open $name\n";
    exit;
  }
  
  my $err;
  
  # Check that all required information is present
  $err=0;
  foreach (@required) {
    unless (defined $Init{$_}) {
      print STDERR "! No value for $_ given in $name\n";
      $err=1;
    }
  }
  exit if $err;

  
} # Initialise

#----------------------------------------------------------------------------
sub InitSerial()
{
	# Open the serial port to the receiver
	$rxmask = "";
	$port = $Init{"receiver:port"};
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

sub OpenDataFile
{
  # The first variable passed is the current MJD
  my $mjd=$_[0];

  # Fixup path and extension if needed

  my $ext=$Init{"receiver:file extension"};
  
  my $name=$Init{"paths:receiver data"}.$mjd.$ext;
  my $old=(-e $name); # already there ? May have restarted logging.

  Debug("Opening $name");

  open OUT,">>$name" or die "Could not write to $name";
  select OUT;
  $|=1;
  printf "# %s $0 (version $Init{version}) %s\n",
    &TFTimeStamp(),($_[1]? "beginning" : "continuing");
  printf "# %s file $name\n",
    ($old? "Appending to" : "Beginning new");
  printf "\@ MJD=%d\n",$mjd;
  select STDOUT;

} # OpenDataFile

#----------------------------------------------------------------------------


# +-----------------------------------------------------------------------------+
# |                                                                             | 
# |  NOTE1: COMMANDS SETTING STUFF UP COMMENTED OUT TO DEBUG 1PPS "drift" ISSUE |
# |                                                                             | 
# |  NOTE2: ANTENNA DELAY CANNED TO -3.5 us TO ALLOW EACH 1PPS TO BE MEASURED   |
# |                                                                             | 
# +-----------------------------------------------------------------------------+


#----------------------------------------------------------------------------

sub ConfigureReceiver
{
  if ($opt_r) # hard reset
  {
    print OUT "# Resetting receiver\n";	
    sendCmd("\x01\x00\x01\x21\x01\x00\x00");
    #          |   |   |   |   |   |   |   
    #          |   |   |   |   |   |   +--- Reboot type: x00 is Cold start, x01 is Warm start
    #          |   |   |   |   |   +------- Constant, always x00
    #          |   |   |   |   +----------- Constant, always x01
    #          |   |   |   +--------------- Constant, always x21
    #          |   |   +------------------- Constant, always x01
    #          |   +----------------------- Constant, always x00
    #          +--------------------------- ID: x01 is the Reboot control message
    # The receiver does not send any response to this message 
    #
    # Wait a second
    sleep(1);
  }
  # Set type of messages for COM1 (UART A) - if we are not using it so we may as well turn it off.
  #                                        - If we are using it as a source of time of day for NTP,
  #                                          we need to set it up to send NMEA messages
  sendCmd("\x0B\x01\x00\xC2\x01\x00\x02"); # Setup for NMEA messages.
  #sendCmd("\x0B\x01\x00\xC2\x01\x00\x01"); # Setup to turn port off.
  #          |   |   |           |   |  
  #          |   |   +-----+-----+   +--- Protocol type: 0 current protocol, 1 no protocol, 2 NMEA protocol, 3 RTCM protocol, 4 BINR protocol, 5 BINR2 protocol  
  #          |   |         +------------- Baud rate: 4800 to 230400 baud, x00 x01 xC2 x00 is 115200 baud
  #          |   +----------------------- Port number: x00 is current port, x01 is port 1 (UART A), x02 is port 2 (UART B)
  #          +--------------------------- ID: x0B is the Request for / Setting of Port Status control message
  # Response is message 50h
  #
  # Cancel all transmission requests (turns off transmisson of all messages)
  sendCmd("\x0E");
  #          | 
  #          +----------- ID: x0E is the Cancellation of all Transmission Requests control message
  # No response message is sent (which makes sense, as we do NOT want any output!)
  #
  # Request receiver Time Zone (returns time, date and time zone information)
  sendCmd("\x23");
  #          | 
  #          +--- ID: x23 is the Request / Set Receiver Time Zone control message
  # Response is message 46h
  #
  # Set receiver operating parameters: Select coordinate system
  #sendCmd("\x0D\x01\x00");
  #          |   |   |   
  #          |   |   +--- Coordinate system: x00 WGS84, x01 PZ-90, etc.
  #          |   +------- Data type: x01 is Selection of Coordinate system
  #          +----------- ID: x0D is the Receiver Operating Parameters control message
  # Response is message 51h
  #
  # Set receiver operating parameters: GPS only tracked
  my $observations = 'gps';
  if (defined($Init{"receiver:observations"})){
		$observations=lc $Init{"receiver:observations"};
  }
  
  if ($observations=~/gps/){
		if ($observations =~ /glonass/){
			if ($observations =~ /sbas/){
				sendCmd("\x0D\x02\x0a"); # GPS+GLONASS+SBAS
			}
			else{
				sendCmd("\x0D\x02\x00"); # GPS+GLONASS
			}
		}
		else{
			if ($observations =~ /sbas/){
				sendCmd("\x0D\x02\x0b"); # GPS+SBAS
			}
			else{
				sendCmd("\x0D\x02\x01"); # GPS
			}
		}
  }
  elsif ($observations =~/glonass/){
		if ($observations =~ /sbas/){
				sendCmd("\x0D\x02\x0c"); # GLONASS+SBAS
		}
		else{
			sendCmd("\x0D\x02\x02"); # GLONASS
		}
  }
  elsif ($observations =~ /galileo/){
		sendCmd("\x0D\x02\x03");
  }
  
  #sendCmd("\x0D\x02\x01");
  #          |   |   |  
  #          |   |   +--- Operational Navigation System(s): 0 GPS+GLONASS, 1 GPS, 2 GLONASS, 3 GALILEO, 10 GPS+GLONASS+SBAS, 11 GPS+SBAS, 12 GLONASS+SBAS
  #          |   +------- Data type: x02 is Selection of Satellite Navigation system
  #          +----------- ID: x0D is the Receiver Operating Parameters control message
  # Response is message 51h
  #
  # Set receiver operating parameters: Elevation angle, signal to noise and RMS navigation error
  my($elevMask)=10; # degrees
  if (defined($Init{"receiver:elevation mask"})) {$elevMask=$Init{"receiver:elevation mask"};}
  my($em) = pack "C",$elevMask;
  #sendCmd("\x0D\x03".$em."\x1E\x00\x00");
  #          |   |    |     |   |   | 
  #          |   |    |     |   +-+-+--- minimum RMS error for coordinates, setting it at 0 keeps previous value 
  #          |   |    |     +----------- Minimum SNR x1E is 30 dB-Hz
  #          |   |    +----------------- Elevation angle in degrees: x0A is 10 degrees
  #          |   +---------------------- Data type: x03 is PVT setting
  #          +-------------------------- ID: x0D is the Receiver Operating Parameters control message
  # Response is message 51h
  #
  # Set receiver to survey mode, survey position for 1140 minutes (maximum allowed) 
  # or as long as set by user in configuration file ("receiver:position survey time" entry)
  #
  # Set receiver Operating mode
  #sendCmd("\x1D\x00\x02");
  #          |   |   |  
  #          |   |   +--- Operating modes: 0 Standalone mode, 1 Mode with fixed coordinates, 2 Averaging of coordinates
  #          |   +------- Data type: x00 is Operating mode setting
  #          +----------- ID: x1D is the Receiver Operating Modes control message
  # Response is message 73h
  
  my $rxmode = 'survey';
  if (defined($Init{'receiver:positioning mode'})){
		$rxmode=lc $Init{'receiver:positioning mode'};
	}
	
	if ($rxmode eq 'dynamic'){ # ie standalone
		sendCmd("\x1D\x00\x00");
	}
	elsif ($rxmode eq 'fixed'){
		my ($lat,$lon,$ht);
		($lat,$lon,$ht)= ecef2lla($Init{'antenna:x'},$Init{'antenna:y'},$Init{'antenna:z'});
		my ($antlat) = pack 'd1',$lat;
		my ($antlon) = pack 'd1',$lon;
		my ($antht) = pack 'd1',$ht;
		sendCmd("\x1D\x00\x01"); # fixed mode
		sendCmd("\x1D\x07".$antlat.$antlon.$antht);
	}
	elsif ($rxmode eq 'survey'){
		sendCmd("\x1D\x00\x02");
	}
	else{
		print STDERR "Unknown receiver positioning mode\n";
	}
 
  
  #
  # Set receiver Antenna delay
  # Antenna delay is set to zero here. The actual antenna cable delay is taken care of in the processing software.
  # Convert the data type to FP64 (as expected by the receiver)
  #my($antDelFP64) = pack "d1",0;


  # OK we need to use this feature to offset the PPS signal...
  # The command accepts a value in miiliseconds, so we need to convert the nanosecond value in
  # the configuration file to milliseconds.
  my($ppsOffset) = pack "d1",-($Init{"receiver:pps offset"})/1.0E6;
  sendCmd("\x1D\x01".$ppsOffset);

  # For testing set antenna delay to -3.5 microseconds so that counter can catch each 1PPS
  # my($antDelFP64) = pack "d1",-0.0035; # milliseconds
  # sendCmd("\x1D\x01".$antDelFP64);
  #          |   |        |  
  #          |   |        +--- Antenna delay in ms, in FP64 format
  #          |   +------------ Data type: x01 is Antenna cable data entry
  #          +---------------- ID: x1D is the Receiver Operating Modes control message
  # Response is message 73h
  #
  # Set receiver Coordinate averaging time
  # This is defined in configuration file (posinttime) in minutes, convert the data type to INT16U.
  my($coordAver) = 1440; # minutes; 1440 minutes = 1 day: (05A0h)
  if(defined($Init{"receiver:position survey time"})) {$coordAver = $Init{"receiver:position survey time"};}
  my($cs) = pack "S1", $coordAver;
  #sendCmd("\x1D\x06".$cs);
  #          |   |    |   
  #          |   |    +--- Averaging time in minutes
  #          |   +-------- Data type: x06 is Coordinate averaging time setting
  #          +------------ ID: x1D is the Receiver Operating Modes control message
  # Response is message 73h
  #
  # Set receiver Time Zone
  #sendCmd("\x23\x00\x00");
  #          |   |    |   
  #          |   |    +--- UTC correction: minutes
  #          |   +-------- UTC correction: hours
  #          +------------ ID: x23 is the Request / Set Receiver Time Zone control message
  # Response is message 46h
  #
  # Set additional receiver parameters
  # The PPS pulse length is defined in configuration file (ppspulselen) in microseconds, receiver expects a value in
  # nanoseconds. Convert the data type to INT32U.
  my($ppslen) = 100; # microseconds
  if(defined($Init{"receiver:pps pulse length"})) {$ppslen = $Init{"receiver:pps pulse length"};}
  $ppslen =  $ppslen * 1000; # nanoseconds. 100 000 nanoseconds is 100 microseconds
  my($ps) = pack "I1",$ppslen;
  #sendCmd("\xD7\x05\x1E\x00".$ps);
  #          |   |    |   |   |
  #          |   |    |   |   +--- length of PPS pulse, in ns
  #          |   |    |   +------- Internal time scale: 0 off, 1 keep within +/- 1 ms of UTC
  #          |   |    +----------- PPS control byte: 0001 1110b
  #          |   |                                   bit 0: 0 Software type (PPS), 1 Hardware type, sync'ed to RX internal time scale
  #          |   |                                   bit 1 (for Software type only): 0 PVT rate, 1 Every second
  #          |   |                                   bit 2: 0 Do not verify PPS, 1 verify PPS and disable if failed
  #          |   |                                   bit 3: 0 Inverted, 1 Direct
  #          |   |                                   bit 4-7: 001 - GPS, 010 - GLONASS, 011 - UTC, 100 - UTC(SU)
  #          |   +---------------- Data type: PPS control
  #          +-------------------- ID: xD7 is the Set Additional Operating Parameters control message
  # Response is E7 message
  #
  # Turn on required messages
  #
  # Request for software version
  sendCmd("\x1B"); # No longer required, request message F4h takes care of this
  #          |     # but leave it in, otherwise $nv08id is undefined when status is 
  #          |     # written for the first time.
  #          | 
  #          +--- ID: x1B is the Request for Software Version control message
  # Response is message 70h
  #
  # Request status of port 2
  sendCmd("\x0B\x02");
  # For message definition, see above
  #
  # Response is message 50h
  #
  # Request for number of satellites used and DOP
  sendCmd("\x21\x3C");
  #          |   |
  #          |   +--- Send message interval: x3C = 60 seconds
  #          +------- ID: x21 is the Request for Number of Satellites and DOP
  # Response is message 60h
  #
  # Request for PVT vector
  sendCmd("\x27\x3C");
  #          |   |
  #          |   +--- Send message interval: x3C = 60 seconds
  #          +------- ID: x27 is the Request for PVT Vector
  # Response is message 88h
  #
  # Request for DOP and RMS for calculated PVT vector
  sendCmd("\x31\x3C");
  #          |   |
  #          |   +--- Send message interval: x3C = 60 seconds
  #          +------- ID: x31 is the Request for DOP and RMS for Calculated PVT
  # Response is message 61h
  #
  # Request for time and frequency parameters
  sendCmd("\x1F\x01");
  #          |   |
  #          |   +--- Send message interval: x01 = 1 second (i.e. every second)
  #          +------- ID: x1F is the Request for Time and Frequency Parameters
  # Response is message 72h
  #
  # Request for visible satellites
  sendCmd("\x24\x3C");
  #          |   |
  #          |   +--- Send message interval: x3C = 60 seconds
  #          +------- ID: x24 is the Request for Visible Satellites control message
  # Response is message 52h
  #
  # Request for raw data output
  sendCmd("\xF4\x0A");
  #          |   |
  #          |   +--- Send message interval: x0A = 10 * 100 milliseconds = 1 second
  #          +------- ID: xF4 is the Request for raw data output
  # Response is messages 70h (single message), 4Ah (every 2 minutes), 
  #                      4Bh (every 2 minutes), F5h (at interval requested in F4h, 
  #                      F6h (every minute), and F7h (at rate of updated ephemeris)
  #
} # ConfigureReceiver

#----------------------------------------------------------------------------

#writeStatus($nv08id,$sats,$glosats,$hdop,$vdop,$tdop);
#           ($nv08id,$sats,$glosats,$gpssats,$hdop,$vdop,$tdop)
sub writeStatus # $nv08id,$sats,$glosats,$hdop,$vdop,$tdop
{
  my($nv08id,$sats,$glosats,$gpssats,$hdop,$vdop,$tdop) = (@_);
  #my($nv08id,$sats,$glosats,$gpssats,$hdop,$vdop) = (@_);
 
  open  STA, ">$rxStatus";
  print STA $nv08id,"\n";
  # Does it still make sense to report GLONASS satellites?
  print STA "Number of visible GLONASS sats: ",$glosats,"\n";
  print STA "Number of visible GPS sats: ",$gpssats,"\n";
  print STA "prns=",$sats,"\n";
  print STA sprintf("Reported precision - HDOP: %0.1f VDOP: %0.1f TDOP: %0.2f\n",$hdop,$vdop,$tdop);
  #print STA sprintf("Reported precision - HDOP: %0.1f VDOP: %0.1f\n",$hdop,$vdop);
  close STA;
} # writeStatus

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
# Convert ECEF to WGS84 (lat,lon,h) in (rad,rad,m)
# This is an approximation only 
# ECEF coordinates are assumed to be 'right' for the particular version of WGS84 in use on the
# receiver. The uncertainty is about 10 cm if they are not 'right'
# Tested using http://www.oc.nps.edu/oc2902w/coord/llhxyz.htm

sub ecef2lla
{
	
	my ($X,$Y,$Z) = ($_[0],$_[1],$_[2]);
	
	my $a = 6378137.00;  #semi-major axis
	my $inverse_flattening = 298.257223563;
	
	my $p=sqrt($X*$X + $Y*$Y);
	my $r=sqrt($p*$p + $Z*$Z);
	my $f=1.0/$inverse_flattening;
	my $esq=2.0*$f-$f*$f;
	my $u=atan2($Z/$p, 1.0/(1.0-$f+$esq*$a/$r));
	
	# Convention used by NV08 is that 
	# latitude: North +ve, South -ve
	# longitude: East +ve, West -ve
	# atan2 does the job
	
	my $longitude = atan2($Y,$X);
	my $latitude = atan2($Z*(1.0-$f) + $esq * $a * sin($u)**3 ,
					(1.0-$f)*($p  - $esq * $a * cos($u)**3 ) );
	my $height = $p*cos($latitude) + $Z*sin($latitude) - 
									$a*sqrt(1.0-$esq* sin($latitude)**2 );
	
	return ($latitude,$longitude,$height);
}
