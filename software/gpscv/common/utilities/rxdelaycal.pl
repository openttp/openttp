#!/usr/bin/perl -w
#
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

# File names are assumed to be of the form MJD.cctf
# Delays is a file containing details of the various delays.
# 
# Modification history
#
# 12-10-2004 MJW First VERSION
# 19-10-2004 MJW Seems to work ...
# 28-08-2015 MJW Many changes to tidy this up and work with rin2cggts-produced files
# 05-02-2016 MJW More flexible handling of data paths added


use POSIX qw(strftime);
use Getopt::Std;

use subs qw(MyPrint);
use vars qw($opt_c $opt_d $opt_h $opt_i $opt_r $opt_s $opt_t $opt_v $opt_x $opt_y);

$VERSION = "2.0.1";

$MINTRACKLENGTH=750;
$DSGMAX=200;
$USEIONO=1;
$ELEVMASK=0;

$PRN=0;
$MJD=2;
$STTIME=3;
$TRKL=4;
$ELV=5;
$REFSV=7;
$SRSV=8;
$REFGPS=9;
$DSG=11;
$MDIO=15;
$MSIO=17; # only for dual frequency
$SMSI=18; # only for dual frequency
$ISG=19;  # only for dual frequency

$REFIONO=$MDIO;
$CALIONO=$MDIO;
$REFDIFF=$REFGPS;

$0=~s#.*/##; # strip path from executable name

if (!getopts('c:d:e:hir:st:vx:y:')){
	ShowHelp();	
	exit;
}

if ($opt_h){
	ShowHelp();
	exit;
}

if ($opt_v){
	print "$0 version $VERSION\n";
	exit;
}

if ($opt_d){
	$DSGMAX=$opt_d*10.0;
}

if ($opt_e){
	$ELEVMASK=$opt_e*10.0;
}

if ($opt_s){
	$REFDIFF=$REFSV;
}

if ($opt_t){
	$MINTRACKLENGTH=$opt_t;
}

if ($opt_i){
	$USEIONO=0;
}

$refrxext = "cctf";
$calrxext = "cctf";
if ($opt_x){$refrxext = $opt_x;}
if ($opt_y){$calrxext = $opt_y;}

if ($#ARGV == 3){
	$refrx  =   $ARGV[0];
	$calrx  =   $ARGV[1];
	$startMJD = $ARGV[2];
	$stopMJD  = $ARGV[3];
}
else{
	ShowHelp();
	exit();
}

$svn=-1;
if ($#ARGV==4){
	$svn=$ARGV[4];
}

