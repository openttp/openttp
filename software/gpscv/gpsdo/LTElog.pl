#!/usr/bin/perl
# LTElog.pl
use warnings;
use strict;

# LTElog.pl
# based on gpsdolog.pl (written for Jackson Labs ULN1100 GPSDO)
#
# Version 1.0 start date: 2016-01-13 by Louis Marais
# Version 1.0 finalised : 2016-06-08
#
# Last modification date: 2016-06-08 by Louis Marais
#

# Load required libraries

# Need the POSIX library so that all the serial port flags are defined
# They are used in the TFSerialConnect call
use POSIX;
# Contains the serial connect subroutine
use TFLibrary;
# Declare $tmask globally, as well as command line options
# opt_d turns debug on, opt_h displays help
use vars qw($tmask $opt_d $opt_c $opt_h $opt_v);
# To enable branching on message IDs
use Switch;
# To enable use of 'getopts' (get hold of switches added to the script call)
use Getopt::Std;
# library to decode gpsdo nmea messages
use LTELite::DecodeNMEA;

# Variable declarations, required because of 'use strict'

# Need to go through all of these to check that they are still being used. This 
# list is a holdover from the original gpsdo script for the GPSDO used in the 
# TTSdev version 3 system.

my ($AUTHORS,$VERSION,$DEBUG,$home,$configFile,$logPath,$lockFile,$gpsdoTimeout);
my ($killed,$nfound,$rxmask,$rx,$input,$save,$msg,$nmea,$now,$mjd,@info,$gpsMode);
my ($next,$gpsdostatus,$then,$lastMsg,$nowstr,$filteredUTC,$rawUTC);
my ($noOfCapturedPulses,$lockStatus,$EFCvoltage,$EFCpercentage,$estimatedFreqAcc);
my ($secsInHoldover,$nmbrsats,$health,%Init);

$AUTHORS = "Louis Marais";
$VERSION = "1.0";

# Default debug state is OFF
$DEBUG = 0;

$0=~s#.*/##;

$home=$ENV{HOME};

# Default configuration file
$configFile = "$home/etc/gpscv.conf";

