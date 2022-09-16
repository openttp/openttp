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


# Modification history:
# 27.10.99  RBW  First version
#  7. 4.00  RBW  Add HOSTNAME; optional commands based on selection clause;
#                add "tail clockset.log" for sites using clockset
# 18. 7.01  RBW  Add check for ntpd for relevant sites
#  3. 9.01  RBW  Add check for tclog as well as for onclog
# 25. 2.02  RBW  Add substitution of MJD
#	               Add check for Topcon output messages
# 21. 1.03  RBW  Ternary forms "{test} if-true : if-false"
#                Each branch can include multiple statements with separator &
#                Use Sys::Hostname, but don't use TFLibrary
#                Various new commands added
# 14. 1.04  RBW  Change ternary form syntax: [] instead of {} (easier for
#                commands; fixes long-standing error in temperature log check)
#                Add check of GPS tracking schedule
# 11-03-2008 MJW Import into CVS from topcondf2. Various cleanups. A few more checks.
# 30-03-2016 MJW Rewrite of check_status. Uses gpscv.conf to determine a few paths etc.
#                Closer conformance with our 'standard' Perl script.
# 2018-09-13 ELM Added ubloxlog to executables to be checked
# 2020-06-16 ELM Added hp5313xlog, LTElog, nv08log, ticclog, log1Wtemp and 
#                logpicputemp to executable to be checked.
#                Also changed -107 > -7 in approx. line 123; we only want last 7 days.
# 2022-05-01 ELM Found another -107, approx line 139
# 2022-09-16 ELM Added command for chrony for sensible results when used instead of
#                ntpd. Fixed issue in 'find ... check' line. Bumped version to 0.2
#

use Sys::Hostname;
use POSIX;
use Getopt::Std;
use TFLibrary;

use vars qw($opt_h $opt_v);

$VERSION="0.2";
$AUTHOR="Bruce Warrington, Michael Wouters";

# Check command line
if ($0=~m#^(.*/)#) {$path=$1} else {$path="./"} # read path info
$0=~s#.*/##;                                    # then strip it

if (!(getopts('hv')) || $opt_h){
	&ShowHelp();
	exit;
}

if ($opt_v){
	print "$0 version $VERSION\n";
	print "Written by $AUTHOR\n";
	exit;
}

$ENV{HOSTNAME}=hostname() unless defined $ENV{HOSTNAME}; # very old Linuxen

$home=$ENV{HOME};

# Assume the following all exist
#$logpath="$home/logs";
#$binpath="$home/bin"; 
$configpath="$home/etc"; 
$cggttspath="$home/cggtts"; 
$rxpath="$home/raw";  
$ticpath="$home/raw";

$configFile=$configpath."/gpscv.conf";
if (!(-e $configFile)){
	ErrorExit("The configuration file $configFile was not found!");
	exit;
}

%Init = &TFMakeHash2($configFile,(tolower=>1));

# Check we got the info we need from the config file
@check=();
foreach (@check) {
  $tag=$_;
  $tag=~tr/A-Z/a-z/;	
  unless (defined $Init{$tag}) {ErrorExit("No entry for $_ found in $configFile")}
}

if (defined $Init{"paths:cggtts"}){
	$cggttspath=$Init{"paths:cggtts"};
	$cggttspath=TFMakeAbsolutePath($cggttspath,$home);
}

if (defined $Init{"paths:receiver data"}){
	$rxpath=$Init{"paths:receiver data"};
	$rxpath=TFMakeAbsolutePath($rxpath,$home);
}

if (defined $Init{"paths:counter data"}){
	$ticpath=$Init{"paths:counter data"};
	$ticpath=TFMakeAbsolutePath($ticpath,$home);
}

@command=(
 'echo $HOSTNAME',
 'date',
 'df',
 'uptime',
# "find $cggttspath -mtime -107 -printf \"%Ab %Ad %AH:%AM %s\t%p\n\" | sort ",
 "find $cggttspath -mtime -7 -printf \"%Ab %Ad %AH:%AM %s\t%p\n\" | sort ",
# 'ps x | grep --extended-regexp "jnslog|restlog|prs10log|okxemlog|plrxlog" | grep -v grep',
# 'ps x | grep --extended-regexp "jnslog|restlog|prs10log|okxemlog|plrxlog|ubloxlog" | grep -v grep',
 'ps x | grep --extended-regexp "ublox9log|jnslog|restlog|prs10log|okxemlog|plrxlog|ubloxlog|hp5313xlog|LTElog|nv08log|ticclog|log1Wtemp|logpicputemp" | grep -v grep',
# 'find . -name "*.check" -printf "%Ab %Ad %AH:%AM %f\n"',
 'find $HOME -name "*.check" -printf "%Ab %Ad %AH:%AM %f\n"',
 'ps ax | grep ntpd | grep -v grep & /usr/local/bin/ntpq -p',
 'ps ax | grep chrony | grep -v grep & chronyc sources'
 );
 
if ($ticpath eq  $rxpath){ # avoid duplicate output
	push (@command,"find $ticpath -follow -mtime -7 -printf \"%Ab %Ad %AH:%AM %s\t%p\n\" | sort ");
}
else{
 #push (@command,"find $ticpath -follow -mtime -7 -printf \"%Ab %Ad %AH:%AM %s\t%p\n\" | sort ;".
 #"find $rxpath -follow -mtime -107 -printf \"%Ab %Ad %AH:%AM %s\t%f\n\" | sort ");
 # Louis Marais, 2022-05-01 Changed -107 to -7; we only want the last 7 days not the last 107 days.
 push (@command,"find $ticpath -follow -mtime -7 -printf \"%Ab %Ad %AH:%AM %s\t%p\n\" | sort ;".
 "find $rxpath -follow -mtime -7 -printf \"%Ab %Ad %AH:%AM %s\t%f\n\" | sort ");
}


#  ,
#  
#  '[`ps x | grep jnslog | grep -v grep`]' .
#     'grep "@ RXID" '."$rawpath/%%.rxrawdata &" .
#     'tail -100000 '."$rawpath/%%.rxrawdata | $binpath/listmsg",
#  "[-e \"$logpath/prs10.history\"] tail -5 $logpath/prs10.history",
#  'today=`date "+%d/%m/%y"`; grep -H $today '."$logpath/*_restart.log",
#  
# );

$mjd=int(time()/86400) + 40587;
@execute=();
foreach (@command) {
	if (/^\[(.+)\]([^:]+)(:(.+))?/) {
		if (eval $1) {$_=$2}
		elsif ($3) {$_=$4}
		else {next}
	}
	s/%%/$mjd/g;
	push @execute,(split /&/,$_);
}

foreach (@execute) {s/^\s+//; print "# $_\n",`$_ 2>&1`,"\n"}

#-----------------------------------------------------------------------
sub ShowHelp{
	print "Usage: $0 [OPTION] ...\n";
  print "\t-h show this help\n";
  print "\t-v print version\n";
}

#-----------------------------------------------------------------------
sub ErrorExit {
  my $message=shift;
  @_=gmtime(time());
  printf "%02d/%02d/%02d %02d:%02d:%02d $message\n",
    $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
  exit;
}
