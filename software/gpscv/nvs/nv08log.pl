#!/usr/bin/perl
# nv08log.pl 
# originally called trystuffagain.pl
use warnings;
use strict;

# nvlog.pl
# based on restlog.pl
#
# Version 1.0 start date: 2015-06-22 by Louis Marais
# Last modification date: 2015-06-25
#
# Versioning held in %Init structure $Init{version} - see approx line 72

# Load required libraries

# For measuring time with better granuality than 1 s
use Time::HiRes qw(gettimeofday);
# Need the POSIX library so that all the serial port flags are defined
# They are used in the TFSerialConnect call
use POSIX;
# Contains the serial connect subroutine
use TFLibrary;
# Declare $tmask and $opt_r globally
use vars qw($tmask $opt_r);
# To enable branching on message IDs
use Switch;
# To enable use of 'getopts' (get hold of switches added to the script call)
use Getopt::Std;
# library to decode NV08C binary messages
use NV08C::DecodeMsg qw(decodeMsg46 decodeMsg51 decodeMsg52 decodeMsg60 decodeMsg61 decodeMsg70 decodeMsg72 decodeMsg73 decodeMsg88 decodeMsgE7 hexStr);

# declare variables - required because of 'use strict'
my($home,$configpath,$logpath,$lockfile,$pid,$DEBUG,$rx,$rcvrstatus);
my($now,$mjd,$next,$then,$input,$save,$killed,$tstart,$receiverTimeout);
my($lastMsg,$msg,$rxmask,$nfound,$first,$msgID,$ret,$nowstr,$sats);
my($glosats,$hdop,$vdop,$tdop,$nv08id,$dts,$saw);
my(%Init);

# Set default debug status
$DEBUG=0;
#$DEBUG=1;

# Find the current user's home directory
$home=$ENV{HOME};

# For debugging
Debug(sprintf("\$home: %s",$home));

# Assign path to configuration files
if (-d "$home/etc")  {$configpath="$home/etc";}  else 
  {$configpath="$home/Parameter_Files";}

# For debugging
Debug(sprintf("\$configpath: %s",$configpath));

if (-d "$home/logs")   # -d checks if directory exists
{
  $logpath="$home/logs";
} else {
  $logpath="$home/Log_Files";
}

# For debugging
Debug(sprintf("\$logpath: %s",$logpath));

# Find the filename of this script
$0=~s#.*/##;

# For debugging
Debug($0);

# Start populating %Init structure
$Init{version}="1.0";
$Init{file}=$configpath."/nv08c.setup";

# Make sure configuration file exists
if (!(-e $Init{file}))
{
  print "Configuration file: ",$Init{file}," does not exist.\n";
  exit;
}

# For debugging
Debug(sprintf("\$Init{version}: %s, \$Init{file}: %s",$Init{version},$Init{file}));

if(!(getopts('r')) || !(@ARGV<=2)) 
{
  select STDERR;
  print "Usage: $0 [-r] [initfile]\n";
	print "  -r reset receiver on startup\n";
  print "  The default initialisation file is $Init{file}\n";
  exit;
}

# Check for an existing lock file
$lockfile = $logpath."/gnss_rx.lock";

# For debugging
Debug(sprintf("\$lockfile: %s",$lockfile));

# Check if the process is already running. Note that this 
# assumes the user is conscientious about keeping the lock
# file up to date... You have to write the process id to 
# the file when you start it, and delete the file when you
# kill the process.
if (-e $lockfile) # -e checks of file exists
{
  open(LCK,"<$lockfile");
  $pid = <LCK>;
  chomp $pid;

  # For debugging
  Debug(sprintf("\$pid: %d",$pid));

  # Does the PID (file) exist?
  if (-e "/proc/$pid")
  {
    printf STDERR "Process $pid already running\n";
    exit;
  }
  close LCK;
}

# Write to the lockfile, telling everyone else we are using the
# gnss receiver.
open(LCK,">$lockfile");
print LCK $$,"\n";
close LCK;

# For debugging:
if (@ARGV==1) { Debug(sprintf("\@ARGV: %s, \$ARGV[0]: %s",@ARGV,$ARGV[0])); }

# This checks if a filename was passed as part of the call that started the script. 
# (The argument would be interpreted as a configuration file). 
# If no argument was passed, the initialisation file defined earlier is assigned to
# to the ARGV array. Either way the filename of the configuration file is passed to
# the Initialise subroutine.
Initialise(@ARGV==1? $ARGV[0] : $Init{file});

