#!/usr/bin/perl
# logpicputemp.pl

use warnings;
use strict;

# Logs Pi CPU temperature 

#
# The MIT License (MIT)
#
# Copyright (c) 2020-2025 E. Louis Marais
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the 'Software'), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

# 2020-04-29 ELM First version
# 2022-09-15 ELM Location of the vcgencmd command changed in the Raspberry Pi
#                distribution. Version bumped to 1.1. Cleaned up debugging
#                output.
# 2025-10-01 ELM Cleaned up comments. Add license. 'log' now preferred location
#                for $logPath, 'var' now preferred location for lock and status
#                files. Version now 1.2.

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
$VERSION = "1.2";

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

if (-d "$home/log")
{
  $logPath = "$home/log";
}
elsif (-d "$home/logs")
{
  $logPath = "$home/logs";
}
else
{
  ErrorExit("No ~/log or ~/logs directory found!\n");
}

if (-d "$home/var")  
{
  $lockPath="$home/var";
}
elsif (-d "$home/lockStatusCheck")  
{
  $lockPath="$home/lockStatusCheck";
}
elsif (-d "$home/status") # added this for NTP auditor application
{
  $lockPath="$home/status";
}
else
{
  ErrorExit("No ~/var or ~/lockStatusCheck or ~/status directory found!\n");
}

$statusFile=TFMakeAbsoluteFilePath("cputemp",$home,$lockPath);

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
Debug("Lock path: $lockPath");
Debug("Configuration file: $configFile");

Initialise($configFile);

# Check for an existing lock file
$lockFile = TFMakeAbsoluteFilePath($Init{"cputemp:lock file"},$home,$lockPath);
Debug("\$lockFile: $lockFile");

# Check if the process is already running.
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

$killed = 0;

$SIG{TERM} = sub {$killed=1}; 
$SIG{INT} = sub {$killed=1};

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