@refrxpath = split /\//,$refrx;
if ($#refrxpath>=0){
	$refrxname=$refrxpath[$#refrxpath];
}
else{
	$refrxname=$refrx;
}

@calrxpath = split /\//,$calrx;
if ($#calrxpath>=0){
	$calrxname=$calrxpath[$#calrxpath];
}
else{
	$calrxname=$calrx;
}
open LOG, ">$refrxname.$calrxname.report.txt";

MyPrint "$VERSION\n\n";
MyPrint "Run " . (strftime "%a %b %e %H:%M:%S %Y", gmtime)."\n\n";

MyPrint "Mininimum track length = $MINTRACKLENGTH\n";
MyPrint "Ionosphere corrections removed = ".($USEIONO?"yes":"no")."\n";
MyPrint "Maximum DSG = ".$DSGMAX/10.0." ns\n";
MyPrint "Elevation mask = ".$ELEVMASK/10.0." deg\n";
MyPrint "Using ".($opt_s?"REFSV":"REFGPS")."\n";
MyPrint "\n";

$CCTFINTDLY=0;
$CCTFCABDLY=1;
$CCTFREFDLY=2;
$REPINTDLY=3;
$REPCABDLY=4;
$REPREFDLY=5;

@refdelay=(0,0,0,0,0,0,0);
@caldelay=(0,0,0,0,0,0,0);

$refdf = 1; # ref is dual frequency ?
$caldf = 1; # cal is dual frequency ?

# Read CCTF files
for ($i=$startMJD;$i<=$stopMJD;$i++){
	@d=ReadCCTF($refrx,$i,$refrxext);
	($refdelay[$CCTFINTDLY],$refdelay[$CCTFCABDLY],$refdelay[$CCTFREFDLY],$refdf)=splice @d,$#d-3; 
	push @ref,@d;
	MyPrint "\tINT= $refdelay[$CCTFINTDLY] CAB= $refdelay[$CCTFCABDLY] REF= $refdelay[$CCTFREFDLY]\n"; 
	
	@d=ReadCCTF($calrx,$i,$calrxext);
	($caldelay[$CCTFINTDLY],$caldelay[$CCTFCABDLY],$caldelay[$CCTFREFDLY],$caldf)=splice @d,$#d-3;	
	push @cal,@d;
	MyPrint "\tINT= $caldelay[$CCTFINTDLY] CAB= $caldelay[$CCTFCABDLY] REF= $caldelay[$CCTFREFDLY]\n";	
}

MyPrint "\nREF  contains ".($refdf?"dual":"single")." frequency data\n";
MyPrint   "CAL  contains ".($caldf?"dual":"single")." frequency data\n";

if ($opt_c){
	if ($opt_c =~/model/){
		$CALIONO=$MDIO;
	}
	elsif ($opt_c =~/meas/){
		if (!$caldf){
			print "\n!!! Calibration receiver is not dual frequency: '-c measured' is not a valid option\n\n";
			ShowHelp();
			exit;
		}
		$CALIONO=$MSIO;
	}
}

if ($opt_r){
	if ($opt_r =~/model/){
		$REFIONO=$MDIO;
	}
	elsif ($opt_r =~/meas/){
		if (!$refdf){
			print "\n!!! Reference receiver is not dual frequency: '-r measured' is not a valid option\n\n";
			ShowHelp();
			exit;
		}
		$REFIONO=$MSIO;
	}
}

# Dump REFGPS data for later plotting
open(OUT, ">$refrxname.ref.refgps.all.txt"); # just in case names are the same, use "ref" in name
for ($i=0;$i<$#ref;$i++){
	print OUT "$ref[$i][$MJD] $ref[$i][$STTIME] $ref[$i][$REFGPS]\n";
}
close(OUT);

open(OUT, ">$calrxname.cal.refgps.all.txt");
for ($i=0;$i<$#ref;$i++){
	print OUT "$ref[$i][$MJD] $ref[$i][$STTIME] $ref[$i][$REFGPS]\n";
}
close(OUT);

# Filter REF data
$N = $#ref + 1;
MyPrint "\nReference receiver $refrx\n";
MyPrint "\tTotal tracks $N\n";
$shorttracks=0;
$badmsio=0;
$badsmsi=0;
$badisg=0;
$badsrsv=0;
$baddsg=0;
$badelev=0;

for ($i=0;$i<=$#ref;$i++){
	if ($refdf){
		if (($ref[$i][$TRKL]<$MINTRACKLENGTH) || 
		  ($ref[$i][$SRSV] == 99999) ||
			($ref[$i][$MSIO] == 9999) ||
			($ref[$i][$SMSI] == 999)||
			($ref[$i][$ISG]  == 999) ||
			($ref[$i][$ELV]  < $ELEVMASK ) ||
			($ref[$i][$DSG] > $DSGMAX))
		{
			if ($ref[$i][$TRKL]<$MINTRACKLENGTH)    {$shorttracks++;}
			if ($ref[$i][$SRSV] == 99999) {$badsrsv++;}
			if ($ref[$i][$MSIO] == 9999) {$badmsio++;}
			if ($ref[$i][$SMSI] == 999)  {$badsmsi++;}
			if ($ref[$i][$ISG]  == 999)  {$badisg++;}
			if ($ref[$i][$DSG] > $DSGMAX){$baddsg++;}
			if ($ref[$i][$ELV]  < $ELEVMASK ){$badelev++;}
			
			splice @ref,$i,1;
			$i--; # since we've removed an element 
		}
	}
	else{
		if ($ref[$i][$TRKL]<$MINTRACKLENGTH || 
				($ref[$i][$SRSV] == 99999) || 
				($ref[$i][$ELV]  < $ELEVMASK ) ||
				($ref[$i][$DSG] > $DSGMAX)) {
			
			if ($ref[$i][$TRKL]<$MINTRACKLENGTH)    {$shorttracks++;}
			if ($ref[$i][$SRSV] == 99999) {$badsrsv++;}
			if ($ref[$i][$DSG] > $DSGMAX){$baddsg++;}
			if ($ref[$i][$ELV]  < $ELEVMASK ){$badelev++;}
			splice @ref,$i,1;
			$i--; # since we've removed an element 
		}
	}
	
}
$N = $#ref + 1;
MyPrint "\t $N good tracks\n";
MyPrint "\t-->$shorttracks short tracks\n";
MyPrint "\t-->$badsrsv bad SRSV\n";
MyPrint "\t-->$baddsg bad DSG\n";
MyPrint "\t-->$badelev bad elevation\n";
if ($refdf){
	
	MyPrint "\t-->$badmsio bad MSIO\n";
	MyPrint "\t-->$badsmsi bad SMSI\n";
	MyPrint "\t-->$badisg bad ISG\n";
}

# Filter CAL data
$N = $#cal + 1;
MyPrint "\nCalibration receiver $calrx\n";
MyPrint "\tTotal tracks $N\n";
$shorttracks=0;
$badsrsv=0;
$badmsio=0;
$badsmsi=0;
$badisg=0;
$baddsg=0;
$badelev=0;

for ($i=0;$i<=$#cal;$i++){
	if ($caldf){
		if (($cal[$i][$TRKL]<$MINTRACKLENGTH) ||
			($cal[$i][$SRSV] == 99999) ||
			($cal[$i][$MSIO] == 9999) ||
			($cal[$i][$SMSI] == 999)||
			($cal[$i][$ISG]  == 999) ||
			($cal[$i][$ELV]  < $ELEVMASK ) ||
			($cal[$i][$DSG] > $DSGMAX)
			) 
		{
			if ($cal[$i][$TRKL]<$MINTRACKLENGTH)    {$shorttracks++;}
			if ($cal[$i][$SRSV] == 99999) {$badsrsv++;}
			if ($cal[$i][$MSIO] == 9999) {$badmsio++;}
			if ($cal[$i][$SMSI] == 999)  {$badsmsi++;}
			if ($cal[$i][$ISG]  == 999)  {$badisg++;}
			if ($cal[$i][$DSG] > $DSGMAX){$baddsg++;}
			if ($cal[$i][$ELV]  < $ELEVMASK ){$badelev++;}
			splice @cal,$i,1;
			$i--;
		}
	}
	else{
		if ($cal[$i][$TRKL]<$MINTRACKLENGTH || 
			($cal[$i][$SRSV] == 99999) || 
			($cal[$i][$ELV]  < $ELEVMASK ) ||
			($cal[$i][$DSG] > $DSGMAX)){
			if ($cal[$i][$TRKL]<$MINTRACKLENGTH)    {$shorttracks++;}
			if ($cal[$i][$SRSV] == 99999) {$badsrsv++;}
			if ($cal[$i][$DSG] > $DSGMAX){$baddsg++;}
			if ($cal[$i][$ELV]  < $ELEVMASK ){$badelev++;}
			splice @cal,$i,1;
			$i--;
		}
	}
}
$N = $#cal + 1;
MyPrint "\t$N good tracks\n";
MyPrint "\t-->$shorttracks short tracks\n";
MyPrint "\t-->$badsrsv bad SRSV\n";
MyPrint "\t-->$baddsg bad DSG\n";
MyPrint "\t-->$badelev bad elevation\n";
if ($caldf){
	
	MyPrint "\t-->$badmsio bad MSIO\n";
	MyPrint "\t-->$badsmsi bad SMSI\n";
	MyPrint "\t-->$badisg  bad ISG\n";
}

# Ask for real delays
$refdelay[$REPINTDLY]=$refdelay[$CCTFINTDLY];
$refdelay[$REPCABDLY]=$refdelay[$CCTFCABDLY];
$refdelay[$REPREFDLY]=$refdelay[$CCTFREFDLY];
print "REF/HOST receiver ($refrx)\n";
print "reported internal  delay [$refdelay[$REPINTDLY] ns]?";
$stuff = <STDIN>;
chomp $stuff;	
if ($stuff=~/^([+-]?\d+\.?\d*|\.\d+)$/){
	$refdelay[$REPINTDLY]=$stuff;
}
print "reported cable     delay [$refdelay[$REPCABDLY] ns]?";
$stuff = <STDIN>;
chomp $stuff;
if ($stuff=~/^([+-]?\d+\.?\d*|\.\d+)$/){
	$refdelay[$REPCABDLY]=$stuff;
}
print "reported reference delay [$refdelay[$REPREFDLY] ns]?";
$stuff = <STDIN>;
chomp $stuff;
if ($stuff=~/^([+-]?\d+\.?\d*|\.\d+)$/){
	$refdelay[$REPREFDLY]=$stuff;
}

$caldelay[$REPINTDLY]=$caldelay[$CCTFINTDLY];
$caldelay[$REPCABDLY]=$caldelay[$CCTFCABDLY];
$caldelay[$REPREFDLY]=$caldelay[$CCTFREFDLY];
print "\nCAL/TRAVELLING receiver ($calrx)\n";
print "reported internal  delay [$caldelay[$CCTFINTDLY] ns]?";
$stuff = <STDIN>;
chomp $stuff;	
if ($stuff=~/^([+-]?\d+\.?\d*|\.\d+)$/){
	$caldelay[$REPINTDLY]=$stuff;
}
print "reported cable     delay [$caldelay[$CCTFCABDLY] ns]?";
$stuff = <STDIN>;
chomp $stuff;
if ($stuff=~/^([+-]?\d+\.?\d*|\.\d+)$/){
	$caldelay[$REPCABDLY]=$stuff;
}
print "reported reference delay [$caldelay[$CCTFREFDLY] ns]?";
$stuff = <STDIN>;
chomp $stuff;
if ($stuff=~/^([+-]?\d+\.?\d*|\.\d+)$/){
	$caldelay[$REPREFDLY]=$stuff;
}

MyPrint "\nREF/HOST receiver reported delays($refrx)\n";
MyPrint "\tINT= $refdelay[$REPINTDLY] CAB= $refdelay[$REPCABDLY] REF= $refdelay[$REPREFDLY]\n";

MyPrint "CAL/TRAVELLING receiver reported delays($calrx)\n";
MyPrint "\tINT= $caldelay[$REPINTDLY] CAB= $caldelay[$REPCABDLY] REF= $caldelay[$REPREFDLY]\n";

$refCCTFDelay  = $refdelay[$CCTFINTDLY] + $refdelay[$CCTFCABDLY] - $refdelay[$CCTFREFDLY] ;
$refRepDelay   = $refdelay[$REPINTDLY]  + $refdelay[$REPCABDLY]  - $refdelay[$REPREFDLY];
$refCorrection = $refRepDelay - $refCCTFDelay;

$calCCTFDelay  = $caldelay[$CCTFINTDLY] + $caldelay[$CCTFCABDLY] - $caldelay[$CCTFREFDLY] ;
$calRepDelay   = $caldelay[$REPINTDLY]  + $caldelay[$REPCABDLY]  - $caldelay[$REPREFDLY];
$calCorrection = $calRepDelay - $calCCTFDelay;

print "Matching tracks ...\n";

# Match tracks
for ($i=0;$i<=$#ref;$i++){
	for ($j=0;$j<=$#cal;$j++){
		if (($ref[$i][$PRN] == $cal[$j][$PRN]) &&
		    ($ref[$i][$MJD] == $cal[$j][$MJD]) &&
		    ($ref[$i][$STTIME] == $cal[$j][$STTIME]))
		{
			# Convert STTIME to decimal MJD
			$ref[$i][$STTIME]=~/(\d\d)(\d\d)(\d\d)/;			
			$t = $ref[$i][$MJD]+($1*3600.0+$2*60.0+$3)/86400.0-$startMJD;
			# Note that the measured ionospheric delay is added back in ...
			push @matches, [$t,
				$ref[$i][$REFGPS]/10.0,$cal[$j][$REFGPS]/10.0,
				($ref[$i][$REFDIFF] + $USEIONO*$ref[$i][$REFIONO])/10.0-
				($cal[$j][$REFDIFF] + $USEIONO*$cal[$j][$CALIONO])/10.0  + $calCorrection - $refCorrection];
			last;
		}
	}
}
$N =$#matches + 1;
MyPrint "\n$N matching tracks\n";

# Do the linear fit
$sumx = 0;
$sumy = 0;
$sumx2 = 0;
$sumxy = 0;
$sumvar = 0;
for ($i=0;$i<=$#matches;$i++){
	$sumx+=$matches[$i][0];
	$sumy+=$matches[$i][3];
	$sumx2 += $matches[$i][0]**2;
	#$sumy2 += $matches[$i][3]**2;
	$sumxy += $matches[$i][0]*$matches[$i][3];
}

$N = $#matches+1;
if($N> 2 && ($N*$sumx2 - $sumx**2) != 0 ){ # A linear regression can be performed
	$B1 = ($N*$sumxy - $sumy*$sumx)/($N*$sumx2 - $sumx**2);	
	$B0 = ($sumy*$sumx2 - $sumxy*$sumx)/($i*$sumx2 - $sumx**2);
}
else{
	print "Not enough matches to do a linear regression!\n";
	exit 1;
}

for($i=0; $i<$N;$i++){
	$sumvar += ($matches[$i][3] - ($B1*$matches[$i][0] + $B0))**2;
}
$s2 = $sumvar/($N-2);
$s2B1 = $N*$s2/($N*$sumx2 - ($sumx**2));
$sB1 = sqrt $s2B1;
$rms= sqrt $s2;

# Calculate the mean offset
$meanMJD = $matches[0][0] + ($matches[$#matches][0]-$matches[0][0])/2.0;
$meanOffset = $B0 + $B1*$meanMJD;
$B1  *= 1000.0;
$sB1 *= 1000.0;
MyPrint "###########################################################\n\n";
MyPrint "mean offset (REF - CAL) $meanOffset ns [subtract from CAL 'INT DLY' to correct]\n";
MyPrint "slope $B1 ps/day SD $sB1 ps/day\n";
MyPrint "RMS of residuals $rms ns\n";
MyPrint "\n###########################################################\n";

print "Report in $refrxname.$calrxname.report.txt\n";
print "Plots in $refrxname.$calrxname.ps\n";

# Dump stuff for plotting
open(OUT, ">$refrxname.$calrxname.matches.txt");
for ($i=0;$i<=$#matches;$i++){
	print OUT "$matches[$i][0] $matches[$i][1] $matches[$i][2] $matches[$i][3]\n"; 
}
close OUT;

$tlast = int($matches[0][0]*86400);
$nav=0;
$av=0;
open(OUT, ">$refrxname.$calrxname.av.matches.txt");
for ($i=0;$i<=$#matches;$i++){
	if (int($matches[$i][0]*86400) == $tlast){
		$nav++;
		$av += $matches[$i][3];
	}
	else{
		print OUT $tlast/86400.0, " " , $av/$nav,"\n";
		$nav=1;
		$av=$matches[$i][3];
		$tlast = int($matches[$i][0]*86400);
	}
}
print OUT $tlast/86400.0, " " , $av/$nav,"\n";
close OUT;

# Produce some plots for eyeballing.
# First plot  is filtered REFGPS for REF receiver
# Second plot is filtered REFGPS for CAL receiver
# Third plot  is filtered differences with line of best fit

$nr=3; # number of rows on each page
$nc=1; # number of columns
$xm=0.02; # margins
$ym=0.02;
$xstep = (1.0-2.0*$xm)/$nc;
$ystep = (1.0-2.0*$ym)/$nr;

open(OUT, ">plotcmds.gnuplot");
print OUT "set terminal postscript portrait dashed \"Helvetica\" 10\n";
print OUT "set output \"$refrxname.$calrxname.ps\"\n";
print OUT "set multiplot\n";
print OUT "set nolabel\n";
print OUT "set size   $xstep, $ystep\n";
print OUT "set xlabel \"MJD - $startMJD\"\n";
print OUT "set ylabel \"ns\"\n";
print OUT "set format x \"%g\"\n";
print OUT "set format y \"%g\"\n";
print OUT "set nokey\n";
print OUT "set timestamp\n";

print OUT "set title \"REF:$refrxname filtered tracks\"\n";
$x0=$xm;
$y0=$ym;
print OUT "set origin $x0 , $y0\n";
print OUT "plot \"$refrxname.$calrxname.matches.txt\" using 1:2 with points pt 6 ps 0.5\n";

print OUT "set title \"CAL:$calrxname filtered tracks\"\n";
$y0 +=$ystep;
print OUT "set origin $x0 , $y0\n";
print OUT "plot \"$refrxname.$calrxname.matches.txt\" using 1:3 with points pt 6 ps 0.5\n";

print OUT "set title \"REF($refrxname) - CAL($calrxname) (filtered)\"\n";
$y0 +=$ystep;
print OUT "set origin $x0 , $y0\n";
print OUT "plot \"$refrxname.$calrxname.matches.txt\" using 1:4 with points pt 6 ps 0.5,\"$refrxname.$calrxname.av.matches.txt\" using 1:2 with lines\n";

close OUT;

`gnuplot  plotcmds.gnuplot`;
close LOG;

# ------------------------------------------------------------------------
sub ShowHelp
{
	print "Usage: $0 [-hv] [-c <modeled|measured>] [-d <val>] [-e <val>] [-r <modeled|measured>] [-s] [-t <val>] ref_rx_directory cal_rx_directory start_MJD stop_MJD\n\n";
	print "-c  <val> set ionospheric correction used for CAL receiver\n";
	print "-d  <val> set maximum DSG (default=". $DSGMAX/10.0. " ns)\n";
	print "-e  <val> set elevation mask (default = $ELEVMASK degrees)\n";
	print "-h        show this help\n";
	print "-i        remove ionosphere correction (zero baseline data)\n";
	print "-r  <val> set ionospheric correction used for REF receiver\n";
	print "-s        use REFSV instead of REFGPS\n";
	print "-t  <val> set mininimum track length (default = $MINTRACKLENGTH s)\n";
	print "-x  <val> file extension for reference   receiver (default = cctf)\n";
	print "-y  <val> file extension for calibration receiver (default = cctf)\n";
	print "-v        show version\n";
}

# ------------------------------------------------------------------------
sub ReadCCTF
{
	my @data;
	my $gotIntDly=0;
	my $gotCabDly=0;
	my $gotRefDly=0;
	my ($intDly,$cabDly,$refDly,$df);
	
	$rcvr=$_[0];
	$mjd =$_[1];
	$rxext=$_[2];
	$f = "$rcvr/$mjd.$rxext";
	
	if (-e $f){
		MyPrint "Read $f\n";
		open (IN,"<$f");
		while ($line=<IN>)  # Parse header
		{
			last if ($line=~/hhmmss/);
			if ($line =~ /PRN CL  MJD  STTIME/){
			  $df=$line =~ /MSIO/;
			}
			elsif ($line=~/INT DLY/){
				@parms=split " ",$line;
				$intDly=$parms[3];
				$gotIntDly=1;
			}
			elsif ($line=~/CAB DLY/){
				@parms=split " ",$line;
				$cabDly=$parms[3];
				$gotCabDly=1;
			}
			elsif ($line=~/REF DLY/){
				@parms=split " ",$line;
				$refDly=$parms[3];
				$gotRefDly=1;
			}
		}
		if (!$gotIntDly && !$gotCabDly && !$gotRefDly){
			print "Unable to get delays from $mjd.$rcvr\n";
			exit;
		}
		while (<IN>){
			@parms=split " ",$_;
			push @data,[@parms]; # assume it's valid :-)
		}
		close(IN);
	}
	return (@data,$intDly,$cabDly,$refDly,$df);
}

# ------------------------------------------------------------------------
sub MyPrint
{
	print $_[0];
	print LOG $_[0];
}
