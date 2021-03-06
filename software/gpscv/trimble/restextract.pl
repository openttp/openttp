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
# Extracts useful information from the raw data file
#

use Getopt::Std;
use TFLibrary;
use vars qw($opt_a $opt_c $opt_f $opt_h $opt_m $opt_l $opt_L $opt_u $opt_p $opt_r $opt_s $opt_t $opt_v $opt_x);

$VERSION="2.0.1";

$M_PI=4*atan2(1,1);
$CVACUUM=299792458.0;

$SKIP=-1;
$MSG8F=1;
$MSG6C=2; #SMT360
$MSG6D=3;
$MSG45=4;
$MSG47=5;
$MSG58=6;
$MSG5A=7;
$MSG84=8;

$RESOLUTION_T=0;
#$RESOLUTION_SMT=1;
$RESOLUTION_360=2;

$0=~s#.*/##;

# Parse command line
if (!getopts('c:fhm:pr:stlLuavx') || $opt_h){
  &ShowHelp();
  exit;
}

if ($opt_v){
	print "$0 version $VERSION\n";
	exit;
}

$svn=999;

if (defined $opt_m){$mjd=$opt_m;}else{$mjd=int(time()/86400) + 40587;}
if (defined $opt_r){$svn=$opt_r;}

$home=$ENV{HOME};
if (-d "$home/etc")  {$configpath="$home/etc";}  else 
	{$configpath="$home/Parameter_Files";} # backwards compatibility

# More backwards compatibility fixups

