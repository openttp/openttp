#!/usr/bin/perl -w

#
# The MIT License (MIT)
#
# Copyright (c) 2015 Michael J. Wouters
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
#

# Adapted from get_counter_data_prs10
#
# 2010-05-10 MJW More user friendliness - remove crontab, kill any running 
# 								get_counter_data and reinstallcrontab when done
# 2010-05-11 MJW Added update of prs10.history
#								 Added phase and frequency step
# 2010-06-10 MJW Add removal of PRS10 semaphore file 
# 2010-06-29 MJW Fixed up GPS checking on cold start so that script could be
#								 used to rephase the PRS10. Debugging logged to file. Test for PRS10
#								frequency lock before adjustment (cold start ...)
# 2011-08-29 MJW Added enabling of 1 pps output. PRS10 should be configured to disable
#								1 pps by default. Made phase setting loop a few times before giving up.
#								Default PRS10 UTC offset is now 0 s.
#

use Getopt::Std;
use POSIX;
use Time::HiRes qw( usleep gettimeofday);
use TFLibrary;
use vars ( '$tmask' );
use vars qw( $opt_a $opt_d $opt_h $opt_f $opt_s $opt_v);


$MAXTRIES=10; # maximum number of times to try to get a sensible response from the PRS10
$GPSOFFSET=3500;

$alarmTimeout = 120; # SIGAARLM timout - needs to be greater than status polling time
$reinstallCrontab=0;

$PWRFLAG = "$ENV{HOME}/logs/rb.semaphore";
$DEBUGLOG = "$ENV{HOME}/logs/prs10adjust.log";

