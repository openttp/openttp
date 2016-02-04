#!/usr/bin/perl -w

#
#
# The MIT License (MIT)
#
# Copyright (c) 2015  R. Bruce Warrington
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

# Begun RBW 9.8.05
#  8.11.05 RBW  Add YA mode
#  5. 6.06 RBW  NP tracking, -c option to compress, warn no tracking
#  6. 6.06 RBW  vt output of satellite visibility
#  7. 6.06 RBW  tr output of satellite tracks (azimuth and elevation vs time)
#               tv output (list SV for each time) 
# 31.11.06 RBW  Add DO and ST output
# 10.08.07 RBW  Add BP output
# 2015-01-28 MJW Added UO output (to look at incorrect GPS A0 broadcast on 57413
#                Imported into OpenTTP and renamed from rawqc
#

use Getopt::Std;
use vars qw($opt_k);

$0=~s#.*/##;

$ok=getopts('cb:e:gi:ko:p:r') && (@ARGV==1) && !($opt_g && $opt_r);
if ($opt_o) {
  $opt_o=~tr/A-Z/a-z/;
  $ok=$ok && ($opt_o=~/bp|cn|do|si|np|tn|tr|vt|tv|to|ya|za|st|uo/);
}
else {$opt_o=""}
if ($opt_b) {
  $_=&ParseHoursMinutes($opt_b);
  if ($_>=0) {$opt_b=$_} else {$ok=0}
}
else {$opt_b=0}
if ($opt_e) {
  $_=&ParseHoursMinutes($opt_e);
  if ($_>=0) {$opt_e=$_} else {$ok=0}
}
else {$opt_e=86400}
$ok=$ok && ($opt_e>=$opt_b);
unless ($ok) {
  select STDERR;
  print "Usage: $0 infile\n";
  print "  -b x  Start time (s or hh:mm, default 0)\n";
  print "  -c    Compress (eg skip repeat values) if possible\n";
  print "  -e x  Stop time (s or hh:mm, default 24:00)\n";
  print "  -g    Select GPS only\n";
  print "  -i x  Decimation interval (s, default 1)\n";
  print "  -k    Keep uncompressed file if created\n";
  print "  -o x  Output mode:\n";
  print "          bp  Rx sync to reference time (BP message)\n";
  print "          cn  Carrier-to-noise vs elevation\n";
  print "          do  DO derivative of time offset vs time\n";
  print "          si  Visibility vs time (SI message)\n";
  print "          np  Visibility vs time (NP message)\n";
  print "          vt  Visibility vs time (list time for each SV)\n";
  print "          tv  Visibility vs time (list SV for each time)\n";
  print "          tr  Satellite tracks (PRN, azimuth, elevation)\n";
  print "          tn  Satellite tracks (PRN, azimuth, elevation, CN)\n";
  print "          to  TO time offset vs time\n";
  print "          ya  YA time offset vs time\n";
  print "          za  ZA time offset vs time\n";
  print "          st  ST solution time tag vs time\n";
  print "          uo  UO UTC parameters\n";
  print "  -r    Select GLONASS only\n";
  exit;
}
$opt_i=1 unless $opt_i;
if ($opt_o eq "tn") {$opt_o="tr"; $cnflag=1} else {$cnflag=0}

$file=$ARGV[0];
if ($file=~/\.gz$/) {
  $_=$file;
  s#.*/##;
  s/\.gz$//;
  $_="/tmp/$_";
  print STDERR "> Decompressing $ARGV[0] --> $_ ...\n";
  `gzip -dc $file >$_`;
  $file=$_;
}

open IN,$file or die "Bollocks.\n";