# Assign the filename for the receiver status dump (from the configuration file)
$rcvrstatus = "nv08.status";
if (defined($Init{"receiver status"})) {$rcvrstatus=$Init{"receiver status"};}

$now=time();
$mjd=int($now/86400) + 40587;	# don't call &TFMJD(), for speed
OpenDataFile($mjd,1);
$next=($mjd-40587+1)*86400;	# seconds at next MJD
$then=0;

# Configure the NV08C for GLONASS operation only, set elevation mask, etc.
ConfigureReceiver();

$input="";
$save="";
$killed=0;
$SIG{TERM}=sub {$killed=1}; # This intercepts the termination signal, and in this case sets
                            # the variable $killed = 1. This variable controls the main loop.
                            # Very clever!
$tstart = time;
$tdop = 999.9;

$receiverTimeout=600;
if (defined($Init{"receiver timeout"})) {$receiverTimeout=$Init{"receiver timeout"};}
$lastMsg=time(); 

LOOP: while (!$killed)
{
  # see if there is text waiting (every 100 ms)
  $nfound=select $tmask=$rxmask,undef,undef,0.1;
  next unless $nfound;
  # Read until we have a complete message
  #if (length($input) >= 0)  # Kept getting "negative length" messages when process is killed.
  #{
    sysread $rx,$input,$nfound,length($input);    
  #}
  #if($DEBUG) {print hexStr($input);}
  # Check to see if we can find the end of a message
  if ($input=~/(\x10+)\x03/)
  {
    if ((length $1) & 1)
    {
      # ETX preceded by odd number of DLE: got the packet termination
      # Is the first character a DLE? If not we may have stray bytes
      # transmitted...
      # Strip off first character until string is too short or DLE is found
      $first = 0x00;
      while ($first != 0x10) {
        $first = ord(substr $input,0,1);
        # strip off first character if we have not found DLE
        if ($first != 0x10) { $input = substr $input,1; }
        # Check length: if it is too short, force the exit   
        if (length($input) <= 3) { $first = 0x10; }
      }   
      # check length to see if we were forced to exit
      if (length($input) <= 3) { $first = 0x11; }
      if ($first != 0x10)
      {
        printf "! Parse error - bad packet start char 0x%02X\n",$first;
        # Clear out buffer, ready for next message
        $input = "";
      }
      else
      {
        # Save message to file - in binary format with all characters still attached.
        $now = time(); # got one - tag the time
        $mjd = int($now/86400) + 40587;
        saveData($mjd,$input);
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
        $input = stripDLEandETX ($input);
        # Strip out double DLE in message
        $input =~ s/\x10\x10/\x10/g;
        # Check message ID and call appropriate subroutine
        $msgID = substr $input,0,1;
        $msg = substr $input,1;        
        switch ($msgID)
        {
          case "\x46" 
          {
            ($ret,$dts) = decodeMsg46($msg);
          } 
          case "\x51"
          {
            $ret = decodeMsg51($msg);
          }
          case "\x52"
          {
            ($ret,$sats) = decodeMsg52($msg);
            if ($ret ne "") 
            {
            $ret = "\n".
                   "                               Carrier\n".
                   "                    Satellite frequency                      Signal\n".
                   "Satellite    SVN     on-board    slot   Elevation  Azimuth  strength\n".
                   " System     number    number    number  (degrees) (degrees)  (dBHz)\n\n".
                   $ret;
            }  
            # Write current status to file
                      # $nv08id,$sats,$glosats,$hdop,$vdop,$tdop
            writeStatus($nv08id,$sats,$glosats,$hdop,$vdop,$tdop);
            # Check if the receiver is seeing GLONASS satellites, if it is, set the 
            # $lastMsg variable to the current time.
            if ($glosats > 0) { $lastMsg = $now; }
            # For this running without an antenna, I modified it. So now the software will 
            # only exit if NOTHING is received for longer than timeout...
            $lastMsg = $now;
            # For debugging:
            #print $dts." : msg52h - No of GLONASS satellites: ",$glosats,"\n";
            #print $dts." : msg52h - GLONASS satellites: ",$sats,"\n";
          } 
          case "\x60"
          {
            ($ret,$glosats,$hdop,$vdop) = decodeMsg60($msg);
            # For debugging:
            #print $dts." : msg60h - No of GLONASS satellites: ",$glosats,"\n";
          } 
          case "\x61"
          {
            ($ret,$tdop) = decodeMsg61($msg);
            # For debugging:
            #print $dts." : msg61h - TDOP: ",$tdop,"\n";
          } 
          case "\x70"
          {
            $ret = decodeMsg70($msg);
            $nv08id = $ret;
          } 
          case "\x72"
          {
            $ret = decodeMsg72($msg);
            # request message 46h (for date and time)
            sendCmd("\x23");                        
          } 
          case "\x73"
          {
            ($ret,$saw) = decodeMsg73($msg);
          } 
          case "\x88"
          {
            $ret = decodeMsg88($msg);
          } 
          case "\xE7"
          {
            $ret = decodeMsgE7($msg);
          } 
          else
          {
            $ret = sprintf("Unknown message ID: %s",hexStr($msgID));
          }
        }                
        # Clear out buffer, ready for next message
        $input = "";
        # For debugging
        Debug($ret);
      }
    }
  }
  # The $lastMsg variable is updated when msg52h show that there are GLONASS 
  # satellites available.
  if (time()-$lastMsg > $receiverTimeout)
  {
    @_=gmtime();
    $msg=sprintf("%04d-%02d-%02d %02d:%02d:%02d no satellites visible - exiting\n",
                  $_[5]+1900,$_[4]+1,$_[3],$_[2],$_[1],$_[0]);
    printf OUT "# ".$msg;
    goto BYEBYE;	
  }
}

