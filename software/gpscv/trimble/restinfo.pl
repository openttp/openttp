#!/usr/bin/perl -w

#
# The MIT License (MIT)
#
# Copyright (c) 2015  R. Bruce Warrington, Michael J. Wouters
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

#restinfo - Perl script to query Trimble receivers for useful information

# Modification history:
# 2015-08-25 MJW First version
# 2017-02-24 MJW Path tidyups for the configuration file
#                

use Time::HiRes qw( gettimeofday);
use TFLibrary;
use POSIX;
use Getopt::Std;
use POSIX qw(strftime);
use vars  qw($tmask $opt_c $opt_d $opt_h $opt_v);

$AUTHORS="Michael Wouters";
$VERSION="0.2";

$RESOLUTION_T=0;
#$RESOLUTION_SMT=1;
$RESOLUTION_SMT_360=2;

$0=~s#.*/##;

$home=$ENV{HOME};

if (-d "$home/etc"){
	$configPath="$home/etc";
}
elsif (-d "$home/Parameter_Files"){
	$configPath="$home/Parameter_Files";
}
else{
	ErrorExit("No $configPath directory found!\n");
}

if ((-d "$home/logs")){
	$logPath="$home/logs";
}
elsif (-d "$home/Log_files"){
	$logPath = "$home/Log_Files";
}
else{
	ErrorExit("No ~/logs or ~/Log_Files directory found!\n");
}

