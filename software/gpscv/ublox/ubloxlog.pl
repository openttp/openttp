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
#

use Time::HiRes qw( gettimeofday);
use TFLibrary;
use POSIX;
use Getopt::Std;
use POSIX qw(strftime);
use vars  qw($tmask $opt_c $opt_r $opt_d $opt_h $opt_v);

$VERSION="0.1";
$AUTHORS="Michael Wouters";

$0=~s#.*/##;

$home=$ENV{HOME};
$configFile="$home/etc/gpscv.ublox.conf"; # FIXME temporary

if( !(getopts('c:dhrv')) || ($#ARGV>=1) || $opt_h) {
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

if (-d "$home/logs")  {
	$logPath="$home/logs";
} 
else{
	ErrorExit("No ~/logs directory found!\n");
}

if (defined $opt_c){
	$configFile=$opt_c;
}

if (!(-e $configFile)){
	ErrorExit("A configuration file was not found!\n");
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

$Init{version}=$VERSION;

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
$save="";
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
  
	# see if there is text waiting
	$nfound=select $tmask=$rxmask,undef,undef,0.2;
	next unless $nfound;
	if ($nfound<0) {$killed=1; last}
	# read what's waiting, and append it to $input
	sysread $rx,$input,$nfound,length $input;
	# look for a message in what we've accumulated
	# $`=pre-match string, $&=match string, $'=post-match string (Camel book p128)

	
	if ($input=~/(\$G\w{4})(.+\*[A-F_0-9]{2})\r\n/){ # grab NMEA
		$input=$'; # save the dangly bits
		if ($1 eq '$GNZDA') {
			print "$1 $2\n";
		}
		else{
			print $1,"\n";
		}
	}
	# Header structure for UBX packets is 
	# Sync char 1 | Sync char 2| Class (1 byte) | ID (1 byte) | payload length (2 bytes) | payload | cksum_a | cksum_b
	
	if ($input=~/\xb5\x62(.{4})/){ # if we've got a UBX header
		# UBX fields are little endian 
		($class,$id,$payloadLength) = unpack("CCv",$1);
		# have we got the lot ?
		$packetLength = $payloadLength + 8;
		$inputLength=length($input);
		if ($packetLength <= $inputLength){ # it's all there ! yay !
			printf "%02x %02x %i %i\n",$class,$id,$packetLength,$inputLength;
			if ($packetLength == $inputLength){
				$input="";
			}
			else{
				#printf "%i %i %i\n",$inputLength,length($'),$payloadLength+2;
				$input=substr $input,6+$payloadLength+2;
				#printf "%i %i %i\n",length($input),length($'),$payloadLength+2;
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
	printf "# %s $0 (version $Init{version}) %s\n",
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
  print $rx $cmd.$cksum;	
} # SendCommand

#----------------------------------------------------------------------------
sub ConfigureReceiver
{

	if ($opt_r){ # hard reset
		
	}
	# Navigation/measurement rate settings
	$msg = "\xb5\x62\x06\x08\x06\x00\xe8\x03\x01\x00\x01\x00"; # CFG_RATE											---- 
	SendCommand($msg);
	
} # ConfigureReceiver


