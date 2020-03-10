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
# 2017-03-16 MJW Additions for Debian on BeagleBone Block. Version changed to 0.1.1
# 2017-05-11 MJW Ubuntu16
# 2017-12-08 MJW CentOS 7 fixups

use English;
use Getopt::Std;
use Carp;
use FileHandle;
use TFLibrary;
use POSIX;

use vars qw($opt_d $opt_h $opt_i $opt_t $optu $opt_v);

$0=~s#.*/##;	# strip path

$VERSION = "version 0.1.3 ";

$ECHO=1;
$UNKNOWN=-1;
#$INSTALLFROMSRC=0;
#$INSTALLFROMDIST=1;
$GPSCVUSER=$ENV{USER};

@os =(
	["Red Hat Enterprise Linux (WS|Workstation) release 6","rhel6"], # first entry is OS-defined string, second is our name
	["CentOS release 6","centos6"],
	["CentOS Linux 7","centos7"],
	["Ubuntu 14.04","ubuntu14"],
	["Ubuntu 16.04","ubuntu16"],
	["BeagleBoard.org Debian","bbdebian8"], # FIXME this may not be set in stone ...
	["Raspbian GNU/Linux 9 (stretch)","rpidebian9"]
	);

@receivers = (
	["Trimble","Resolution T"], # manufacturer, model
	["Javad","any"],
	["NVS","NV08C"],
	["ublox","NEO8T"]
);

$receiver=$UNKNOWN;

if (!getopts('nvhi:tu:d') || $opt_h){
	ShowHelp();	
	exit;
}

if ($opt_v){
	print "$0 $VERSION\n";
	exit;
}

if (defined $opt_u){
	$GPSCVUSER=$opt_u;
}

$ret = `grep $GPSCVUSER /etc/passwd`;
if (!($ret =~/$GPSCVUSER/)){
	print "\nThe GPSCV user ($GPSCVUSER) was not detected\n";
	print "Please check the user name\n";
	exit;
}
$HOME="/home/$GPSCVUSER";

open (LOG,">./install.log");
#`chown $GPSCVUSER. $HOME/install.log`;

Log ("\n\n+++++++++++++++++++++++++++++++++++++++\n",$ECHO);
Log ("+   OpenTTP GPSCV software installation\n",$ECHO);
Log ("+   $VERSION\n",$ECHO);
Log ("+++++++++++++++++++++++++++++++++++++++\n",$ECHO);

# Detect the operating system version
# Note that you can use regexes in the OS strings 

# Try for /etc/os-release first (systemd systems only)
if ((-e "/etc/os-release")){
        $thisos = `grep '^PRETTY_NAME' /etc/os-release | cut -f 2 -d "="`;
        chomp $thisos;
        $thisos =~ s/\"//g;
}
else{
        $thisos = `cat /etc/issue`;
        chomp $thisos;
}

Log ("\nDetected $thisos\n",$ECHO);

for ($i=0;$i<=$#os;$i++){
	last if ($thisos =~/\Q$os[$i][0]\E/);
}

