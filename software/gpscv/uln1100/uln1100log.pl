#!/usr/bin/perl
# uln1100log.pl
use warnings;
use strict;

# uln1100log.pl
# Based on lcxolog.pl
#
# Version 1.0 
# Start date            : 2017-04-28 by Louis Marais
# Last modification date: 2017-05-18 by Louis Marais
#
# Version {next}
# Start date            :
# Last modification date:
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# Modification record:
# ~~~~~~~~~~~~~~~~~~~~
#   Date         Done by        Notes
# ~~~~~~~~~~  ~~~~~~~~~~~~~~~  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# 2017-05-18   Louis Marais     Original version (1.0)
#
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#

# Load required libraries
use POSIX;
use TFLibrary;
use vars qw($tmask $opt_c $opt_d $opt_h $opt_n $opt_r $opt_v);
use Switch;
use Getopt::Std;
use gpsdo::DecodeJacksonLabs;

# define variables - required because of 'use strict;'
my($VERSION,$AUTHORS,$home,$configFile,$lockPath,$lockFile,$statusPath);
my(%Init,$uln1100status,$mjd,$now,$next,$then,$nowstr,$input,$save,$killed);
my($receiverTimeout,$lastMsg,$nfound,$rxmask,$rx,$msg,@required,$rcvrID);
my($ffe,$tie,$health,$efc,$data,$sats,$elvs,$azim,$snrs,$lat,$lon,$alt);
my($boardID);

$AUTHORS = "Louis Marais";
$VERSION = "1.0";

$0 =~ s#.*/##;

$home = $ENV{HOME};
$configFile = "$home/etc/gpsdo.conf";

