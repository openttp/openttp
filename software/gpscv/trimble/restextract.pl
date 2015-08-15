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
use vars qw($opt_a $opt_h $opt_m $opt_l $opt_L $opt_u $opt_s $opt_t $opt_v);

$SKIP=-1;
$MSG8F=1;
$MSG6D=2;
$MSG45=3;
$MSG47=4;
$MSG58=5;

# Parse command line
if (!getopts('hm:stlLuav')){
  &ShowHelp();
  exit;
}

if ($opt_h){&ShowHelp();exit;}

if ($opt_m){$mjd=$opt_m;}else{$mjd=int(time()/86400) + 40587;}

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
	$infile=$Init{"data path"}."/$mjd".$Init{"gps data extension"};
}
elsif ($localMajorVersion==2){
		$infile=$Init{"paths:receiver data"}."/$mjd".$Init{"receiver:file extension"};
}

$infile = &FixPath( $infile );

$zipfile=$infile.".gz";
$zipit=0;
if (-e $zipfile)
{
	`gunzip $zipfile`;
	$zipit=1;
}
open (RXDATA,$infile) or die "Can't open data file $infile: $!\n";
$lastt="";

while (<RXDATA>)
{
	$msg = $SKIP;
	if (/^8F /) {$msg=$MSG8F;}
	elsif(/^6D /){$msg=$MSG6D;}
	elsif(/^45 /){$msg=$MSG45;}
	elsif(/^47 /){$msg=$MSG47;}
	elsif(/^58 /) {$msg=$MSG58;}
	next unless ($msg != $SKIP);
	
  chop;
  @_=split;
	
	if (!($lastt eq $_[1])) 
	{
		if (!$opt_v) {print "\n$_[1] ";}
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
		elsif (($opt_t || $opt_l) && ($submsg eq "ac"))
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
			&ReverseByteOrder(36,43,$_[2]);
			&ReverseByteOrder(44,51,$_[2]);
			&ReverseByteOrder(52,59,$_[2]);
			&ReverseByteOrder(60,63,$_[2]);
			&ReverseByteOrder(64,67,$_[2]);
			@data=unpack "C4IS2C4f2If2d3fI",(pack "H*",$_[2]);
			if ($opt_l) {
				$bits = ($data[6] & (0x01 << 7)) >> 7;
				print $bits," ";}
			if ($opt_t) {print $data[15]," ";}
		}
		elsif ($opt_v && ($submsg eq "41"))
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
	elsif(($msg==$MSG45) && $opt_v)
	{
	
		$_[2]=substr $_[2],2;
		@data=unpack "C10",(pack "H*",$_[2]);
		
		printf "SW application version: %d.%d %4d-%02d-%02d\n",
			$data[0],$data[1],$data[4]+1900,$data[2],$data[3];
		printf "GPS core version:       %d.%d %4d-%02d-%02d\n",
			$data[5],$data[6],$data[9]+1900,$data[7],$data[8];
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
	
}
print "\n";
close RXDATA;

if (1==$zipit) {`gzip $infile`;}

#----------------------------------------------------------------------------
sub ShowHelp()
{
	print "Usage: $0 [-h] [-m mjd] [-a] [-t] [-l] [-L] [-u] [-v]\n";
  print "  -m mjd  MJD\n";
	print "  -a      extract S/N for visible satellites (47)\n";
	print "  -a      show this help\n";
	print "  -t      extract temperature (8FAC)\n";
	print "  -l      extract leap second warning(8FAC)\n";
	print "  -L      extract leap second info (5805)\n";
	print "  -s      extract number of visible satellites\n";
	print "  -u      extract UTC offset (8FAB)\n";
	print "  -v      extract software versions\n";
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

