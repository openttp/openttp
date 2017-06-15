#!/usr/bin/perl -w

#
# The MIT License (MIT)
#
# Copyright (c) 2016 Michael J. Wouters
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

# ubloxlog - Perl script to configure ublox LEA8MT GNSS receivers and download data

# Modification history:					
# 02-05-2016 MJW First version, derived restlog.pl
# 27-02-2017 MJW Minor cleanups for configuration path

use Time::HiRes qw( gettimeofday);
use TFLibrary;
use POSIX;
use Getopt::Std;
use POSIX qw(strftime);
use vars  qw($tmask $opt_c $opt_r $opt_d $opt_h $opt_v);

$VERSION="0.1";
$AUTHORS="Michael Wouters";

$REMOVE_SV_THRESHOLD=600; #interval after which  a SV marked as disappeared is removed
$COLDSTART_HOLDOFF = 0;
$EPHEMERIS_POLL_INTERVAL=7000; # a bit less than the 7200 s threshold for
															 # ephemeris validity as used in processing S/W
															 
# Track each SV so that we know when to ask for an ephemeris
# Record SVN
# Record time SV appeared (Unix time)
# Record time we last got an ephemeris (Unix time)
# Flag for marking whether still visible

$SVN=0; # constants for indexing into the array tracking each SV
$APPEARED=1;
$LAST_EPHEMERIS_RECEIVED=2;
$LAST_EPHEMERIS_REQUESTED=5;
$STILL_VISIBLE=3;
$DISAPPEARED=4;

# Need to track some parameters too
$UTC_IONO_POLL_INTERVAL=14400;

$LAST_REQUESTED=0;
$LAST_RECEIVED=1;

$UTC_IONO_PARAMETERS= 0;
#Initialise parameters
$params[$UTC_IONO_PARAMETERS][$LAST_REQUESTED]=-1;
$params[$UTC_IONO_PARAMETERS][$LAST_RECEIVED]=-1;

$0=~s#.*/##;

$home=$ENV{HOME};
$configPath="$home/etc";
if (!(-d "$home/etc")){
	ErrorExit("No $configPath directory found!\n");
}

$logPath="$home/logs";
if (!(-d "$home/logs")){
	ErrorExit("No ~/logs directory found!\n");
}

if (-e "$configPath/ublox.conf"){ # this takes precedence
	$configFile=$configPath."/ublox.conf";
}
elsif (-e "$configPath/gpscv.conf"){
	$configFile=$configPath."/gpscv.conf";
}
else{
	ErrorExit("A configuration file was not found!");
}

