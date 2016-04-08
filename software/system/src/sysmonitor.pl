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

#
# Monitored -- class to monitor something of interest
#
package Monitored;

sub new
{
	my $class = shift;
	my $self = {
		id => shift, # numeric identifier
		msg => shift, # long form of alarm message
		shortMsg => shift, # short form of alarm message
		alerterID=>shift,
		testfn => shift, # function which is called to test monitored condition
		methods => shift, # alert methods to use 
		threshold=> 10,  # delay (in s) after which to raise an alarm
		maxPerDay=> 6, # maximum alarms per day
		nToday=>0,
		lastDay=>0,
		errLastUpdate => 0, #last time an error was updated
		errLastClear => 0, # last time an error cleared
		errTime => 0,  # time error  has been true 
		isError => 0,   # flags error 
	};
	bless $self,$class;
	return $self;
}

#
# Main
#

package main;

use POSIX;
use Getopt::Std;
use TFLibrary;
use vars qw($opt_c $opt_d $opt_h $opt_v);

$AUTHORS="Michael Wouters";
$VERSION="1.0";

$MAX_FILE_AGE=30;
$BOOT_GRACE_TIME=30;

# $ALARM_AUDIBLE=0x01; # deprecated: just annoying
# $ALARM_EMAIL=0x02; # deprecated: delegated to Alerter
# $ALARM_SMS=0x04; # deprecated: delegated to Alerter
$ALARM_LOG=0x08;
# $ALARM_SNMP=0x1; # deprecated: delegated to Alerter
$ALARM_ALERTER=0x20;
$ALARM_SYSLOG=0x40;
$ALARM_STATUS_FILE=0x80;
	
my $home=$ENV{HOME};

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

if (-d "$home/logs") {$logPath="$home/logs";} else {die "$logPath missing"}; 
if (-d "$home/etc")  {$configPath="$home/etc";} else {die "$configPath missing";};


# Read the system gpscv.conf

my $gpscvConfigFile = $configPath."/gpscv.conf";

if (!(-e $gpscvConfigFile)){
	ErrorExit("The configuration file $configFile was not found!\n");
}

my %GPSCVInit = &TFMakeHash2($gpscvConfigFile,(tolower=>1));

# Check we got the info we need from the config file
my @check=('reference:oscillator','reference:status file','receiver:manufacturer','receiver:status file');
foreach (@check) {
  $tag=$_;
  $tag=~tr/A-Z/a-z/;	
  unless (defined $GPSCVInit{$tag}) {ErrorExit("No entry for $_ found in $configFile")}
}

my $refOscillator = lc $GPSCVInit{'reference:oscillator'};
my $refStatusFile = TFMakeAbsoluteFilePath($GPSCVInit{'reference:status file'},$home,$logPath);
my $refPowerFlag;
my $checkRefPower=0;
if (defined $GPSCVInit{'reference:power flag'}){
	$refPowerFlag = $GPSCVInit{'reference:power flag'};
	$checkRefPower = !((lc $refPowerFlag) eq 'none');
}

my $receiver = $GPSCVInit{'receiver:manufacturer'};
my $rxStatusFile = TFMakeAbsoluteFilePath($GPSCVInit{'receiver:status file'},$home,$logPath);

# Read this application's .conf

my $configFile=$configPath."/sysmonitor.conf";
if (defined $opt_c){
	$configFile=$opt_c;
}

if (!(-e $configFile)){
	ErrorExit("The configuration file $configFile was not found!\n");
}

%Init = &TFMakeHash2($configFile,(tolower=>1));

$alarmPath= $logPath."/alarms/";
if (defined $Init{"alarm path"}){
	$alarmPath = TFMakeAbsolutePath($alarmPath,$home);
}

$logFile = $logPath."/sysmonitor.log";
if (defined $Init{"log file"}){
	$logFile = TFMakeAbsoluteFilePath($logFile,$home,$logPath);
}

# Create all the monitors
@monitors = ();

# Reference monitors

# $mon = new Monitored(32, "Reference logging not running", "Ref not logging",99,\&CheckRefLogging);
# $mon->{statusFile}=$refStatusFile;
# $mon->{oscillator}=$refOscillator;
# $mon->{methods} = $ALARM_LOG | $ALARM_STATUS_FILE | $ALARM_SYSLOG;
# push @monitors,$mon;

