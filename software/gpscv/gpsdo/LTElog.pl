#!/usr/bin/perl

#
# The MIT License (MIT)
#
# Copyright (c) 2016 Louis Marais
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

# LTElog.pl
use warnings;
use strict;

# LTElog.pl
# based on gpsdolog.pl
#
# Version 1.0 start date: 2016-01-13 by Louis Marais
# Last modification date: 2017-03-17
#
# Versioning held in %Init structure $Init{version}

# Load required libraries

# Need the POSIX library so that all the serial port flags are defined
# They are used in the TFSerialConnect call
use POSIX;
# Contains the serial connect subroutine
use TFLibrary;
# Declare $tmask globally
use vars qw($tmask $opt_c $opt_d $opt_h $opt_r $opt_v);
# To enable branching on message IDs
use Switch;
# To enable use of 'getopts' (get hold of switches added to the script call)
use Getopt::Std;
# library to decode gpsdo binary messages
use LTELite::DecodeNMEA;

# Variable declarations, required because of 'use strict'
my ($AUTHORS,$VERSION,$home,$configFile,$logPath,$lockFile,$gpsdoTimeout);
my ($killed,$nfound,$rxmask,$rx,$input,$save,$msg,$nmea,$now,$mjd,@info,$gpsMode);
my ($next,$gpsdoStatus,$then,$lastMsg,$filteredUTC,$rawUTC);
my ($noOfCapturedPulses,$lockStatus,$EFCvoltage,$EFCpercentage,$estimatedFreqAcc);
my ($secsInHoldover,$nmbrsats,$health,%Init);
my ($configPath,$dataPath,$alarmTimeout,$port,$logInterval,$lastLog);
my ($logState,$WAITING,$LOGGING_MSGS);

$AUTHORS="Louis Marais,Michael Wouters";
$VERSION="1.2";

# logging state machine states
$WAITING=1; 
$LOGGING_MSGS=2;

$alarmTimeout = 120; # SIGAARLM timout 

$0=~s#.*/##;

$home=$ENV{HOME};

if (-d "$home/etc")  {
	$configPath="$home/etc";
}
else{	
	ErrorExit("No ~/etc directory found!\n");
} 

$configFile = $configPath.'/gpscv.conf';

if (-d "$home/logs")  {
	$logPath="$home/logs";
} 
else{
	ErrorExit("No ~/logs directory found!\n");
}

if (!(-e $configFile)){
  ErrorExit("A configuration file was not found!\n");
}

if (!(getopts('c:dhrv')) || $opt_h){
	&ShowHelp();
	exit;
}

if ($opt_v){
	print "$0 version $VERSION\n";
	print "Written by $AUTHORS\n";
	exit;
}

if (defined $opt_c){
  $configFile=$opt_c;
}

&Initialise($configFile);

# Check the lock file
$lockFile = TFMakeAbsoluteFilePath($Init{"reference:lock file"},$home,$logPath);
if (!(TFCreateProcessLock($lockFile))){
	ErrorExit('Unable to lock - already running?');
}

$dataPath = TFMakeAbsolutePath($Init{"paths:reference data"},$home);
Debug($dataPath);
# Assign the filename for the receiver status dump (from the configuration file)
$gpsdoStatus = TFMakeAbsoluteFilePath($Init{"reference:status file"},$home,$logPath);

# open the serial port to the receiver
$rxmask = "";
$port = $Init{"reference:port"};

unless (`/usr/local/bin/lockport $port $0`==1) {
	printf "! Could not obtain lock on $port. Exiting.\n";
	TFRemoveProcessLock($lockFile);
	exit;
}

# Open port to GPSDO at 38400 baud, 8 data bits, 1 stop bit,
# no parity, no flow control (see GPSDO user guide, p 25)
# To decipher the flags, look in the termios documentation

$rx = &TFConnectSerial($port,
			(ispeed=>B38400,ospeed=>B38400,iflag=>IGNBRK,
				oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|CLOCAL));
# set up a mask which specifies this port for select polling later on
vec($rxmask,fileno $rx,1)=1;
Debug("Port $port to GPSDO (Jackson Labs LTE-Lite) is open\n");
# Wait a bit
select (undef,undef,undef,0.1);

$now=time();
$mjd=int($now/86400) + 40587;

# Set initial state of script to running fine.
$killed = 0;
# This intercepts the termination signal, and in this case sets
# the variable $killed = 1. This $killed variable controls the main loop.
# Very clever!
$SIG{TERM} = sub {$killed=1}; 
# Also capture Ctrl-C to gracefully exit during testing...
$SIG{INT} = sub {$killed=1};

# Configure - no software configuration options available for the LTE-Lite, 
# so nothing to do here.

$input = "";
$save = "";

$alarmTimeout = 600;
if (defined($Init{"reference:timeout"})) {$alarmTimeout=$Init{"reference:timeout"};}
$lastLog =-1; 
$lastMsg=time();

# GPS mode (PVT, Survey or Position Hold) is sent in the $PSTI message.
# We create the gpsMode variable to hold the mode
$gpsMode = "Unknown";

