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
# GPSCV software installation script
#
# Modification history
# 


use English;
use Getopt::Std;
use Carp;
use FileHandle;
use TFLibrary;
use POSIX;

use vars qw($opt_d $opt_h $opt_t $optu $opt_v);

$VERSION = "install.pl version 0.1";
$ECHO=1;

# Receivers
$UNKNOWN=-1;
$TRIMBLE=0;
@receivers = ($TRIMBLE);
@receiverNames = ("Trimble");
@receiverDirs  = ("trimble");

$GPSCVUSER=$ENV{USER};

$INSTALLFROMSRC=0;
$INSTALLFROMDIST=1;

getopts('nvhtu:d');
unless (defined $opt_t) {$opt_t=0;}

if (defined $opt_u){
	$GPSCVUSER=$opt_u;
}
$HOME="/home/$GPSCVUSER";

if ($opt_v){
	print "$VERSION\n";
	exit;
}

if ($opt_h){
	print "install.pl $VERSION\n";
	print "-d make distribution only\n";
	print "-h print help\n";
	print "-t verbose\n";
	print "-u <user> install in this users home directory\n";
	print "-v print version\n";
	exit;
}

# Set the gpscv user
$ret = `grep $GPSCVUSER /etc/passwd`;
if (!($ret =~/$GPSCVUSER/)){
	print "\nThe GPSCV user ($GPSCVUSER) was not detected\n";
	print "Please specify the user with the -u option\n";
	exit;
}

open (LOG,">$HOME/install.log");
#`chown $GPSCVUSER. $HOME/install.log`;

Log ("\n\n+++++++++++++++++++++++++++++++++++\n",$ECHO);
Log ("+   OpenTTP GPSCV software installation\n",$ECHO);
Log ("+   $VERSION\n",$ECHO);
Log ("+++++++++++++++++++++++++++++++++++\n",$ECHO);


# Detect the operating system version
@os =(
			["Red Hat Enterprise Linux WS release 6","rhel6"],
			["Centos 6","centos6"],
			["Ubuntu 14.04","ubuntu14"]
			);

$thisos = `cat /etc/issue`;
chomp $thisos;
Log ("\nDetected $thisos\n",$ECHO);

for ($i=0;$i<=$#os;$i++){
	last if ($thisos =~/$os[$i][0]/);
}

$osid="linux";
if ($i > $#os){
	Log("This does not appear to be a supported operating system\n",$ECHO);
	print "\nThe supported operating systems are:\n";
	for ($i=0;$i<=$#os;$i++){
		print "\t(",$i+1,") $os[$i][0]\n";
	}
	if (!GetYesNo("Continue anyway?","Continuing installation")){
		exit(1);
	}
}
else{
	$osid = $os[$i][1];
}

# Determine the installation method
$installMethod = $INSTALLFROMDIST;
if (-e "dist"){
	Log("\nInstalling from a binary distribution\n",$ECHO);
}
else{
	$installMethod = $INSTALLFROMSRC;
	Log("\nInstalling from source\n",$ECHO);
}

# Detect whether there is an existing installation that we are upgrading
$upgrade=0;
$config= "/home/$GPSCVUSER/etc/cggtts.conf";
if (-e $config){
	Log("\nDetected an existing installation\n",$ECHO);
	Log("\nUsing $config for configuration\n",$ECHO);
	%Config=TFMakeHash2($config,tolower=>1);
	$upgrade=1;
}
else {
	Log("\nAn existing installation was not detected\n",$ECHO);
}

# Detect/choose GPS receiver
$INSTALLDIR="/home/$GPSCVUSER";
if ($opt_d){
	ChooseReceiver();
	$DIST="$osid-".$receiverNames[$receiver];
	$INSTALLDIR="dist/$DIST";
	if (!(-e $INSTALLDIR)) {`mkdir -p $INSTALLDIR`;}
}
else {
	if (!DetectReceiver()){
		ChooseReceiver();
	}
	$DIST="$osid-".$receiverNames[$receiver];
}


# Offer choice of a test installation if this is an upgrade
if (!$opt_d && $upgrade){
	print "\nA test-only installation can be done. This will allow validation of the new\n";
	print "software by processing existing data and comparing the results with CGGTTS\n";
	print "data created by the current processing software.\n";
	print "The existing software and configuration will not be affected.\n";
	$testInstall=GetYesNo("\nDo you want to do a test install","Test install");
	
	if ($testInstall){
		$INSTALLDIR = "$INSTALLDIR/test-install";
		Log("The test installation will be in $INSTALLDIR\n",$ECHO);
		if (-e "$INSTALLDIR"){
			Log("$INSTALLDIR already exists\n",$ECHO);
			$zap=GetYesNo("Is it OK to remove this directory?","Remove $INSTALLDIR");
			if ($zap){
				`rm -rf $INSTALLDIR`;
			}
			else{
				print "Not sure what you want to do, so ...\n";
				exit;
			}
		}
		mkdir $INSTALLDIR;
	}
}

if (!($testInstall || $opt_d)){ ArchiveSystem(); } 

$BIN = "$INSTALLDIR/bin";
$CGGTTS = "$INSTALLDIR/cggtts";
$PROCESS = "$INSTALLDIR/process";
$CONFIGS = "$INSTALLDIR/etc";
$LOGS = "$INSTALLDIR/logs";
$DATA = "$INSTALLDIR/raw";
$RINEX = "$INSTALLDIR/rinex";

Log ("\n*** Creating installation directories in $INSTALLDIR\n",$ECHO);

