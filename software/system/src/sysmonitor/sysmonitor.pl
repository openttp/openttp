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

# sysmonitor.pl
# Monitors various things and raises alarm
# Runs continuously
#

# Modification history
# 

use POSIX;
use Getopt::Std;
use Linux::SysInfo qw(sysinfo);
use Sys::Syslog;                        
use Sys::Syslog qw(:standard :macros);  
use TFLibrary;
use Time::HiRes qw( gettimeofday );

use vars qw($opt_c $opt_d $opt_h $opt_v);

sub CheckForOldAlarm;

#
# Monitored -- class to monitor something of interest
#
package Monitored;

sub new
{
	my $class = shift;
	my $self = {
		category => shift, # NTP, GPS, Oscillator, ...
		msg => shift, # long form of alarm message
		shortMsg => shift, # short form of alarm message
		alerterID=>shift,
		testfn => shift, # function which is called to test monitored condition
		methods => shift, # alert methods to use 
		threshold=> 60,  # delay (in s) after which to raise an alarm
		maxPerDay=> 6, # maximum alarms per day
		nToday=>0,
		lastDay=>0,
		alarmLastUpdate => 0, #last time an alarm was updated
		alarmLastClear => 0, # last time an alarm cleared
		alarmRunTime => 0,  # time alarm  has been true 
		isAlarm => 0,   # flags alarm 
	};
	bless $self,$class;
	return $self;
}

#
# Main
#

package main;

$AUTHORS="Michael Wouters";
$VERSION="1.0.1";

#$MAX_FILE_AGE=60; # file can be up to this old before an alarm is raised
$MAX_FILE_AGE=180; # Increased it to 180 seconds, because GPSDO data is  
                   # only logged once a minute (Louis, 2017-07-21)
$BOOT_GRACE_TIME=60; # wait at least this long after boot to let processes start

# $ALARM_AUDIBLE=0x01; # removed: just annoying
# $ALARM_EMAIL=0x02; # removed: delegated to Alerter
# $ALARM_SMS=0x04; # removed: delegated to Alerter
$ALARM_LOG=0x08;
# $ALARM_SNMP=0x1; # removed: delegated to Alerter
$ALARM_ALERTER=0x20;
$ALARM_SYSLOG=0x40;
$ALARM_STATUS_FILE=0x80;

$DEFAULT_ALARMS = $ALARM_LOG | $ALARM_STATUS_FILE | $ALARM_SYSLOG;

$ALARM_THRESHOLD=60;

$MDSTAT='/proc/mdstat';

$ROOT = '/usr/local';

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

# Normally, this script is run as a system daemon, so we use /usr/local
if (-d "$ROOT/log") {$logPath="$ROOT/log";} else {die "$logPath missing"}; 
if (-d "$ROOT/etc")  {$configPath="$ROOT/etc";} else {die "$configPath missing";};

# Read this application's .conf

$configFile=$configPath."/sysmonitor.conf";
if (defined $opt_c){
	$configFile=$opt_c;
}

if (!(-e $configFile)){
	AlarmExit("The configuration file $configFile was not found!\n");
}
Debug("Using config $configFile");

%Init = &TFMakeHash2($configFile,(tolower=>1));

$alarmPath= $logPath.'/alarms/';
if (defined $Init{"alarm path"}){
	$alarmPath = TFMakeAbsolutePath($Init{"alarm path"},$ROOT);
}
Debug("Alarm path = $alarmPath");

$logFile = $logPath.'/sysmonitor.log';
if (defined $Init{'log file'}){
	$logFile = TFMakeAbsoluteFilePath($Init{'log file'},$ROOT,$logPath);
}
Debug("Log file = $logFile");

$gpscvHome ='/home/cvgps';
if (defined $Init{'gpscv account'}){
	$gpscvHome = '/home/'.$Init{'gpscv account'};
}

$ntpHome ='ntp-admin';
if (defined $Init{'ntp account'}){
	$ntpHome = '/home/'.$Init{'ntp account'};
}

$alerterQueue = '/usr/local/log/alert.log';
if (defined $Init{'alerter queue'}){
	$alerterQueue = $Init{'alerter queue'};
}
Debug("Alerter queue = $logFile");