if (!(getopts('c:dchv')) || ($#ARGV >= 1) || $opt_h)
{
  ShowHelp();
  exit;
}

if ($opt_d) { $DEBUG = 1; }

if ($opt_v)
{
  print "$0 version $VERSION\n";
  print "Written by $AUTHORS\n";
}

if (!(-d "$home/etc"))
{
  ErrorExit("No ~/etc directory found!\n");
}

if (-d "$home/logs")
{
  $logPath = "$home/logs";
}
else
{
  ErrorExit("No ~/logs directory found!\n");
}

if (defined $opt_c)
{
  $configFile = $opt_c;
}

if (!(-e $configFile))
{
  ErrorExit("A configuration file was not found!\n");
}

Debug("Script name: $0");
Debug("Authors: $AUTHORS");
Debug("Version: $VERSION");
Debug("Home directory: $home");
Debug("Log path: $logPath");
Debug("Configuration file: $configFile");

Initialise($configFile);

# Check for an existing lock file
$lockFile = TFMakeAbsoluteFilePath($Init{"gpsdo:lock file"},$home,$logPath);
Debug("\$lockFile: $lockFile");

# Check if the process is already running. Note that this 
# assumes the user is conscientious about keeping the lock
# file up to date... You have to write the process id to 
# the file when you start it, and delete the file when you
# kill the process.
if (-e $lockFile)
{
  open(LCK,"<$lockFile");
  @info = split ' ', <LCK>;
  close LCK;
  if (-e "/proc/$info[1]")
  {
    printf STDERR "Process $info[1] already running\n";
    exit;
  }
  else
  {
    open(LCK,">$lockFile");
    print LCK "$0 $$\n";
    close LCK;
  }
}
else
{
  open(LCK,">$lockFile");
  print LCK "$0 $$\n";
  close LCK;
}

$Init{version} = $VERSION;
$gpsdostatus = $Init{"gpsdo:status file"};
$gpsdostatus = TFMakeAbsoluteFilePath($gpsdostatus,$home,$logPath);

$Init{"paths:gpsdo data"} = TFMakeAbsolutePath($Init{"paths:gpsdo data"},$home);

Debug($Init{"paths:gpsdo data"});

if (!($Init{"gpsdo:file extension"} =~ /^\./)) # do we need a period ?
{
    $Init{"gpsdo:file extension"} = ".".$Init{"gpsdo:file extension"};
}

$now=time();
$mjd=int($now/86400) + 40587;
OpenDataFile($mjd,1);
$next=($mjd-40587+1)*86400;	# seconds at next MJD
$then=0;

# Set initial state of script to running fine.
$killed = 0;
# This intercepts the termination signal, and in this case sets
# the variable $killed = 1. This $killed variable controls the main loop.
# Very clever!
$SIG{TERM} = sub {$killed=1}; 
# Also capture Ctrl-C to gracefully exit during testing...
$SIG{INT} = sub {$killed=1};

# Configuration of GPSDO: No software configuration options are available for 
# the LTE-Lite, so nothing to do here. In fact, it does not accept any 
# communications, it just sends stuff out to the user. So we'll just listen.

$input = "";
$save = "";

$gpsdoTimeout = 600;
if (defined($Init{"gpsdo:timeout"})) {$gpsdoTimeout=$Init{"gpsdo:timeout"};}
$lastMsg = time(); 

# GPS mode (PVT, Survey ot Position Hold) is sent in the $PSTI message.
# We create the gpsMode variable to hold the mode
$gpsMode = "Unknown";

# Main loop - read data, handle messages and store required information

while (!$killed)
{
  # see if there is text waiting (every 100 ms)
  $nfound=select $tmask=$rxmask,undef,undef,0.1; # CPU load was a bit high
                                                 # so I changed this to 0.2
                                                 # but it made no difference. 
  next unless $nfound;
  # to prevent sysread attempting to read a negative length:
  if ($nfound < 0) { $killed = 1; last; }
  # Read until we have a complete message
  sysread $rx,$input,$nfound,length($input);    
  ($input,$save,$msg,$nmea) = HandleInput($input,$save);
  if ($msg ne "") 
  { 
    $now = time(); # got one - tag the time
    $mjd = int($now/86400) + 40587;
    saveData($mjd,$msg); 
    if ($now>=$next)
    {
      # (this way is safer than just incrementing $mjd)
      $mjd=int($now/86400) + 40587;	# don't call &TFMJD(), for speed
      OpenDataFile($mjd,0); # New file
      $next=($mjd-40587+1)*86400;	# seconds at next MJD
    }
    if ($now>$then)
    {
      # update string version of time stamp
      @_=gmtime $now;
      $nowstr=sprintf "%02d:%02d:%02d",$_[2],$_[1],$_[0];
      $then=$now;
    }
    # Decode the GPS status message, if available
    if ($msg =~ m/\$PSTI/)
    {
      $gpsMode = decodeMsgPSTI($msg);
    }
    # Decode the GPSDO status message
    if ($msg =~ m/\$PJLTS/)
    {
      ($filteredUTC,$rawUTC,$noOfCapturedPulses,$lockStatus,$EFCvoltage,$EFCpercentage,
      $estimatedFreqAcc,$secsInHoldover,$nmbrsats,$health) = decodeMsgPJLTS($msg);
      # update status file 
      writeStatus($filteredUTC,$rawUTC,$noOfCapturedPulses,$lockStatus,$EFCvoltage,$EFCpercentage,
      $estimatedFreqAcc,$secsInHoldover,$nmbrsats,$health,$gpsMode);
      # Can we see satellites? If we can reset the timeout counter
      if ($nmbrsats > 0) { $lastMsg = time(); }
    }
  }
  # Add timeout stuff
  if (time()-$lastMsg > $gpsdoTimeout)
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

@_=gmtime();
$msg=sprintf ("%04d-%02d-%02d %02d:%02d:%02d $0 killed\n",
              $_[5]+1900,$_[4]+1,$_[3],$_[2],$_[1],$_[0]);
printf $msg;
printf OUT "# ".$msg;
close OUT;

exit;

# End of main

#----------------------------------------------------------------------------
# Subroutines
#----------------------------------------------------------------------------

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

sub ErrorExit {
  my $message=shift;
  @_=gmtime(time());
  printf "%02d/%02d/%02d %02d:%02d:%02d $message\n",
    $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
  exit;
}

#-----------------------------------------------------------------------------

sub Initialise 
{
  my $name = shift; # The name of the configuration file.

  my @required=( "paths:gpsdo data","gpsdo:file extension","gpsdo:port","gpsdo:status file",
    "gpsdo:lock file");
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

  # open the serial port to the receiver
  $rxmask = "";
  my($port) = $Init{"gpsdo:port"};

  unless (`/usr/local/bin/lockport $port $0`==1) 
  {
    printf "! Could not obtain lock on $port. Exiting.\n";
    exit;
  }

  $port="/dev/$port" unless $port=~m#/#;

  # Open port to GPSDO at 38400 baud, 8 data bits, 1 stop bit,
  # no parity, no flow control (see GPSDO user guide, p 25)
  # To decipher the flags, look in the termios documentation

  $rx = &TFConnectSerial($port,
        (ispeed=>B38400,ospeed=>B38400,iflag=>IGNBRK,
         oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|CLOCAL));

  # set up a mask which specifies this port for select polling later on
  vec($rxmask,fileno $rx,1)=1;
  print "> Port $port to GPSDO (Jackson Labs LTE-Lite) is open\n";
  # Wait a bit
  select (undef,undef,undef,0.1);
} # Initialise

#----------------------------------------------------------------------------

sub OpenDataFile
{
  # The first variable passed is the current MJD
  my $mjd=$_[0];

  # Fixup path and extension if needed

  my $ext=$Init{"gpsdo:file extension"};
  
  my $name=$Init{"paths:gpsdo data"}.$mjd.$ext;
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

sub saveData # mjd, data
{
  my($mjd,$data) = (shift,shift);
  if($DEBUG) {
    print "saveData subroutine\n";
    print "\$mjd: ",$mjd,"\n";
    print "\$data: ",$data;
  }
  my($name) = $Init{"paths:gpsdo data"}.$mjd.$Init{"gpsdo:file extension"};
  open DTA,">>$name" or die "Could not write to $name\n"; 
  my($now) = strftime "%H:%M:%S",gmtime;
  print DTA $now," ",$data;
  close DTA;
} # saveData

#----------------------------------------------------------------------------

sub writeStatus
{
  my($filteredUTC,$rawUTC,$noOfCapturedPulses,$lockStatus,$EFCvoltage,$EFCpercentage,
     $estimatedFreqAcc,$secsInHoldover,$nmbrsats,$health,$gpsMode) = (@_);
  # Write current GPSDO status to the file specified in $Input{receiver status}
  open  STA, ">$gpsdostatus";
  print STA "Filtered UTC offset (ns)      : $filteredUTC\n";
  print STA "Raw UTC offset (ns)           : $rawUTC\n";
  print STA "Number of captured 1PPS pulses: $noOfCapturedPulses\n";
  print STA "Lock status                   : $lockStatus - ",decodeLockStatus($lockStatus),"\n";
  print STA "EFC voltage (V)               : $EFCvoltage\n";
  print STA "EFC percentage (%)            : $EFCpercentage\n";
  print STA "Estimated frequency accuracy  : $estimatedFreqAcc\n";
  print STA "Seconds in holdover           : $secsInHoldover\n";
  print STA "Number of satellites tracked  : $nmbrsats\n";
  print STA "GPSDO health                  : $health - ",decodeHealth($health),"\n";
  # It seems the $PSTI message never arrives, so showing the mode makes no sense.
  #print STA "GPSDO mode                    : $gpsMode\n";
  close STA;
} # writeStatus

#----------------------------------------------------------------------------

sub Debug
{
  my($now) = strftime "%H:%M:%S",gmtime;
  if ($DEBUG) {print "$now $_[0] \n";}
} # Debug

#----------------------------------------------------------------------------

# The code below checks for common errors in the received data stream for NMEA
# data and handles those errors correctly.
sub HandleInput
{
  ($input,$save) = (shift,shift);
  my ($data,$cs,$pl,$ccs);
  if ($input =~ /\x0D\x0A/) 
  {
    Debug("Message terminator matched.");
    # Construct message string
    $data = $save.$`.$&;
    Debug("\$data: $data");
    # $` is PREMATCH string, $& is MATCHED string
    # Possible NMEA data, test for start of "$" 
    if ((substr $data,0,1) eq "\$")
    {
      Debug("Possible NMEA message found."); 
      if (length($data) >= 6) 
      {
        # Extract checksum and compare to computed value
        $cs = ord(pack "H*",substr $data,-4,2);
        Debug(sprintf("Transmitted checksum: %d",$cs));
        $pl = substr $data,1,-5;
        $ccs = ord(chksum($pl));
        Debug(sprintf("Calculated checksum: %d",$ccs));
        if ($cs == $ccs) 
        { 
          Debug("Valid NMEA message found.");    
          # Clear out saved data
          $save = "";
          return ($',$save,$data,1);
        }
        else
        {
            # Checksums do not match
            Debug(sprintf("Checksums do not match - calculated: %d transmitted: %d",$ccs,$cs));
            # because it is NMEA we have to assume erroneous data was transmitted, and throw the
            # message away 
            $save = "";
            return ($',$save,"",1);
        }
      }
    }
    # Throw processed data away, keep post match string.
    $input = $';
    # $' is the POSTMATCH string
  }
  return ($input,$save,"",0);
} # HandleInput

#----------------------------------------------------------------------------

# Check sum subroutine based of stuff from Steve Quigg's NV08C script
sub chksum
{
  my($s1) = @_;
  my (@ch) = split("",$s1);
  my($cks) = 0;
  for(my $i=0;$i < length($s1);$i++)
  {
    $cks ^= ord($ch[$i]);
  }
  return(chr($cks));
} # chksum

#----------------------------------------------------------------------------