@dirs = ($BIN,$PROCESS,$CONFIGS,$LOGS,$DATA,$CGGTTS,$RINEX);

for ($i=0;$i<=$#dirs;$i++){
	Log ("$dirs[$i]",$opt_t);
	if ((-e $dirs[$i])){
		Log (" exists already - skipping\n",$opt_t);
	}
	else{	
		mkdir $dirs[$i];
		Log (" created\n",$opt_t);
	}
}

if ($installMethod==$INSTALLFROMDIST){
	InstallFromBinaryDistribution();
}
else{
	InstallFromSource();
}


FINISH:

Log("\nFinished installation :-)\n",$ECHO);
print("\n\n!!! Please look at the sample configuration files in $CONFIGS -\n".
	"you will need to remove 'sample' from their names to use them.\n");
print "\n\nA log of the installation has been saved in $HOME/install.log\n"; 
print "\t\t\n";
print "\t\t  o\n";
print "\t\t   o\n";
print "\t\t  o\n";  
print "\t\t____\n";
print "\t\t| o |\n";
print "\t\t|   |\n";  
print "\t\t|   |\n";
print "\t\t\\  /\n";
print "\t\t ||\n";
print "\t\t ||\n";
print "\t\t ||\n";
print "\t\t====\n";

close LOG;
exit;

# ------------------------------------------------------------------------
sub Log
{
	my ($msg,$flags)=@_;
	if ($flags == $ECHO) {print $msg;}
	print LOG $msg;
}

# ------------------------------------------------------------------------
sub Install
{
	my $dst = $_[1];
	@files = glob($_[0]);
	for ($i=0;$i<=$#files;$i++){
		if (!(-d $files[$i]) && ((-x $files[$i]) || ($files[$i] =~ /\.conf|\.setup/) )){
			`cp -a $files[$i] $dst`;
			Log ("\tInstalled $files[$i] to $dst\n",$opt_t);
		}
	}
}

# -------------------------------------------------------------------------
sub GetYesNo
{
	my ($prompt,$logmsg)=@_;
	ASKYN:
	print "$prompt (y/n) ?:";
	$ans = <STDIN>;
	chomp $ans;
	if (!($ans eq "y" || $ans eq "Y" || $ans eq "n" || $ans eq "N")){goto ASKYN;}
	
	Log("$logmsg = $ans\n",!$ECHO);
	return ( ($ans eq "y") || ($ans eq "Y"));	
}

# ------------------------------------------------------------
sub DetectReceiver{
	if ($Config{"receiver:manufacturer"}=~/Trimble/){
		$receiver=$TRIMBLE;
		Log("Detected Trimble receiver\n",$ECHO);
	}
	return ($receiver != $UNKNOWN);
}

#---------------------------------------------------------------------------------
sub ChooseReceiver
{
	print "\nSelect your GPS receiver:\n";
	$first = 1;
	$last = $#receivers + 1;
	ASK:
	for ($i=$first;$i<=$last;$i++)
	{
		print "  [$i] ",$receiverNames[$i-1],"\n";
	}
	print "Choose ($first-$last): ";
	$receiver = <STDIN>;
	chomp $receiver;
	if ($receiver < $first || $receiver > $last) {goto ASK;}
	$receiver--;
}

#-----------------------------------------------------------------------------------
sub ArchiveSystem
{
	Log("\nArchiving the existing system ...\n",$ECHO);
	# What do we do
	#		mv process/*
	my $home = "/home/$GPSCVUSER";
	my $archive="$home/archive";
	my ($i,$src);

	if (!(-e $archive)){mkdir $archive;}
	$archive .= '/'.strftime("%Y%m%d",localtime);
	Log("\nArchiving the existing system to $archive\n",$ECHO);
	if ((-e $archive)){
		print "$archive already exists!\n";
		exit;
	}
	mkdir $archive;
	
	@stdbackups = ("bin","etc","logs","firmware","process");
	for ($i=0;$i<=$#stdbackups;$i++){
		$src= "$home/$stdbackups[$i]";
		if (-e $src){
			Log("Backing up $src\n",$opt_t);
			`cp -a $src $archive`;
		}
	}
}

#-----------------------------------------------------------------------------------
sub InstallFromBinaryDistribution
{
	$instimage = "$DIST.tgz";
	if (!-e ("dist/$instimage")){
		print "There is no binary distribution $instimage for your system\n";
		exit;
	}
	
	$tmpdir = "dist/$DIST";
	if (!(-e $tmpdir)){mkdir $tmpdir;}
	`cp dist/$instimage $tmpdir`;
	Log("\nDecompressing installation image\n",$ECHO);
	`cd $tmpdir && tar -xzf $instimage`;
	
	Log("\nCopying files\n",$ECHO);
	
	Log("\nInstalling processing software ...\n",$ECHO);
	
}

#-----------------------------------------------------------------------------------
sub InstallFromSource
{
	Log ("\nCompiling the processing software ...\n",$ECHO);
	
	$out = `cd common/process/mktimetx && make clean && make 2>&1 && cd ../../..`;
	if ($opt_t) {print $out;}
	if ($? >> 8){
		Log ("\nSoftware compilation failed :-(\n",$ECHO);
		exit(1);
	}

	Log("\nInstalling processing software ...\n",$ECHO);
	Install("common/process/mktimetx/*",$PROCESS);
	
	Log("\nInstalling configuration files in $CONFIGS\n",$ECHO);
	Install("common/etc/*",$CONFIGS);
	
	Log("\nInstalling receiver scripts in $BIN\n",$ECHO);
	Install("$receiverDirs[$receiver]/*",$BIN);
}




