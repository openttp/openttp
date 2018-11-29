#!/usr/bin/perl -w

#
# The MIT License (MIT)
#
# Copyright (c) 2016-2018 Michael J. Wouters
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
# 16-02-2017 MJW uucp lock file from config
# 19-02-2018 MJW Added checksumming and basic status info
# 01-05-2018 MJW Native output
# 08-05-2018 MJW Stupid bug in native output
# 16-05-2018 MJW Path fixups for consistency with other scripts
# 02-11-2018 ELM Fix issues with uBlox constellation configuration
# 30-11-2018 ELM Add options for tracking Galileo

use Time::HiRes qw( gettimeofday);
use TFLibrary;
use POSIX;
use Getopt::Std;
use POSIX qw(strftime);
use vars  qw($tmask $opt_c $opt_r $opt_d $opt_h $opt_v);
use Switch;

$VERSION="0.1.3";
$AUTHORS="Michael Wouters, Louis Marais";

$OPENTTP=0;
$NATIVE=1;

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

if (-d "$home/lockStatusCheck"){ # OpenTTP
  $lockPath="$home/lockStatusCheck";
  $statusPath="$home/lockStatusCheck";
}
elsif (-d "$home/logs"){
  $lockPath=$logPath;
  $statusPath=$logPath;
}
else{
  ErrorExit("No ~/lockStatusCheck or ~/logs directory found!\n");
}

if (defined $opt_c){
  $configFile=$opt_c;
}

if (!(-e $configFile)){
  ErrorExit("A configuration file was not found!\n");
}

$ubxmsgs=":";
&Initialise($configFile);


# Check for an existing lock file
$lockFile = TFMakeAbsoluteFilePath($Init{"receiver:lock file"},$home,$logPath);
if (!TFCreateProcessLock($lockFile)){
	ErrorExit("Process is already running\n");
}

# Open the serial port to the receiver
$rxmask="";
$port=$Init{"receiver:port"};
$port="/dev/$port" unless $port=~m#/#;

$uucpLockPath="/var/lock";
if (defined $Init{"paths:uucp lock"}){
		$uucpLockPath = $Init{"paths:uucp lock"};
	}

unless (`/usr/local/bin/lockport -d $uucpLockPath $port $0`==1) {
	printf "! Could not obtain lock on $port. Exiting.\n";
	exit;
}

$rx=&TFConnectSerial($port,
		(ispeed=>0010002,ospeed=>0010002,iflag=>IGNBRK,
			oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|CLOCAL));
			
# Set up a mask which specifies this port for select polling later on
vec($rxmask,fileno $rx,1)=1;
Debug("Port $port to Rx is open");

$rxStatus=$Init{"receiver:status file"};
$rxStatus=&TFMakeAbsoluteFilePath($rxStatus,$home,$logPath);

$Init{"paths:receiver data"}=TFMakeAbsolutePath($Init{"paths:receiver data"},$home);

if (!($Init{"receiver:file extension"} =~ /^\./)){ # do we need a period ?
		$Init{"receiver:file extension"} = ".".$Init{"receiver:file extension"};
}