$alarmThreshold = $ALARM_THRESHOLD;
if (defined $Init{'alarm threshold'}){
	$alarmThreshold = $Init{'alarm threshold'};
}
Debug("Alarm threshold = $alarmThreshold");

# Read the system gpscv.conf
$gpscvConfigFile = $gpscvHome.'/etc/gpscv.conf';
if (!(-e $gpscvConfigFile)){
	AlarmExit("The configuration file $configFile was not found!\n");
}

%GPSCVInit = &TFMakeHash2($gpscvConfigFile,(tolower=>1));

$gpscvLogPath=$gpscvHome.'/logs/';

# Check we got the info we need from the config file
 @check=('reference:oscillator','reference:status file',
 'receiver:manufacturer','receiver:status file',
 'counter:status file');
 
foreach (@check) {
  $tag=$_;
  $tag=~tr/A-Z/a-z/;	
  unless (defined $GPSCVInit{$tag}) {AlarmExit("No entry for $_ found in $configFile")}
}

$refOscillator = lc $GPSCVInit{'reference:oscillator'};
$refStatusFile = TFMakeAbsoluteFilePath($GPSCVInit{'reference:status file'},$gpscvHome,$gpscvLogPath);
$checkRefPower=0;
if (defined $GPSCVInit{'reference:power flag'}){
	$refPowerFlag = $GPSCVInit{'reference:power flag'};
	$checkRefPower = !((lc $refPowerFlag) eq 'none');
}

$receiver = $GPSCVInit{'receiver:manufacturer'};
$rxStatusFile = TFMakeAbsoluteFilePath($GPSCVInit{'receiver:status file'},$gpscvHome,$gpscvLogPath);

$counterStatusFile = TFMakeAbsoluteFilePath($GPSCVInit{'counter:status file'},$gpscvHome,$gpscvLogPath);

# Create all the monitors
@monitors = ();

# NTP monitors
if (defined $Init{'ntpd refclocks'}){
	my @refclks = split /,/,$Init{'ntpd refclocks'};
	foreach (@refclks){
		my $clk = lc $_;
		$clk =~ s/^\s+//; # clean white space
		$clk =~ s/\s+$//;
		if (defined $Init{"$clk:refid"}){
			my $name = 'ref clk';
			if (defined $Init{"$clk:name"}){
				$name = $Init{"$clk:name"};
			}
			$mon = new Monitored("NTP", "NTP refclk $name dead", "NTP $name dead",99,\&CheckNTPRefClock);
			$mon->{refid}=$Init{"$clk:refid"};
			$mon->{methods} = $DEFAULT_ALARMS;
			$mon->{threshold}=$alarmThreshold;
			push @monitors,$mon;
			CheckForOldAlarm($mon);
			Debug("Added $clk ($name)");
		}else{
			AlarmExit("$clk:refid undefined");
		}
	}
}

# Reference monitors

$mon = new Monitored("TIC", "TIC logging not running", "TIC not logging",99,\&CheckTICLogging);
$mon->{statusFile}=$counterStatusFile;
$mon->{methods} = $DEFAULT_ALARMS;
$mon->{threshold}=$alarmThreshold;
CheckForOldAlarm($mon);
push @monitors,$mon;

$mon = new Monitored("Oscillator", "Reference logging not running", "Ref not logging",99,\&CheckRefLogging);
$mon->{statusFile}=$refStatusFile;
$mon->{oscillator}=$refOscillator;
$mon->{methods} = $DEFAULT_ALARMS;
$mon->{threshold}=$alarmThreshold;
CheckForOldAlarm($mon);
push @monitors,$mon;

$mon = new Monitored("Oscillator", "Reference unlocked", "Ref unlocked",99,\&CheckRefLocked);
$mon->{statusFile}=$refStatusFile;
$mon->{methods} = $DEFAULT_ALARMS;
$mon->{oscillator}=lc $refOscillator;
$mon->{threshold}=$alarmThreshold;
CheckForOldAlarm($mon);
push @monitors,$mon;

if ($checkRefPower){
	$mon = new Monitored("Oscillator", "Reference power failure", "Ref power failure",99,\&CheckRefPowerFlag);
	$mon->{powerFlag}=$refPowerFlag;
	$mon->{methods} = $DEFAULT_ALARMS;
	$mon->{oscillator}=$refOscillator;
	$mon->{threshold}=$alarmThreshold;
	CheckForOldAlarm($mon);
	push @monitors,$mon;
}

