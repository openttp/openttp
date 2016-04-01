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

# Script for controlling the Opal Kelly XEM6001, via okcounterd
# Derived from get_counter_data_okxem
#
# Modification history
# 2016-03-17 MJW Initial version 0.1
# 2016-04-01 MJW Removed backwards compatibility
#

use POSIX;
use Getopt::Std;
use IO::Socket;
use TFLibrary;

use vars qw($opt_d $opt_c $opt_g $opt_h $opt_o $opt_q $opt_v);

$VERSION="0.1";
$AUTHOR="Michael Wouters";

# Check command line
if ($0=~m#^(.*/)#) {$path=$1} else {$path="./"}	# read path info
$0=~s#.*/##;					# then strip it

if (!(getopts('c:dg:ho:qv')) || $opt_h){
	&ShowHelp();
	exit;
}

if ($opt_v){
	print "$0 version $VERSION\n";
	print "Written by $AUTHOR\n";
	exit;
}

# Read the config file
$configFile = "";
if ($opt_c){
	$configFile=$opt_c;
}
else{
	$user = "cvgps";
	$configFile="/home/$user/etc/gpscv.conf";
	if (!(-e $configFile)){
		print "Unable to find $configFile";
		exit;
	}
}


%Init = &TFMakeHash2($configFile,(tolower=>1));

# Check we got the info we need from the setup file
@check=("counter:port");
foreach (@check) {
  $tag=$_;
  $tag=~tr/A-Z/a-z/;	
  unless (defined $Init{$tag}) {ErrorExit("No entry for $_ found in $configFile")}
}

$port = $Init{"counter:port"};

$sock=new IO::Socket::INET(PeerAddr=>'localhost',PeerPort=>$port,Proto=>'tcp',);
unless ($sock) {ErrorExit("Could not create a socket at $port - okcounterd not running?");} 

if (defined $opt_g){
	if ($opt_g eq '0' || $opt_g eq '1'){
		$cmd = "CONFIGURE GPIO $opt_g";
		Debug(($opt_g ? "Enabling" : "Disabling")." the GPIO ");
		Debug("Sending $cmd\n");
		$sock->send($cmd);
		sleep(1);
	}
	else{
		print "Invalid value for option -g: $opt_g\n";
		ShowHelp();
		exit;
	}
}

if (defined $opt_o){
	if ($opt_o >=1 && $opt_o <=6){
		Debug("Setting the counter output pps source");
		$cmd = "CONFIGURE PPSSOURCE $opt_o";
		Debug("Sending $cmd\n");
		$sock->send($cmd);
		sleep(1);
	}
	else{
		print "Invalid value for option -o: $opt_g\n";
		ShowHelp();
		exit;
	}
}

if (defined $opt_q){
	Debug("Querying the counter configuration");
	$cmd = "QUERY CONFIGURATION";
	Debug("Sending $cmd\n");
	$sock->send($cmd);
	$sock->recv($response,64);
	sleep(1);
	print "Configuration: $response \n";
}

$sock->close();

# End of main program

#-----------------------------------------------------------------------
sub ShowHelp{
	print "Usage: $0 [OPTION]\n";
  print "\t-d        turn on debugging\n";
  print "\t-c <file> use this gpscv configuration file\n";
  print "\t-g <0/1>  disable/enable GPIO\n";
  print "\t-h        show this help\n";
  print "\t-o <1..6> set counter output pps source\n";
  print "\t-q        query configuration\n";
  print "\t-v        print version\n";
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
	if ($opt_d)
	{
		($sec,$min,$hour)=gmtime(time());
		printf STDERR "%02d:%02d:%02d $_[0]\n",$hour,$min,$sec;
	}
}