# $mon = new Monitored(32, "Reference unlocked", "Ref unlocked",99,\&CheckRefLocked);
# $mon->{statusFile}=$refStatusFile;
# $mon->{oscillator}=$refOscillator;
# push @monitors,$mon;
# 
# if ($checkRefPower){
# 	$mon = new Monitored(32, "Reference power failure", "Ref power failure",99,\&CheckRefPowerFlag);
# 	$mon->{powerFlag}=$refPowerFlag;
# 	$mon->{oscillator}=$refOscillator;
# 	push @monitors,$mon;
# }

$mon = new Monitored(32, "GPS insufficient satellites", "GPS low sats",99,\&CheckGPSSignal);
$mon->{statusFile}=$rxStatusFile;
$mon->{receiver}=$receiver;
$mon->{methods} = $ALARM_LOG | $ALARM_STATUS_FILE | $ALARM_SYSLOG;
push @monitors,$mon;

SysmonitorLog("Started");

while (1){
	for ($i=0;$i<=$#monitors;$i++){
		my $ret = $monitors[$i]->{testfn}->($monitors[$i]);
		if ($ret){
			ClearError($monitors[$i]);
		}
		else{
			SetError($monitors[$i]);
		}
	}
	sleep(1);
}

#-----------------------------------------------------------------------
sub ShowHelp{
	print "Usage: $0 [OPTION] ...\n";
	print "\t-c <file> specify an alternate configuration file\n";
  print "\t-d turn on debugging\n";
  print "\t-h show this help\n";
  print "\t-v print version\n";
}

#-----------------------------------------------------------------------
sub ErrorExit {
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
			Debug("Created before boot. Just booted ? " .($uptime < $BOOT_GRACE_TIME));
			return ($uptime < $BOOT_GRACE_TIME);
		}
	}
	else{
		# System may have just booted so to avoid a spurious alarm, allow a grace period
		Debug("System just booted? ". ($uptime < $BOOT_GRACE_TIME));
		return ($uptime < $BOOT_GRACE_TIME);
	}
	return 0; 
}