BYEBYE:
if (-e $lockfile) {unlink $lockfile;}

@_=gmtime();
$msg=sprintf ("%04d-%02d-%02d %02d:%02d:%02d $0 killed\n",
              $_[5]+1900,$_[4]+1,$_[3],$_[2],$_[1],$_[0]);
printf $msg;
printf OUT "# ".$msg;
close OUT;

# end of main 

#----------------------------------------------------------------------------

sub Initialise 
{
  my $name=shift; # The name of the configuration file.
  # For debugging
  Debug(sprintf("\$name: %s",$name));

  # Define the parameters that MUST have values in the configuration file
  # note that all these values come in as lowercase, no matter how they are
  # written in the configuration file
  my @required=("nv08 port","data path","glonass data extension","receiver status");
  # Define some variables for later use
  my ($tag,$err);  
  # This reads the initialisation file and finds all the setup stuff
  open IN,$name or die "Could not open initialisation file $name: $!";
  while (<IN>) {
    s/#.*//;            # delete comments
    s/\s+$//;           # and trailing whitespace
    if (/(.+)=(.+)/) {  # got a parameter?
      $tag=$1;
      $tag=~tr/A-Z/a-z/;        
      $Init{$tag}=$2;
      # For debugging
      Debug(sprintf("\$tag: %s, \$Init{\$tag}: %s",$tag,$Init{$tag}));
    }
  }
  # check that all required information is present
  $err=0;
  foreach (@required) {
    unless (defined $Init{$_}) {
      print STDERR "! No value for $_ given in $name\n";
      $err=1;
    }
  }
  exit if $err;

  # open the serial port to the receiver
  $rxmask = "";
  my($port) = $Init{"nv08 port"};
  
  # For debugging
  Debug(sprintf("\$port: %s",$port));

  unless (`/usr/local/bin/lockport $port $0`==1) {
    printf "! Could not obtain lock on $port. Exiting.\n";
    exit;
  }
  $port="/dev/$port" unless $port=~m#/#;

  # For debugging
  Debug(sprintf("\$port: %s",$port));

  # Open port to NV08C UART B (COM2) at 115200 baud, 8 data bits, 1 stop bit,
  # odd parity, no flow control (see paragraph 2, page 8 of 91 in BINR
  # protocol specification).
  # To decipher the flags, look in the termios documentation

  $rx = &TFConnectSerial($port,
        (ispeed=>0010002,ospeed=>0010002,iflag=>IGNBRK,
         oflag=>0,lflag=>0,cflag=>CS8|CREAD|PARENB|PARODD|HUPCL|CLOCAL));

  # set up a mask which specifies this port for select polling later on

  vec($rxmask,fileno $rx,1)=1;

  print "> Port $port to NV08C (GLONASS) Rx is open\n";
  # Wait a bit
  sleep(1);
} # Initialise

