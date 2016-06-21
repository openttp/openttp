#!/usr//bin/perl -w

#
# The MIT License (MIT)
#
# Copyright (c) 2016  Michael J. Wouters
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
# Modification history
#

use POSIX;
use Getopt::Std;
use vars qw($opt_c $opt_h $opt_l $opt_r $opt_v);

$VERSION = '0.1';
$AUTHORS = 'Michael Wouters';

$0=~s#.*/##; # strip path from executable name

@receivers = ('javad','trimble');

if (!getopts('c:hlr:v') || $opt_h){
	ShowHelp();	
	exit;
}

if ($opt_v){
	print "$0 version $VERSION\n";
	print "Written by $AUTHORS\n";
	exit;
}

$rx="";
if ($opt_r){
	$rx=$opt_r;
}

if ($opt_l){
	$fname = strftime "%F-%H:%M:%S",gmtime;
	open (OUT,">log/$fname.txt");
}

$mktimetx = '../mktimetx/mktimetx';
if (!(-e $mktimetx)){
	print "Unable to find $mktimetx !\n";
	exit;
}

$rxdelaycal = '../../bin/rxdelaycal.pl';
if (!(-e $rxdelaycal)){
	print "Unable to find $rxdelaycal !\n";
	exit;
}

Log("Run at ".(strftime "%F %H:%M:%S",gmtime));

if ($opt_c){
	Log("Comment:" . $opt_c);
}

# Get versions etc
@ver = split /\n/, `$mktimetx --version`;
Log("Using $mktimetx ($ver[0])");

@ver = split /\n/, `$rxdelaycal -v`;
Log("Using $rxdelaycal ($ver[0])");

$mjdStart=-1;
$mjdStop=-1;
for ($r=0;$r<=$#receivers;$r++){
	next unless (($rx eq "") || ($rx eq $receivers[$r]));
	Log("\nValidating $receivers[$r] CGGTTS output");
	# Determine the available MJDs
	@files = glob("$receivers[$r]/raw/*.rxrawdata");
	for ($f=0;$f<=$#files;$f++){
		$files[$f] =~ /(\d{5})\.rxrawdata/;
		$mjd=$1;
		if ($f==0){$mjdStart=$mjd;}
		if ($f==$#files){$mjdStop=$mjd;}
		Log("Processing $mjd");
		`$mktimetx --configuration $receivers[$r]/etc/gpscv.conf -m $mjd`;
		# Get the processing log
		$log = `cat $receivers[$r]/tmp/mktimetx.log`;
		Log($log);
		
	}
	`$rxdelaycal -a -f tmp $receivers[$r]/cggtts-old $receivers[$r]/cggtts $mjdStart $mjdStop`;
	
	$ret = `grep matching tmp/ref.cal.report.txt`;
	$ret =~/^\s*(\d+)/;
	Log("Matched tracks = $1");
	
	$ret = `grep 'mean offset' tmp/ref.cal.report.txt`;
	@bits = split /\s+/,$ret;
	Log("Mean offset = $bits[5] ns");
	
	$ret = `grep 'RMS' tmp/ref.cal.report.txt`;
	@bits = split /\s+/,$ret;
	Log("RMS residuals = $bits[3] ns");
	
	$ret = `grep 'slope' tmp/ref.cal.report.txt`;
	@bits = split /\s+/,$ret;
	Log("Slope = $bits[1] ps/s");
	
}

if	($opt_l){
	close OUT;
}
	
# ------------------------------------------------------------------------
sub ShowHelp
{
	print "Usage: $0 [options]\n\n";
	print "-c <comment> add comment to log\n";
	print "-h show this help\n";
	print "-l log results to file in validations/\n";
	print "-r <receiver> validate <receiver> only\n";
	print "-v print version\n";
	print "\n valid receivers are ";
	foreach $rx (@receivers){
		print "$rx ";
	}
	print "\n";
}

# ------------------------------------------------------------------------
sub Log
{
	print $_[0],"\n";
	if ($opt_l){
		print OUT $_[0],"\n";
	}
}