$mon = new Monitored("GPS", "GPS logging not running", "GPS not logging",99,\&CheckGPSLogging);
$mon->{statusFile}=$rxStatusFile;
$mon->{methods} = $DEFAULT_ALARMS;
$mon->{threshold}=$alarmThreshold;
CheckForOldAlarm($mon);
push @monitors,$mon;

$mon = new Monitored("GPS", "GPS insufficient satellites", "GPS low sats",99,\&CheckGPSSignal);
$mon->{statusFile}=$rxStatusFile;
$mon->{receiver}=$receiver;
$mon->{methods} = $DEFAULT_ALARMS;;
$mon->{threshold}=$alarmThreshold;
CheckForOldAlarm($mon);
push @monitors,$mon;

# Check for RAID
if (-e $MDSTAT){
	open (IN,"<$MDSTAT");
	while ($line=<IN>){
		if ($line =~/Personalities/ && $line =~/raid/ ){
			DEBUG('RAID detected');
			$mon = new Monitored("PC", 'RAID disk failure', 'RAID failure',99,\&CheckRAID);
			$mon->{methods} = $ALARM_LOG | $ALARM_STATUS_FILE | $ALARM_SYSLOG;
			push @monitors,$mon;
			CheckForOldAlarm($mon);
			last;
		}
	}
	close IN;
}