# check command line
if ($0=~m#^(.*/)#) {$path=$1} else {$path="./"}	# read path info
$0=~s#.*/##;					# then strip it

# Parse command line
if (!getopts('adfhs:v:')) {
  ShowHelp();
  exit;
}

if ($opt_h){
	ShowHelp();
	exit;
}

$debugOn= defined $opt_d;

$count=0;
open (PS, "ps ax|");
while (<PS>){
  @_=split;
  $count++ if ($_[4]=~/^[\w\/]*$0$/);
}
close PS;
if ($count>1) {ErrorExit("$0 process already present")}


# Read setup info from file
$filename=$ENV{HOME}."/etc/cctf.setup";
unless ($filename=~m#/#) {$filename=$path.$filename}

%Init = &TFMakeHash($filename,(tolower=>1));

# Check we got the info we need from the setup file
@check=("counter port","receiver status","receiver","counter logger");
foreach (@check) {
  $tag=$_;
  $tag=~tr/A-Z/a-z/;	
  unless (defined $Init{$tag}) {ErrorExit("No entry for $_ found in $filename")}
}

if (!("Trimble" eq $Init{"receiver"})){
	ErrorExit("This script only works with Trimble receivers");
}

if (defined $Init{"rb power failure"}) {$PWRFLAG=$Init{"rb power failure"};}

$user = "cvgps";
if (defined $ENV{USER}){$user=$ENV{USER};}
# Save the crontab for reinstallation

Debug("Saving and removing $user crontab");
$crontab = $ENV{HOME}."/crontab.tmp";
`/usr/bin/crontab -l >$crontab`;
`/usr/bin/crontab -r`;
$reinstallCrontab=1;
sleep 1;

$ctrlogger = $Init{"counter logger"};
`/usr/bin/killall -u $user $ctrlogger >/dev/null 2>&1`;
Debug("Stopping  logging");
sleep 1;

# Create a lock on the port - this will stop other UUCP lock file
# aware applications from using the port
$port = $Init{"counter port"};
unless (`/usr/local/bin/lockport -p $$ $port $0`==1){
	ErrorExit("Failed to lock port");
}


# set up a timeout in case we get stuck because of a dead serial port
$SIG{ALRM} = sub {ErrorExit("Timed out - exiting.")};

# catch kill signal so we can log that we were killed
$SIG{TERM} = sub {ErrorExit("Received SIGTERM - exiting.")};

# initialise the port

$PORT = &TFConnectSerial($Init{"counter port"},
	(ispeed=>B9600,ospeed=>B9600,
	 iflag=>IGNPAR,oflag=>0,lflag=>0,
	 cflag=>CS8|CLOCAL|CREAD));

# set up a mask which specifies this port for select polling later on
$rxmask="";
vec($rxmask,fileno $PORT,1)=1;

# flush port and wait a moment just in case
tcflush($PORT,TCIOFLUSH);
sleep 1;

# Send a few dummy commands to flush junk
# This also ensures that the PRS10 is talking
#
PRS10Cmd("ID?");
PRS10Cmd("ID?");

# OK, what do we have to do ?
if ($opt_s){ # step PRS10 phase
	$toffset = $opt_s;
	if ($toffset =~/^\d+$/){
		print $PORT "PP$toffset\r";
		if ($debugOn){
			sleep 2; # wait a bit to get a sensible reading
			$tt=GetTimeTag();
			while ($tt == -1){
				$tt=GetTimeTag();
			}
			Debug("Read $tt after rephase");
		}
		PRS10Log("1 pps stepped $toffset ns");
		Debug("Removing $PWRFLAG if it's there");
		if (-e $PWRFLAG) {unlink $PWRFLAG;}
	}
	if (!$opt_v){goto CLEANUP;}
}

if ($opt_v){ # step PRS10 frequency
	
	$ffo=$opt_v;
	if (! $ffo =~/([+|-]?\d+)/){
		Debug("Bad frequency");
		goto CLEANUP;
	}

	$df = 10.0E6*$ffo*1.0E-12;

	# First need to get the magnetic offset and slope calibration factor
	$oldMagneticOffset=PRS10Cmd("MO?");
	chop $oldMagneticOffset;
	$slopeCalibration=PRS10Cmd("SS?");
	chop $slopeCalibration;
	# Use the SS value to calculate a scaling factor for the
	# magnetic field
	$sf=0.0;
	if ($slopeCalibration != 0){
		$sf = 4096.0*4096.0*1.0E-5/$slopeCalibration;
		Debug("Estimated calibration factor is (nominal 0.08) $sf");
	}
	else{
		Debug("Bad slope calibration $slopeCalibration");
		goto CLEANUP;
	}
	# Estimate the new MO
	$mo = int (sqrt(4096.0*4096.0*$df/$sf + $oldMagneticOffset*$oldMagneticOffset)+0.5);
	# Check whether this is in the allowed range
	if (($mo < 2300) || ($mo > 3600)){
		Debug("MO offset out of range");
		PRS10Log("MO offset = $mo is out of range");
		goto CLEANUP;
	}
	
	# Write the new value
	Debug("Old MO=$oldMagneticOffset New MO=$mo");
	PRS10Cmd("MO$mo");
	# and store it to EEPROM
	PRS10Cmd("MO!");
	# wait a bit
	sleep 1;
	$mo = PRS10Cmd("MO?");
	chop $mo;
	$storedmo = PRS10Cmd("MO!?");
	chop $storedmo;
	Debug("New MO=$mo Stored MO=$storedmo");
	PRS10Log("Current MO value changed from $oldMagneticOffset to $mo");
	if ($storedmo == $mo){
		PRS10Log("MO = $storedmo written to EEPROM");
	}
	else{
		PRS10Log("MO: EEPROM write failure");
	}

	goto CLEANUP;
}

if (!$opt_a){
	goto CLEANUP;
}

# First check - has the PRS10 lost power ?
# Definitely have comms at this point because dummy commands will
# have triggered a timeout and exit if the PRS10 is not talking

$status = GetStatus();
@statusbytes=split ",",$status;

if (129 > $statusbytes[5] && !$opt_f){
	Debug("No power loss detected - PRS10 was not stepped");
	goto CLEANUP;
}

# Create a power loss flag in case we bomb out before cleanup
# What we are being careful of here is that reading the status
# bytes now means that information about the power loss has
# been erased so cannot be detected by any another process
if (129 <= $statusbytes[5]) {
	if (!(-e $PWRFLAG)) # if there is already one, preserve it
	{
		open (OUT,">$PWRFLAG");
		$tt=time();
		($sec,$min,$hour)=gmtime($tt);
		$time=sprintf("%02d:%02d:%02d",$hour,$min,$sec);
		$mjd=int($tt / 86400)+40587;
		print OUT "$mjd $time Power loss detected\n"; # contents not used
		close OUT;
		Debug("Set power loss flag");
	}
}

# Check whether the GPS Rx is locked 

# Kickstart the receiver
$checkrx = $ENV{HOME}."/bin/check_rx";
if (!(-e $checkrx)){$checkrx=$ENV{HOME}."/data_acq/check_rx";}
if (!(-e $checkrx)){
	Debug("Unable to kickstart the GPS receiver");
	goto CLEANUP;
}

`$checkrx`;
$rx=$Init{"receiver status"};
if (-e $rx) {# could be stale
	@rxstat = stat $rx;
	if (time() - $rxstat[9] > 3){ # stale
		Debug("Stale receiver status file - waiting for GPS logging to start");
		sleep 15;
	}
}
else {# no file
	Debug("No GPS receiver status file - waiting for GPS logging to start");
	sleep 15;
}

# Status file should be there now
if (!(-e $rx)) { # still not there - decline to do anything
	Debug("Receiver status file is not updating");
	goto CLEANUP;
	exit;
} 


@rxstat = stat $rx;
if (time() - $rxstat[9] < 3) {# final check that the file is up to date
	# demand at least 5 satellites
	Debug("Waiting for GPS receiver to acquire at least 5 satellites (max 2 minutes)");
	$nsats=0;
	$tstart=time();
	$tnow=$tstart;
	while (($nsats) < 5 && ($tnow - $tstart)<120){
		open(IN,"<$rx");
		$nsats = <IN>;
		chomp $nsats;
		@bits=split "=",$nsats;
		$nsats=$bits[1];
		close (IN);
		Debug("Receiver locked to $bits[1] satellites");
		if ($nsats < 5) {# don't wait if all is OK ...
			sleep 15;
		}
		$tnow = time();
	}
	if ($nsats < 5){
		Debug("Insufficient satellites - giving up");
		goto CLEANUP;
	}


	Debug("No. of satellites = $nsats - OK");

	# Check that the frequency lock loop is on - otherwise TT  message gives rubbish
	$tstart=time();
	$tnow=$tstart;
	while (($statusbytes[3] & 1) && ($tnow - $tstart)<500){ # PRS10 spec is lock in less than 6 minutes
		$status = GetStatus();
		@statusbytes=split ",",$status;
		$tnow=time();
		last if (!($statusbytes[3] & 1));
		Debug("Waiting for PRS10 to warm up and lock");
		sleep 60;
	}

	if ($statusbytes[3] & 1){
		Debug("PRS10 still unlocked");
		goto CLEANUP;
	}

	Debug("PRS10 locked (status = $status) - proceeding");

	# Enable 1 pps output
	# No TT value will be returned otherwise
	#
	Debug("Enabling 1 pps output ...");
	print $PORT "LM1\r"; # 1 pps enabled unless the PRS10 is unlocked
	sleep 2;

	$ntries = 0;
	$ok = 0;
	while ($ntries < $MAXTRIES && $ok==0){
		$tt=GetTimeTag();
		if (length($tt) == 0){
			$ntries++;
			sleep 2;
			next;
		}

		if ($tt > -1){
			$toffset = (1000000000 + $GPSOFFSET - $tt) % 1000000000;
			Debug("Got TT data $tt, stepping $toffset");
			print $PORT "PP$toffset\r";
			sleep 2; # wait a bit to get a sensible reading
			$tt=GetTimeTag();
			#while ($tt == -1){$tt=GetTimeTag();}
		
			# Log some stuff
			Debug("Read $tt after rephase");
			PRS10Log("1 pps stepped $toffset ns");

			Debug("Removing $PWRFLAG");
			if (-e $PWRFLAG) {unlink $PWRFLAG;} # should be there anyway since we made one
			$ok=1; # yay !
		}
		else{
			$ntries++;
			sleep 2;
			next;
		}
	}

	if ($ok == 0){
		Debug("Failed to rephase the PRS10");
		Debug("Attempting to disable the 1 pps");
		print $PORT "LM2\r";
		sleep 2;
		goto CLEANUP;
	}
	
}

CLEANUP:

Debug("Reinstalling saved crontab");

`/usr/bin/crontab $crontab`;
close($PORT);
`/usr/local/bin/lockport -r  $port`;
Debug("Restarting logging");
$checkcntr = $ENV{HOME}."/bin/check_cntr";
`$checkcntr`;
sleep 1;

# End of main program
#-----------------------------------------------------------------------

sub ShowHelp
{
	print "Usage: $0 [-a] [-d] [-f] [-h] [-s <time_step>] [-v <frequency_step>]\n";
	print "  -a automatic step\n";
	print "  -d debug\n";
	print "  -f force step\n";
	print "  -h show this help\n";
	print "  -s step\n";
	print "  -v step frequency\n";
}

#-----------------------------------------------------------------------
sub PRS10Log
{
	my $logmsg=$_[0];
	# Update the history file
	my $prs10history = $ENV{HOME}."/logs/prs10.history";
	if (!(-e $prs10history)){$prs10history = $ENV{HOME}."/Parameter_Files/prs10.history";}
	if (-e $prs10history){
		@tod = gmtime(time());
		open (OUT,">>$prs10history");
		printf OUT "%4d-%02d-%02d %02d:%02d:%02d $logmsg\n",
			$tod[5]+1900,$tod[4]+1,$tod[3],$tod[2],$tod[1],$tod[0];
		close OUT;
	}
}

#-----------------------------------------------------------------------
sub ErrorExit {
  my $message=shift;
  @_=gmtime(time());
  my $errmsg = sprintf "%02d/%02d/%02d %02d:%02d:%02d $message",
    $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
	Debug($errmsg);
	if ($reinstallCrontab){
		`/usr/bin/crontab $crontab`;
	}
  exit;
}

#-----------------------------------------------------------------------
sub GetResponse {
	$done =0;
	$input="";
	#print "GetResponse\n";
	while (!$done){
		$nfound=select $tmask=$rxmask,undef,undef,1.0;
		if ($nfound==0) {return "";}
  		next unless $nfound;
		sysread $PORT,$input,$nfound,length $input;
		
		# if last character is newline then we've got the message
		$lastch=substr $input,length($input)-1,1;
		if ($lastch eq chr(13) ){
			# Test for PRS_10 in the response. This is automatically output 
			# by the PRS10 on reset and may be lurking to confuse us.
			$done = !($input =~/PRS_10/);
		}
	}
	return $input;
}

# ------------------------------------------------------------------------
sub PRS10Cmd
{
	alarm $alarmTimeout; # set the timeout
	$tries=0;
	while ($tries < 5){
		print $PORT "$_[0]\r";
		$response = GetResponse();
		last if (length($response) > 0);
		$tries++;
	}
	alarm 0; # cancel the timeout
	return $response;
}

# -------------------------------------------------------------------------
sub Debug
{
	if ($debugOn){
		($sec,$min,$hour)=gmtime(time());
		printf STDERR "%02d:%02d:%02d $_[0]\n",$hour,$min,$sec;
		open (DEBUG,">>$DEBUGLOG");
		printf DEBUG "%02d:%02d:%02d $_[0]\n",$hour,$min,$sec;
		close DEBUG;
	}
}

# -------------------------------------------------------------------------
sub GetTimeTag
{
	my ($r,$i);
	for ($i=0;$i<$MAXTRIES;$i++){
		$r=PRS10Cmd("tt?");
		chop $r; # remove terminator
		# valid responses are -1 and a +ve integer
		if (($r eq -1) || ($r=~/^\d+$/)){
			return $r;
		}
	}
	return "";
}

# -------------------------------------------------------------------------
sub GetStatus
{
	
	my ($r,$i);
	for ($i=1;$i<=$MAXTRIES;$i++){
		$r=PRS10Cmd("ST?");
		chop $r; # remove terminator
		# valid response is six comma-separated integers
		if ($r=~/^\d+,\d+,\d+,\d+,\d+,\d+/){
			Debug("Read PRS10 status $r (num tries=$i)");  
			return $r;
		}
	}
	return "";
}

