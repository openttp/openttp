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

# 
# Modification history
#
# 12-10-2004 MJW First VERSION
# 19-10-2004 MJW Seems to work ...
# 28-08-2015 MJW Many changes to tidy this up and work with rin2cggts-produced files
# 05-02-2016 MJW More flexible handling of data paths added
# 19-04-2016 MJW SVNs, IOE added to matched track output file. Option to required matched ephemeris
# 12-01-2017 MJW Detect r2cggtts-generated P3 and bail out. Fixed bug in construction of BIPM file names.
#

use POSIX qw(strftime);
use Getopt::Std;

use subs qw(MyPrint);
use vars qw($opt_a $opt_c $opt_d $opt_f $opt_h $opt_i $opt_m $opt_n $opt_o $opt_p $opt_q $opt_r $opt_s $opt_t $opt_v $opt_x $opt_y);

$VERSION = "2.0.2";

$MINTRACKLENGTH=750;
$DSGMAX=200;
$IONOCORR=1; # this means the ionospheric correction is removed!
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
$IOE=12;
$MDIO=15;
$MSIO=17; # only for dual frequency
$SMSI=18; # only for dual frequency
$ISG=19;  # only for dual frequency

$REFIONO=$MDIO;
$CALIONO=$MDIO;
$REFDIFF=$REFGPS;
$MATCHEPHEM=0;

$OUTPUTDIR = ".";

$0=~s#.*/##; # strip path from executable name

if (!getopts('ac:d:e:f:him:n:op:q:r:st:vx:y:') || $opt_h){
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

if ($opt_o){
	$MATCHEPHEM=1;
}

if ($opt_s){
	$REFDIFF=$REFSV;
}

if ($opt_t){
	$MINTRACKLENGTH=$opt_t;
}

if ($opt_i){
	$IONOCORR=0;
}

if ($opt_f){
	$OUTPUTDIR=$opt_f;
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

$refrxname="ref";
if ($opt_m){$refrxname = $opt_m;}

$calrxname="cal";
if ($opt_n){$calrxname = $opt_n;}

$refrxprefix="";
if ($opt_p){$refrxprefix=$opt_p;}

$calrxprefix="";
if ($opt_q){$calrxprefix=$opt_q;}

open LOG, ">$OUTPUTDIR/$refrxname.$calrxname.report.txt";

MyPrint "$0 version $VERSION\n\n";
MyPrint "Run " . (strftime "%a %b %e %H:%M:%S %Y", gmtime)."\n\n";

MyPrint "Mininimum track length = $MINTRACKLENGTH\n";
MyPrint "Ionosphere corrections removed = ".(($IONOCORR)?"yes":"no")."\n";
MyPrint "Maximum DSG = ".$DSGMAX/10.0." ns\n";
MyPrint "Elevation mask = ".$ELEVMASK/10.0." deg\n";
MyPrint "Using ".($opt_s?"REFSV":"REFGPS")."\n";
MyPrint "Filtering by matched ephemeris = ".($MATCHEPHEM?"yes":"no")."\n";
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
	@d=ReadCCTF($refrx,$i,$refrxprefix,$refrxext);
	($refdelay[$CCTFINTDLY],$refdelay[$CCTFCABDLY],$refdelay[$CCTFREFDLY],$refdf)=splice @d,$#d-3; 
	push @ref,@d;
	MyPrint "\tINT= $refdelay[$CCTFINTDLY] CAB= $refdelay[$CCTFCABDLY] REF= $refdelay[$CCTFREFDLY]\n"; 
	
	@d=ReadCCTF($calrx,$i,$calrxprefix,$calrxext);
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
			print "\n!!! CAL data is not dual frequency: '-c measured' is not a valid option\n\n";
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
			print "\n!!! REF data is not dual frequency: '-r measured' is not a valid option\n\n";
			ShowHelp();
			exit;
		}
		$REFIONO=$MSIO;
	}
}