$SI_flag=0x01; $flag{"SI"}=$SI_flag;
$AZ_flag=0x02; $flag{"AZ"}=$AZ_flag;
$EL_flag=0x04; $flag{"EL"}=$EL_flag;
$E1_flag=0x08; $flag{"E1"}=$E1_flag;
$E2_flag=0x10; $flag{"E2"}=$E2_flag;
$EC_flag=0x20; $flag{"EC"}=$EC_flag;
$CN_flags=$E1_flag|$E2_flag|$EC_flag;
$YA_flag=0x40; $flag{"YA"}=$YA_flag;
$ZA_flag=0x80; $flag{"ZA"}=$ZA_flag;
$TO_flag=0x100; $flag{"TO"}=$TO_flag;
$NP_flag=0x200; $flag{"NP"}=$NP_flag;
$DO_flag=0x400; $flag{"DO"}=$DO_flag;
$ST_flag=0x800; $flag{"ST"}=$ST_flag;
$BP_flag=0x1000; $flag{"BP"}=$BP_flag;
$UO_flag=0x2000; $flag{"UO"}=$UO_flag;


if ($opt_o) {
  @_=gmtime;
  printf "# $0 run %02d:%02d:%02d %02d/%02d/%02d on file $file\n",
    $_[2],$_[1],$_[0],$_[3],$_[4]+1,$_[5]%100;
  print "# GPS SVs ONLY\n" if $opt_g;
  print "# GLONASS SVs ONLY\n" if $opt_r;
  if ($opt_o eq "bp") {
    print "# receiver time accuracy (s, BP msg; >>1ms means no pos soln)\n";
  }
  elsif ($opt_o eq "cn") {
    print "# elevation P/L1 P/L2 CA (all C/N in dB-Hz)\n";
  }
  elsif ($opt_o eq "do") {
    print "# time derivative of receiver time offset (s/s)\n";
  }
  elsif ($opt_o eq "st") {
    print "# solution time tag (time of day, ms)\n";
  }
  elsif ($opt_o eq "si") {
    print "# Visibility vs time from SI message\n";
    print "# time";
    print " nGPS" unless $opt_r;
    print " nGLONASS" unless $opt_g;
    print "\n";
  }
  elsif ($opt_o eq "np") {
    print "# Visibility vs time from NP message\n";
    print "# time nsats\n";
  }
  elsif ($opt_o eq "vt") {
    print "# Visibility vs time from SI message\n";
  }
  elsif ($opt_o eq "tv") {
    print "# Visibility vs time from SI message\n";
    print "# List of tracked SVs at each time\n";
  }
  elsif ($opt_o eq "tr") {
    print "# Continuous satellite tracks (PRN labelled in leading comment)\n";
    print "# time azimuth(deg) elevation(deg)";
    print " C/N(dB-Hz)" if $cnflag;
    print "\n";
  }
  elsif ($opt_o eq "to") {
    print "# time  TO-value\n";
  }
  elsif ($opt_o eq "ya") {
    print "# time  YA-value\n";
  }
  elsif ($opt_o eq "za") {
    print "# time  ZA-value\n";
  }
  elsif ($opt_o eq "uo") {
    print "# time  UTC parameters\n";
  }
}

$last{"gps"}=-1;
$last{"glo"}=-1;
$last{"done"}=-1;
if ($opt_o eq "vt") {
  $gap=($opt_i > 10? $opt_i+1 : 10);
}
$format="^(..) (\\d\\d):(\\d\\d):(\\d\\d)";

