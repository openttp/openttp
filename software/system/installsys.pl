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
# System software installation script
#
# Modification history
# 2016-06-19 MJW First version
# 2017-12-08 MJW Fixups for CentOS 7 and added missing GetYesNo
#

use English;
use Getopt::Std;
use Carp;
use FileHandle;
use POSIX;

use vars qw($opt_h $opt_i $opt_l $opt_m $opt_t $opt_v);

$0=~s#.*/##;	# strip path

$VERSION = "version 0.1.2";
$ECHO=1;

$UPSTART="upstart";
$SYSTEMD="systemd";

# first entry is OS-defined string, second is our name for tarballs etc,
# then init system,Perl library path
@os =(
	["Red Hat Enterprise Linux (WS|Workstation) release 6","rhel6",$UPSTART,"/usr/local/lib/site_perl"], 
	["CentOS release 6","centos6",$UPSTART,"/usr/local/lib/site_perl"],
	["CentOS Linux 7","centos7",$SYSTEMD,"/usr/local/lib64/perl5","/usr/local/lib/python2.7/site-packages"],
	["Ubuntu 14.04","ubuntu14",$UPSTART,
		"/usr/local/lib/site_perl","/usr/local/lib/python2.7/site-packages"],
	["Ubuntu 16.04","ubuntu16",$UPSTART,
		"/usr/local/lib/site_perl","/usr/local/lib/python2.7/site-packages"],
	["Ubuntu 18.04","ubuntu18",$SYSTEMD,
		"/usr/local/lib/site_perl","/usr/local/lib/python2.7/site-packages"],
	["BeagleBoard.org Debian","bbdebian8",$SYSTEMD,"/usr/local/lib/site_perl",
		"/usr/local/lib/python2.7/site-packages"],
	['Debian GNU/Linux 9 (stretch)',"bbdebian9",$SYSTEMD,"/usr/local/lib/site_perl",
		"/usr/local/lib/python2.7/site-packages"],
	['Raspbian GNU/Linux 9 (stretch)',"rpidebian9",$SYSTEMD,"/usr/local/lib/site_perl",
		"/usr/local/lib/python2.7/site-packages"],

	);

@defaulttargets = ("libconfigurator","dioctrl","lcdmon","ppsd",
	"sysmonitor","tflibrary","kickstart","gziplogs","misc","ottplib",
	"okcounterd","okbitloader","udevrules","gpscvperllibs");

@minimaltargets = ("libconfigurator","tflibrary","kickstart","gziplogs","misc","ottplib");
	
$hints="";

if (!getopts('hi:lmtv') || $opt_h){
	ShowHelp();	
	exit;
}

@targets=@defaulttargets;
if ($opt_l){
	print "Available targets:\n";
	foreach $target (@targets){
		print "\t$target\n";
	}
	exit;
}

if ($opt_v){
	print "$0 $VERSION\n";
	exit;
}

if ($opt_m){
	@targets = @minimaltargets;
	print "Minimal install:\n";
	foreach $target (@targets){
		print "\t$target\n";
	}
}

if ($opt_i){
	@targets = ();
	foreach $target (@defaulttargets){
		if ($target eq $opt_i){
			@targets=($opt_i);
			last;
		}
	}
}

if ($#targets==-1){
	print "No valid installation targets!\n";
	print "Available targets:\n";
	foreach $target (@defaulttargets){
		print "\t$target\n";
	}
	exit;
}
	
if ($EFFECTIVE_USER_ID >0){
	print "$0 must be run as superuser! (EUID =$EFFECTIVE_USER_ID) \n";
	exit;
}

open (LOG,">./installsys.log");

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

Log("\n/etc/issue: $thisos\n");

for ($i=0;$i<=$#os;$i++){
	last if ($thisos =~ /\Q$os[$i][0]\E/);
}

$osindex = $i;

if ($osindex <= $#os){
	Log("Detected $os[$i][0]\n",$ECHO);
}