#----------------------------------------------------------------------------

sub Debug
{
  my($now) = strftime "%H:%M:%S",gmtime;
  if ($DEBUG) {print "$now $_[0] \n";}
} # Debug

#----------------------------------------------------------------------------

sub OpenDataFile
{
  # The first variable passed is the current MJD
  my($mjd)=$_[0];
  # The filename is built from the path specified in the configuration file,
  # the MJD and the file extension specified in the configuration file.
  # Note that this is the STATUS file for that file, the actual data is written
  # to the file in BINARY, and we cannot mix text and data in the same file.
  my($name)=$Init{"data path"}.$mjd.$Init{"glonass data extension"};
  my($st_name)=$name.".status";
  # Does the file already exist?
  my($old)=(-e $name);
  # open the file
  open OUT,">>$st_name" or die "Could not write to $name";
  # All print statements now point to OUT
  select OUT;
  $|=1;
  # Add comments (headings?) to the file, content depends on value of 
  # 2nd parameter
  printf "# %s $0 (version $Init{version}) %s\n",
    &TFTimeStamp(),($_[1]? "beginning" : "continuing");
  printf "# %s file $name\n",
    ($old? "Appending to" : "Beginning new");
  printf "\@ MJD=%d\n",$mjd;
  # All print statements now point to STDOUT
  select STDOUT;
} # OpenDataFile

#----------------------------------------------------------------------------

# Save binary data to file

sub saveData # mjd, data
{
  my($mjd,$data) = (shift,shift);
  if($DEBUG) {
    print "saveData subroutine\n";
    print "\$mjd: ",$mjd,"\n";
    print "\$data: ",hexStr($data),"\n";
  }
  my($name) = $Init{"data path"}.$mjd.$Init{"glonass data extension"};
  open RAW,">>$name" or die "Could not write to $name\n"; 
  binmode RAW;
  print RAW $data;
  close RAW;
} # saveData

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
  # Turn off messages for COM1 (UART A) - we are not using it so we may as well turn it off.
  sendCmd("\x0B\x01\x00\xC2\x01\x00\x02"); # Turn ON NMEA messages on port 1
