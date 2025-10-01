package gpsdo::DecodeJacksonLabs;
use strict;
use warnings;

# Library of functions used by Jackson Labs logging scripts (lcxolog.pl uln1100log.pl)
# 
# Revision history
#
#   Date     Version        Author           Notes
# ~~~~~~~~~~ ~~~~~~~ ~~~~~~~~~~~~~~~~~~~ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# 2017-04-28   1.0    Louis Marais       Initial version 
# 2017-08-16   1.1    Louis Marais       Fixed issue in line 75 and lines 262/263 
#                                        with undefined values
#
# Last modification date: 2017-08-16

# Export routines from the library

use Exporter;
use vars qw(@ISA @EXPORT);

@ISA = qw(Exporter);

@EXPORT = qw(decodeMsgXYZSP decodeHealth decodeMsgStatus);

# message decode routines

#----------------------------------------------------------------------------

sub decodeMsgXYZSP
{
  my $s = shift;
  my $xspd = 0;
  my $yspd = 0;
  my $zspd = 0;
  my $uspd = 0;
  # LC-XO format
  if($s =~ /VX VY VZ Accuracy cm\/s TOWms: (-?\d+) (-?\d+) (-?\d+) (\d+)/)
  {
    if (defined $1) { $xspd = $1; }
    if (defined $2) { $yspd = $2; }
    if (defined $3) { $zspd = $3; }
    if (defined $4) { $uspd = $4; }
  }
  # ULN1100 (Firefly-II) format
  if($s =~ /VX VY VZ Accuracy cm\/s TOWms Leapseconds: (-?\d+) (-?\d+) (-?\d+) (\d+)/)
  {
    if (defined $1) { $xspd = $1; }
    if (defined $2) { $yspd = $2; }
    if (defined $3) { $zspd = $3; }
    if (defined $4) { $uspd = $4; }
  }
  # For debugging
  #print "\$xspd: $xspd  \$yspd: $yspd  \$zspd: $zspd  \$uspd: $uspd\n";
  return($xspd,$yspd,$zspd,$uspd);
} # decodeMsgXYZSP

#----------------------------------------------------------------------------

sub stripSpaces
{
  my($s) = @_;
  # strip off leading spaces
  $s =~ s/^\s+//;
  # strip off trailing spaces
  $s =~ s/\s+$//;
  # return result
  return $s
} # stripSpaces

#----------------------------------------------------------------------------

sub Addstring
{
  my ($s,$t) = (@_);
  if(!defined $s) { $s = ""; }
  if(!defined $t) { $t = ""; }
  if ($s ne "") { $s = $s.", "; }
  return ($s.$t);
} # Addstring

#----------------------------------------------------------------------------
# Health is the or-ed of the following:
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#   1:   0x1  OCXO DAC at 255
#   2:   0x2  OCXO DAC at 0
#   4:   0x4  UTC phase offset > 250 ns
#   8:   0x8  Run-time < 300 s
#  16:  0x10  GPS holdover > 60 s
#  32:  0x20  Freq estimate out of bounds
#  64:  0x40  OCXO over voltage
# 128:  0x80  OCXO under voltage
# 256: 0x100  Short term drift > 100 ns
# 512: 0x200  Within 7 minutes of phase reset | coarse DAC change
#
sub decodeHealth
{
  my $s = shift;
  # for debugging
  #print "health: $s\n";
  my $health = "";
  if($s =~ /0x(\w+)/)
  {
    my $h = sprintf("%04s",$1);
    my $hn = ord(pack("H2",substr $h,0,2))*256+ord(pack("H2",substr $h,2,2));
    # For debugging
    #print "$hn\n";
    if (($hn & 1) == 1) { $health = Addstring($health,"OCXO DAC at 255"); }
    if (($hn & 2) == 2) { $health = Addstring($health,"OCXO DAC at 0"); }
    if (($hn & 4) == 4) { $health = Addstring($health,"UTC phase offset > 250 ns"); }
    if (($hn & 8) == 8) { $health = Addstring($health,"Run-time < 300 s"); }
    if (($hn & 16) == 16) { $health = Addstring($health,"GPS holdover > 60 s"); }
    if (($hn & 32) == 32) { $health = Addstring($health,"Freq estimate out of bounds"); }
    if (($hn & 64) == 64) { $health = Addstring($health,"OCXO over voltage"); }
    if (($hn & 128) == 128) { $health = Addstring($health,"OCXO under voltage"); }
    if (($hn & 256) == 256) { $health = Addstring($health,"Short term drift > 100 ns"); }
    if (($hn & 512) == 512) { $health = Addstring($health,"Within minutes of phase reset | coarse DAC change"); }
    # For ULN1100 (Firefly II)
    if (($hn & 2048) == 2048) { $health = Addstring($health,"Strong GPS jamming signal present"); }    
  }
  # For debugging
  #print "\$health: $health\n";
  if ($health eq "") { $health = "Healthy."; } else {$health = "Unhealthy! ".$health; }  
  # For debugging
  #print "Health: $health\n";
  return $health;
} # decodeHealth

