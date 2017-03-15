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
# 

use English;
use Getopt::Std;
use Carp;
use FileHandle;
use POSIX;

use vars qw($opt_h $opt_i $opt_l $opt_m $opt_t $opt_v);

$0=~s#.*/##;	# strip path

$VERSION = "version 0.1";
$ECHO=1;

$UPSTART="upstart";
$SYSTEMD="systemd";

# first entry is OS-defined string, second is our name for tarballs etc,
# then init system,Perl library path
@os =(
	["Red Hat Enterprise Linux (WS|Workstation) release 6","rhel6",$UPSTART,"/usr/local/lib/site_perl"], 
	["CentOS release 6","centos6",$UPSTART,"/usr/local/lib/site_perl"],
	["Ubuntu 14.04","ubuntu14",$UPSTART,
		"/usr/local/lib/site_perl","/usr/local/lib/python2.7/site-packages"],
	["BeagleBoard.org Debian","bbdebian8",$SYSTEMD,"/usr/local/lib/site_perl",
		"/usr/local/lib/python2.7/site-packages",]
	);

@defaulttargets = ("libconfigurator","dioctrl","lcdmon","ppsd",
	"sysmonitor","tflibrary","kickstart","gziplogs","misc","ottplib",
	"okcounterd","okbitloader","udevrules");

if (!getopts('hi:lmtv') || $opt_h){
	ShowHelp();	
	exit;
}

@targets=@defaulttargets;
if ($opt_l){
	print "Available targets:\n";
	foreach $target (@targets){
		print "$target\n";
	}
	exit;
}

if ($opt_v){
	print "$0 $VERSION\n";
	exit;
}

if ($opt_i){
	@targets = ($opt_i);
}
	
if ($EFFECTIVE_USER_ID >0){
	print "$0 must be run as superuser!\n";
	exit;
}

open (LOG,">./installsys.log");

$thisos = `cat /etc/issue`;
chomp $thisos;

Log("\n/etc/issue: $thisos\n");

for ($i=0;$i<=$#os;$i++){
	last if ($thisos =~/$os[$i][0]/);
}

if ($i <= $#os){
	Log("Detected $os[$i][0]\n",$ECHO);
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

$initsys = $os[$i][2];

# Low-level stuff first

if (!(-e '/usr/local/lib/bitfiles')){
	`mkdir /usr/local/lib/bitfiles`;
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
				`mkdir $dir`;
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
if (grep (/^dioctrl/,@targets)) {CompileTarget('dioctrl','src/dioctrl','install');}
if (grep (/^lcdmon/,@targets)) {CompileTarget('lcdmon','src/lcdmon','install');}
if (grep (/^okbitloader/,@targets)) {CompileTarget('okbitloader','src/okbitloader','install');}

if (grep (/^okcounterd/,@targets)) {
	CompileTarget('okcounterd','src/okcounterd','install'); # installs executables only
	if ($initsys eq $SYSTEMD){
			InstallScript('src/okcounterd/okcounterd.service','/lib/systemd/system');
			`systemctl enable okcounterd.service`;
			#`systemctl load okcounterd.service`; # seem to need the full name
		}
	elsif ($initsys eq $UPSTART){
			InstallScript('src/okcounterd/okcounterd.upstart.conf','/etc/init/okcounterd.conf');
	}
}

if (grep (/^ppsd/,@targets) && !($os[$i][1] eq 'bbdebian8')){ #FIXME disabled temporrarily for ARM
	CompileTarget('ppsd','src/ppsd','install');
	if ($initsys eq $SYSTEMD){
			InstallScript('src/ppsd/ppsd.service','/lib/systemd/system');
			`systemctl enable ppsd.service`;
			`systemctl load ppsd.service`; # seem to need the full name
	}
	elsif ($initsys eq $UPSTART){
			InstallScript('src/ppsd/ppsd.upstart.conf','/etc/init/ppsd.conf');
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
			`systemctl load sysmonitor.service`; # seem to need the full name
		}
		elsif ($initsys eq $UPSTART){
			InstallScript('src/sysmonitor/sysmonitor.upstart.conf','/etc/init/sysmonitor.conf');
		}
}


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