while (<IN>) {
  next unless /$format/;
  $msg=$1;
  $this=(($2*60)+$3)*60+$4;
  next unless (($this>=$opt_b) && ($this<=$opt_e));
  if (defined $last{"time"}) {
    if ($this<$last{"time"})
      {&Out("! Backwards at $& (line $.)\n")}
    elsif ($this>($last{"time"}+1)) 
      {&Out(sprintf "! Missing %d at $& (line $.)\n",$this-$last{"time"}-1)}
    $flag=0 unless $this==$last{"time"};
  }
  else {$flag=0}
  $last{"time"}=$this;
  next unless (($this % $opt_i)==0);
  
  next unless $msg=~/SI|NP|AZ|EL|E1|E2|EC|TO|BP|DO|ST|UO|YA|ZA/;
  $flag|=$flag{$msg};
  @_=split;
  $time=$_[1];
  $data=($msg eq "NP"? $_[2] : pack "H*",$_[2]);

  if ($msg eq "SI") {
    @si=unpack "C*",$data;
    pop @si;	# drop checksum
    @gps=grep {($_>= 1) && ($_<=37)} @si;
    if (@gps==0) {
      $warning{"gps start"}=$time unless defined $warning{"gps start"};
      $warning{"gps stop"}=$time;
    }
    elsif (defined $warning{"gps start"}) {
      &Out(sprintf "! No GPS tracking %s - %s\n",
        $warning{"gps start"},$warning{"gps stop"});
      $warning{"gps start"}=undef;
    }
    @glo=grep {($_>=38) && ($_<=70)} @si;
    map {$gps{$_}=1} @gps;

    if ($opt_o eq "si") {
      if ($opt_c) {
        if ( (((scalar @gps)!=$last{"gps"}) && !$opt_r) ||
	     (((scalar @glo)!=$last{"glo"}) && !$opt_g) ) {
	  if ($last{"gps"}>=0) {
            print $last{"stamp"}," ";
            printf " %d",$last{"gps"} unless $opt_r;
            printf " %d",$last{"glo"} unless $opt_g;
            print "\n";
	  }
          print "$time ";
          printf " %d",(scalar @gps) unless $opt_r;
          printf " %d",(scalar @glo) unless $opt_g;
          print "\n";
	}
	else {
	  $last{"stamp"}=$time;
	}
      }
      else {
        print "$time ";
        printf " %d",(scalar @gps) unless $opt_r;
        printf " %d",(scalar @glo) unless $opt_g;
        print "\n";
      }
      $last{"gps"}=scalar @gps;
      $last{"glo"}=scalar @glo;
    }

    elsif ($opt_o eq "vt") {
      foreach $sv (@si) {
        next if $opt_g && (($sv<1) || ($sv>37));
        next if $opt_r && (($sv<38) || ($sv>70));
	if (defined $visible{$sv}) {
	  $prev=$visible{$sv}[-1][-1];
	  if (($this-$prev)<=$gap) {
	    $visible{$sv}[-1][-1]=$this;
	  }
	  else {
	    $n=scalar @{$visible{$sv}};
	    $visible{$sv}[$n][0]=$this;
	    $visible{$sv}[$n][1]=$this;
	  }
	}
	else {
	  $visible{$sv}[0][0]=$this;
	  $visible{$sv}[0][1]=$this;
	}
      }
    }

    elsif ($opt_o eq "tv") {
      $_="";
      foreach $sv (sort {$a<=>$b} @si) {
        next if $opt_g && (($sv<1) || ($sv>37));
        next if $opt_r && (($sv<38) || ($sv>70));
        $_.=sprintf " %02d",$sv;
      }
      if ($opt_c) {
        if (@visible) {
	  if ($_ ne $visible[-1][1]) {
	    $n=scalar @visible;
	    $visible[$n][0]=$this;
	    $visible[$n][1]=$_;
	  }
	}
	else {
	  $visible[0][0]=$this;
	  $visible[0][1]=$_;
	}
      }
      else {
        print &TimeStamp($this),$_,"\n";
      }
    }
  }

  elsif (($msg eq "NP") && ($opt_o eq "np")) {
    if ($data=~/{([0-9]+),[0-9]+}/) {
      if ($opt_c) {
        if ( (((scalar @gps)!=$last{"gps"}) && !$opt_r) ||
	     (((scalar @glo)!=$last{"glo"}) && !$opt_g) ) {
	  if ($last{"gps"}>=0) {
            print $last{"stamp"}," ";
            printf " %d",$last{"gps"} unless $opt_r;
            printf " %d",$last{"glo"} unless $opt_g;
            print "\n";
	  }
          print "$time ";
          printf " %d",$1 unless $opt_r;
          printf " %d",$2 unless $opt_g;
          print "\n";
	}
      }
      else {
        print "$time ";
        printf " %d",$1 unless $opt_r;
        printf " %d",$2 unless $opt_g;
        print "\n";
      }
      $last{"gps"}=$1;
      $last{"glo"}=$2;
    }
  }

  elsif ($msg eq "AZ") {
    @az=unpack "C*",$data;
    pop @az;
    @az=map {2*$_} @az;		# azimuth values must be doubled, see GRIL
  }
  elsif ($msg eq "EL") {
    @el=unpack "c*",$data;
    pop @el;
  }
  elsif ($msg=~/E[12C]/) {
    @cn=unpack "C*",$data;
    pop @cn;
  }
  elsif (($msg eq "TO") && ($opt_o eq "to")) {
    @to=unpack "dC",$data;
    print "$time $to[0]\n";
  }
  elsif (($msg eq "BP") && ($opt_o eq "bp")) {
    @bp=unpack "fC",$data;
    print "$time $bp[0]\n";
  }
  elsif (($msg eq "DO") && ($opt_o eq "do")) {
    @do=unpack "fC",$data;
    print "$time $do[0]\n";
  }
  elsif (($msg eq "ST") && ($opt_o eq "st")) {
    @st=unpack "ICC",$data;
    print "$time $st[0]\n";
  }
  elsif (($msg eq "YA") && ($opt_o eq "ya")) {
    @ya=unpack "dCC",$data;
    print "$time $ya[0]\n";
  }
  elsif (($msg eq "ZA") && ($opt_o eq "za")) {
    @za=unpack "fC",$data;
    print "$time $za[0]\n";
  }
	elsif (($msg eq "UO") && ($opt_o eq "uo")) {
    @uo=unpack "dfISc",$data;
    print "$time $uo[0] $uo[1] $uo[2] $uo[3] $uo[4] \n";
  }
  
  if (($opt_o eq "cn") && ($this!=$last{"done"}) &&
      ($flag & $SI_flag) && ($flag & $CN_flags) && ($flag & $EL_flag) &&
      (@el==@si) && (@cn==@si)) {
    for ($i=0; $i<@si; $i++) {
      next if ($si[$i]>= 1) && ($si[$i]<=37) && $opt_r;
      next if ($si[$i]>=38) && ($si[$i]<=70) && $opt_g;
      $CN{$el[$i]}{$msg}{$cn[$i]}=1;
    }
    $last{"done"}=$this;
  }

  if (($opt_o eq "tr") && ($this!=$last{"done"}) &&
      ($flag & $SI_flag) && ($flag & $EL_flag) && ($flag & $AZ_flag) &&
      (@el==@si) && (@az==@si) &&
      (!$cnflag || (($flag & $CN_flags) && (@cn==@si)))) {
    for ($i=0; $i<@si; $i++) {
      $sv=$si[$i];
      next if $opt_g && (($sv<1) || ($sv>37));
      next if $opt_r && (($sv<38) || ($sv>70));
      if (defined $track{$sv}) {
        if (($az[$i]!=$track{$sv}[-1][-1][1]) || 
            ($el[$i]!=$track{$sv}[-1][-1][2]) ||
	    ($cnflag && (abs($cn[$i]-$track{$sv}[-1][-1][3])>=2))) {
          if ((abs($az[$i]-$track{$sv}[-1][-1][1])<=4) &&
              (abs($el[$i]-$track{$sv}[-1][-1][2])<=1)) {
	    # continue current track
	    $j=-1;
	    $k=scalar @{$track{$sv}[-1]};
	  }
	  else {
	    # new track for this SV
	    $j=scalar @{$track{$sv}};
	    $k=0;
	  }
	}
	else {
	  # same as last time, just skip it
	  $j=$k=-2;
	}
      }
      else {
        # first track for this SV
	$j=$k=0;
      }
      if (($j>=-1) && ($k>=-1)) {
        $track{$sv}[$j][$k][0]=$this;
        $track{$sv}[$j][$k][1]=$az[$i];
        $track{$sv}[$j][$k][2]=$el[$i];
        $track{$sv}[$j][$k][3]=$cn[$i] if $cnflag;
      }
    }
    $last{"done"}=$this;
  }
}
close IN;