#----------------------------------------------------------------------------
# The multiline response from the SYST:STAT? query plus some extra stuff we
# can ignore is passed to this routine. The information we are interested in
# starts at the "ACQUISITION" line.
sub decodeMsgStatus
{
  my($s) = @_;
  # Split multiline input into array of strings
  my @v = split /^/,$s;
  # Quick sanity check
  if(scalar @v < 2) { return; }
  my($i); # predefine, so value is available at end
  my($tracked,$notracked);
  for ($i = 0; $i < (scalar @v); $i++)
  {
    chomp($v[$i]);
    # Find the line containing "Tracking:..."
    if ($v[$i] =~ /Tracking:\s?(\d+)/)
    {
      $tracked = $1;
      # Find number of non-tracked satellites
      if ($v[$i] =~ /Not Tracking:\s?(\d+)/)
      {
        $notracked = $1;
      }
      last;
    }
  }
  $i++; 
  # Strip off leading space, get array of resulting substrings
  # If array is larger than 4 items, we also have untracked sat info
  my @satinfo;
  my $sats = "";
  my $elvs = "";
  my $azim = "";
  my $snrs = "";
  for ($i = $i; $i < (scalar @v); $i++)
  {
    chomp($v[$i]);
    # Strip leading and lagging whitespace
    $v[$i] = stripSpaces($v[$i]);
    # Split string into substrings
    @satinfo = split /\s+/,$v[$i];
    if ((scalar @satinfo) == 0) { last; }
    # Extract satellite information (PRN, elevation, azimuth, signal strength
    if ($satinfo[0] =~ m/\d+/) # ignore header line
    {
      $sats = Addstring($sats,$satinfo[0]);
      $elvs = Addstring($elvs,$satinfo[1]);
      $azim = Addstring($azim,$satinfo[2]);
      $snrs = Addstring($snrs,$satinfo[3]);
    }
  }
  $sats =~ s/ //g;
  $elvs =~ s/ //g;
  $azim =~ s/ //g;
  $snrs =~ s/ //g;
  $i++;
  # Extract latitude and format as such: (-)DD.ddddddd
  my ($lat) = "";
  for ($i = $i; $i < (scalar @v); $i++)
  {
    chomp($v[$i]);
    $v[$i] = stripSpaces($v[$i]);
    # Find line containing "LAT ..."
    #                                              LAT      S 33:46:58.271
    if ($v[$i] =~ m/LAT/)
    {
      $lat = ExtractPos($v[$i]);
      last;
   }
  }
  $i++;
  # Extract longitude and format as such: DDDMM.mmmm [E|W]
  my ($lon) = "";
  for ($i = $i; $i < (scalar @v); $i++)
  {
    chomp($v[$i]);
    $v[$i] = stripSpaces($v[$i]);
    # Find line containing "LON ..."
    #                                              LON      E 151:9:5.859
    if ($v[$i] =~ m/LON/)
    {
      $lon = ExtractPos($v[$i]);
      last;
   }
  }
  $i++;
  # Extract altitude
  my $alt = "";
  for ($i = $i; $i < (scalar @v); $i++)
  {
    chomp($v[$i]);
    $v[$i] = stripSpaces($v[$i]);
    # Find line containing "HGT"
    #                       HGT             80.80 m (MSL)
    if ($v[$i] =~ m/HGT/)
    {
      $_ = $v[$i];
      if(m/(\d+.\d+\s+m\s+\(MSL\))$/)
      {
        $alt = $1;
      }
      last;
   }
  }
  # For debugging
  #print "Sats: $sats\n";
  #print "SNRs: $snrs\n";
  #print "Elv: $elvs\n";
  #print "Azim: $azim\n";
  #print "lat: $lat\n";
  #print "lon: $lon\n";
  #print "alt: $alt\n";  
  return ($sats,$elvs,$azim,$snrs,$lat,$lon,$alt);
} # decodeMsgStatus

#----------------------------------------------------------------------------

sub ExtractPos
{
  my($s) = @_;
  my($pos) = "";
  # For debugging
  #print "\$s: $s\n";
  if ($s =~ /\D+\s+(\D)\s+(\d+):(\d+):(\d+.\d+)/)
  {
    # For debugging
    #print "$1  $2  $3  $4\n";
    # Change minutes, seconds and fractions of seconds to fractions of degrees
    my($fracdeg)=(($3 + $4/60)/60)*100000000;
    $fracdeg = int($fracdeg);
    # put fractional degrees into correct format
    my($fdg) = sprintf("%8s",$fracdeg);
    # protect $1 and $2 values before next regex...
    my $one = $1;
    my $two = $2;
    # replace spaces in formatted strings with zeroes
    $fdg =~ s/ /0/g; 
    # generate position result
    $pos =$two.".".$fdg;
    if($one eq "S" || $one eq "W") { $pos = "-".$pos; }
  }
  return $pos;
} # ExtractPos

#----------------------------------------------------------------------------