# Try to detect P3 data, and bail out if we are trying to remove the ionosphere
if (1==$IONOCORR){
	if ($refdf){
		# No sure way to detect P3 but if MDIO == MSIO then the file was probably produced by r2cggtts
		# Check a few values
		$ntotest = 30;
		if ($ntotest > $#ref + 1){
			$ntotest = $#ref + 1;
		}
		for ($i=0;$i<$ntotest;$i++){
			last if ($ref[$i][$MSIO] != $ref[$i][$MDIO]);
		}
		if ($i==$ntotest){
			print "\n!!! REF data appears to be P3 and this is not supported.\n";
			exit;
		}
	}
	if ($caldf){
		$ntotest = 30;
		if ($ntotest > $#cal + 1){
			$ntotest = $#cal + 1;
		}
		for ($i=0;$i<$ntotest;$i++){
			last if ($cal[$i][$MSIO] != $cal[$i][$MDIO]);
		}
		if ($i==$ntotest){
			print "\n!!! CAL data appears to be P3 and this is not supported.\n";
			exit;
		}
	}
}

# Dump REFGPS data for later plotting
open(OUT, ">$OUTPUTDIR/$refrxname.refgps.all.txt"); 
for ($i=0;$i<$#ref;$i++){
	print OUT "$ref[$i][$MJD] $ref[$i][$STTIME] $ref[$i][$REFGPS]\n";
}
close(OUT);

open(OUT, ">$OUTPUTDIR/$calrxname.refgps.all.txt");
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

if (!(defined $opt_a)){
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
}

$caldelay[$REPINTDLY]=$caldelay[$CCTFINTDLY];
$caldelay[$REPCABDLY]=$caldelay[$CCTFCABDLY];
$caldelay[$REPREFDLY]=$caldelay[$CCTFREFDLY];