if (($opt_o eq "si") && $opt_c) {
  print $last{"stamp"}," ";
  printf " %d",$last{"gps"} unless $opt_r;
  printf " %d",$last{"glo"} unless $opt_g;
  print "\n";
}

@_=sort {$a <=> $b} keys %gps;
&Out(sprintf "> GPS (%d): @_\n",scalar @_);

if ($opt_o eq "cn") {
  foreach $el (sort {$a<=>$b} (keys %CN)) {
    $n=$max=0;
    foreach $msg ("E1","E2","EC") {
      $cn[$n]=[sort {$a<=>$b} (keys %{$CN{$el}{$msg}})];
      $len=scalar @{$cn[$n]};
      $max=$len if $len>$max;
      $n++;
    }
    for ($n=0; $n<3; $n++) {
      until (@{$cn[$n]}==$max) {push @{$cn[$n]},"?"}
        # pad to same length with 'missing' value (for gnuplot)
    }
    for ($n=0; $n<$max; $n++) {
      print "$el $cn[0][$n] $cn[1][$n] $cn[2][$n]\n";
    }
  }
}

elsif ($opt_o eq "vt") {
  foreach $sv (sort {$a<=>$b} (keys %visible)) {
    printf "%02d: ",$sv;
    for ($i=0; $i<@{$visible{$sv}}; $i++) {
      printf " %s-%s",
        &TimeStamp($visible{$sv}[$i][0]),&TimeStamp($visible{$sv}[$i][1]);
    }
    print "\n";
  }
}

