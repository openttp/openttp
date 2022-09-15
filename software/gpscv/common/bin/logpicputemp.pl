#!/usr/bin/perl
# logpicputemp.pl
use warnings;
use strict;

# Logs Pi CPU temperature - note this is a quick hack based on
# the OpenTTP log1Wtemp.pl script
#
# 2020-04-29 ELM First versiob
# 2022-09-15 ELM Location of the vcgencmd command changed in the Raspberry Pi
#                distribution. Version bumped to 1.1. Cleaned up debugging
#                output.


# Original comments for log1Wtemp.pl

# Logs the CPU temperature to a file {MJD}.cputemp in the data
# directory specified in gpscv.conf on an OpenTTP system. Also
# writes the temperature to the cputemp file in the ~/logs
# directory
#
# based on logCPUtemp.pl
# Version 1.0 start date: 2016-12-20 (Louis Marais)
# Version 1.0 finalised:  2016-12-21
#
# Version 1.1 start date: 2017-07-19 (Louis Marais)
# Version 1.1 finalised:  2017-??-??
# ~~~~~~~~~~~~~~~~~~~~ Why am I changing it? ~~~~~~~~~~~~~~~~~~~~~~
# OpenTTP hardware specification finalised. Only one 1W temperature
# sensor is installed, a surface mount unit, mounted underneath the
# GPSDO miniPCIe module. In version 1.0 there are provision for up
# to 7 sensors.
# ~~~~~~~~~~~~~~~~~~~~~~~ Changes made ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# 1. Code modified to work with only one 1W sensor, called GPSDO. 
#    All code related to other sensors removed.
# 2. Changed status and lock file location to lockStatusCheck 
#    directory instead of logs directory. The lockStatusCheck
#    directory is a temporary directory for files that are 
#    frequently written, located in RAM (tmpfs).
#
# Last modification date: 2016-12-21
#

# Libraries, etc. to use
use POSIX;
use TFLibrary;
use vars qw($opt_d $opt_c $opt_h $opt_v);
use Getopt::Std;

# Use strict requires variables to be declared
my ($AUTHORS,$VERSION,$home,$configFile,$DEBUG,$logpath,$lockPath,$now,$mjd,$next,$killed);
my ($nowstr,%Init,$temp,$logPath,$then,$lockFile,$statusFile,@info,$msg,$sec);
my ($GPSDO,@dirs,$dir,$sn,$gpsdo_temp);

$AUTHORS = "Louis Marais";
$VERSION = "1.1";

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

