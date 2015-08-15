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

# restplayer.pl plays back Resolution T GPS data files to a serial port
# This is useful for testing ntpd
# It waits in a loop 

use Getopt::Std;
use Time::HiRes qw( usleep gettimeofday);
use TFLibrary;
use POSIX;
use vars qw( $opt_h );

$port="/dev/ttyS2";

if ($#ARGV != 4){
	&ShowHelp();
	exit;
}

$startMJD=$ARGV[0];
$startTime=$ARGV[1]; # seconds into day
$stopMJD=$ARGV[2];
$stopTime=$ARGV[3]; # seconds into the day
$port = $ARGV[4];

$home=$ENV{HOME};
if (-d "$home/etc")  {$configpath="$home/etc";}  else 
	{$configpath="$home/Parameter_Files";} # backwards compatibility

# More backwards compatibility fixups
if (-e "$configpath/cggtts.conf"){
	$configFile=$configpath."/cggtts.conf";
	$localMajorVersion=2;
}
elsif (-e "$configpath/cctf.setup"){
	$configFile="$configpath/cctf.setup";
	$localMajorVersion=1;
}
else{
	print STDERR "No configuration file found!\n";
	exit;
}

&Initialise($configFile);

if ($localMajorVersion==1){
	$rawpath=$Init{"data path"}; #."/$mjd".
	$ext = $Init{"gps data extension"};
}
elsif ($localMajorVersion==2){
	$rawpath=$Init{"paths:receiver data"}; # ."/$mjd".
	$ext = $Init{"receiver:file extension"};
}

$rawpath = &FixPath( $rawpath);


for ($mjd=$startMJD;$mjd<=$stopMJD;$mjd++){ # decompress
	$infile=$rawpath.'/'.$mjd.$ext;
	$zipfile=$infile.".gz";
	if (-e $zipfile){
		`gunzip $zipfile`;
		push @zipped,$infile;
	}
}

print "Opening\n";

$rx=&TFConnectSerial($port,
      (ispeed=>0010002,ospeed=>0010002,iflag=>IGNBRK,
       oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|CLOCAL));

print "Port open\n";
	 
for ($mjd=$startMJD;$mjd<=$stopMJD;$mjd++){
	$lastTime=86399;
	if ($mjd==$stopMJD){
		$lastTime=$stopTime;
	}
	
	$infile=$rawpath.'/'.$mjd.$ext;
	print "$infile\n";
	open (IN,"<$infile");
		
	if ($mjd==$startMJD){ # skip to startTime
		while ($msg=<IN>){
			if ($msg =~ /^# /) { next;}
			if ($msg =~ /^@ /) { next;}
			($msgid,$tod,$msgdata) = split " ",$msg;
			chomp $msgdata;
			$tod =~ /(\d{2}):(\d{2}):(\d{2})/;
			unless ((defined $1) && (defined $2) && (defined $3)) {next;}
			$tod = $1*3600 + $2*60 + $3;
			if ($tod >= $startTime){
				last;
			}
		}
	}
	else{
		while ($msg=<IN>){
			$msg=<IN>;
			if ($msg =~ /^# /) { next;}
			if ($msg =~ /^@ /) { next;}
			($msgid,$tod,$msgdata) = split " ",$msg;
			chomp $msgdata;
			$tod =~ /(\d{2}):(\d{2}):(\d{2})/;
			unless ((defined $1) && (defined $2) && (defined $3)) {next;}
			$tod = $1*3600 + $2*60 + $3;
			last;
		}
	}
	
	# Don't worry about the first message
	$gotFAB=0;
	while (($tod <= $lastTime) && !eof(IN)){
		($tv_sec,$tv_usec) = gettimeofday();
		usleep(1000000-$tv_usec);
		print STDERR "#### $tod $tv_usec \n";
		Translate($msgdata); # spit out wot we last got - should be the first message
		$gotFAB=($msgdata=~/^8fab/);
		while ($msg=<IN>){
			if ($msg =~ /^# /) { next;}
			if ($msg =~ /^@ /) { next;}
			($msgid,$newtod,$msgdata) = split " ",$msg;
			chomp $msgdata;
			$newtod =~ /(\d{2}):(\d{2}):(\d{2})/;
			unless ((defined $1) && (defined $2) && (defined $3)) {next;}
			$newtod = $1*3600 + $2*60 + $3;
			if ($newtod == $tod){
				if (($tod==86399) && ($msgdata=~/^8fab/) && ($gotFAB)){ # this is a leap second
					last; # do it again
				}
				
				Translate($msgdata);
			}
			elsif ($newtod == $tod + 1){
				$tod=$newtod;
				last;
			}
			elsif ($newtod > $tod+1){
				$secstosleep = $newtod - $tod - 1;
				sleep($secstosleep);
				$tod=$newtod;
				last;
			}
			else
			{
				
			}
		}
	}
	
	close(IN);	
}

for ($i=0;$i<=$#zipped;$i++){
	`gzip $zipped[$i]`;
}

#----------------------------------------------------------------------------
sub Initialise 
{
  my $name=shift;
  my ($err);
  my @required=();
  if ($localMajorVersion==1){
		@required=( "data path","gps data extension");
		%Init=&TFMakeHash($name,(tolower=>1));
	}
	elsif ($localMajorVersion==2){
		@required=( "paths:receiver data","receiver:file extension");
		%Init=&TFMakeHash2($name,(tolower=>1));
	}
 
	$err=0;
	foreach (@required){
		unless (defined $Init{$_}){
			print STDERR "! No value for $_ given in $name\n";
			$err=1;
		}
	}
	exit if $err;
}

#----------------------------------------------------------------------------
sub ShowHelp()
{
	print "Usage: $0 [-h] StartMJD StartTOD StopMJD StopTOD serial_port\n";
	print "-h   Show this help\n";
}

#----------------------------------------------------------------------------
sub FixPath()
{
	my $path=$_[0];
	if (!($path=~/^\//)){
		$path =$ENV{HOME}."/".$path;
	}
	return $path;
}

# -----------------------------------------------------------------------------------------
sub Translate
{
	# Turn the hexified, unstuffed message back into TSIP
	# TSIP starts with <DLE> and terminates with <DLE><ETX>
	# The message is stuffed by doubling any DLE bytes
	my $tmp=shift;
	#print $tmp,"\n";
	$packed = pack "H*",$tmp;
	#print length($tmp)," ",length($packed);
	$packed=~s/\x10/\x10\x10/g;		# 'stuff' (double) DLE in data
	#print " ",length($packed),"\n";
  print $rx "\x10".$packed."\x10\x03";
}
