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

use POSIX;
use TFLibrary;

# check command line
if ($0=~m#^(.*/)#) {$path=$1} else {$path="./"}	# read path info
$0=~s#.*/##;					# then strip it
unless (@ARGV==1) {
  print "Usage: $0 filename\n";
  print "  where <filename> is the name of the setup file\n";
  exit;
}

$count=0;
open (PS, "ps xw|");
while (<PS>) {if (/$0/ && !/ vi /) {$count++}}
close PS;
if ($count>2) {ErrorExit("$0 process already present")}

# Read setup info from file
$filename=$ARGV[0];
unless ($filename=~m#/#) {$filename=$path.$filename}
%Init = &TFMakeHash($filename,(tolower=>1));

# Check we got the info we need from the setup file
@check=("counter port","counterparfile","data path",
  "counter data extension","counterhpib");
foreach (@check) {
  $tag=$_;
  $tag=~tr/A-Z/a-z/;	
  unless (defined $Init{$tag}) {ErrorExit("No entry for $_ found in $filename")}
}

# other initialisation
if (-d "$ENV{HOME}/etc")  {$configpath="$ENV{HOME}/etc";}  else {$configpath="$ENV{HOME}/Parameter_Files";}
$par_file=$Init{"counterparfile"};
unless ($par_file=~m#/#) {$par_file="$configpath/$par_file";}
unless ($Init{"data path"}=~m#/$#) {$Init{"data path"}.="/"}
$errorCount=0;
$oldmjd=0;

# set up a timeout in case we get stuck
$SIG{ALRM} = sub {ErrorExit("Timed out - exiting.")};
alarm 60;

# catch kill signal so we can log that we were killed
$SIG{TERM} = sub {ErrorExit("Received SIGTERM - exiting.")};

# initialise the port, which also powers the converter, and let it reset
$port = $Init{"counter port"};
unless (`/usr/local/bin/lockport -p $$ $port $0`==1) {
    printf "! Could not obtain lock on $port. Exiting.\n";
    exit;
  }
connectserial($port);
sleep 1;

# send 5 CR's for auto-baud sense
for ($i=0;$i<5;$i++) {
  print PORT chr(13);
  sleep 1;
}

# set up Micro488/p - see manual for details
&cmd("I");			# initialise interface	
&cmd("H;0");			# handshaking off
&cmd("X;0");			# XON/XOFF disabled
&cmd("EC;0");			# echo off >>> MUST DO THIS <<<
&cmd("TC;1");			# set LF as serial terminator
&cmd("TB;1");			# set LF as GPIB bus terminator
&cmd("A");			# Interface Clear - regain bus if reqd
&cmd("C");			# Device Clear - reset counter

# reset counter......
&send("*RST");			# reset counter
&send("*CLS");			# reset status command

# ....and set mode, trigger, levels, coupling etc from parameter file
open IN,$par_file or die "Can't open parameter file $par_file: $!";	
while (<IN>) {
  chop;
  s/#.*$//;			# trim trailing comments
  s/\s*$//;			# and white space
  last unless ($_);
  &send($_);			# send parameter to counter
}
close(IN);
&send(":TINT:ARM:STOP:SOUR IMM");

&read("*IDN?");			# these are just dummy lines
&read("*IDN?");			# to flush garbage from the buffer


while ($errorCount<10) {
  # read counter
  &read("");			# empty string interpreted as "READ?"

  # check for numeric response
  if ($_ && /^\s*[+-]?(\d*)(\.\d*)?([eE][+-]?\d+)?$/ && 
      ((defined($1) && length($1)) || (defined($2) && length($2)))) {
    chop;			# strip off terminator
    alarm 60;			# reset timeout counter
    $errorCount=0;		# and count of error responses

    # get MJD and time
    $tt=time();
    $mjd=int($tt / 86400)+40587;
    ($sec,$min,$hour)=gmtime($tt);

    # check for new day and create new filename if necessary
    if ($mjd!=$oldmjd) {
      $oldmjd=$mjd;
      $file_out=$Init{"data path"} . $mjd . $Init{"counter data extension"};
    }

    # write time-stamped data to file
    $time=sprintf("%02d:%02d:%02d",$hour,$min,$sec);
    next unless (open OUT,">>$file_out"); 
    print OUT $time,"  ",$_,"\n";
    close OUT;
    # print  $time,"  ",$_,"\n";
  }

  else {
    # Got a null string back from the counter - don't log to file, but keep
    # a count so we can die if we get too many in a row
    $errorCount++;
  }
}
close(PORT);

# End of main program
#-----------------------------------------------------------------------

# subroutine to send a command to the Micro488/p 
sub cmd {
   print PORT $_[0];
}

#-----------------------------------------------------------------------

# subroutine to send a command to the HP53131
sub send {
   print PORT "OA;".(sprintf "%02d",$Init{counterhpib}).";".$_[0].chr(10);
}

#-----------------------------------------------------------------------

# subroutine to send a command to the HP53131 and get a reply
sub read {
   my $text=$_[0];
   my $addr=$Init{counterhpib};
   unless ($text) {$text="READ?"}
   print PORT "OA;".(sprintf "%02d",$addr).";".$text.chr(10);
   print PORT "EN;".(sprintf "%02d",$addr).chr(10);
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

sub ErrorExit {
  my $message=shift;
  @_=gmtime(time());
  printf "%02d/%02d/%02d %02d:%02d:%02d $message\n",
    $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
  exit;
}

#-----------------------------------------------------------------------