if (-d "$home/lockStatusCheck")  {
  $lockPath="$home/lockStatusCheck";
  $statusFile=TFMakeAbsoluteFilePath("cputemp",$home,$lockPath);
}
elsif (-d "$home/status"){ # added this for NTP auditor application
  $lockPath="$home/status";
  $statusFile=TFMakeAbsoluteFilePath("cputemp",$home,$lockPath);
}
else
{
  ErrorExit("No ~/lockStatusCheck or ~/status directory found!\n");
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

# Assign serial numbers to sensor variables

#$GPSDO = $Init{"cputemp:gpsdo"};
#if (!(defined $GPSDO)) { Debug("No GPSDO entry found in config. Exiting."); exit; }

# Check for an existing lock file
$lockFile = TFMakeAbsoluteFilePath($Init{"cputemp:lock file"},$home,$lockPath);
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
$Init{"paths:cputemp data"} = TFMakeAbsolutePath($Init{"paths:cputemp data"},$home);
Debug("Data path: ".$Init{"paths:cputemp data"});

$now=time();
$mjd=int($now/86400) + 40587;
OpenDataFile($mjd,1);
$next=($mjd-40587+1)*86400;     # seconds at next MJD
$then=0;

# Set initial state of script to running fine.
$killed = 0;
# This intercepts the termination signal, and in this case sets
# the variable $killed = 1. This $killed variable controls the main loop.
# Very clever!
$SIG{TERM} = sub {$killed=1}; 
# Also capture Ctrl-C to gracefully exit during testing...
$SIG{INT} = sub {$killed=1};

# Get a list of sensors registered on the system
#opendir(DIR,"/sys/devices/w1_bus_master1/");
#@dirs = grep(/^28-0/,readdir(DIR));
#closedir(DIR);

my $cmd = '/opt/vc/bin/vcgencmd';
if (!(-e $cmd)){
  $cmd = '/usr/bin/vcgencmd';
}

Debug("Found vcgencmd here: $cmd");

while (!$killed)
{
  $temp = `$cmd measure_temp`;
  chomp($temp);

  Debug("vcgencmd output: $temp");

  if($temp=~/temp=(\d+\.\d+)'C/){
    $temp = $1;
  } else {
    $temp = '999.9';
  }

  Debug("Value that will be stored: $temp");

  # Store temperatures
  $now = time(); 
  $mjd = int($now/86400) + 40587;

  saveData($mjd,$temp); 
  saveStatus($temp);

  if ($now>=$next)
  {
    # (this way is safer than just incrementing $mjd)
    $mjd=int($now/86400) + 40587;     # don't call &TFMJD(), for speed
    OpenDataFile($mjd,0); # New file
    $next=($mjd-40587+1)*86400;       # seconds at next MJD
  }
  if ($now>$then)
  {
    # update string version of time stamp
    @_=gmtime $now;
    $nowstr=sprintf "%02d:%02d:%02d",$_[2],$_[1],$_[0];
    $then=$now;
  }

  # Wait approximately a minute - wait for the second to be 58 or larger
  $sec = 0;
  while ($sec <= 58)
  {
    @_=gmtime time();
    $sec = $_[0];
    sleep(1);
    if($killed) { last; }
  }
}

BYEBYE:
if (-e $lockFile) {unlink $lockFile;}

@_=gmtime();
$msg=sprintf ("%04d-%02d-%02d %02d:%02d:%02d $0 killed\n",
              $_[5]+1900,$_[4]+1,$_[3],$_[2],$_[1],$_[0]);
printf $msg;

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

sub Initialise
{
  my $name = shift; # The name of the configuration file.

  my @required=( "paths:cputemp data","cputemp:lock file");
  # other temperature sensors are optional
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
}

#-----------------------------------------------------------------------------

sub OpenDataFile
{
  # The first variable passed is the current MJD
  my $mjd=$_[0];

  # Fixup path and extension if needed
  
  my $name=$Init{"paths:cputemp data"}.$mjd.".temp";
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
  print "# Time   CPU\n";
  select STDOUT;

} # OpenDataFile

#----------------------------------------------------------------------------

sub saveData # mjd, temp
{
  my($mjd,$temp) = (shift,shift);
  #if($DEBUG) {
  #  print "saveData subroutine\n";
  #  print "\$mjd: ",$mjd,"\n";
  #  print "\$temp: $temp\n";
  #}
  my($name) = $Init{"paths:cputemp data"}.$mjd.".temp";
  open DTA,">>$name" or die "Could not write to $name\n";
  my($now) = strftime "%H:%M:%S",gmtime;
  print DTA "$now $temp\n";
  close DTA;

  #print "In 'saveData' routine\n";
  #print "Filename: $name\n";
  #print "timestamp: $now\n";
  #print "temperature: $temp\n";

} # saveData

#----------------------------------------------------------------------------

sub saveStatus # temp
{
  my $temp = shift;
  open STS,">$statusFile" or die "Could not open $statusFile\n";
  print STS "CPU temperature (degrees celsius)\n";
  print STS "$temp\n";
  close STS;
}

#----------------------------------------------------------------------------

sub Debug
{
  my($now) = strftime "%H:%M:%S",gmtime;
  if ($DEBUG) {print "$now $_[0] \n";}
} # Debug

#----------------------------------------------------------------------------

sub ErrorExit {
  my $message=shift;
  @_=gmtime(time());
  printf "%02d/%02d/%02d %02d:%02d:%02d $message\n",
    $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
  exit;
}

#-----------------------------------------------------------------------------