$osid="linux";
if ($i > $#os){
	Log("This does not appear to be a supported distribution\n",$ECHO);
	print "\nThe supported distributions are:\n";
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

# Detect whether there is an existing installation that we are upgrading
$upgrade=0;
$config= "$HOME/etc/gpscv.conf";
if (-e $config){
	Log("\nDetected an existing installation\n",$ECHO);
	Log("\nUsing $config for configuration\n",$ECHO);
	%Config=TFMakeHash2($config,tolower=>1);
	$upgrade=1;
}
else {
	Log("\nAn existing installation was not detected\n",$ECHO);
}

$INSTALLDIR = $HOME;
$DATADIR = $HOME;
if ($os[$i][1] eq "bbdebian8" || $os[$i][1] eq "rpidebian9" ){
	$DATADIR = "/mnt/data/$GPSCVUSER";
	die "$DATADIR doesn't exist" unless (-e $DATADIR);
}

# FIXME this is broken on systems where the data directory is symlinked
if ($opt_i){
	$INSTALLDIR="$HOME/$opt_i";
	if (!(-e $INSTALLDIR)){
		die "Unable to create $INSTALLDIR" unless(mkdir $INSTALLDIR);
		Log("Created $INSTALLDIR",$ECHO);
	}
	else{
		print "\n$INSTALLDIR already exists\n";
		if (!GetYesNo("Delete it?","Deleting $INSTALLDIR")){
			exit(1);
		}
		`rm -rf $INSTALLDIR`;
		mkdir $INSTALLDIR;
	}
}

# Detect/choose GPS receiver
if ($opt_d){
	ChooseReceiver();
	$DIST="$osid";
	$INSTALLDIR="dist/$DIST";
	if (!(-e $INSTALLDIR)) {`mkdir -p $INSTALLDIR`;}
}
else {
	if (!DetectReceiver()){
		ChooseReceiver();
	}
	$DIST="$osid";
}

if ($INSTALLDIR eq $HOME){ ArchiveSystem(); } 

$BIN = "$INSTALLDIR/bin";
$CONFIGS = "$INSTALLDIR/etc";
$CGGTTS = "$DATADIR/cggtts";
$LOGS = "$DATADIR/logs";
$DATA = "$DATADIR/raw";
$RINEX = "$DATADIR/rinex";
$TMP = "$DATADIR/tmp";
if ($INSTALLDIR eq $DATADIR){
	Log ("\n*** Creating directories in $INSTALLDIR\n",$ECHO);
}
else{
	Log ("\n*** Creating directories in $INSTALLDIR and $DATADIR\n",$ECHO);
}

@dirs = ($BIN,$CONFIGS);
@datadirs = ($LOGS,$DATA,$CGGTTS,$RINEX,$TMP);

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

for ($i=0;$i<=$#datadirs;$i++){
	Log ("$datadirs[$i]",$opt_t);
	if ((-e $datadirs[$i])){
		Log (" exists already - skipping\n",$opt_t);
	}
	else{	
		mkdir $datadirs[$i];
		Log (" created\n",$opt_t);
	}
	
	if (!($INSTALLDIR eq $DATADIR)){ # symlink needed
		$datadirs[$i] =~ /\/(\w+)$/;
		$symlink = $INSTALLDIR.'/'.$1;
		if (!(-e $symlink)){
			`ln -s $datadirs[$i] $symlink`;
			Log("Created symlink $symlink\n");
		}
	}
	
}

InstallFromSource();

FINISH:

Log("\nFinished installation :-)\n",$ECHO);
print("\n\n!!! Please look at the sample configuration files in $CONFIGS -\n".
	"you will need to remove 'sample' from their names to use them.\n");
print "\n\nA log of the installation has been saved in ./install.log\n"; 
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
sub ShowHelp
{
	print "$0 $VERSION\n";
	print "\t-d make distribution only\n";
	print "\t-h print help\n";
	print "\t-i <path> alternate install path, relative to user's home directory\n";
	print "\t-t verbose\n";
	print "\t-u <user> install in this user's home directory\n";
	print "\t-v print version\n";
}

# ------------------------------------------------------------------------
sub Log
{
	my ($msg,$flags)=@_;
	if (defined $flags){
		if ($flags == $ECHO) {print $msg;}
	}
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

# --------------------------------------------------------------------------
sub DetectReceiver{
	unless ( defined($Config{"receiver:manufacturer"}) && defined($Config{"receiver:model"}) )
		{return ($receiver != $UNKNOWN);}
	my $rxManufacturer = lc $Config{"receiver:manufacturer"};
	my $rxModel = lc $Config{"receiver:model"};
	for ($i=0;$i<=$#receivers;$i++){
		if (($rxManufacturer eq lc $receivers[$i][0]) && ($rxModel eq lc $receivers[$i][1] || $receivers[$i][1] eq "any")) {
			Log("Detected configuration for $receivers[$i][0] $receivers[$i][1]\n",$ECHO);
			$receiver=$i;
			last;
		}
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
	for ($i=$first;$i<=$last;$i++){
		print "  [$i] $receivers[$i-1][0] $receivers[$i-1][1]\n";
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
	my $archive="$HOME/archive";
	my ($i,$src);

	if (!(-e $archive)){mkdir $archive;}
	$archive .= '/'.strftime("%Y%m%d",localtime);
	Log("\nArchiving the existing system to $archive\n",$ECHO);
	if ((-e $archive)){
		print "$archive already exists!\n";
		exit;
	}
	mkdir $archive;
	
	my @stdbackups = ('bin','etc','firmware','process'); # archive 'process'
	for ($i=0;$i<=$#stdbackups;$i++){
		$src= "$HOME/$stdbackups[$i]";
		if (-e $src){
			Log("Backing up $src\n",$opt_t);
			`cp -a $src $archive`;
		}
	}
}

#-----------------------------------------------------------------------------------
sub CompileTarget
{
	my ($target,$targetDir)= @_;
	Log ("\nCompiling $target ...\n",$ECHO);
	my $cwd = getcwd();
	
	chdir $targetDir;
	$out = `make clean && make 2>&1`;
	if ($opt_t) {print $out;}
	if ($? >> 8){
		Log ("\nCompilation of $target failed :-(\n",$ECHO);
		exit(1);
	}
	chdir $cwd;
}

#-----------------------------------------------------------------------------------
sub InstallFromSource
{
	CompileTarget('mktimetx','common/process/mktimetx');
	CompileTarget('prs10c','prs10');
	
	Log("\nInstalling executables in $BIN\n",$ECHO);
	
	Install("common/process/mktimetx/*",$BIN);
	Install("prs10/*",$BIN);
	Install("common/bin/*",$BIN);
	Install("gpsdo/*.pl",$BIN);
	Install("javad/*.pl",$BIN);
	Install("nvs/*.pl",$BIN);
	Install("trimble/*.pl",$BIN);
	Install("ublox/*.pl",$BIN);
	Install("ublox/*.py",$BIN); # FIXME bodge
	
	Log("\nInstalling configuration files in $CONFIGS\n",$ECHO);
	
	Install("common/etc/*",$CONFIGS);
	Install("common/javad/*.conf",$CONFIGS);
	
}