$osid="linux";
if ($osindex > $#os){
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
	$osid = $os[$osindex][1];
}

$initsys = $os[$osindex][2];

$arch = `uname -m`;
chomp $arch;

# Low-level stuff first

if (!(-e '/usr/local/lib/bitfiles')){
	`mkdir /usr/local/lib/bitfiles`;
}

if (!(-e $os[$osindex][3])){ # Perl libraries
	`mkdir $os[$osindex][3]`;
}

if (grep (/^udevrules/,@targets)){
	InstallScript("udev/60-opalkelly.rules","/etc/udev/rules.d");
	$cmdout = `udevadm trigger`;
	if ($opt_t){print $cmdout;}
	Log("Installed udev rules\n",$ECHO);
}

# Installation of TFLibrary (Perl module)
if (grep (/^tflibrary/,@targets)){
	$dir =  $os[$i][3]; 
	if (!(-e $dir)){
		`mkdir $dir`;
		`chmod a+rx $dir`;
		Log("Created $dir\n",$ECHO);
	}
	`cp src/TFLibrary.pm $dir`;
	Log("Installed TFLibrary to $dir\n",$ECHO);
}

# Installation of miscellaneous GPSCV Perl libraries
if (grep (/^gpscvperllibs/,@targets)){
	$dir =  $os[$i][3]; 
	@libs=(["../gpscv/gpsdo/LTELite","LTELite"],["../gpscv/nvs/lib","NV08C"]);
	for ($i=0;$i<=$#libs;$i++){
		$libdir = $dir.'/'.$libs[$i][1];
		if (!(-e $libdir)){
			`mkdir $libdir`;
			`chmod a+rx $libdir`;
			Log("Created $libdir\n",$ECHO);
		}
		`cp $libs[$i][0]/*.pm $libdir`;
		Log("Installed $libs[$i][0]/*.pm to $libdir\n",$ECHO);
	}
}

# Installation of ottplib (Python module)
if (grep (/^ottplib/,@targets)){
	# Check the python version
	$ver = `python -V 2>&1`; # output is to STDERR
	chomp $ver;
	$ver =~ /^Python\s+(\d)\.(\d)/;
	if ($1 == 2){
		if ($2 >= 7){
			$dir = $os[$i][4];
			if (!(-e $dir)){
				`mkdir -p $dir`;
				`chmod a+rx $dir`;
				Log("Created $dir\n",$ECHO);
			}
			`python -c "import py_compile;py_compile.compile('src/ottplib.py')"`;
			`cp src/ottplib.py src/ottplib.pyc $dir`;
			Log("Installed ottlib.py to $dir\n",$ECHO);
		}
		else{
			Log("Python version is $ver - can't install\n",$ECHO);
		}
	}
	else{
		Log("Python version is $ver - can't install\n",$ECHO);
	}
}

if (grep (/^libconfigurator/,@targets)) {CompileTarget('libconfigurator','src/libconfigurator','install')};

if (grep (/^dioctrl/,@targets) ) {
	if ($arch =~ /arm/){
		Log("dioctrl is not supported on ARM\n",$ECHO);
	}
	else{
		CompileTarget('dioctrl','src/dioctrl','install');
	}
}
if (grep (/^lcdmon/,@targets)) {
	CompileTarget('lcdmon','src/lcdmon','install');
	if ($initsys eq $SYSTEMD){
		$hints .= "To start lcdmon, run: systemctl start lcdmonitor.service\n";
	}
	elsif ($initsys eq $UPSTART){
		$hints .= "To start lcdmon, run: start lcdmonitor\n";
	}
	
}
if (grep (/^okbitloader/,@targets)) {CompileTarget('okbitloader','src/okbitloader','install');}

if (grep (/^okcounterd/,@targets)) {
	CompileTarget('okcounterd','src/okcounterd','install'); 
	if ($initsys eq $SYSTEMD){
		$hints .= "To start okcounterd, run: systemctl start okcounterd.service\n";
	}
	elsif ($initsys eq $UPSTART){
		$hints .= "To start okcounterd, run: start okcounterd\n";
	}
}

