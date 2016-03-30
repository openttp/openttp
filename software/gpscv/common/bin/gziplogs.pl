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
# Modification history
# 2016-03-30 MJW Replaces zip_data. First version.
#

use POSIX;
use Errno;
use Getopt::Std;
use IO::Socket;
use TFLibrary;

use vars qw($opt_c $opt_d $opt_h $opt_m $opt_v);

$VERSION="0.1.0";
$AUTHOR="Michael Wouters";

# Check command line
if ($0=~m#^(.*/)#) {$path=$1} else {$path="./"}	# read path info
$0=~s#.*/##;					# then strip it

if (!(getopts('c:dhm:v')) || $opt_h){
	&ShowHelp();
	exit;
}

if ($opt_v){
	print "$0 version $VERSION\n";
	print "Written by $AUTHOR\n";
	exit;
}

$tnow = time();
$mjd =  int($tnow/86400 + 40587-1); # yesterday
@gmt = gmtime ($tnow - 86400);
$ymd = strftime("%Y%m%d",@gmt);

if (defined $opt_m){
	$mjd=$opt_m;
	$tthen = ($mjd - 40587)*86400;
	@gmt = gmtime ($tthen);
	$ymd = strftime("%Y%m%d",@gmt);
}

$home=$ENV{HOME};
if (-d "$home/etc")  {
	$configpath="$home/etc";
}
else{	
	ErrorExit("No ~/etc directory found!\n");
} 

$configFile=$configpath."/gziplogs.conf";
if (defined $opt_c){
	$configFile=$opt_c;
}

if (!(-e $configFile)){
	ErrorExit("The configuration file $configFile was not found!\n");
}

%Init = &TFMakeHash2($configFile,(tolower=>1));

# Check we got the info we need from the config file
@check=("files");
foreach (@check) {
  $tag=$_;
  $tag=~tr/A-Z/a-z/;	
  unless (defined $Init{$tag}) {ErrorExit("No entry for $_ found in $configFile")}
}

@files = split /,/,$Init{"files"};

for ($i=0;$i<=$#files;$i++){
	$files[$i]=~ s/^\s+//; # trim white space
	$files[$i]=~ s/\s+$//;
	$files[$i]=~ s/{MJD}/$mjd/;
	$files[$i]=~ s/{YYYYMMDD}/$ymd/;
	$files[$i]=TFMakeAbsoluteFilePath($files[$i],$home,$home."/raw");
	Debug("Processing $files[$i]");
	if (-e $files[$i]){
		`gzip $files[$i]`;
	}
}


# End of main program

#-----------------------------------------------------------------------
sub ShowHelp{
	print "Usage: $0 [OPTION] ...\n";
	print "\t-c <config> specify alternate configuration file\n";
  print "\t-d turn on debugging\n";
  print "\t-h show this help\n";
  print "\t-m <MJD>  compress files for specified MJD\n";
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

# -------------------------------------------------------------------------
sub Debug
{
	if ($opt_d){
		printf STDERR "$_[0]\n";
	}
}