Debug("Initialized ".($#monitors+1). " monitors");

my $killed = 0;

# Catch the kill signal so we can log that we were killed
$SIG{TERM} = sub { SysmonitorLog("Received SIGTERM - exiting."); $killed = 1; };

SysmonitorLog("Started");

$ntpqClks={}; # NB global
$lastNtpq = 0; 

while ($killed == 0){
	
	my $now =time;
	
	if ($now - $lastNtpq > 128){ # clocks are polled at 16 s typically and it takes 8 polls for reachability to hit zero
		Debug("Running ntpq");
		@ntpqOut= split /\n/,`ntpq -pn 2>/dev/null`; # if ntpd is not running get 'Connection refused'
		if ($#ntpqOut >= 1){
			shift @ntpqOut; shift @ntpqOut; # first two lines are uninteresting
			foreach (@ntpqOut){
				my @fields = split /\s+/,$_;
				if ($#fields == 9){
					$fields[0]=~/(\d+\.\d+\.\d+\.\d+)/;
					$ntpqClks{$1}=$fields[6]; # hash for easy lookup
				}
			}
			$lastNtpq=$now;
		}
	}
	
	for ($i=0;$i<=$#monitors;$i++){
		my $ret = $monitors[$i]->{testfn}->($monitors[$i]);
		if ($ret){
			ClearAlarm($monitors[$i]);
		}
		else{
			SetAlarm($monitors[$i]);
		}
	}
	
	if($killed == 0) { sleep(1); }
	
}

if ($killed == 0) { SysmonitorLog("Unexpected exit"); } else {SysmonitorLog("Finished"); }

#-----------------------------------------------------------------------
sub ShowHelp{
	print "Usage: $0 [OPTION] ...\n";
	print "\t-c <file> specify an alternate configuration file\n";
  print "\t-d turn on debugging\n";
  print "\t-h show this help\n";
  print "\t-v print version\n";
}

#-----------------------------------------------------------------------
sub AlarmExit {
  my $message=shift;
  @_=gmtime(time());
  printf "%04d-%02d-%02d %02d:%02d:%02d $message\n",
   $_[5]+1900,$_[4]+1,$_[3],$_[2],$_[1],$_[0];
  exit;
}

# -------------------------------------------------------------------------
sub Debug
{
	if ($opt_d){
		printf STDERR "$_[0]\n";
	}
}

# -------------------------------------------------------------------------
# On startup, check for an alarm, and set run time for alarm to maximum
# It should then clear 
sub CheckForOldAlarm()
{
	my $mon=$_[0];
	my ($tvnow_secs,$tvnow_usecs) = gettimeofday;
	my $tvnow = $tvnow_secs+$tvnow_usecs/1.0E6; 
	my $fout = $alarmPath."/". ($mon->{shortMsg});
	if (-e $fout){
		$mon->{alarmLastClear}=0;
		$mon->{alarmLastUpdate}=$tvnow;	
		$mon->{alarmRunTime} =  $mon->{threshold}; # set run time for alarm to maximum
		$mon->{isAlarm} =1;
		
		Debug("Old alarm found:".$mon->{shortMsg});
	}	
}

# -------------------------------------------------------------------------
sub CheckFile
{
	my $f = $_[0];
	my $si = sysinfo;
	my $uptime = $si->{uptime};
	my $now = time;
	
	Debug("Checking $f (now=$now,uptime=$uptime");
	
	if (-e $f){
		@statinfo = stat $f;
		$mtime = $statinfo[9];
		Debug("$f created $mtime");
		# Was it created since the last boot ?
		if ($mtime > $now - $uptime){
			Debug("Modified since boot");
			if ($now -$mtime < $MAX_FILE_AGE){
				Debug("$f OK (".($now -$mtime)." s old)");
				return 1;
			}
			else{
				Debug("$f too old (".($now -$mtime)." s old)");
				return 0;
			}
		}
		else{
			# File may not have been updated yet
			Debug('Created before boot. Just booted? ' .(($uptime < $BOOT_GRACE_TIME)?'Yes':'No'));
			return ($uptime < $BOOT_GRACE_TIME);
		}
	}
	else{
		# System may have just booted so to avoid a spurious alarm, allow a grace period
		Debug('System just booted? '. (($uptime < $BOOT_GRACE_TIME)?'Yes':'No'));
		return ($uptime < $BOOT_GRACE_TIME);
	}
	return 0; 
}

# -------------------------------------------------------------------------
sub CheckNTPRefClock
{
	Debug("\n-->CheckNTPRefClk");
	my $mon = $_[0];
	Debug("Checking $mon->{refid}");
	if (defined $ntpqClks{$mon->{refid}}){
		Debug("$mon->{refid} reachability $ntpqClks{$mon->{refid}}");
		return ($ntpqClks{$mon->{refid}} >0);
	}
	return 0; # if it's not there, there may have been a problem with device initialization by OS
}

# -------------------------------------------------------------------------
sub CheckTICLogging
{
	Debug("\n-->CheckTICLogging");
	return CheckFile($_[0]->{statusFile});
}

# -------------------------------------------------------------------------
sub CheckRefLogging
{
	Debug("\n-->CheckRefLogging");
	return CheckFile($_[0]->{statusFile});
}

# -------------------------------------------------------------------------
sub CheckRefPowerFlag
{
	Debug("\n-->CheckRefPowerFlag");
	return CheckFile($_[0]->{powerFlag});
}

# -------------------------------------------------------------------------
sub CheckRefLocked
{
	Debug("\n-->CheckRefLocked");
	my $mon = $_[0];
	my $statusFile = $mon->{statusFile};
	my $line;
	if (-e $statusFile){
		open(IN,"<$statusFile");
		if ($mon->{oscillator} eq 'prs10'){
			my @statFlags = split /\s+/,<IN>;
			close IN;
			Debug('Got PRS10 status');
			if ($#statFlags == 5){
				return  (!(($statFlags[3] & 0x01) || ($statFlags[1] & 0x01)) );
			}
			# don't report an alarm if it failed to parse - maybe we read it as it changed
			Debug("Alarm parsing $statusFile");
		}
		elsif ($mon->{oscillator} eq 'gpsdo'){
			while ($line=<IN>){
				if ($line=~/GPSDO\s+health/){
					Debug("Got GPSDO status:");
					if (($line =~ /:\s+0x0/) || ($line =~ /:\s*Completely\s+healthy/)){
						Debug('GPSDO healthy');
						return 1;
					}
					else {
						Debug('GPSDO unhealthy');
						return 0;
					}
				}
			}
			close IN;
			Debug("Alarm parsing $statusFile");
		}
		close IN;
	}
	return 1; # if it's not there, then CheckTICLogging will pick that up
}

# -------------------------------------------------------------------------
sub CheckGPSLogging
{
	Debug("\n-->CheckGPSLogging");
	return CheckFile($_[0]->{statusFile});
}

# -------------------------------------------------------------------------
sub CheckNTPD
{
	Debug("\n-->CheckNTPD UNIMPLEMENTED");
}

# -------------------------------------------------------------------------
sub CheckGPSSignal
{
	Debug("\n-->CheckGPSSignal");
	my $mon=$_[0];
	my $line;
	my $statusFile = $mon->{statusFile};
	if ($mon->{receiver} eq "Trimble"){
		# format is nsats = xxx
		if (-e $statusFile){
			# won't check age
			open (IN,"<$statusFile");
			$line=<IN>;
			chomp $line;
			close IN;
			Debug("$mon->{receiver} $line");
			if ($line =~/nsats\s*=\s*(\d+)/){
				return $1>= 4;
			}
		}
		return 1; # not there or parse alarm
	}
	elsif ($mon->{receiver} eq "Javad"){
		# format of the message is
		# NP HH:MM:SS ,NAVPOS,%C,%6.2F,%1D,%C%C,{%2D,%2D} etc
		# When no satellites are visible get something like
		# NP HH:MM:SS ,NAVPOS,%C,%6.2F,%1D
		if (-e $statusFile){
			open (IN,"<$statusFile");
			$line=<IN>;
			chomp $line;
			close IN;
			Debug("$mon->{receiver} $line");
			if ($line =~ /\{(\d{2}),\d{2}\}/){
				return $1>= 4;
			}
			else{
				return 0;
			}
		}
		return 1;
	}
	elsif ($mon->{receiver} eq "NVS"){
		if (-e $statusFile){
			open (IN,"<$statusFile");
			while ($line=<IN>){
				chomp $line;
				Debug("$mon->{receiver} $line");
				if ($line =~ /GPS sats:\s*(\d+)/){
					return $1>= 4;
				}
			}
			close IN;
			return 0;
		}
		return 1;
	}
	elsif ($mon->{receiver} eq "ublox"){
		if (-e $statusFile){
			open (IN,"<$statusFile");
			while ($line=<IN>){
				chomp $line;
				Debug("$mon->{receiver} $line");
				if ($line =~ /GPS/){
					@gps = split('=',$line);
					if ($#gps == 1){
						@prns = split ',',$gps[1];
						$ngps = $#prns+1;
						Debug("$mon->{receiver} $ngps GPS visible");
						return $ngps >= 4;
					}
				}
			}
			close IN;
			return 0;
		}
		return 1;
	}
	return 0;
}

# -------------------------------------------------------------------------
sub CheckRAID
{
	Debug("\n-->CheckRAID UNIMPLEMENTED");
}

# -------------------------------------------------------------------------
sub SetAlarm{
	Debug("\n-->SetAlarm");
	
	my $mon=$_[0];
	
	my ($tvnow_secs,$tvnow_usecs) = gettimeofday;
	my $tvnow = $tvnow_secs+$tvnow_usecs/1.0E6; 
	
	# Reset the time of last clear event
	$mon->{alarmLastClear}=0;
	
	if ($mon->{isAlarm}){ # alarm condition is already running 
		$mon->{alarmRunTime} += $tvnow - $mon->{alarmLastUpdate};
		$mon->{alarmLastUpdate}=$tvnow;
		if ( $mon->{alarmRunTime} >= $mon->{threshold} ){
			$mon->{alarmRunTime} =  $mon->{threshold};
		}
		Debug("Existing alarm: running time = " . $mon->{alarmRunTime});
		return 0; 
	}
	
	if ($mon->{alarmLastUpdate}== 0){ # no running alarm condition yet so initialise 
		$mon->{alarmLastUpdate} = $tvnow;
		$mon->{alarmRunTime}=0;
	}
	
	$mon->{alarmRunTime} += $tvnow - $mon->{alarmLastUpdate};
	$mon->{alarmLastUpdate}=$tvnow;	
	
	Debug("New alarm: running time = " . $mon->{alarmRunTime});

	# If the elapsed time has reached the threshold, an alarm is raised 
	if ( $mon->{alarmRunTime} >= $mon->{threshold} ){
		Debug("Raising alarm");
		# Clamp the alarm run time to the threshold so that the system
		# recovers quickly from a long-lasting alarm
		$mon->{alarmRunTime} =  $mon->{threshold};
		$mon->{isAlarm} =1;
		
		if (($mon->{methods} | $ALARM_LOG)){     # log everything 
			Debug("Logging");
			SysmonitorLog($mon->{msg});
		}
		
		if (($mon->{methods} | $ALARM_STATUS_FILE)){
			# Writes a file for other processes to use 
			my $fout = $alarmPath."/". ($mon->{shortMsg});
			open(OUT,">$fout");
			close OUT;
			Debug("Writing to $fout");
		}

		# Check for day rollover so that the number of alerts for today is reset 
		my @gmt=gmtime(time);
		if ($mon->{lastDay} != $gmt[7]){	
			
			$mon->{nToday}=0;
			$mon->{lastDay} = $gmt[7];
		}
		
		# Number of alerts is limited to maxPerDay for email, SMS, SNMP etc*
		if ($mon->{nToday} < $mon->{maxPerDay}){
		
			if (($mon->{methods} | $ALARM_SYSLOG)) {
				Debug("syslog() ...");
				openlog("sysmonitor",LOG_PID,LOG_USER);
				syslog(LOG_WARNING,$mon->{msg});
				closelog();
			}
			
			if (($mon->{methods} | $ALARM_ALERTER)) {
				if (-e $alerterQueue){
					open(OUT,">>$alerterQueue");
					printf OUT "%02d/%02d/%02d %02d:%02d:%02d %s (%i)\n",
						$gmt[3],$gmt[4]+1,$gmt[5] % 100,
						$gmt[2],$gmt[1],$gmt[0],
						$mon->{shortMsg},$mon->{alerterID};
					close OUT;
				}
			}
			
			$mon->{nToday}+=1;
		}
		return 1;
	} 
	return 0;	
}

# -------------------------------------------------------------------------
sub ClearAlarm{
	Debug("\n-->ClearAlarm");
	
	my $mon=$_[0];

	if ($mon->{alarmLastUpdate} ==0){ # no alarm so return 
		return 1;
	}
	
	($tvnow_secs,$tvnow_usecs) = gettimeofday;
	my $tvnow = $tvnow_secs+$tvnow_usecs/1.0E6; 
	
	if ($mon->{alarmLastClear}== 0){ # flags first clear event 
		$mon->{alarmLastClear}=$tvnow;
	}
	
	$mon->{alarmRunTime} -= $tvnow - $mon->{alarmLastClear}; # decrement the running time
	$mon->{alarmLastClear}=$tvnow;
	
	Debug("Alarm time = ".$mon->{alarmRunTime});
	if ($mon->{isAlarm} && ($mon->{alarmRunTime} <= 0.0)){ # alarm has cleared 
		my $msg = $mon->{msg}.' (cleared)';
		
		if (($mon->{methods} | $ALARM_LOG))  {
			SysmonitorLog($msg);
		}
		if (($mon->{methods} | $ALARM_STATUS_FILE)){
				unlink  $alarmPath.'/'. ($mon->{shortMsg});
		}
		
		if ($mon->{nToday} <=$mon->{maxPerDay}){ # remember to catch last one 
			my @gmt=gmtime(time);	
			
			if (($mon->{methods} | $ALARM_SYSLOG)) {
				openlog('sysmonitor',LOG_PID,LOG_USER);
				syslog(LOG_WARNING,$mon->{msg});
				closelog();
			}
			
			if (($mon->{methods} | $ALARM_ALERTER)) {
				if (-e $alerterQueue){
					open(OUT,">>$alerterQueue");
					printf OUT "%02d/%02d/%02d %02d:%02d:%02d %s cleared (%i)\n",
						$gmt[3],$gmt[4]+1,$gmt[5] % 100,
						$gmt[2],$gmt[1],$gmt[0],
						$mon->{msg},$mon->{alerterID};
					close OUT;
				}
			}
		}
		
		$mon->{isAlarm}=0;
		$mon->{alarmLastUpdate}=0;
		
		return 1;
	}
	return 0; # alarm still persisting 
}

# -------------------------------------------------------------------------
sub SysmonitorLog{
	my $msg = shift;
	@_=gmtime(time());
  $logmsg = sprintf "%04d-%02d-%02d %02d:%02d:%02d $msg",
   $_[5]+1900,$_[4]+1,$_[3],$_[2],$_[1],$_[0];
  Debug($logmsg);
  open(OUT,">>$logFile");
  print OUT $logmsg,"\n";
  close(OUT);
}