$fileFormat = $OPENTTP;
if (defined($Init{"receiver:file format"})){
	if ($Init{"receiver:file format"} eq "native"){
		$fileFormat = $NATIVE;
		$Init{"receiver:file extension"}=".ubx";
	}
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
		if ($fileFormat == $OPENTTP){
			printf OUT "# ".$msg;
		}
		else{
			printf "# ".$msg;
		}
		goto BYEBYE;
	}
  
	# Wait for input
	$nfound=select $tmask=$rxmask,undef,undef,0.2;
	next unless $nfound;
	
	if ($nfound<0) {$killed=1; last}
	
	# Read the new input and append it to $input
	sysread $rx,$currinput,$nfound;			
	$input .= $currinput;
	
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
				# TODO ELM Does PollVersionInfo do this? It looks like it does.
				PollVersionInfo();
				PollChipID();
				$next=($mjd-40587+1)*86400;	# seconds at next MJD
			}
			
			if ($now>$then) {
				# Update string version of time stamp
				@_=gmtime $now;
				$nowstr=sprintf "%02d:%02d:%02d",$_[2],$_[1],$_[0];
				$then=$now;
			}
			
			if ( $ubxmsgs =~ /:$classid:/){ # we want this one
				($class,$id)=unpack("CC",$classid);
				#printf "%02x%02x $nowstr %i %i %i\n",$class,$id,$payloadLength,$packetLength,$inputLength;
				if ($fileFormat  == $OPENTTP){
					printf OUT "%02x%02x $nowstr %s\n",$class,$id,(unpack "H*",(substr $data,0,$payloadLength+2));
				}
				if ($class == 0x01 && $id == 0x35){
					if (CheckSumOK($input)){
						UpdateSVInfo($data);
						UpdateStatus();
					}
					else{
						Debug('Bad checksum')
					}
				}
				
				if ($class == 0x01 && $id == 0x22){ # last of periodic messages is the clock solution
					UpdateUTCIonoParameters();
					UpdateGPSEphemeris();
				}
				
				if ($class == 0x0b){
					&ParseGPSSystemDataMessage($id,$payloadLength,$data);
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
	
	if ($fileFormat == $NATIVE){
		print OUT $currinput;
	}
	
}

BYEBYE:
if (-e $lockFile) {unlink $lockFile;}

@_=gmtime();
$msg=sprintf "%02d/%02d/%02d %02d:%02d:%02d $0 killed\n",
  $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
printf $msg;
if ($fileFormat == $OPENTTP){
	printf OUT "# ".$msg;
}
close OUT;

if ($fileFormat == $OPENTTP){
	select STDOUT;
}

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
	
	if ($fileFormat == $OPENTTP){
		select OUT;
	}
	elsif ($fileFormat == $NATIVE){
		binmode OUT;
	}
	
	$|=1;
	
	if ($fileFormat == $OPENTTP){
		printf "# %s $0 (version $VERSION) %s\n",
		&TFTimeStamp(),($_[1]? "beginning" : "continuing");
		printf "# %s file $name\n",
		($old? "Appending to" : "Beginning new");
		printf "\@ MJD=%d\n",$mjd;
		select STDOUT;
	}

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
		Debug("Resetting");
		$msg="\x06\x04\x04\x00\xff\xff\x00\x00";
		SendCommand($msg);
		sleep 5;
	}
	
	Debug("Configuring receiver");
	
	my $observations = 'gps'; # default is GPS
	# Valid combinations for the ublox 8 LEA 8MT are
	# GPS, Galileo
	# GPS, Galileo, GLONASS
	# GPS, Galileo, Beidou
	# GPS, GLONASS
	# GPS, Beidou
	# Galileo, GLONASS
	# Galileo, BeiDou
	# GLONASS, BeiDou
	# SBAS, QZSS can only be enabled with GPS enabled 
	
	# The following are supported (all tested, support for Galileo added 2018-11-30):
	# GPS
	# GPS, GLONASS
	# GPS, Beidou
	# GLONASS
	# GLONASS, Beidou
	# Beidou
	# GPS, Galileo
	# GPS, Galileo, GLONASS
	# GPS, Galileo, Beidou
	# GLONASS, Galileo
	# Beidou, Galileo
	# Galileo
	
	# 2018-11-02 Notes: (ELM, after much testing and fixing some bugs)
	# 1. If invalid config is sent, the default config is loaded, that is 
	#    GPS (8 - 16 channels), QZSS (0 - 3 channels), GLONASS (8 - 14 channels)
	# 2. Even if a valid config is sent, there has to be enough channels available
	#    for it to execute. With the default config there is only 16 channels 
	#    available, so trying to enable GPS and Beidou with min = 16 channels each
	#    does not work. I changed the min to 8 below. Remember there are 72 
	#    channels, but only 32 is for user tracking.
	# 3. A valid config message will not be accepted if there are not enough 
	#    channels available. It's best to send a complete config message, i.e.
	#    include ALL constellations, disabling the ones you do not want/need.
	# 4. This has now been tested with all valid combinations currently supported.
	#    It should be easy to update the code below to enable other constellations,
	#    just keep in mind that some combinations are not valid. Refer to the table
	#    under the default for observations above.
	
	if (defined($Init{"receiver:observations"})){
		$observations=lc $Init{"receiver:observations"};
		
		# WARNING! Only a few (useful) combinations are supported here (no SBAS, IMES or QZSS)
		my $ngnss = 0; # number of GNSS systems enabled
		my $enabled = ""; # string to hold gnss systems to enable. This prevents invalid combinations.
		# Calculate the number of GNSS systems to enable
		if ($observations =~ /gps/){ # GPS combos
			$ngnss=1;
			$enabled .= "gps ";
			if ($observations =~ /galileo/){
				$ngnss++;
				$enabled .= "galileo ";
				if($observations =~ /glonass/){
					$ngnss++;
					$enabled .= "glonass ";
				} 
				elsif ($observations =~ /beidou/){
					$ngnss++;
					$enabled .= "beidou ";
				}
			}
			elsif ($observations =~ /glonass/){
				$ngnss++;
				$enabled .= "glonass ";
			}
			elsif ($observations =~ /beidou/){
				$ngnss++;
				$enabled .= "beidou ";
			}
		}
		elsif ($observations =~ /glonass/){ # GLONASS combos 
			$ngnss=1;
			$enabled .= "glonass ";
			if ($observations =~ /beidou/){
				$ngnss++;
				$enabled .= "beidou ";
			} 
			elsif ($observations =~ /galileo/){
				$ngnss++;
				$enabled .= "galileo ";
			}
		}
		elsif ($observations =~ /beidou/){ # Beidou combos
			$ngnss=1;
			$enabled .= "beidou ";
			if ($observations =~ /galileo/){
				$ngnss++;
				$enabled .= "galileo ";
			}
		}
		elsif($observations =~ /galileo/){
			$ngnss++;
			$enabled .= "galileo ";
		}
		if ($ngnss == 0){
			ErrorExit("Problem with receiver:observations: no valid GNSS systems were found");
		}
		
		$msgSize=pack('v',4+8*7); # little-endian, 16 bits unsigned
		$numConfigBlocks=pack('C',7);
		
		my $cfg = "\x06\x3e$msgSize\x00\xff\xff$numConfigBlocks";
		my $en = 0;
		# GPS
		if($enabled =~ /gps/) { $en = 1; } else { $en = 0; }
		$cfg .= ConfigGNSS('gps',8,16,$en);
		# SBAS
		$cfg .= ConfigGNSS('sbas',1,3,0);
		# Galileo
		if($enabled =~ /galileo/) { $en = 1; } else { $en = 0; }
		$cfg .= ConfigGNSS('galileo',4,10,$en); # ...,4,8,0); <-- default in u-center
		                                        # Very important: The max number of channels that
		                                        # can be allocated to Galileo is 10 - see page 
		                                        # 13 of 394 in u-blox M 8 receiver description 
		                                        # manual (part no: UBX-13003221 - R16). The pain
		                                        # is that the receiver will allow you to configure 
		                                        # more channels but then will NOT allow you to
		                                        # enable Galileo. It took a few DAYS to find this
		                                        # little gem!
		# Beidou
		if($enabled =~ /beidou/) { $en = 1; } else { $en = 0; }
		$cfg .= ConfigGNSS('beidou',8,24,$en);	
		# IMES
		$cfg .= ConfigGNSS('imes',0,8,0);
		# QZSS
		$cfg .= ConfigGNSS('qzss',0,3,0);
		# GLONASS
		if ($enabled =~ /glonass/) { $en = 1 } else { $en = 0; }
		$cfg .= ConfigGNSS('glonass',12,16,$en);
		
		SendCommand($cfg);
	
		# Cold start is recommended after GNSS reconfiguration
		# This doesn't scrub the GNSS selection
		Debug("Restarting after GNSS reconfiguration");
		$msg="\x06\x04\x04\x00\xff\xff\x00\x00";
		SendCommand($msg);
		
		close $rx;
		
		sleep 30;
		
		# We get a disconnect on reset so have have to close the serial port
		# and reopen it
	
		Debug("Opening again");
		$rx=&TFConnectSerial($port, (ispeed=>0010002,ospeed=>0010002,iflag=>IGNBRK,
			oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|CLOCAL));
		
		# No need to update the locks
		
  }
  
  Debug("Configuring");
  SendCommand('\x06\x3e\x00\x00'); # Get the new configuration
  
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
	
	Debug("Done configuring");
	
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
	# Flags whether the list was updated
	#
	my $data=shift;
	my $numSVs=(length($data)-8-2)/12;
	my $i;
	my ($cno,$azim,$prRes,$nowstr,$elev);
	my $satsUpdated = 0;
	
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
			$satsUpdated=1;
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
				$satsUpdated=1;
			}
			elsif (time - $SVdata[$i][$DISAPPEARED] > $REMOVE_SV_THRESHOLD){	
				Debug("$SVdata[$i][$SVN] removed");
				splice @SVdata,$i,1;
				$satsUpdated=1;
			}	
		}
	}
	
	return $satsUpdated;
}