# Main loop - read data, handle messages and store required information
$logInterval=60;
$logState = $WAITING;
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
	
	# If we skip HandleInput()
	# then the CPU load goes down to about 2.5%
	($input,$save,$msg,$nmea) = HandleInput($input,$save);
	# If we skip the rest of the processing, CPU load on BB is still 6 to 7 % ie unchanged
	# So CPU load is all in reading the serial port
	if ($msg ne "") 
	{ 
		$now = time();
		$mjd = int($now/86400) + 40587;
		# Decode the GPS status message, if available
		if ($msg =~ m/\$PSTI/){ # always decode and save this since it's irregular
			$gpsMode = decodeMsgPSTI($msg);
			SaveData($mjd,$msg);
			next;
		}
		if ($now - $lastLog < $logInterval && $lastLog != -1){ next;}
		if ($msg =~ /\$GPGGA/){ # first message of the second
			$logState=$LOGGING_MSGS;
			Debug("Logging msgs start");
		}
		
		if ($logState == $LOGGING_MSGS) {
			SaveData($mjd,$msg);
		} 
		else{
			next;
		}
		
		# Decode the GPSDO status message
		if ($msg =~ m/\$PJLTS/)
		{
			($filteredUTC,$rawUTC,$noOfCapturedPulses,$lockStatus,$EFCvoltage,$EFCpercentage,
			$estimatedFreqAcc,$secsInHoldover,$nmbrsats,$health) = decodeMsgPJLTS($msg);
			# update status file 
			WriteStatus($filteredUTC,$rawUTC,$noOfCapturedPulses,$lockStatus,$EFCvoltage,$EFCpercentage,
			$estimatedFreqAcc,$secsInHoldover,$nmbrsats,$health,$gpsMode);
			# Can we see satellites? If we can, reset the timeout counter
			if ($nmbrsats > 0) { $lastMsg = $now; }
		}
		
		if ($msg =~ /\$GPZDA/){ # last message
			$lastLog=$now;
			$logState=$WAITING;
			Debug("Logging msgs stop");
		}
	}
	# Add timeout stuff
	if (time()-$lastMsg > $alarmTimeout)
	{
		@_=gmtime();
		#$msg=sprintf("%04d-%02d-%02d %02d:%02d:%02d no satellites visible - exiting\n",
		#							$_[5]+1900,$_[4]+1,$_[3],$_[2],$_[1],$_[0]);
		#printf OUT "# ".$msg;
		$killed = 1;
	}
}

BYEBYE:
TFRemoveProcessLock($lockFile);

@_=gmtime();
$msg=sprintf ("%04d-%02d-%02d %02d:%02d:%02d $0 killed\n",
              $_[5]+1900,$_[4]+1,$_[3],$_[2],$_[1],$_[0]);
printf $msg;
#printf OUT "# ".$msg;
#close OUT;

exit;

# End of main

#----------------------------------------------------------------------------
# Subroutines
#----------------------------------------------------------------------------

#-----------------------------------------------------------------------

sub ShowHelp{
	print "Usage: $0 [OPTION] ...\n";
	print "\t-c <config> specify alternate configuration file\n";
	print "\t-d turn on debugging\n";
	print "\t-h show this help\n";
	print "\t-r reset the GPSDO\n";
	print "\t-v print version\n";
	print "  The default configuration file is $configFile\n";
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
		my ($sec,$min,$hour)=gmtime(time());
		printf STDERR "%02d:%02d:%02d $_[0]\n",$hour,$min,$sec;
	}
}

# -------------------------------------------------------------------------

sub Initialise 
{
  my $name = shift; # The name of the configuration file.
  # Define the parameters that MUST have values in the configuration file
  # note that all these values come in as lowercase, no matter how they are
  # written in the configuration file
  my @required=("reference:file extension","reference:port","reference:status file","reference:lock file",
		"paths:reference data");
	
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

#----------------------------------------------------------------------------

sub SaveData # mjd, data
{
	my($mjd,$data) = (shift,shift);
	if ($opt_d) {
		print "SaveData subroutine\n";
		print "\$mjd: ",$mjd,"\n";
		if ((substr $data,0,1) eq "\$") {
			print "\$data (NMEA): ",$data;
		}
		else{
			print "\$data (scpi): ",$data;
		}
	}
	my($name) = $dataPath.$mjd.$Init{"reference:file extension"};
	open DTA,">>$name" or die "Could not write to $name\n"; 
	my($now) = strftime "%H:%M:%S",gmtime;
	if ((substr $data,0,1) eq "\$"){  # NMEA sentence
		print DTA $now," ",$data;
	}
	else {# SCPI sentence
		print DTA $now," scpi >\n$data\n";
	}
	close DTA;
}

#----------------------------------------------------------------------------

sub WriteStatus
{
	my($filteredUTC,$rawUTC,$noOfCapturedPulses,$lockStatus,$EFCvoltage,$EFCpercentage,
			$estimatedFreqAcc,$secsInHoldover,$nmbrsats,$health,$gpsMode) = (@_);
	# Write current GPSDO status to the file specified in $Input{receiver status}
	open  STA, ">$gpsdoStatus";
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
