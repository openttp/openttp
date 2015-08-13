#!/usr/bin/perl -w

#restlog - Perl script to configure Trimble Resolution T GPS Rx and download data

# Modification history:
#  2. 8.01    RBW  Begun for Trimble ACE UTC
#  07-06-2005 MJW  Resumed. Modification of the script used for the ACE UTC
#  14-10-2005 MJW Many changes over the past month to allow polling of the
# 								 ephemeris etc. Still tuning this up.
#  07-06-2007 MJW Enable UTC time in 8FAB packet for use by ntpd
#  08-07-2007 MJW	1 pps offset now set on start up
#
#  18-12-2008 MJW Write receiver config to flash on startup
#  19-02-2009 MJW Timeout if no messages received 
#                 PRNS in rcvr_status
#                 Bug fix. rcvr_status message empty of there are no 
#													 satellites
#  26-02-2009 MJW Added lockfile for interoperation with lcdmonitor
#  20-08-2009 MJW Version info output
#  20-04-2010 MJW Extra status information in rx.status
#  14-02-2012 MJW Woho ! Real bug fix. Any trailing 0x10's in a data 
#									packet were erroneously removed
#									

# Improvements?
# Use 6D for tracking visible satellites but this is not strictly correct
# since this reports satelites used for position/time fix NOT visible
# satellites
#
# Still to do:
# Disable packet output at exit
# Elevation mask

use Time::HiRes qw( gettimeofday);
use TFLibrary;
use POSIX;
use Getopt::Std;
use POSIX qw(strftime);
use vars  qw($tmask $opt_r);

$DEBUG=0;

$REMOVE_SV_THRESHOLD=600; #interval after which  a SV marked as disappeared is removed
$COLDSTART_HOLDOFF = 0;

# Track each SV so that we know when to ask for an ephemeris
# Record PRN
# Record time SV appeared (Unix time)
# Record time we last got an ephemeris (Unix time)
# Flag for marking whether still visible

$EPHEMERIS_POLL_INTERVAL=7000; # a bit less than the 7200 s threshold for
															 # ephemeris validity as used in read_rinex_sv

$PRN=0; # for indexing into the data array
$APPEARED=1;
$LAST_EPHEMERIS_RECEIVED=2;
$LAST_EPHEMERIS_REQUESTED=5;
$STILLVISIBLE=3;
$DISAPPEARED=4;

# Need to track some parameters too
$UTC_POLL_INTERVAL=14400;
$IONO_POLL_INTERVAL=14400;

$UTC_PARAMETERS=0;
$IONO_PARAMETERS=1;

$LAST_REQUESTED=0;
$LAST_RECEIVED=1;

# Some filthy globals

$rxTemperature="unknown";
$rxMinorAlarms="";

$0=~s#.*/##;

$home=$ENV{HOME};
if (-d "$home/etc")  {$configpath="$home/etc";}  else 
	{$configpath="$home/Parameter_Files";} # backwards compatibility

if (-d "$home/logs")  {$logpath="$home/logs";} else 
	{$logpath="$home/Log_Files";}
	
# More backwards compatibility fixups
if (-e "$configpath/cggtts.conf"){
	$configFile=$configpath."/cggtts.conf";
	$localMajorVersion=2;
}
elsif (-e "$configpath/cctf.setup"){
	$configFile="$configpath/cctf.setup";
	$localMajorVersion=1;
}
else{
	print STDERR "No configuration file found!\n";
	exit;
}

if(!(getopts('r')) || !(@ARGV<=2)) {
  select STDERR;
  print "Usage: $0 [-r] [initfile]\n";
	print "  -r reset receiver on startup\n";
  print "  The default initialisation file is $configFile\n";
  exit;
}

# Check for an existing lock file
$lockfile = $logpath."/rx.lock";
if (-e $lockfile){
	open(LCK,"<$lockfile");
	$pid = <LCK>;
	chomp $pid;
	if (-e "/proc/$pid"){
		printf STDERR "Process $pid already running\n";
		exit;
	}
	close LCK;
}

open(LCK,">$lockfile");
print LCK $$,"\n";
close LCK;

&Initialise(@ARGV==1? $ARGV[0] : $configFile);
$Init{version}="2.0";

#Open the serial ports to the receiver
$rxmask="";
$port=&GetConfig($localMajorVersion,"gps port","receiver:port");
$port="/dev/$port" unless $port=~m#/#;

unless (`/usr/local/bin/lockport $port $0`==1) {
	printf "! Could not obtain lock on $port. Exiting.\n";
	exit;
}