if (-e "$configPath/rest.conf"){ # this takes precedence
	$configFile=$configPath."/rest.conf";
	$localMajorVersion=2;
}
elsif (-e "$configPath/gpscv.conf"){
	$configFile=$configPath."/gpscv.conf";
	$localMajorVersion=2;
}
elsif (-e "$configPath/cctf.setup"){
	$configFile="$configPath/cctf.setup";
	$localMajorVersion=1;
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

&Initialise($configFile);

# Check for an existing lock file
# Check the lock file
$lockFile = TFMakeAbsoluteFilePath($Init{"receiver:lock file"},$home,$logPath);
if (-e $lockFile){
	open(LCK,"<$lockFile");
	@info = split ' ', <LCK>;
	close LCK;
	if (-e "/proc/$info[1]"){
		printf STDERR "Process $info[1] already running\n";
		exit;
	}
	else{
		open(LCK,">$lockFile");
		print LCK "$0 $$\n";
		close LCK;
	}
}
else{
	open(LCK,">$lockFile");
	print LCK "$0 $$\n";
	close LCK;
}

$rxModel=$RESOLUTION_SMT_360;
if ($localMajorVersion == 2){
	if ($Init{"receiver:model"} eq "Resolution T"){
		$rxModel=$RESOLUTION_T;
	}
#	elsif ($Init{"receiver:model"} eq "Resolution SMT"){
#		$rxmodel=$RESOLUTION_SMT;
#	}
	elsif ($Init{"receiver:model"} eq "Resolution SMT 360"){
		$rxModel=$RESOLUTION_SMT_360;
	}
	else{
		print "Unknown receiver model: ".$Init{"receiver:model"}." - assuming SMT 360\n";
	}
}

#Open the serial ports to the receiver
$rxmask="";
$port=&GetConfig($localMajorVersion,"gps port","receiver:port");
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

$now=time();

$killed=0;
$SIG{TERM}=sub {$killed=1};

$receiverTimeout=600;
$configReceiverTimeout=&GetConfig($localMajorVersion,"receiver timeout","receiver:timeout");
if (defined($configReceiverTimeout)){$receiverTimeout=$configReceiverTimeout;}

if ($rxModel == $RESOLUTION_SMT_360){
	GetHardwareVersion();
	GetFirmwareVersion();
}

GetSoftwareVersion();

GetIOOptions();

GetPrimaryConfiguration();

if (-e $lockFile) {unlink $lockFile;}
# end of main 


#----------------------------------------------------------------------------
sub ShowHelp
{
	print "Usage: $0 [OPTIONS ...]\n";
	print "  -c <file> use the specified configuration file\n";
	print "  -d debug\n";
	print "  -h show this help\n";
	print "  -v show version\n";
	print "  The default initialisation file is $configFile\n";
}

#----------------------------------------------------------------------------
sub Initialise 
{
  my $name=shift;
  if ($localMajorVersion == 1){
		my @required=("gps port");
		%Init=&TFMakeHash($name,(tolower=>1));
	}
	elsif($localMajorVersion == 2){
		@required=("receiver:port","receiver:status file");
		%Init=&TFMakeHash2($name,(tolower=>1));
	}

  if (!%Init){
	print "Couldn't open $name\n";
	exit;
  }

  my ($tag,$err);
  
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
sub GetConfig()
{
	my($ver,$v1tag,$v2tag)=@_;
	if ($ver == 1){
		return $Init{$v1tag};
	}
	elsif ($ver==2){
		return $Init{$v2tag};
	}
}

#----------------------------------------------------------------------------
sub FixPath()
{
	my $path=$_[0];
	if (!($path=~/^\//)){
		$path =$ENV{HOME}."/".$path;
	}
	return $path;
}

#-----------------------------------------------------------------------------
sub Debug
{
	$now = strftime "%H:%M:%S",gmtime;
	if ($opt_d) {print "$now $_[0] \n";}
}

#----------------------------------------------------------------------------
sub SendCommand
{
  my $cmd=shift;
  $cmd=~s/\x10/\x10\x10/g;# 'stuff' (double) DLE in data
  print $rx "\x10".$cmd."\x10\x03";	# DLE at start, DLE/ETX at end
} # SendCommand

#----------------------------------------------------------------------------
sub GetReport
{
	my ($cmd,$packetid,$subpacketid) = @_;
	my $lastMsg=time(); 
	my $input="";
	my $save="";
	my $waiting=1;
	my $data="";
	my $dle;
	my $nfound;
	
	LOOP: while (!$killed && $waiting)
	{
		
		if (time()-$lastMsg > $receiverTimeout){ # too long, so byebye
			print "no response from receiver\n";
			if (-e $lockFile) {unlink $lockFile;}
			exit;
		}
		
		SendCommand($cmd);
		
		# See if there is text waiting
		$nfound=select $tmask=$rxmask,undef,undef,0.2;
		next unless $nfound;
		if ($nfound<0) {$killed=1; last}
		# read what's waiting, and add it on the end of $input
		sysread $rx,$input,$nfound,length $input;
		# look for a message in what we've accumulated
		# $`=pre-match string, $&=match string, $'=post-match string (Camel book p128)
		if ($input=~/(\x10+)\x03/){
			if ((length $1) & 1){
				# ETX preceded by odd number of DLE: got the packet termination
				
				$dle = substr $&,1,-1; # drop the last DLE
				$data=$save.$`.$dle;
				
				$first=ord substr $data,0,1;
				if ($first==0x10){
					$lastMsg=time();
					$data=substr $data,1;		  # drop leading DLE
					$data=~s/\x10{2}/\x10/g;	# un-stuff doubled DLE in data
					# @bytes=unpack "C*",$data;
					
					# It's possible that we might ask for system data when there is none
					# available so filter out empty responses
					$id=ord substr $data,0,1;
					#	
					if ($id == $packetid ){ 
						Debug(sprintf("Got %02x",$packetid));
						if ($subpacketid != 0x0){
							$operation = ord substr $data,1,1;
							Debug(sprintf("Got subpacket %02x",$operation));
							if ($operation == $subpacketid){
								return $data;

							}
						}
						else{
							return $data;
						}
						
					}
				}
				$save="";
			}
			else{
				# ETX preceded by even number of DLE: DLEs "stuffed", this is packet data
				# Remove from $input for next search, but save it for later
				$save=$save.$`.$&;
			}
			$input=$';	
		}  
	}
}

#----------------------------------------------------------------------------
sub GetHardwareVersion
{
	print "\n+++ Hardware version (0x1C-03)\n";
	my $data = GetReport("\x1C\x03",0x1C,0x83);
	
	&ReverseByteOrder(2,5,$data);
	my $serialNumber = unpack('I',substr $data,2,5);
	print "Serial no: $serialNumber\n";
	
	my $day=unpack('C',substr $data,6,6);
	my $mon=unpack('C',substr $data,7,7);
	ReverseByteOrder(8,9,$data);
	my $year=unpack('S',substr $data,8,9);
	my $hr=unpack('C',substr $data,10,10);
	print "Build time: $year-$mon-$day $hr:00\n";
	
	ReverseByteOrder(11,12,$data);
	$hwCode=unpack('S',substr $data,11,12);
	print "Hardware code: $hwCode\n";
}

#----------------------------------------------------------------------------
sub GetFirmwareVersion
{
	print "\n+++ Firmware version (0x1C-01)\n";
	my $data = GetReport("\x1C\x01",0x1C,0x81);

	my $majorVersion = unpack('C',substr $data,3,3);
	my $minorVersion = unpack('C',substr $data, 4,4);
	print "Version: $majorVersion.$minorVersion\n";

	my $buildNumber = unpack('C',substr $data,5,5);
	print "Build number: $buildNumber\n";

	my $mon=unpack('C',substr $data,6,6);
	my $day=unpack('C',substr $data,7,7);
	&ReverseByteOrder(8,9,$data);
	my $year=unpack('S',substr $data,8,9);
	print "Build date: $year-$mon-$day\n";

	my $L1 = unpack('C',substr $data,10,10);
	my $productName=substr $data,11,10+$L1;
	print "Product name: $productName\n";
}

#----------------------------------------------------------------------------
sub GetSoftwareVersion
{
	my $yearOffset=2000;
	if ($rxModel == $RESOLUTION_T){
		$yearOffset = 1900;
	}

	print "\n+++ Software version (0x1F)\n";
	my $data = GetReport("\x1F",0x45,0x00);

	my $majorVersion = unpack('C',substr $data,1,1);
	my $minorVersion = unpack('C',substr $data, 2,2);
	print "Application version: $majorVersion.$minorVersion\n";

	my $mon=unpack('C',substr $data,3,3);
	my $day=unpack('C',substr $data,4,4);
	my $year=unpack('C',substr $data,5,5);
	print "Application build date: ".($year+$yearOffset)."-$mon-$day\n";

	$majorVersion = unpack('C',substr $data,6,6);
	$minorVersion = unpack('C',substr $data, 7,7);
	print "GPS core version: $majorVersion.$minorVersion\n";

	$mon=unpack('C',substr $data,8,8);
	$day=unpack('C',substr $data,9,9);
	$year=unpack('C',substr $data,10,10);
	print "Application build date: ".($year+$yearOffset)."-$mon-$day\n";
	
}

#----------------------------------------------------------------------------
sub GetIOOptions
{
	
	my $data = GetReport("\x35",0x55,0x00);
	
	print "\n+++ I/O options (0x35)\n";
	
	my $pos=unpack('C',substr $data,1,1);
	print "Position: \n";
	print "\tECEF ".($pos & 0x01 ?"on":"off")."\n";
	print "\tLLA  ".($pos & 0x02 ?"on":"off")."\n";
	print "\tDatum ".($pos & 0x04 ?"HAE":"MSL geoid")."\n";
	print "\tPrecision ".($pos & 0x10 ?"single":"double")."\n";
	
	my $velocity=unpack('C',substr $data,2,2);
	print "Velocity: \n";
	print "\tECEF ".($velocity & 0x01 ?"on":"off")."\n";
	print "\tENU ".($velocity & 0x02 ?"on":"off")."\n";
	
	my $timing=unpack('C',substr $data,3,3);
	print "Timing: \n";
	print "\tReference ".($timing & 0x01 ?"UTC":"GPS")."\n";
	
	my $aux=unpack('C',substr $data,4,4);
	print "Auxiliary: \n";
	print "\t5A packet ".($aux & 0x01 ?"on":"off")."\n";
	
}

#----------------------------------------------------------------------------
sub GetPrimaryConfiguration
{
	my @modes=("automatic","single satellite","","horizontal","full position","DGPS reference",
		"2D clock hold","over determined clock");
	my @dynamics=("","land","sea","air","stationary");
	my @foliage =("never","sometimes","always");
	
	my $data = GetReport("\xBB\x00",0xBB,0x00);
	
	print "\n+++ Primary configuration (0xBB)\n";
	
	my $rxMode=unpack('C',substr $data,2,2);
	print "Receiver mode: ".$modes[$rxMode]." [$rxMode]\n";
	
	&ReverseByteOrder(6,9,$data);
	my $elevMask = unpack('f',substr $data,6,9);
	printf "Elevation mask: %g deg\n",$elevMask*180.0/3.1415927;
	
	&ReverseByteOrder(10,13,$data);
	my $CNMask = unpack('f',substr $data,6,9);
	printf "C/N mask: %g\n",$CNMask;
	
	if ($rxModel == $RESOLUTION_T){
		my $dynamicsCode= unpack('C',substr $data,4,4);
		print "Dynamics mode: ".$dynamics[$dynamicsCode]." [$dynamicsCode]\n";
		
		&ReverseByteOrder(14,17,$data);
		my $PDOPMask = unpack('f',substr $data,14,17);
		printf "PDOP mask: %g\n",$PDOPMask;
	
		&ReverseByteOrder(18,21,$data);
		my $PDOPSwitch = unpack('f',substr $data,18,21);
		printf "PDOP switch: %g\n",$PDOPSwitch;
		
		my $foliageMode=unpack('C',substr $data,23,23);
		print "Foliage mode: ".$foliage[$foliageMode]." [$foliageMode]\n";
	}
	elsif ($rxModel == $RESOLUTION_SMT_360){
		my $jamMode=unpack('C',substr $data,23,23);
		print "Anti-jam mode: $jamMode\n";
		
		my $constellation=unpack('C',substr $data,28,28);
		print "Constellations: ";
		if ($constellation & 0x01){print "GPS ";}
		if ($constellation & 0x02){print "GLONASS ";}
		if ($constellation & 0x08){print "Beidou ";}
		if ($constellation & 0x10){print "Galileo ";}
		if ($constellation & 0x20){print "QZSS ";}
		print "\n";	
	}
}

#----------------------------------------------------------------------------

sub ReverseByteOrder
{
	my $start = $_[0];
	my $stop =  $_[1];
	my $swaps= ($stop-$start+1)/2;
	for ($i=0;$i<$swaps;$i++){	
		$indx0=$start+$i;# save the first byte
		$tmp0=substr $_[2], $indx0,1;
		$indx1=$stop-$i;
		substr ($_[2],$indx0,1) = substr ($_[2],$indx1,1);
		substr ($_[2],$indx1,1) = $tmp0;
	}
}