# ---------------------------------------------------------------------------
sub UpdateStatus
{
	my $nvis=$#SVdata+1;
	open(STA,">$rxStatus");
	print STA "sats=$nvis\n";
	print STA "prns=";
	for (my $i=0;$i<=$#SVdata-1;$i++){
		print STA $SVdata[$i][$SVN],",";
	}
	if ($nvis > 0) {print STA $SVdata[$#SVdata][$SVN];}
		print STA "\n";
	close STA;
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

# ------------------------------------------------------------------------------
sub EnableGNSS
{
	my ($gnss,$resTrkCh,$maxTrkCh) = @_;
	$resTrkCh = pack('C',$resTrkCh);
	$maxTrkCh = pack('C',$maxTrkCh);
	my ($gnssID,$flags);
	if ($gnss eq 'gps'){
		$gnssID = pack('C',0);
		$flags = pack('V',0x01 | 0x010000); # little endian, unsigned 32 bits
	}
	elsif ($gnss eq 'beidou'){
		$gnssID = pack('C',3);
		$flags = pack('V',0x01 | 0x010000);
	}
	elsif ($gnss eq 'glonass'){
		$gnssID = pack('C',6);
		$flags = pack('V',0x01 | 0x010000);
	}
	return "$gnssID$resTrkCh$maxTrkCh\x00$flags";
}

# ------------------------------------------------------------------------------
sub ConfigGNSS
{
	my ($gnss,$resTrkCh,$maxTrkCh,$en) = @_;
	$resTrkCh = pack('C',$resTrkCh);
	$maxTrkCh = pack('C',$maxTrkCh);
	my ($gnssID,$flags);
	switch($gnss){
		case 'gps'     { $gnssID = 0; }
		case 'sbas'    { $gnssID = 1; }
		case 'galileo' { $gnssID = 2; }
		case 'beidou'  { $gnssID = 3; }
		case 'imes'    { $gnssID = 4; }
		case 'qzss'    { $gnssID = 5; }
		case 'glonass' { $gnssID = 6; }
		else { ErrorExit("Invalid gnss system ($gnss) sent to ConfigGNSS routine."); }
	}
	$gnssID = pack('C',$gnssID);
	if($en == 1){
		$flags = pack('V',0x01 | 0x010000); # little endian, unsigned 32 bits
	} else {
		$flags = pack('V',0x00 | 0x010000); # little endian, unsigned 32 bits
	}
	return "$gnssID$resTrkCh$maxTrkCh\x00$flags";
}

# ------------------------------------------------------------------------------
# Verify the checksum
# Note that a full message, including the UBX header, is assumed
#
sub CheckSumOK
{
	my $str = $_[0];
	my @msg=split "",$str; # convert string to array
	my $cka=0;
	my $ckb=0;
	my $l = length($str);
	for (my $i=2;$i<$l-2;$i++){ # skip the two bytes of the header (start) and the checksum (end) 
		$cka = $cka + ord($msg[$i]);
		$cka = $cka & 0xff;
		$ckb = $ckb + $cka;
		$ckb = $ckb & 0xff;
	}
	return (($cka == ord($msg[$l-2])) && ($ckb == ord($msg[$l-1])));
}