# -------------------------------------------------------------------------
sub CheckTICLogging
{
	Debug("\n-->CheckTICLogging");
	return CheckFile($_[0]->{TICStatusFile});
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
	if (-e $statusFile){
		open(IN,"<$statusFile");
		if ($mon->{oscillator} eq 'prs10'){
			my @statFlags = split /\s+/,<IN>;
			Debug("Got PRS10 status:");
			if ($#statFlags == 5){
				return  (!(($statFlags[3] & 0x01) || ($statFlags[1] & 0x01)) );
			}
			# don't report an error if it failed to parse - maybe we read it as it changed
			Debug("Error parsing $statusFile");
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
	Debug("\n-->CheckNTPD");
}

# -------------------------------------------------------------------------
sub CheckGPSSignal
{
	Debug("\n-->CheckGPSSignal");
	my $mon=$_[0];
	my $statusFile = $mon->{statusFile};
	if ($mon->{receiver} eq "Trimble"){
		# format is nsats = xxx
		if (-e $statusFile){
			# won't check age
			open (IN,"<$statusFile");
			my $line=<IN>;
			chomp $line;
			close IN;
			Debug("$mon->{receiver} $line");
			if ($line =~/nsats\s*=\s*(\d+)/){
				return $1>= 4;
			}
		}
		return 1; # not there or parse error
	}
	elsif ($mon->{receiver} eq "Javad"){
		# format of the message is
		# NP HH:MM:SS ,NAVPOS,%C,%6.2F,%1D,%C%C,{%2D,%2D} etc
		# When no satellites are visible get something like
		# NP HH:MM:SS ,NAVPOS,%C,%6.2F,%1D
		if (-e $statusFile){
			open (IN,"<$statusFile");
			my $line=<IN>;
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
	}
	return 0;
}

# -------------------------------------------------------------------------
sub CheckRAID
{
	Debug("\n-->CheckRAID");
}

# -------------------------------------------------------------------------
sub SetError{
	Debug("\n-->SetError");
	
	my $mon=$_[0];
	
	($tvnow_secs,$tvnow_usecs) = gettimeofday;
	my $tvnow = $tvnow_secs+$tvnow_usecs/1.0E6; # double has enough significant figures
	# Reset time of last clear 
	$mon->{errLastClear}=0;
	
	if ($mon->{isError}){ # error condition already running 
		$mon->{errTime} += $tvnow - $mon->{errLastUpdate};
		$mon->{errLastUpdate}=$tvnow;
		if ( $mon->{errTime} >= $mon->{threshold} ){
			$mon->{errTime} =  $mon->{threshold};
		}
		Debug("Error time = " . $mon->{errTime});
		return 0; 
	}
	
	if ($mon->{errLastUpdate}== 0){ # no running error condition yet so initialise 
		$mon->{errLastUpdate} = $tvnow;
		$mon->{errTime}=0;
	}
	
	$mon->{errTime} += $tvnow - $mon->{errLastUpdate};
	$mon->{errLastUpdate}=$tvnow;	
	
	Debug("Error time = " . $mon->{errTime});

	# If the elapsed time has reached the threshold, an alarm is raised 
	if ( $mon->{errTime} >= $mon->{threshold} ){
		# Clamp the error run time to the threshold so that the system
		# recovers quickly from a long-lasting error
		$mon->{errTime} =  $mon->{threshold};
		$mon->{isError} =1;
		
		if (($mon->{methods} | $ALARM_LOG)){     # log everything 
			SysmonitorLog($mon->{msg});
		}
		
		if (($mon->{methods} | $ALARM_STATUS_FILE)){
			# Writes a file for other processes to use 
			my $fout = $alarmPath."/". ($mon->{shortMsg});
			open(OUT,">$fout");
			close OUT;
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
#				if ((f=fopen(a->alerterQueue,"a"))){
# 					if (a->msg) 
# 						fprintf(f,"%02d/%02d/%02d %02d:%02d:%02d  %s (%i)\n",
# 							gmt->tm_mday,gmt->tm_mon+1,gmt->tm_year % 100,
# 							gmt->tm_hour,gmt->tm_min,gmt->tm_sec,
# 							a->msg,a->alerterID);
# 					fclose(f);
#			}
			}
			
			$mon->{nToday}+=1;
		}
		return 1;
	} 
	return 0;	
}

# -------------------------------------------------------------------------
sub ClearError{
	Debug("\n-->ClearError");
	
	my $mon=$_[0];

	if ($mon->{errLastUpdate} ==0){ # no error so return 
		return 1;
	}
	
	($tvnow_secs,$tvnow_usecs) = gettimeofday;
	my $tvnow = $tvnow_secs+$tvnow_usecs/1.0E6; # double has enough significant figures
	
	if ($mon->{errLastClear}== 0){ # flags first clear event 
		$mon->{errLastClear}=$tvnow;
	}
	
	$mon->{errTime} -= $tvnow - $mon->{errLastClear}; #decrement 
			
	Debug("Error time = ".$mon->{errTime});
	if ($mon->{isError} && ($mon->{errTime} <= 0.0)){ # error has cleared 
		my $msg = $mon->{msg}." (cleared)";
		if (($mon->{methods} | $ALARM_LOG))  {
			SysmonitorLog($msg);
		}
		if (($mon->{methods} | $ALARM_STATUS_FILE)){
				unlink  $alarmPath."/". ($mon->{shortMsg});
		}
		if ($mon->{nToday} <=$mon->{maxPerDay}){ #remember to catch last one 
			my @gmt=gmtime(time);	
			if (($mon->{methods} | $ALARM_SYSLOG)) {
				openlog("sysmonitor",LOG_PID,LOG_USER);
				syslog(LOG_WARNING,$mon->{msg});
				closelog();
			}
#			if (($mon->{methods} | $ALARM_ALERTER)) {
# 				if ((f=fopen(a->alerterQueue,"a")))
#         {
# 					
# 					if (a->msg) 
# 						fprintf(f,"%02d/%02d/%02d %02d:%02d:%02d  %s cleared (%i)\n",
# 							gmt->tm_mday,gmt->tm_mon+1,gmt->tm_year % 100,
# 							gmt->tm_hour,gmt->tm_min,gmt->tm_sec,
# 							a->msg,a->alerterID);
# 					fclose(f);
# 				}
#			}
		
		}
		
		$mon->{isError}=0;
		$mon->{errLastUpdate}=0;
		
		return 1;
	}
	return 0; # error still persisting 
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