if (defined $opt_c){ 
  if (-e $opt_c){
    $configFile=$opt_c;
    if ($opt_c =~ /setup/){
      $localMajorVersion=1;
    }
    else{
      $localMajorVersion=2;
    }
  }
  else{
    print STDERR "$opt_c not found!\n";
    exit;
  }
}
elsif (-e "$configpath/rest.conf"){ # this takes precedence
	$configFile=$configpath."/rest.conf";
	$localMajorVersion=2;
}
elsif (-e "$configpath/gpscv.conf"){
	$configFile=$configpath."/gpscv.conf";
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
$model=$RESOLUTION_360;
if ($localMajorVersion==1){
	$infile=$Init{"data path"}."/$mjd".$Init{"gps data extension"};
}
elsif ($localMajorVersion==2){
	$gpsDataPath=TFMakeAbsolutePath($Init{"paths:receiver data"},$home);
	$gpsExt = ".rx";
	if (defined ($Init{"receiver:file extension"})){
		$gpsExt = $Init{"receiver:file extension"};
		if (!($gpsExt =~ /^\./)){
			$gpsExt = ".".$gpsExt;
		}
	}
	
	$infile=$gpsDataPath."$mjd".$gpsExt; # FIXME assumptions here ...
	if ($Init{"receiver:model"} eq "Resolution T"){
		$model=$RESOLUTION_T;
	}
#		elsif ($Init{"receiver:model"} eq "Resolution SMT"){
#			$model=$RESOLUTION_SMT;
#		}
	elsif ($Init{"receiver:model"} eq "Resolution SMT 360"){
		$model=$RESOLUTION_360;
	}
	else{
		print "Incorrect receiver model: ".$Init{"receiver:model"}."\n";
		exit;
	}
}

$yearOffset=2000;
if ($model==$RESOLUTION_T){
	$yearOffset=1900;
}

$zipfile=$infile.".gz";
$zipit=0;
if (-e $zipfile){
	`gunzip $zipfile`;
	$zipit=1;
}
open (RXDATA,$infile) or die "Can't open data file $infile: $!\n";
$lastt="";

while (<RXDATA>)
{
	$msg = $SKIP;
	if (/^8F /) {$msg=$MSG8F;}
	elsif(/^6C /){$msg=$MSG6C;}
	elsif(/^6D /){$msg=$MSG6D;}
	elsif(/^45 /){$msg=$MSG45;}
	elsif(/^47 /){$msg=$MSG47;}
	elsif(/^58 /) {$msg=$MSG58;}
	elsif(/^5A /) {$msg=$MSG5A;}
	elsif(/^84 /) {$msg=$MSG84;}
	next unless ($msg != $SKIP);
	
  chop;
  @_=split;
	
	if (!($lastt eq $_[1])) 
	{
		if (!$opt_f) {print "\n$_[1] ";}
		$lastt = $_[1];
		($hh,$mm,$ss)=split /:/,$lastt;
		$tt = $hh*3660+$mm*60+$ss;
	}
	
	if ($msg==$MSG8F)
	{
		$submsg=substr $_[2],2,2;
		if ($opt_u && ($submsg eq "ab"))
		{
			# Remove first byte so that indexing corresponds to docs
			$_[2]=substr $_[2],2;
			&ReverseByteOrder(1,4,$_[2]);	  # GPS seconds of week
			&ReverseByteOrder(5,6,$_[2]); 	# GPS week number
			&ReverseByteOrder(7,8,$_[2]); 	# UTC Offset
			&ReverseByteOrder(15,16,$_[2]); # Year
			@data=unpack "CISsC6S",(pack "H*",$_[2]);
			if ($opt_u) {print $data[3]," ";}
		}
		elsif (($opt_t || $opt_l || $opt_x) && ($submsg eq "ac"))
		{
			$_[2]=substr $_[2],2;
			&ReverseByteOrder(4,7,$_[2]); 
			&ReverseByteOrder(8,9,$_[2]);
			&ReverseByteOrder(10,11,$_[2]);
			&ReverseByteOrder(16,19,$_[2]); # local clock bias
			&ReverseByteOrder(20,23,$_[2]);
			&ReverseByteOrder(24,27,$_[2]);
			&ReverseByteOrder(28,31,$_[2]);
			&ReverseByteOrder(32,35,$_[2]);
			&ReverseByteOrder(36,43,$_[2]); # lat
			&ReverseByteOrder(44,51,$_[2]); # lon
			&ReverseByteOrder(52,59,$_[2]); # alt
			&ReverseByteOrder(60,63,$_[2]);
			&ReverseByteOrder(64,67,$_[2]);
			@data=unpack "C4IS2C4f2If2d3fI",(pack "H*",$_[2]);
			if ($opt_l) {
				$bits = ($data[6] & (0x01 << 7)) >> 7;
				print $bits," ";}
			if ($opt_t) {print $data[15]," ";}
			if ($opt_x){ printf "%04x %02x %e %e %e",$data[6],$data[7],$data[16]*180.0/$M_PI,$data[17]*180.0/$M_PI,$data[18];} 
		}
		elsif ($opt_f && ($submsg eq "41"))
		{
			# Remove first byte so that indexing corresponds to docs
			$_[2]=substr $_[2],2;
			&ReverseByteOrder(1,2,$_[2]);	  # board serial number prefix
			&ReverseByteOrder(3,6,$_[2]); 	# board serial number
			&ReverseByteOrder(11,14,$_[2]); # oscillator offset
			@data=unpack "CsIC4f",(pack "H*",$_[2]);
			print  "Board serial number prefix: $data[1]\n";
			print  "Board serial number:        $data[2]\n";
			printf "Build date and time:        %4d-%02d-%02d %02d:00\n",
				2000+$data[3],$data[4],$data[5],$data[6];
		}
	}
	elsif(($msg==$MSG6C) && $opt_s)
	{
		# don't bother reversing bytes of stuff we don't want
		@data=unpack "C2f4C",(pack "H*",$_[2]);
		print $data[6];
		
	}
	elsif(($msg==$MSG6D) && $opt_s)
	{
		# Remove first byte so that indexing corresponds to docs
		$_[2]=substr $_[2],2,2*17;
		&ReverseByteOrder(1,4,$_[2]);	# PDOP 
		&ReverseByteOrder(5,8,$_[2]);
		&ReverseByteOrder(9,12,$_[2]);
		&ReverseByteOrder(13,16,$_[2]);
		@data=unpack "Cf4",(pack "H*",$_[2]);
		print $data[0] >> 4;
	}
	elsif(($msg==$MSG45) && $opt_f)
	{
	
		$_[2]=substr $_[2],2;
		@data=unpack "C10",(pack "H*",$_[2]);
		
		printf "SW application version: %d.%d %4d-%02d-%02d\n",
			$data[0],$data[1],$data[4]+$yearOffset,$data[2],$data[3];
		printf "GPS core version:       %d.%d %4d-%02d-%02d\n",
			$data[5],$data[6],$data[9]+$yearOffset,$data[7],$data[8];
	}
	elsif(($msg==$MSG47) && $opt_a)
	{
		$_[2]=substr $_[2],2;
		#print $_[2],"\n";
		#print ":",substr($_[2],0,2);
		($cnt)=unpack "C",(pack "H*",substr($_[2],0,2));
		#print $cnt;
		for ($s=0;$s<$cnt;$s++)
		{
			&ReverseByteOrder(2+$s*5,5+$s*5,$_[2]);
			($id,$amu)=unpack "Cf",(pack "H*",substr($_[2],2*(1+$s*5),10));
			open(OUT,">>$mjd.$id.dat");
			print OUT "$tt $amu\n";
			close(OUT);
			
		}
	}
	elsif (($msg==$MSG58) && $opt_L)
	{
			$submsg=substr $_[2],2,4;
			if ($opt_L && ($submsg eq "0205"))
			{
				$_[2]=substr $_[2],2;
				&ReverseByteOrder(17,24,$_[2]);
				&ReverseByteOrder(25,28,$_[2]);
				&ReverseByteOrder(29,30,$_[2]);
				&ReverseByteOrder(31,34,$_[2]);
				&ReverseByteOrder(35,36,$_[2]);
				&ReverseByteOrder(37,38,$_[2]);
				&ReverseByteOrder(39,40,$_[2]);
				&ReverseByteOrder(41,42,$_[2]);
      	@utc=unpack "C17dfsfS3s",(pack "H*",$_[2]);
				print " delta_t_ls=$utc[19] delta_t_lsf=$utc[24] "
			}
	}
	elsif(($msg==$MSG5A) && $opt_r)
	{
		$_[2]=substr $_[2],2;
		&ReverseByteOrder(1,4,$_[2]);
		&ReverseByteOrder(5,8,$_[2]);
		&ReverseByteOrder(9,12,$_[2]);
		&ReverseByteOrder(13,16,$_[2]);
		&ReverseByteOrder(17,24,$_[2]);
		@rawmeas = unpack("Cf4d",pack "H*",$_[2]);
		if  ($svn==999){ 
			print "\n$rawmeas[0] ",$rawmeas[3]*61.0948*1.0E-9," $rawmeas[5]";
		}
		elsif ($svn==$rawmeas[0]){
			print "$rawmeas[0] ",$rawmeas[3]*61.0948*1.0E-9," $rawmeas[5]";
		}
	}
	elsif(($msg==$MSG84) && $opt_p)
	{
		$_[2]=substr $_[2],2;
		&ReverseByteOrder(0,7,$_[2]); # lat
		&ReverseByteOrder(8,15,$_[2]); # lon
		&ReverseByteOrder(16,23,$_[2]);# alt
		&ReverseByteOrder(24,31,$_[2]); # clock bias
		&ReverseByteOrder(32,35,$_[2]); # time of fix
		@pos = unpack("d4f",pack "H*",$_[2]);
		print " ",$pos[0]*180.0/$M_PI," ",$pos[1]*180.0/$M_PI," ",$pos[2]," ",$pos[3]/$CVACUUM," ",$pos[4];
	}
}
print "\n";
close RXDATA;

if (1==$zipit) {`gzip $infile`;}

#----------------------------------------------------------------------------
sub ShowHelp()
{
	print "Usage: $0 [-c config_file] [-f] [-h] [-m mjd] [-a] [-r] [-t] [-l] [-L] [-u] [-v]\n";
	print "  -c <config_file> use the specified config file\n";
	print "  -f      show firmware version\n";
  print "  -m mjd  MJD\n";
	print "  -a      extract S/N for visible satellites (47)\n";
	print "  -h      show this help\n";
	print "  -t      temperature (8FAC)\n";
	print "  -l      leap second warning(8FAC)\n";
	print "  -L      leap second info (5805)\n";
	print "  -p      position fix message\n";
	print "  -r svn  pseudoranges (svn=999 reports all)\n";
	print "  -s      number of visible satellites\n";
	print "  -u      UTC offset (8FAB)\n";
	print "  -v      show version\n";
	print "  -x      alarms and gps decoding status\n";
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

#-----------------------------------------------------------------------

sub ReverseByteOrder()
{
	my $start=$_[0];
	my $stop=$_[1];
	
	#$tmp = substr $_[2],$start*2,($stop-$start+1)*2 ;
	#print "before $tmp \n";
	# Need to do ($stop-$start+1)/2 swaps to reverse all bytes
	$swaps=($stop-$start+1)/2;
	for ($i=0;$i<$swaps;$i++)
	{
		# save the first  hex byte
		$indx0=$start*2+$i*2;
		$tmp0=substr $_[2], $indx0,1;
		$tmp1=substr $_[2], $indx0+1,1;
		# and copy the contents of the hex byte we're swapping with
		$indx1=$stop*2-$i*2;
		#print "$indx0 $tmp0 $tmp1 $indx1\n";
		substr ($_[2],$indx0,1) = substr ($_[2],$indx1,1);
		substr ($_[2],$indx0+1,1) = substr ($_[2],$indx1+1,1);
		substr ($_[2],$indx1,1) = $tmp0;
		substr ($_[2],$indx1+1,1)= $tmp1;
	}
	#$tmp = substr $_[2],$start*2,($stop-$start+1)*2 ;
	#print "after $tmp \n";
}