if (!(defined $opt_a)){
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
		    ($ref[$i][$STTIME] == $cal[$j][$STTIME]) &&
		    !( $MATCHEPHEM && ($ref[$i][$IOE] != $cal[$j][$IOE]))
		    )
		{
			
			# Convert STTIME to decimal MJD
			$ref[$i][$STTIME]=~/(\d\d)(\d\d)(\d\d)/;			
			$t = $ref[$i][$MJD]+($1*3600.0+$2*60.0+$3)/86400.0-$startMJD;
			# Note that the measured ionospheric delay is added back in ...
			push @matches, [$t,
				$ref[$i][$REFGPS]/10.0,$cal[$j][$REFGPS]/10.0,
				($ref[$i][$REFDIFF] + $IONOCORR*$ref[$i][$REFIONO])/10.0-
				($cal[$j][$REFDIFF] + $IONOCORR*$cal[$j][$CALIONO])/10.0  + $calCorrection - $refCorrection,
				$ref[$i][$PRN],$ref[$i][$IOE],$cal[$j][$IOE] ];
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

print "Report in $OUTPUTDIR/$refrxname.$calrxname.report.txt\n";
if (`which gnuplot` eq ""){
	print "Can't find gnuplot so skipping plots\n";
	close LOG;
	exit;
}

# Dump stuff for plotting
open(OUT, ">$OUTPUTDIR/$refrxname.$calrxname.matches.txt");
for ($i=0;$i<=$#matches;$i++){
	print OUT "$matches[$i][0] $matches[$i][1] $matches[$i][2] $matches[$i][3] $matches[$i][4] $matches[$i][5] $matches[$i][6]\n"; 
}
close OUT;

$tlast = int($matches[0][0]*86400);
$nav=0;
$av=0;
open(OUT, ">$OUTPUTDIR/$refrxname.$calrxname.av.matches.txt");
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

open(OUT, ">$OUTPUTDIR/plotcmds.gnuplot");
print OUT "set terminal postscript portrait dashed \"Helvetica\" 10\n";
print OUT "set output \"$OUTPUTDIR/$refrxname.$calrxname.ps\"\n";
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
print OUT "plot \"$OUTPUTDIR/$refrxname.$calrxname.matches.txt\" using 1:2 with points pt 6 ps 0.5\n";

print OUT "set title \"CAL:$calrxname filtered tracks\"\n";
$y0 +=$ystep;
print OUT "set origin $x0 , $y0\n";
print OUT "plot \"$OUTPUTDIR/$refrxname.$calrxname.matches.txt\" using 1:3 with points pt 6 ps 0.5\n";

print OUT "set title \"REF($refrxname) - CAL($calrxname) (filtered)\"\n";
$y0 +=$ystep;
print OUT "set origin $x0 , $y0\n";
print OUT "plot \"$OUTPUTDIR/$refrxname.$calrxname.matches.txt\" using 1:4 with points pt 6 ps 0.5,\"$OUTPUTDIR/$refrxname.$calrxname.av.matches.txt\" using 1:2 with lines\n";

close OUT;

`gnuplot  $OUTPUTDIR/plotcmds.gnuplot`;
print "Plots in $OUTPUTDIR/$refrxname.$calrxname.ps\n";

close LOG;

# ------------------------------------------------------------------------
sub ShowHelp
{
	print "Usage: $0 [options] ref_rx_directory cal_rx_directory start_MJD stop_MJD\n\n";
	print "-a        accept delays in header (no prompts)\n";
	print "-c  <modeled|measured> set ionospheric correction used for CAL receiver\n";
	print "-d  <val> set maximum DSG (default=". $DSGMAX/10.0. " ns)\n";
	print "-e  <val> set elevation mask (default = $ELEVMASK degrees)\n";
	print "-f  <val> output folder\n";
	print "-h        show this help\n";
	print "-i        ionosphere correction is used (zero baseline data assumed otherwise)\n";
	print "-m  <val> name to use for REF receiver in output (default = \"ref\")\n";
	print "-n  <val> name to use for CAL receiver in output (default = \"cal\")\n";
	print "-o        filter by matched ephemeris (default=no)\n";
	print "-p  <val> prefix to use for constructing REF file name\n";
	print "-q  <val> prefix to use for constructing CAL file name\n";
	print "-r  <modeled|measured> set ionospheric correction used for REF receiver\n";
	print "-s        use REFSV instead of REFGPS\n";
	print "-t  <val> set mininimum track length (default = $MINTRACKLENGTH s)\n";
	print "-x  <val> file extension for reference   receiver (default = cctf)\n";
	print "-y  <val> file extension for calibration receiver (default = cctf)\n";
	print "-v        show version\n";
	print "\n";
	print "Example: simple use: files are named eg 57402.cctf and in the refdata and caldata directories\n";
	print "		rxdelaycal refdata caldata 57402 57403\n";
	print "Example: reference files are named according to the BIPM convention\n";
	print "		rxdelaycal -p GMAU01 refdata caldata 57402 57403\n";
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
	$rxprefix=$_[2];
	$rxext=$_[3];
	
	$f = "$rcvr/$rxprefix$mjd.$rxext"; 
	if (!-e $f){ # try BIPM style name - $rxext is ignored 
		$mjdYY=int($mjd/1000);
		$mjdXXX=$mjd % 1000;
		$f = sprintf("$rcvr/$rxprefix$mjdYY.%03d",$mjdXXX);
	}
	
	if (!-e $f){
		print "Unable to open either $rcvr/$rxprefix$mjd.$rxext or $f\n"; 
		exit(1);
	}
	
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
		$parms[0] =~ s/[GR]//;
		push @data,[@parms]; # assume it's valid :-)
	}
	close(IN);
	
	return (@data,$intDly,$cabDly,$refDly,$df);
}

# ------------------------------------------------------------------------
sub MyPrint
{
	print $_[0];
	print LOG $_[0];
}