if (grep (/^ppsd/,@targets) ){ #FIXME disabled temporarily for ARM
	if ($arch =~ /arm/){
		Log("ppsd is not supported on ARM\n",$ECHO);
	}
	else{
		CompileTarget('ppsd','src/ppsd','install');
		if ($initsys eq $SYSTEMD){
			$hints .= "To start ppsd, run: systemctl start ppsd.service\n";
		}
		elsif ($initsys eq $UPSTART){
			$hints .= "To start ppsd, run: start ppsd\n";
		}
	}
}

if (grep (/^misc/,@targets)) {CompileTarget('misc','src','install');}

if (grep (/^kickstart/,@targets)){
	`cp src/kickstart.pl /usr/local/bin`;
	Log("Installed kickstart.pl to /usr/local/bin\n",$ECHO);
}

if (grep (/^gziplogs/,@targets)){
	`cp src/gziplogs.pl /usr/local/bin`;
	Log("Installed gziplogs.pl to /usr/local/bin\n",$ECHO);
}

# Installation of sysmonitor
if (grep (/^sysmonitor/,@targets)){
	MakeDirectory('/usr/local/log');
	MakeDirectory('/usr/local/log/alarms');
	InstallScript('src/sysmonitor/sysmonitor.pl','/usr/local/bin',
		'src/sysmonitor/sysmonitor.conf','/usr/local/etc');
		if ($initsys eq $SYSTEMD){
			InstallScript('src/sysmonitor/sysmonitor.service','/lib/systemd/system');
			`systemctl enable sysmonitor.service`;
			#`systemctl start sysmonitor.service`; # seem to need the full name
			$hints .= "To start sysmonitor, run: systemctl start sysmonitor.service\n";
		}
		elsif ($initsys eq $UPSTART){
			InstallScript('src/sysmonitor/sysmonitor.upstart.conf','/etc/init/sysmonitor.conf');
			$hints .= "To start sysmonitor, run: start sysmonitor\n";
		}
}


print $hints;

close LOG;

# ------------------------------------------------------------------------
sub ShowHelp
{
	print "$0 $VERSION\n";
	print "\t-h print help\n";
	print "\t-i <target> install target\n"; 
	print "\t-l list targets\n";
	print "\t-m minimal installation\n";
	print "\t-t verbose\n";
	print "\t-v print version\n";
	print "\nMust be run as superuser\n";
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

#-----------------------------------------------------------------------------------
sub CompileTarget
{
	my ($target,$targetDir)= @_;
	my $makeargs = "";
	if (defined $_[2]){$makeargs=$_[2];}
	
	Log ("\nCompiling $target ...\n",$ECHO);
	my $cwd = getcwd();
	chdir $targetDir;
	
	if (-e 'configure.pl'){
		my $configout = `./configure.pl`;
		if ($opt_t) {print $configout;}
	}
	
	my $makeout = `make clean && make $makeargs 2>&1`;
	if ($opt_t) {print $makeout;}
	if ($? >> 8){
		Log ("\nCompilation/installation of $target failed :-(\n",$ECHO);# ------------------------------------------------------------------------
		exit(1);
	}
	chdir $cwd;
}

# ------------------------------------------------------------------------
sub MakeDirectory
{
	$dir = $_[0];
	if (!(-e $dir)){
		`mkdir $dir`;
		Log("Created $dir\n",$ECHO);
	}
	else{
		Log("$dir already exists - skipped\n",$ECHO);
	}
}

# ------------------------------------------------------------------------
# Arguments are pairs of (file,destination)
sub InstallScript
{
	my @targets = @_;
	my $i;
	for ($i=0;$i<=$#targets;$i+=2){
		`cp $targets[$i] $targets[$i+1]`;
		Log("Copied $targets[$i] to $targets[$i+1]\n",$ECHO);
	}
	
}