#$rx=&TFConnectSerial($port,
#    (ispeed=>0010002,ospeed=>0010002,iflag=>IGNBRK,
#     oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|PARENB|PARODD|CLOCAL));

$rx=&TFConnectSerial($port,
		(ispeed=>0010002,ospeed=>0010002,iflag=>IGNBRK,
			oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|CLOCAL));
# Set up a mask which specifies this port for select polling later on
vec($rxmask,fileno $rx,1)=1;
print "> Port $port to Rx is open\n";

#Initialise parameters
$params[$UTC_PARAMETERS][$LAST_REQUESTED]=-1;
$params[$UTC_PARAMETERS][$LAST_RECEIVED]=-1;

$params[$IONO_PARAMETERS][$LAST_REQUESTED]=-1;
$params[$IONO_PARAMETERS][$LAST_RECEIVED]=-1;

$rcvrstatus=&GetConfig($localMajorVersion,"receiver status","receiver:status");
$rcvrstatus=&FixPath($rcvrstatus);

$now=time();
$mjd=int($now/86400) + 40587;	# don't call &TFMJD(), for speed
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
$configReceiverTimeout=&GetConfig($localMajorVersion,"receiver timeout","receiver:timeout");
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
      if ($first!=0x10){
        printf STDERR "! Parse error - bad packet start char 0x%02X\n",$first;
      }
      else{
			
        $now=time();			# got one - tag the time
				$lastMsg=$now;
        if ($now>=$next){
          # (this way is safer than just incrementing $mjd)
          $mjd=int($now/86400) + 40587;	# don't call &TFMJD(), for speed
          &OpenDataFile($mjd,0);
					# Request receiver and software versions
					&SendCommand("\x8E\x41");
					&SendCommand("\x8E\x42");
					&SendCommand("\x1F");
          $next=($mjd-40587+1)*86400;	# seconds at next MJD
        }
        if ($now>$then){
          # update string version of time stamp
          @_=gmtime $now;
          $nowstr=sprintf "%02d:%02d:%02d",$_[2],$_[1],$_[0];
          $then=$now;
        }
				$data=substr $data,1;		  # drop leading DLE
      	$data=~s/\x10{2}/\x10/g;	# un-stuff doubled DLE in data
    	  # @bytes=unpack "C*",$data;
				
				# It's possible that we might ask for system data when there is none
				# available so filter out empty responses
				$id=ord substr $data,0,1;
				$operation = ord substr $data,1,1;	
				if ($id == 0x58 ){ # is it system data ?
					if ($operation == 0x02){
     				printf OUT "%02X $nowstr %s\n",(ord substr $data,0,1),(unpack "H*",$data);
					}
				}
				elsif ($id == 0x8f && $operation == 0xac){
					&ParseSupplementalTimingPacket();
					printf OUT "%02X $nowstr %s\n",(ord substr $data,0,1),(unpack "H*",$data);
				}
				else{
					printf OUT "%02X $nowstr %s\n",(ord substr $data,0,1),(unpack "H*",$data);
				}
				
				UpdateSatellites();
				PollEphemerides();
				PollUTCParameters();
				PollIonoParameters();
				ParseGPSSystemDataMessage();
				
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

BYEBYE:
if (-e $lockfile) {unlink $lockfile;}

@_=gmtime();
$msg=sprintf "%02d/%02d/%02d %02d:%02d:%02d $0 killed\n",
  $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
printf $msg;
printf OUT "# ".$msg;
close OUT;

# end of main 

#----------------------------------------------------------------------------
sub Initialise 
{
  my $name=shift;
  if ($localMajorVersion == 1){
		my @required=("gps port",  "data path", "gps data extension","receiver status","ppsOffset");
		%Init=&TFMakeHash($name,(tolower=>1));
	}
	elsif($localMajorVersion == 2){
		@required=( "paths:receiver data","receiver:file extension","receiver:port","receiver:status file","receiver:pps offset");
		%Init=&TFMakeHash2($name,(tolower=>1));
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

	if ($localMajorVersion == 1){
		$Init{"data path"}=FixPath($Init{"data path"});
		$Init{"receiver status"}=FixPath{"receiver status"};
	}
	elsif ($localMajorVersion == 2){
		$Init{"paths:receiver data"}=FixPath($Init{"paths:receiver data"});
		$Init{"receiver:status"}=FixPath($Init{"receiver:status file"});
	}
 
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
	if ($DEBUG) {print "$now $_[0] \n";}
}

#-----------------------------------------------------------------------------
sub OpenDataFile
{
  my $mjd=$_[0];
  my $dataPath= &GetConfig($localMajorVersion,"data path","paths:receiver data");
  my $dataExt= &GetConfig($localMajorVersion,"gps data extension","receiver:file extension");
  my $name=$dataPath."/".$mjd.$dataExt;
  
  my $old=(-e $name);

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

sub SendCommand
{
  my $cmd=shift;
  $cmd=~s/\x10/\x10\x10/g;		# 'stuff' (double) DLE in data
  print $rx "\x10".$cmd."\x10\x03";	# DLE at start, DLE/ETX at end
} # SendCommand

#----------------------------------------------------------------------------

sub ConfigureReceiver
{

	if ($opt_r){ # hard reset
		print OUT "# Resetting receiver\n";	
  	&SendCommand("\x1E\x4b");
		sleep 3; # wait a bit
	}
	
	# Turn on broadcast of timing information and automatic output packets
	&SendCommand("\x8E\xA5\x00\x45\x00\x05");
	#  Set up for GPS time 
	#  Turn on raw measurement reports nb automatic output packets must be
	#  turned on as well
	&SendCommand("\x35\x12\x02\x00\x01");
	# Configure for GPS aligned 1 pps and UTC time-of day in 8FAB packet
	&SendCommand("\x8E\xA2\x01");
	# Set the 1 pps offset
	
	$ppsOffset = &GetConfig($localMajorVersion,"ppsoffset","receiver:pps offset")/1.0E9; # convert to seconds
	$pps=DoubleToStr($ppsOffset);
	$biasUncertaintyThreshold ="\x43\x96\x00\x00";
	&SendCommand("\x8E\x4a\x01\x00\x00".$pps.$biasUncertaintyThreshold);
	# Request receiver information
	
	# Wait a bit ....
	
	# Save configuration
	&SendCommand("\x8E\x26");
	# This will cause a reset
	sleep 3;

	&SendCommand("\x8E\x41");
	&SendCommand("\x8E\x42");
	&SendCommand("\x1F");
} # ConfigureReceiver

#----------------------------------------------------------------------------

sub UpdateSatellites
{
	my ($id,$i,$n);
	# Visible satellites can be obtained from 0x6D report packet, which is
	# automatically output
	$id=ord substr $data,0,1;
	if ($id == 0x6d){
		
		# $nfix = (ord substr $data,1,1) >> 4;
		# Satellite PRNs begin at position 18
		$n=length $data;
		if ($n > 18){
			# mark all SVs as not visible so that non-visible SVs can be pruned
			#print "$nowstr Tracking [ ";
			for ($i=0;$i<=$#SVdata;$i++){
				$SVdata[$i][$STILLVISIBLE]=0;
				#print " $SVdata[$i][$PRN]";
			}
			#print " ]\n";
			for ($i=18;$i<$n;$i++){
				$prn= unpack('c',substr $data,$i,1); 
				# If the SV is being tracked then no more to do
				for ($j=0;$j<=$#SVdata;$j++){
					if ($SVdata[$j][$PRN] == $prn){
						$SVdata[$j][$STILLVISIBLE]=1;
						$SVdata[$j][$DISAPPEARED]=-1; # reset this timer
						last;
					}
				}
				# On startup sometimes see a prn < 0
				if ($j > $#SVdata && $prn > 0){
					Debug("$prn acquired");
					#push @SVdata, [$prn,time,-1,1]
					$SVdata[$j][$PRN]=$prn;
					$SVdata[$j][$APPEARED]=time;
					$SVdata[$j][$LAST_EPHEMERIS_RECEIVED]=-1;
					$SVdata[$j][$LAST_EPHEMERIS_REQUESTED]=-1;
					$SVdata[$j][$STILLVISIBLE]=1;
					$SVdata[$j][$DISAPPEARED]=-1;
				}
			}
			# Now prune satellites which have disappeared
			# This is only done if the satellite has not been
			# visible for some time - satellites can drop in and out of
			# view so we'll try to avoid excessive polling
			for ($i=0;$i<=$#SVdata;$i++){
				if ($SVdata[$i][$STILLVISIBLE]==0){
					if ($SVdata[$i][$DISAPPEARED]==-1){
						$SVdata[$i][$DISAPPEARED]=time;
						Debug("$SVdata[$i][$PRN] has disappeared");
					}
					elsif (time - $SVdata[$i][$DISAPPEARED] > $REMOVE_SV_THRESHOLD){	
						Debug("$SVdata[$i][$PRN] removed");
						splice @SVdata,$i,1;	
					}	
				}
			}
			$nvis=$#SVdata+1;
			open(STA,">$rcvrstatus");
			print STA "sats=$nvis\n";
			print STA "prns=";
			for ($i=0;$i<=$#SVdata-1;$i++){
				print STA $SVdata[$i][$PRN],",";
			}
			if ($nvis > 0) {print STA $SVdata[$#SVdata][$PRN];}
			print STA "\n";
			# extra bits 
			# receiver minor alarms, temperature and s/n
			print STA "alarms=$rxMinorAlarms\n";
			print STA "rxtemp=$rxTemperature\n";
			close(STA);
			#print "After pruning [ ";
			#for ($i=0;$i<=$#SVdata;$i++)
			#{
			#	print " $SVdata[$i][$PRN]";
			#}
			#print " ]\n";
		}
		else{
			open(STA,">$rcvrstatus");
			print STA "sats=0\n";
			print STA "prns=\n";
			print STA "alarms=$rxMinorAlarms\n";
			print STA "rxtemp=$rxTemperature\n";
			close STA;
		} 
	}
	
}

#----------------------------------------------------------------------------

sub PollEphemerides
{
	# Poll for ephemeris if a poll is due
	
	for ($i=0;$i<=$#SVdata;$i++){
		if ($SVdata[$i][$LAST_EPHEMERIS_REQUESTED] ==-1 &&
			time - $tstart > $COLDSTART_HOLDOFF ){ #flags start up for SV
			# Try to get an ephemeris as soon as possible after startup
			$SVdata[$i][$LAST_EPHEMERIS_REQUESTED] = time;
			Debug("Requesting ephemeris for $SVdata[$i][$PRN]");
			$cmd="\x38\x01\x06". chr $SVdata[$i][$PRN];
			&SendCommand($cmd);
		}
		elsif (time - $SVdata[$i][$LAST_EPHEMERIS_REQUESTED] > 30 && 
			$SVdata[$i][$LAST_EPHEMERIS_REQUESTED] > $SVdata[$i][$LAST_EPHEMERIS_RECEIVED] ){
				$SVdata[$i][$LAST_EPHEMERIS_REQUESTED] = time;
				Debug("Requesting ephemeris for $SVdata[$i][$PRN] again!");
				$cmd="\x38\x01\x06". chr $SVdata[$i][$PRN];
				&SendCommand($cmd);
		}
		elsif (($SVdata[$i][$LAST_EPHEMERIS_RECEIVED] != -1) &&
			(time - $SVdata[$i][$LAST_EPHEMERIS_REQUESTED] > $EPHEMERIS_POLL_INTERVAL)){
			$SVdata[$i][$LAST_EPHEMERIS_REQUESTED] = time;
			Debug("Requesting ephemeris for $SVdata[$i][$PRN] cos it be stale");
			$cmd="\x38\x01\x06". chr $SVdata[$i][$PRN];
			&SendCommand($cmd);
		}
	}
}

#----------------------------------------------------------------------------
sub PollUTCParameters
{
	#Poll for UTC parameters 
	
	if ($params[$UTC_PARAMETERS][$LAST_REQUESTED] == -1 && 
		time - $tstart > $COLDSTART_HOLDOFF){
		# Just starting so do this ASAP
		$params[$UTC_PARAMETERS][$LAST_REQUESTED]= time;
		Debug("Requesting UTC parameters");
		&SendCommand("\x38\x01\x05\x00");
	}
	elsif (time - $params[$UTC_PARAMETERS][$LAST_REQUESTED] > 30 &&
		$params[$UTC_PARAMETERS][$LAST_REQUESTED] > 
			$params[$UTC_PARAMETERS][$LAST_RECEIVED]){
		# Polled but no response
		$params[$UTC_PARAMETERS][$LAST_REQUESTED]= time;
		Debug("Requesting UTC parameters again!");
		&SendCommand("\x38\x01\x05\x00");
	}
	elsif (($params[$UTC_PARAMETERS][$LAST_RECEIVED] != -1) &&
		time - $params[$UTC_PARAMETERS][$LAST_REQUESTED] >  $UTC_POLL_INTERVAL){
		# Poll overdue
		$params[$UTC_PARAMETERS][$LAST_REQUESTED]= time;
		Debug("Requesting UTC parameters cos they be stale");
		&SendCommand("\x38\x01\x05\x00");
	}
}

#----------------------------------------------------------------------------
sub PollIonoParameters
{
	#Poll for ionosphere parameters
	
	if ($params[$IONO_PARAMETERS][$LAST_REQUESTED] == -1 &&
		time - $tstart > $COLDSTART_HOLDOFF){
		# Just starting so do this ASAP
		$params[$IONO_PARAMETERS][$LAST_REQUESTED]= time;
		Debug("Requesting iono parameters");
		&SendCommand("\x38\x01\x04\x00");
	}
	elsif (time - $params[$IONO_PARAMETERS][$LAST_REQUESTED] > 30 &&
		$params[$IONO_PARAMETERS][$LAST_REQUESTED] > 
			$params[$IONO_PARAMETERS][$LAST_RECEIVED]){
		# Polled but no response
		$params[$IONO_PARAMETERS][$LAST_REQUESTED]= time;
		Debug("Requesting iono parameters again!");
		&SendCommand("\x38\x01\x04\x00");
	}
	elsif (($params[$IONO_PARAMETERS][$LAST_RECEIVED] != -1) && 
		time - $params[$IONO_PARAMETERS][$LAST_REQUESTED] >  $IONO_POLL_INTERVAL){
		# Poll overdue
		$params[$IONO_PARAMETERS][$LAST_REQUESTED]= time;
		Debug("Requesting iono parameters cos they be stale");
		&SendCommand("\x38\x01\x04\x00");
	}
}
				
#----------------------------------------------------------------------------

sub ParseGPSSystemDataMessage
{
	$id=ord substr $data,0,1;
	$operation = ord substr $data,1,1;
	
	if ($id == 0x58 && $operation==0x02) {
		$dtype=ord substr $data,2,1;
		if ($dtype == 0x04){ # ionosphere
			Debug("Got Ionosphere msg");
			$params[$IONO_PARAMETERS][$LAST_RECEIVED]=time;
		}
		elsif ($dtype==0x05){ # UTC
			Debug("Got UTC msg");
			$params[$UTC_PARAMETERS][$LAST_RECEIVED]=time;
		}
		elsif ($dtype==0x06){ # ephemeris
			$prn = ord substr $data,3,1;
			Debug("Got ephemeris msg for $prn");
			for ($i=0;$i<=$#SVdata;$i++){
				if ($prn == $SVdata[$i][$PRN]){
					$SVdata[$i][$LAST_EPHEMERIS_RECEIVED]=time;
				}
			} 
		}
	}
}

#----------------------------------------------------------------------------
sub ParseSupplementalTimingPacket()
{
	my $dtmp;
	# Already know it's the right packet
	$dtmp=substr $data,1;
	#&ReverseByteOrder(4,7,$_[2]); 
	#&ReverseByteOrder(8,9,$_[2]);
	&ReverseByteOrder(10,11,$dtmp); # minor alarms
	$rxMinorAlarms=unpack ('S',substr $dtmp,10,2);
	#&ReverseByteOrder(16,19,$_[2]); # local clock bias
	#&ReverseByteOrder(20,23,$_[2]);
	#&ReverseByteOrder(24,27,$_[2]);
	#&ReverseByteOrder(28,31,$_[2]);
	&ReverseByteOrder(32,35,$dtmp); # temperature
	$rxTemperature=unpack ('f',substr $dtmp,32,4);
	#&ReverseByteOrder(36,43,$_[2]);
	#&ReverseByteOrder(44,51,$_[2]);
	#&ReverseByteOrder(52,59,$_[2]);
	#&ReverseByteOrder(60,63,$_[2]);
	#&ReverseByteOrder(64,67,$_[2]);
	#@data=unpack "C4IS2C4f2If2d3fI",(pack "H*",$_[2]);
}


#----------------------------------------------------------------------------

sub DoubleToStr
{
	$tmp = pack "d",$_[0];
	return (reverse $tmp);
}

#----------------------------------------------------------------------------

sub ReverseByteOrder
{
	my $start = $_[0];
	my $stop =  $_[1];
	my $swaps= ($stop-$start+1)/2;
	for ($i=0;$i<$swaps;$i++)
	{	
		$indx0=$start+$i;# save the first byte
		$tmp0=substr $_[2], $indx0,1;
		$indx1=$stop-$i;
		substr ($_[2],$indx0,1) = substr ($_[2],$indx1,1);
		substr ($_[2],$indx1,1) = $tmp0;
	}
}