elsif ($opt_o eq "tr") {
  $n=0;
  foreach $sv (sort {$a<=>$b} (keys %track)) {
    for ($j=0; $j<@{$track{$sv}}; $j++) {
      $n++;
      printf "\n# PRN %02d\n",$sv;
      for ($k=0; $k<@{$track{$sv}[$j]}; $k++) {
        printf "%s %d %d",&TimeStamp($track{$sv}[$j][$k][0]),
	  $track{$sv}[$j][$k][1],$track{$sv}[$j][$k][2];
	printf " %d",$track{$sv}[$j][$k][3] if $cnflag;
        print "\n";
      }
    }
  }
  &Out(sprintf "> Total $n tracks for %d SVs\n",scalar keys %track);
}

elsif (($opt_o eq "tv") && $opt_c) {
  for ($i=0; $i<@visible; $i++) {
    print &TimeStamp($visible[$i][0])," ",$visible[$i][1],"\n";
  }
}

if (($file ne $ARGV[0]) && !$opt_k) {
  print STDERR "> Removing $file\n";
  unlink $file;
}

# end

#----------------------------------------------------------------------------

sub TimeStamp {
  my $s=$_[0];
  my ($h,$m);

  $h=int $s/3600;
  $s-=3600*$h;
  $m=int $s/60;
  $s-=60*$m;

  return sprintf "%02d:%02d:%02d",$h,$m,$s;
} # TimeStamp

#----------------------------------------------------------------------------

sub ParseHoursMinutes {
  my $val=$_[0];

  if ($val=~/^(\d{1,2}):(\d\d)$/) 
    {return (($1*60)+$2)*60}
  elsif ($val=~/^\d+$/)
    {return $val}
  else
    {return -1}
} # ParseHoursMinutes

#----------------------------------------------------------------------------

sub Out {
  print STDERR $_[0];
  print "#$_[0]" if $opt_o;
} # Out 

#----------------------------------------------------------------------------
