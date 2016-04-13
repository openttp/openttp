#!/usr/bin/perl -w

#
# The MIT License (MIT)
#
# Copyright (c) 2016 Peter Fisk, Steve Quigg, Bruce Warrington, Michael J. Wouters
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

# Original version SQ, subsequently modified by PF and RBW
# 23.11.99 PF  Read parameter file from ../Parameter_Files/ directory 
# 22.11.99 RBW Only write to file if it was successfully opened; read
#                HPIB address from parameter file
# 17. 2.00 RBW Add Interface Clear and Device Clear commands to 488/p init
#                Check for null input on read; log SIGTERM
# 14. 2.05 MJW Add serial port locking
# 17-04-2008 MJW Use TFLibrary for initialisation
# 2015-04-05 MJW Use new configuration file etc.

use POSIX;
use TFLibrary;
use Getopt::Std;

use vars qw($opt_c $opt_d $opt_h  $opt_v);

$VERSION="1.0";
$AUTHORS="Peter Fisk, Steve Quigg, Bruce Warrington, Michael J. Wouters";

$alarmTimeout = 60; # SIGAARLM timout 

# Check command line
if ($0=~m#^(.*/)#) {$path=$1} else {$path="./"}	# read path info
$0=~s#.*/##;					# then strip it

if (!(getopts('c:dhv')) || $opt_h){
	&ShowHelp();
	exit;
}

if ($opt_v){
	print "$0 version $VERSION\n";
	print "Written by $AUTHORS\n";
	exit;
}

$home=$ENV{HOME};
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

$configFile=$configPath."/gpscv.conf";
if (defined $opt_c){
	$configFile=$opt_c;
}

if (!(-e $configFile)){
	ErrorExit("The configuration file $configFile was not found!\n");
}

%Init = &TFMakeHash2($configFile,(tolower=>1));

# Check we got the info we need from the config file
@check=("counter:port","counter:configuration","counter:gpib address","counter:gpib converter",
 "paths:counter data","counter:file extension","counter:lock file");
foreach (@check) {
  $tag=$_;
  $tag=~tr/A-Z/a-z/;	
  unless (defined $Init{$tag}) {ErrorExit("No entry for $_ found in $configFile")}
}

$ctrExt = $Init{"counter:file extension"};
if (!($ctrExt =~ /^\./)){ # do we need a period ?
	$ctrExt = ".".$ctrExt;
}

$dataPath = TFMakeAbsolutePath($Init{"paths:counter data"},$home);
$port = $Init{"counter:port"};
$GPIBaddress = $Init{"counter:gpib address"};
$cntrConfig = TFMakeAbsoluteFilePath($Init{"counter:configuration"},$home,$configPath);

$headerGen="";
if (defined $Init{"counter:header generator"}){
	$headerGen = TFMakeAbsoluteFilePath($Init{"counter:header generator"},$home,"$home/bin");
}

# Check the lock file
$lockFile = TFMakeAbsoluteFilePath($Init{"counter:lock file"},$home,$logPath);

if (-e $lockFile){
	open(IN,"<$lockFile");
	@info = split ' ',<IN>;
	close IN;
	Debug("$info[0] $info[1]");
	$running = kill 0,$info[1];
	if ($running){
		ErrorExit("already running ($info[1])");
	}
	else{
		open(OUT,">$lockFile");
		print OUT "$0 $$\n"; 
		close OUT;
	}
}
else{
	open(OUT,">$lockFile");
	print OUT "$0 $$\n"; 
	close OUT;
}

# Set up a timeout in case we get stuck
$SIG{ALRM} = sub {unlink $lockFile; ErrorExit("Timed out - exiting.")};
alarm $alarmTimeout;

# Catch kill signal so we can log that we were killed
$SIG{TERM} = sub {unlink $lockFile;ErrorExit("Received SIGTERM - exiting.")};

# Initialise the port, which also powers the converter, and let it reset

unless (`/usr/local/bin/lockport -p $$ $port $0`==1) {
	print "! Could not obtain lock on $port. Exiting.\n";
	exit;
}

connectserial($port);
sleep 1;

if ($Init{"counter:gpib converter"} eq "Micro488"){ 
	Debug("Initializing Micro488: GPIB address $GPIBaddress, device $port");
	# send 5 CR's for auto-baud sense
	for ($i=0;$i<5;$i++) {
		print PORT chr(13);
		sleep 1;
	}

	# set up Micro488/p - see manual for details
	&cmd("I");			# initialise interface	
	&cmd("H;0");		# handshaking off
	&cmd("X;0");		# XON/XOFF disabled
	&cmd("EC;0");		# echo off >>> MUST DO THIS <<<
	&cmd("TC;1");		# set LF as serial terminator
	&cmd("TB;1");		# set LF as GPIB bus terminator
	&cmd("A");			# Interface Clear - regain bus if reqd
	&cmd("C");			# Device Clear - reset counter
}
else{
	print "Unknown GPIB converter $Init\{\"counter:gpib converter\"\}\n";
	exit;
}

Debug("Configuring counter\n");

# reset counter......
&Send("*RST");			# reset counter
&Send("*CLS");			# reset status command

# ....and set mode, trigger, levels, coupling etc from parameter file
open IN,$cntrConfig or die "Can't open parameter file $cntrConfig: $!";	
while (<IN>) {
  chop;
  s/#.*$//;			# trim trailing comments
  s/\s*$//;			# and white space
  last unless ($_);
  &Send($_);		# send parameter to counter
}
close(IN);

&Send(":TINT:ARM:STOP:SOUR IMM");
&Read("*IDN?");			# these are just dummy lines
&Read("*IDN?");			# to flush garbage from the buffer

$errorCount=0;
$oldmjd=0;

Debug("Logging\n");

while ($errorCount<10) {
  # read counter
  &Read("");			# empty string interpreted as "READ?"

  # check for numeric response
  if ($_ && /^\s*[+-]?(\d*)(\.\d*)?([eE][+-]?\d+)?$/ && 
      ((defined($1) && length($1)) || (defined($2) && length($2)))) {
    chop;			# strip off terminator
    alarm $alarmTimeout;			# reset timeout counter
    $errorCount=0;		# and count of error responses

    # get MJD and time
    $tt=time();
    $mjd=int($tt / 86400)+40587;
    ($sec,$min,$hour)=gmtime($tt);

    # check for new day and create new filename if necessary
    if ($mjd!=$oldmjd) {
      $oldmjd=$mjd;
      $file_out=$dataPath . $mjd .$ctrExt;
      if ((-x $headerGen) && !(-e $file_out)){
				if (open OUT,">>$file_out"){
					@header = split /\n/,`$headerGen`;
					for ($i=0;$i<=$#header;$i++){
						print OUT "# $header[0]\n";
					}
					close OUT;
				}
			}
      Debug("Opening $file_out");
    }

    # write time-stamped data to file
    $time=sprintf("%02d:%02d:%02d",$hour,$min,$sec);
    next unless (open OUT,">>$file_out"); 
    print OUT $time,"  ",$_,"\n";
    Debug("$time $_");
    close OUT;
  }

  else {
    # Got a null string back from the counter - don't log to file, but keep
    # a count so we can die if we get too many in a row
    $errorCount++;
  }
}

close(PORT);
unlink $lockFile;

# End of main program

#-----------------------------------------------------------------------

sub ShowHelp{
	print "Usage: $0 [OPTION] ...\n";
	print "\t-c <config> specify alternate configuration file\n";
  print "\t-d turn on debugging\n";
  print "\t-h show this help\n";
  print "\t-v print version\n";
}
#-----------------------------------------------------------------------

sub ErrorExit {
  my $message=shift;
  @_=gmtime(time());
  printf "%02d/%02d/%02d %02d:%02d:%02d $message\n",
    $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
  exit;
}

# -------------------------------------------------------------------------
sub Debug
{
	if ($opt_d){
		($sec,$min,$hour)=gmtime(time());
		printf STDERR "%02d:%02d:%02d $_[0]\n",$hour,$min,$sec;
	}
}

# Send a command to the Micro488/p 
sub cmd {
   print PORT $_[0];
}

#-----------------------------------------------------------------------

# Send a command to the HP5313x
sub Send {
   print PORT "OA;".(sprintf "%02d",$GPIBaddress).";".$_[0].chr(10);
}

#-----------------------------------------------------------------------

# Send a command to the HP5313x and get a reply
sub Read {
   my $text=$_[0];
   unless ($text) {$text="READ?"}
   print PORT "OA;".(sprintf "%02d",$GPIBaddress).";".$text.chr(10);
   print PORT "EN;".(sprintf "%02d",$GPIBaddress).chr(10);
   $_=<PORT>;
}

#-----------------------------------------------------------------------

# subroutine to initialise serial port
# the device to open is passed as a parameter, and comes from the setup file
sub connectserial {
  my $portname=shift;

  sysopen PORT,$portname,O_RDWR|O_NOCTTY;
  $fd=fileno PORT;

  $termios= POSIX::Termios->new(); # See Camel book pp 471 and termios man page
  $termios -> getattr($fd);

  $termios -> setiflag(IGNPAR);
  $termios -> setoflag(0);
  $termios -> setlflag(0);
  $termios -> setcflag(CS8|CREAD|CLOCAL|HUPCL);

  # speed setting must follow cflag above, as it sets low bits in the cflag
  $termios -> setospeed(B9600);
  $termios -> setispeed(B9600);

  $termios -> setattr($fd,&POSIX::TCSANOW);

  select(PORT); $|=1;	# use unbuffered IO ie: auto flushing
  select(STDOUT);
} # connectserial

#-----------------------------------------------------------------------