if( !(getopts('c:dhrv')) || ($#ARGV>=1)) {
  ShowHelp();
  exit;
}

if ($opt_v){
	print "$0 version $VERSION\n";
	print "Written by $AUTHORS\n";
	exit;
}

if (defined $opt_c){ 
  if (-e $opt_c){
    $configFile=$opt_c;
  }
  else{
    ErrorExit( "$opt_c not found!");
  }
}

if ($opt_h){
	ShowHelp();
	exit;
}

$ubxmsgs=":";
&Initialise($configFile);


# Check for an existing lock file
# Check the lock file
$lockFile = TFMakeAbsoluteFilePath($Init{"receiver:lock file"},$home,$logPath);
if (!TFCreateProcessLock($lockFile)){
	ErrorExit("Process is already running\n");
}

#Open the serial ports to the receiver
$rxmask="";
$port=$Init{"receiver:port"};
$port="/dev/$port" unless $port=~m#/#;

unless (`/usr/local/bin/lockport $port $0`==1) {
	printf "! Could not obtain lock on $port. Exiting.\n";
	exit;
}

$rx=&TFConnectSerial($port,
		(ispeed=>0010002,ospeed=>0010002,iflag=>IGNBRK,
			oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|CLOCAL));
			
# Set up a mask which specifies this port for select polling later on
vec($rxmask,fileno $rx,1)=1;
print "> Port $port to Rx is open\n";

$rxStatus=$Init{"receiver:status file"};
$rxStatus=&TFMakeAbsoluteFilePath($rxStatus,$home,$logPath);

$Init{"paths:receiver data"}=TFMakeAbsolutePath($Init{"paths:receiver data"},$home);

if (!($Init{"receiver:file extension"} =~ /^\./)){ # do we need a period ?
		$Init{"receiver:file extension"} = ".".$Init{"receiver:file extension"};
}
	
$now=time();
$mjd=int($now/86400) + 40587;
&OpenDataFile($mjd,1);
$next=($mjd-40587+1)*86400;	# seconds at next MJD
$then=0;

&ConfigureReceiver();

$input="";
$killed=0;

$SIG{TERM}=sub {$killed=1};

$tstart = time;

$receiverTimeout=600;
$configReceiverTimeout=$Init{"receiver:timeout"};
if (defined($configReceiverTimeout)){$receiverTimeout=$configReceiverTimeout;}

$lastMsg=time(); 

LOOP: while (!$killed)
{
	if (time()-$lastMsg > $receiverTimeout){ # too long, so byebye
		@_=gmtime();
		$msg=sprintf "%02d/%02d/%02d %02d:%02d:%02d no response from receiver\n",
			$_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
		printf OUT "# ".$msg;
		goto BYEBYE;
		}
  
	# Wait for input
	$nfound=select $tmask=$rxmask,undef,undef,0.2;
	next unless $nfound;
	if ($nfound<0) {$killed=1; last}
	# Read input, and append it to $input
	sysread $rx,$input,$nfound,length $input;
	# Look for a message in what we've accumulated
	# $`=pre-match string, $&=match string, $'=post-match string (Camel book p128)

	#if ($input=~/(\$G\w{4})(.+\*[A-F_0-9]{2})\r\n/){ # grab NMEA
	#	$input=$'; # save the dangly bits
	#	print $1,$2,"\n";
	#}
	
	# Header structure for UBX packets is 
	# Sync char 1 | Sync char 2| Class (1 byte) | ID (1 byte) | payload length (2 bytes) | payload | cksum_a | cksum_b
	
	if ($input=~ /\xb5\x62(..)(..)([\s\S]*)/) { # if we've got a UBX header
		$input=$&; # chuck away the prematch part
		$classid=$1;
		$payloadLength = unpack("v",$2);
		$data = $3;
		
		# Have we got the full message ?
		$packetLength = $payloadLength + 8;
		$inputLength=length($input);
		if ($packetLength <= $inputLength){ # it's all there ! yay !
			
			$now=time();			# got one - tag the time
			$lastMsg=$now;
			
			if ($now>=$next){
				# (this way is safer than just incrementing $mjd)
				$mjd=int($now/86400) + 40587;	
				&OpenDataFile($mjd,0);
				# Request receiver and software versions
				# TODO
				PollChipID();
				$next=($mjd-40587+1)*86400;	# seconds at next MJD
			}
			
			if ($now>$then) {
				# Update string version of time stamp
				@_=gmtime $now;
				$nowstr=sprintf "%02d:%02d:%02d",$_[2],$_[1],$_[0];
				$then=$now;
				print "---\n";
			}
	
			if ( $ubxmsgs =~ /:$classid:/){ # we want this one
				($class,$id)=unpack("CC",$classid);
				#printf "%02x%02x $nowstr %i %i %i\n",$class,$id,$payloadLength,$packetLength,$inputLength;
				#printf OUT "%02x%02x $nowstr %s\n",$class,$id,(unpack "H*",(substr $data,0,$payloadLength+2));
				if ($class == 0x01 && $id == 0x35){
					UpdateSVInfo($data);
				}
				
				if ($class == 0x01 && $id == 0x22){ # last of periodic messages is the clock solution
					UpdateUTCIonoParameters();
					UpdateGPSEphemeris();
				}
				
				if ($class == 0x0b){
					ParseGPSSystemDataMessage($id,$payloadLength,$data);
				}
			}
			else{
				if ($opt_d){
					($class,$id)=unpack("CC",$classid);
					printf "Unhandled UBX msg %02x %02x\n",$class,$id;
				}
			}
			# Tidy up the input buffer
			if ($packetLength == $inputLength){
				$input="";
			}
			else{
				$input=substr $input,$packetLength;
			}
		}
		else{
			# nuffink
		}
	}
}

BYEBYE:
if (-e $lockFile) {unlink $lockFile;}

@_=gmtime();
$msg=sprintf "%02d/%02d/%02d %02d:%02d:%02d $0 killed\n",
  $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
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
sub ErrorExit {
  my $message=shift;
  @_=gmtime(time());
  printf "%02d/%02d/%02d %02d:%02d:%02d $message\n",
    $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
  exit;
}

#----------------------------------------------------------------------------
sub Initialise 
{
  my $name=shift;
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
	
}# Initialise


#-----------------------------------------------------------------------------
sub Debug
{
	if ($opt_d){
		print strftime("%H:%M:%S",gmtime)." $_[0]\n";
	}
}

#-----------------------------------------------------------------------------
sub OpenDataFile
{
  my $mjd=$_[0];

	# Fixup path and extension if needed

	my $ext=$Init{"receiver:file extension"};
	
	my $name=$Init{"paths:receiver data"}.$mjd.$ext;
	my $old=(-e $name); # already there ? May have restarted logging.

	Debug("Opening $name");

	open OUT,">>$name" or die "Could not write to $name";
	select OUT;
	$|=1;
	printf "# %s $0 (version $VERSION) %s\n",
		&TFTimeStamp(),($_[1]? "beginning" : "continuing");
	printf "# %s file $name\n",
		($old? "Appending to" : "Beginning new");
	printf "\@ MJD=%d\n",$mjd;
	select STDOUT;

} # OpenDataFile

#----------------------------------------------------------------------------
sub Checksum
{
	my @cmsg = unpack('C*',shift);
	my $cka=0;
	my $ckb=0;
	my $i;
	
	for ($i=0;$i<=$#cmsg;$i++){
		$cka = $cka + $cmsg[$i];
		$ckb = $ckb + $cka;
		$cka = $cka & 0xff;
		$ckb = $ckb & 0xff;
	}
	return pack('C2',$cka,$ckb);
}

#----------------------------------------------------------------------------
sub SendCommand
{
  my $cmd=shift;
  my $cksum=Checksum($cmd);
  print $rx "\xb5\x62".$cmd.$cksum;	
} # SendCommand

#----------------------------------------------------------------------------
sub ConfigureReceiver
{

	# Note that reset causes a USB disconnect
	# so it's a good idea to use udev to map the device to a 
	# fixed name
	if ($opt_r){ # hard reset
		$msg="\x06\x04\x04\x00\xff\xff\x00\x00";
		SendCommand($msg);
		sleep 5;
	}
	
	# Navigation/measurement rate settings
	$ubxmsgs .= "\x06\x08:";
	$msg = "\x06\x08\x06\x00\xe8\x03\x01\x00\x01\x00"; # CFG_RATE
	SendCommand($msg);
	
	# Configure various messages for 1 Hz output
	
	# RXM-RAWX raw data message
	$ubxmsgs .= "\x06\x01:";
	$ubxmsgs .= "\x02\x15:";
	$msg="\x06\x01\x03\x00\x02\x15\x01"; #CFG-MSG 0x02 0x15
	SendCommand($msg);
	
	# TIM-TP time pulse message (contains sawtooth error)
	$ubxmsgs .= "\x0d\x01:";
	$msg="\x06\x01\x03\x00\x0d\x01\x01"; #CFG-MSG 0x0d 0x01
	SendCommand($msg);
	
	# Satellite information
	$ubxmsgs .= "\x01\x35:";
	$msg="\x06\x01\x03\x00\x01\x35\x01"; #CFG-MSG 0x01 0x35
	SendCommand($msg); 
	
	# NAV-TIMEUTC UTC time solution 
	$ubxmsgs .= "\x01\x21:";
	$msg="\x06\x01\x03\x00\x01\x21\x01"; #CFG-MSG 0x01 0x21
	SendCommand($msg); 
	
	# NAV-CLOCK clock solution (contains clock bias)
	$ubxmsgs .= "\x01\x22:";
	$msg="\x06\x01\x03\x00\x01\x22\x01"; #CFG-MSG 0x01 0x22
	SendCommand($msg); 
	
	# Polled messages
	$ubxmsgs .= "\x0b\x31:"; # GPS ephemeris
	$ubxmsgs .= "\x0b\x02:"; # GPS UTC & ionosphere
	$ubxmsgs .= "\x05\x00:\x05\01:"; # ACK-NAK, ACK_ACK
	$ubxmsgs .= "\x27\x03:"; # Chip ID
	
	PollVersionInfo();
	PollChipID();
	
} # ConfigureReceiver

# ---------------------------------------------------------------------------
# Gets receiver/software version
sub PollVersionInfo
{
	my $msg="\x0a\x04\x00\x00";
	SendCommand($msg);
}

# ---------------------------------------------------------------------------
sub PollChipID
{
	my $msg="\x27\x03\x00\x00";
	SendCommand($msg);
}

# ---------------------------------------------------------------------------
sub PollGPSEphemeris
{
 
  ($svID)=pack("C",shift);
	my $msg="\x0b\x31\x01\x00$svID";
	SendCommand($msg);
}

# ----------------------------------------------------------------------------
sub PollUTCIonoParameters
{
	my $msg="\x0b\x02\x00\x00"; # AID-HUI GPS health, UTC and ionosphere parameters
	SendCommand($msg);
}

# Poll for ephemeris if a poll is due
sub UpdateGPSEphemeris
{
	for ($i=0;$i<=$#SVdata;$i++){
		if ($SVdata[$i][$LAST_EPHEMERIS_REQUESTED] ==-1 &&
			time - $tstart > $COLDSTART_HOLDOFF ){ #flags start up for SV
			# Try to get an ephemeris as soon as possible after startup
			$SVdata[$i][$LAST_EPHEMERIS_REQUESTED] = time;
			Debug("Requesting ephemeris for $SVdata[$i][$SVN]");
			PollGPSEphemeris($SVdata[$i][$SVN]);
		}
		elsif (time - $SVdata[$i][$LAST_EPHEMERIS_REQUESTED] > 30 && 
			$SVdata[$i][$LAST_EPHEMERIS_REQUESTED] > $SVdata[$i][$LAST_EPHEMERIS_RECEIVED] ){
				$SVdata[$i][$LAST_EPHEMERIS_REQUESTED] = time;
			Debug("Requesting ephemeris for $SVdata[$i][$SVN] again!");
			PollGPSEphemeris($SVdata[$i][$SVN]);
		}
		elsif (($SVdata[$i][$LAST_EPHEMERIS_RECEIVED] != -1) &&
			(time - $SVdata[$i][$LAST_EPHEMERIS_REQUESTED] > $EPHEMERIS_POLL_INTERVAL)){
			$SVdata[$i][$LAST_EPHEMERIS_REQUESTED] = time;
			Debug("Requesting ephemeris for $SVdata[$i][$SVN] cos it be stale");
			PollGPSEphemeris($SVdata[$i][$SVN]);
		}
	}
	
}

# ----------------------------------------------------------------------------
sub UpdateUTCIonoParameters
{
	
	if ($params[$UTC_IONO_PARAMETERS][$LAST_REQUESTED] == -1 && 
		time - $tstart > $COLDSTART_HOLDOFF){
		# Just starting so do this ASAP
		$params[$UTC_IONO_PARAMETERS][$LAST_REQUESTED]= time;
		Debug("Requesting UTC/iono parameters");
		PollUTCIonoParameters();
	}
	elsif (time - $params[$UTC_IONO_PARAMETERS][$LAST_REQUESTED] > 30 &&
		$params[$UTC_IONO_PARAMETERS][$LAST_REQUESTED] > 
			$params[$UTC_IONO_PARAMETERS][$LAST_RECEIVED]){
		# Polled but no response
		$params[$UTC_IONO_PARAMETERS][$LAST_REQUESTED]= time;
		Debug("Requesting UTC/iono parameters again!");
		PollUTCIonoParameters();
	}
	elsif (($params[$UTC_IONO_PARAMETERS][$LAST_RECEIVED] != -1) &&
		time - $params[$UTC_IONO_PARAMETERS][$LAST_REQUESTED] >  $UTC_IONO_POLL_INTERVAL){
		# Poll overdue
		$params[$UTC_IONO_PARAMETERS][$LAST_REQUESTED]= time;
		Debug("Requesting UTC/iono parameters cos they be stale");
		PollUTCIonoParameters();
	}
}

# ---------------------------------------------------------------------------
sub UpdateSVInfo
{
	# Uses 0x01 0x35 message 
	# This is used to track SVs as they appear and disappear so that we know
	# when to request a new ephemeris
	my $data=shift;
	my $numSVs=(length($data)-8-2)/12;
	my $i;
	
	# Mark all SVs as not visible so that non-visible SVs can be pruned
	for ($i=0;$i<=$#SVdata;$i++){
		$SVdata[$i][$STILL_VISIBLE]=0;
	}
	
	#($iTOW,$ver,$numSVs) = unpack("ICC",$data);
	#Debug("numSVs = $numSVs");
	
	for ($i=0;$i<$numSVs;$i++){
		($gnssID,$svID,$cno,$elev,$azim,$prRes,$flags)=unpack("CCCcssI",substr $data,8+12*$i,12);
		$ephAvail=$flags & 0x0800;
		next unless ($gnssID == 0); # only want GPS
		# Debug("Checking $svID ($i)");
		# If the SV is being tracked then no more to do
		for ($j=0;$j<=$#SVdata;$j++){
			if ($SVdata[$j][$SVN] == $svID){
				$SVdata[$j][$STILL_VISIBLE]=1;
				$SVdata[$j][$DISAPPEARED]=-1; # reset this timer
				# Debug("Already tracking $svID ($i)");
				last;
			}
		}

		if ($j > $#SVdata && $svID > 0 && $ephAvail){
			Debug("$svID acquired ($j)");
			$SVdata[$j][$SVN]=$svID;
			$SVdata[$j][$APPEARED]=time;
			$SVdata[$j][$LAST_EPHEMERIS_RECEIVED]=-1;
			$SVdata[$j][$LAST_EPHEMERIS_REQUESTED]=-1;
			$SVdata[$j][$STILL_VISIBLE]=1;
			$SVdata[$j][$DISAPPEARED]=-1;
		}
	}
	
	# Now prune satellites which have disappeared
	# This is only done if the satellite has not been
	# visible for some time - satellites can drop in and out of
	# view so we'll try to avoid excessive polling
	for ($i=0;$i<=$#SVdata;$i++){
		if ($SVdata[$i][$STILL_VISIBLE]==0){
			if ($SVdata[$i][$DISAPPEARED]==-1){
				$SVdata[$i][$DISAPPEARED]=time;
				Debug("$SVdata[$i][$SVN] has disappeared");
			}
			elsif (time - $SVdata[$i][$DISAPPEARED] > $REMOVE_SV_THRESHOLD){	
				Debug("$SVdata[$i][$SVN] removed");
				splice @SVdata,$i,1;	
			}	
		}
	}
			
}

# ---------------------------------------------------------------------------
sub ParseGPSSystemDataMessage()
{
	my ($id,$payloadLength,$data) = @_;
	if ($id == 0x31){ # ephemeris
		my ($svn) = unpack("I",$data); 
		if ($payloadLength == 104){
			Debug("Got ephemeris for $svn");
			for ($i=0;$i<=$#SVdata;$i++){
				if ($SVdata[$i][$SVN]==$svn){
					$SVdata[$i][$LAST_EPHEMERIS_RECEIVED] = time;
					last;
				}
			}
		}
		else{
			Debug("No ephemeris for $svn");
		}
	}
	elsif ($id == 0x02 && $payloadLength != 0){ # UTC + ionosphere parameters
		$params[$UTC_IONO_PARAMETERS][$LAST_RECEIVED] = time;
		Debug("UTC/iono parameters received");
	}
}