if( !(getopts('c:dhnv')) || ($#ARGV>=1) || $opt_h) 
{
  ShowHelp();
  exit;
}

if ($opt_v)
{
  print "$0 version $VERSION\n";
  print "Written by $AUTHORS\n";
  exit;
}

# Need to access the ~/etc directory because that is where the configuration file
# is stored
if (!(-d "$home/etc"))  
{
  ErrorExit("No ~/etc directory found!\n");
} 

# We need access to ~/lockStatusCheck directory because this is where the lock file is stored.
if (-d "$home/lockStatusCheck")  
{
  $lockPath = "$home/lockStatusCheck";
  $statusPath = "$home/lockStatusCheck";
} 
else
{
  ErrorExit("No ~/lockStatusCheck directory found!\n");
}

if (defined $opt_c)
{
  $configFile = $opt_c;
}

if (!(-e $configFile))
{
  ErrorExit("A configuration file was not found!\n");
}

Initialise($configFile);

# Check for an existing lock file
$lockFile = TFMakeAbsoluteFilePath($Init{"lock files:uln1100"},$home,$lockPath);
if (!TFCreateProcessLock($lockFile))
{
  ErrorExit("Process (uln1100log.pl) is already running\n");
}

$Init{version} = $VERSION;
$uln1100status = $Init{"status files:uln1100"};
$uln1100status = TFMakeAbsoluteFilePath($uln1100status,$home,$statusPath);

$Init{"paths:uln1100 data"} = TFMakeAbsolutePath($Init{"paths:uln1100 data"},$home);

if (!($Init{"file extensions:uln1100"} =~ /^\./)) # do we need a period ?
{
  $Init{"file extensions:uln1100"} = ".".$Init{"file extensions:uln1100"};
}

$now = time();
$mjd = int($now/86400) + 40587;	
if (!defined $opt_n) { OpenDataFile($mjd,1); }
$next = ($mjd-40587+1)*86400;	# seconds at next MJD
$then = 0;

# Configure the LC-XO for remote control.
ConfigureGpsdo();

$input = "";
$save = "";
$killed = 0;
# This intercepts the termination signal, and in this case sets
# the variable $killed = 1. This variable controls the main loop.
# Very clever!
$SIG{INT}  = sub { $killed = 1; };  # For 'Ctrl-C'
$SIG{TERM} = sub { $killed = 1; };  # For 'kill' commands, also those issued by OS

$receiverTimeout=600;
if (defined($Init{"timeouts:uln1100"})) {$receiverTimeout=$Init{"timeouts:uln1100"};}
$lastMsg=time(); 
$data = "";

# Initialise some variables to allow checking for complete status information
$rcvrID = "";
$ffe = "";
$tie = "";
$health = "";
$efc = "";
$sats = "";

while (!$killed)
{
  # see if there is text waiting (every 100 ms)
  $nfound=select $tmask=$rxmask,undef,undef,0.1; # ... ,0.1;
  next unless $nfound;
  # to prevent sysread attempting to read a negative length:
  if ($nfound < 0) { $killed = 1; last; }
  # Read until we have a complete message
  sysread $rx,$input,$nfound,length($input);    
  if ($input =~ /\x0D\x0A/)
  {
    chomp($input);
    # For debugging
    #print "\$input: $input\n";
    
    # Parse message
    if($input =~ /VX VY VZ/)
    {
      decodeMsgXYZSP($input);
      # Send requests for other required information
      sendCmd("SYNC?");
      sendCmd("DIAG?");
      sendCmd("SYST:STAT?");
    } 
    elsif($input =~ /Jackson Labs/)
    {
      $rcvrID = $input;
      # For debugging
      #print "\$rcvrID: $rcvrID\n";
    }               #1234567890123456789012
    elsif($input =~ /FREQ ERROR ESTIMATE :/)
    {
      $ffe = substr $input,22;
      # for debugging
      #print "\$ffe: $ffe\n";
    }               #123456789012345678901234567
    elsif($input =~ /TIME INTERVAL DIFFERENCE :/)
    {
      $tie = substr $input,27;
      # For debugging
      #print "\$tie: $tie\n";
    }               #1234567890123456 
    elsif($input =~ /HEALTH STATUS :/)
    {
      $health = decodeHealth(substr $input,16);
      # For debugging
      #print "\$health: $health\n";
    }               #12345678901234567890 
    elsif($input =~ /EFControl Relative:/)
    {
      $efc = substr $input,20;
      # For debugging
      #print "\$efc: $efc\n";
    }
    else
    {
      $data = $data.$input."\n";
      if($data =~ /GPSDO Status :/)
      {
        # For debugging
        #print "$data\n";
        #print "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
        ($sats,$elvs,$azim,$snrs,$lat,$lon,$alt) = decodeMsgStatus($data);
        $data = "";
        # Write status to file
        if(($rcvrID ne "") && ($ffe ne "") && ($tie ne "") && ($health ne "") && ($efc ne "") && ($sats ne ""))
        {
          $boardID = "Jackson Labs ULN1100, ".$rcvrID;
          writeStatus($boardID,$sats,$snrs,$elvs,$azim,$ffe,$tie,$lat,$lon,$alt,$health,$efc);
        }
      }
    }
    # Save data if required
    if(!defined $opt_n)
    {
      $now = time(); # got one - tag the time
      $mjd = int($now/86400) + 40587;
      
      if ($now>=$next)
      {
        # (this way is safer than just incrementing $mjd)
        $mjd = int($now/86400) + 40587;	# don't call &TFMJD(), for speed
        OpenDataFile($mjd,0);  # New file
        $next = ($mjd-40587+1)*86400;	# seconds at next MJD
      }
      if ($now > $then)
      {
        # update string version of time stamp
        @_ = gmtime $now;
        $nowstr = sprintf "%02d:%02d:%02d",$_[2],$_[1],$_[0];
        $then = $now;
      }
      printf OUT "$nowstr %s\n",$input;
    }            
    $input = "";    
    $lastMsg = time();
  }
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
sendCmd("GPS:XYZSP 0");
sendCmd("SYST:COMM:SER:PRO ON");
sendCmd("SYST:COMM:SER:ECHO ON");
TFRemoveProcessLock($lockFile);

@_=gmtime();
$msg=sprintf ("%04d-%02d-%02d %02d:%02d:%02d $0 killed\n",
              $_[5]+1900,$_[4]+1,$_[3],$_[2],$_[1],$_[0]);
printf $msg;
if(!defined $opt_n)
{
  printf OUT "# ".$msg;
  close OUT;
}

exit;
# end of main 


#-----------------------------------------------------------------------------
# Sub routines
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------

sub ShowHelp
{
  print "Usage: $0 [OPTIONS] ..\n";
  print "  -c <file> set configuration file\n";
  print "  -d debug\n";
  print "  -h show this help\n";
  print "  -n do not log data\n";
  print "  -v show version\n";
  print "  The default configuration file is $configFile\n";
} # ShowHelp

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

  @required=("file extensions:uln1100","status files:uln1100",
             "ports:uln1100","lock files:uln1100","paths:uln1100 data");
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

  # Open the serial port to the receiver
  $rxmask = "";
  my($port) = $Init{"ports:uln1100"};
  
  unless (`/usr/local/bin/lockport $port $0`==1) 
  {
    printf "! Could not obtain lock on $port. Exiting.\n";
    exit;
  }
  $port="/dev/$port" unless $port=~m#/#;
  
  # Open port to LC-XO at 115200 baud, 8 data bits, 1 stop bit,
  # no parity, no flow control.
  # To decipher the flags, look in the termios documentation

  $rx = &TFConnectSerial($port,
        (ispeed=>0010002,ospeed=>0010002,iflag=>IGNBRK,
         oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|CLOCAL));

  # Note: For baud rates above 38400, use the following:
  # (from: http://computer-programming-forum.com/53-perl/9708c3126c4e2641.htm,
  # last accessed 2017-04-27)
  #
  #  # Linux-specific Baud-Rates
  #  sub B57600  { 0010001 }
  #  sub B115200 { 0010002 }
  #  sub B230400 { 0010003 }
  #  sub B460800 { 0010004 }                  
         
  # Set up a mask which specifies this port for select polling later on
  vec($rxmask,fileno $rx,1)=1;

  print "> Port $port to ULN1100 is open\n";
  # Wait a bit
  sleep(1);
} # Initialise

#----------------------------------------------------------------------------

sub OpenDataFile
{
  # The first variable passed is the current MJD
  my $mjd=$_[0];

  # Fixup path and extension if needed

  my $ext=$Init{"file extensions:uln1100"};
  
  my $name=$Init{"paths:uln1100 data"}.$mjd.$ext;
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

sub ConfigureGpsdo
{
  # Start by sending a CRLF to make sure we have a command prompt
  sendCmd("");
  # Turn command prompt and echo off
  sendCmd("SYST:COMM:SER:PRO OFF");
  sendCmd("SYST:COMM:SER:ECHO OFF");
  # Request gpsdo identity
  sendCmd("*IDN?");
  # Disable regular GPS NMEA messages - we don't use them, and don't need them
  sendCmd("GPS:GPGGA 0"); 
  sendCmd("GPS:GGAST 0");
  sendCmd("GPS:GPRMC 0");
  sendCmd("GPS:GPZDA 0");
  sendCmd("GPS:PASHR 0");
  sendCmd("GPS:GYRO 0");
  # Turn on 3D velocity vector output once a second to be the "tick" we need
  # to regularly request status data from the gpsdo.
  sendCmd("GPS:XYZSP 1");
} # ConfigureGpsdo

#----------------------------------------------------------------------------
# the parameter passed to the routine must be a complete <message> but without
# the checksum and without DLE stuffing.
sub sendCmd
{
  my($cmd) = shift;
  # Add CR LF
  $cmd = $cmd."\x0D\x0A";
  print $rx $cmd;
  # Have a short (0.1 s) break, you deserve it.
  $nfound=select undef,undef,undef,0.1;
} # sendCmd

#----------------------------------------------------------------------------
# Status of GPSDO

# The status files of the various GPSDOs must be the same to make life easier when
# writing the code that examines these files.
#
# Structure of status file (will be refined as I go along):
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# ID: Make, model, serial number(s) # Free field identification string
# Sats: xx,xx,xx,xx,xx,xx, ...      # PRNs of satellites being tracked
# SS: xx,xx,xx,xx,xx,...            # Signal strength for each satellite (-1 == no data)
# Elv: xx,xx,xx,xx,xx...            # Elevation for each satellite (-1 == no data)
# Azim: xxx,xxx,xxx,xxx,...         # Azimuth for each satellite (-1 == no data)
# ffe: xE-xx                        # Fractional frequency error (1 = no data)
# tie: xx.x                         # 1 pps output estimated error in ns (1E9 = no data)
# lat: sxx.xxxxxx                   # Latitude in degrees
# lon: sxx.xxxxxx                   # longitude in degrees
# alt: sxxxxx.xx                    # altitude in metres
# health:                           # freefield health information
# efc: sxx.xx                       # Electronic frequency control signal amplitude (percentage) (200 = no data)

sub writeStatus
{
  my($boardID,$sats,$sigst,$elv,$azim,$ffe,$tie,$lat,$lon,$alt,$health,$efc) = (@_);
  open STA, ">$uln1100status";
  print STA "ID: $boardID\n";
  print STA "Sats: $sats\n";
  print STA "SS: $sigst\n";
  print STA "Elv: $elv\n";
  print STA "Azim: $azim\n";
  print STA "ffe: $ffe\n";
  print STA "tie: $tie\n";
  print STA "lat: $lat\n";
  print STA "lon: $lon\n";
  print STA "alt: $alt\n";
  print STA "Health: $health\n";
  print STA "efc: $efc\n";
  close STA;
} #writeStatus

#----------------------------------------------------------------------------