#  sendCmd("\x0B\x01\x00\xC2\x01\x00\x01");
  #          |   |   |           |   |  
  #          |   |   +-----+-----+   +--- Protocol type: 0 current protocol, 1 no protocol, 2 NMEA protocol, 3 RTCM protocol, 4 BINR protocol, 5 BINR2 protocol  
  #          |   |         +------------- Baud rate: 4800 to 230400 baud, x00 x01 xC2 x00 is 115200 baud
  #          |   +----------------------- Port number: x00 is current port, x01 is port 1 (UART A), x02 is port 2 (UART B)
  #          +--------------------------- ID: x0B is the Request for / Setting of Port Status control message
  # No response is received for this message
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
  sendCmd("\x0D\x01\x00");
  #          |   |   |   
  #          |   |   +--- Coordinate system: x00 WGS84, x01 PZ-90, etc.
  #          |   +------- Data type: x01 is Selection of Coordinate system
  #          +----------- ID: x0D is the Receiver Operating Parameters control message
  # Response is message 51h
  #
  # Set receiver operating parameters: GLONASS only
  sendCmd("\x0D\x02\x02");
  #          |   |   |  
  #          |   |   +--- Operational Navigation System(s): 0 GNSS, 1 GPS, 2 GLONASS, 3 GALILEO, 10 GNSS & SBAS, 11 GPS & SBAS, 12 GLONASS & SBAS
  #          |   +------- Data type: x02 is Selection of Satellite Navigation system
  #          +----------- ID: x0D is the Receiver Operating Parameters control message
  # Response is message 51h
  #
  # Set receiver operating parameters: Elevation angle, signal to noise and RMS navigation error
  my($elevMask)=10; # degrees
  if (defined($Init{"elevmask"})) {$elevMask=$Init{"elevmask"};}
  my($em) = pack "C",$elevMask;
  sendCmd("\x0D\x03\x0A".$em."\x00\x00");
  #          |   |   |    |     |   | 
  #          |   |   |    |     +-+-+--- minimum RMS error for coordinates, setting it at 0 keeps previous value 
  #          |   |   |    +------------- Minimum SNR x1E is 30 dB-Hz
  #          |   |   +------------------ Elevation angle in degrees: x0A is 10 degrees
  #          |   +---------------------- Data type: x03 is PVT setting
  #          +-------------------------- ID: x0D is the Receiver Operating Parameters control message
  # Response is message 51h
  #
  # Set receiver to survey mode, survey position for 1140 minutes (maximum allowed)
  #
  # Set receiver Operating mode
  sendCmd("\x1D\x00\x02");
  #          |   |   |  
  #          |   |   +--- Operating modes: 0 Standalone mode, 1 Mode with fixed coordinates, 2 Averaging of coordinates
  #          |   +------- Data type: x00 is Operating mode setting
  #          +----------- ID: x1D is the Receiver Operating Modes control message
  # Response is message 73h
  #
  # Set receiver Antenna delay
  # Antenna delay is defined in configuration file (antcabdel) in nanoseconds, receiver expects a value in milliseconds
  # Convert the data type FP64 (as expected by the receiver)
  my($antennaDelay) = 0;
  if(defined($Init{"antcabdel"})) {$antennaDelay = $Init{"antcabdel"};}
  $antennaDelay = $antennaDelay / 1.0E6; # milliseconds, so 675 ns = 0.000675 ms
  my($antDelFP64) = pack "d1",$antennaDelay; #toFP64($antennaDelay);
  sendCmd("\x1D\x01".$antDelFP64);
  #          |   |        |  
  #          |   |        +--- Antenna delay in ms, in FP64 format
  #          |   +------------ Data type: x01 is Antenna cable data entry
  #          +---------------- ID: x1D is the Receiver Operating Modes control message
  # Response is message 73h
  #
  # Set receiver Coordinate averaging time
  # This is defined in configuration file (posinttime) in minutes, convert the data type to INT16U.
  my($coordAver) = 1440; # minutes; 1440 minutes = 1 day: (05A0h)
  if(defined($Init{"posinttime"})) {$coordAver = $Init{"posinttime"};}
  my($cs) = pack "S1", $coordAver;
  sendCmd("\x1D\x06".$cs);
  #          |   |    |   
  #          |   |    +--- Averaging time in minutes
  #          |   +-------- Data type: x06 is Coordinate averaging time setting
  #          +------------ ID: x1D is the Receiver Operating Modes control message
  # Response is message 73h
  #
  # Set receiver Time Zone
  sendCmd("\x23\x00\x00");
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
  if(defined($Init{"ppspulselen"})) {$ppslen = $Init{"ppspulselen"};}
  $ppslen =  $ppslen * 1000; # nanoseconds. 100 000 nanoseconds is 100 microseconds
  my($ps) = pack "I1",$ppslen;
  sendCmd("\xD7\x05\x2E\x00".$ps);
  #          |   |    |   |   |
  #          |   |    |   |   +--- length of PPS pulse, in ns
  #          |   |    |   +------- Internal time scale: 0 off, 1 keep within +/- 1 ms of UTC
  #          |   |    +----------- PPS control byte: 0010 1110b
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
  sendCmd("\x1B");
  #          | 
  #          +--- ID: x1B is the Request for Software Version control message
  # Response is message 70h
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
} # ConfigureReceiver

#----------------------------------------------------------------------------

sub writeStatus # $nv08id,$sats,$glosats,$hdop,$vdop,$tdop
{
  my($nv08id,$sats,$glosats,$hdop,$vdop,$tdop) = (@_);
  # For debugging:
  #print "\$nv08id: ",$nv08id,"\n";
  #print "\$sats: ",$sats,"\n";
  #print "\$glosats: ",$glosats,"\n";
  #print "\$hdop: ",$hdop,"\n";
  #print "\$vdop: ",$vdop,"\n";
  #print "\$tdop: ",$tdop,"\n";
  # Write current nv08 status to the file specified in $Input{receiver status}
  open  STA, ">$rcvrstatus";
  print STA $nv08id,"\n";
  print STA "Number of visible GLONASS sats: ",$glosats,"\n";
  print STA "prns=",$sats,"\n";
  print STA sprintf("Reported precision - HDOP: %0.1f VDOP: %0.1f TDOP: %0.2f\n",$hdop,$vdop,$tdop);
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
} # SendCommand

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

