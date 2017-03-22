package LTELite::DecodeNMEA;
use strict;
use warnings;

# Library of functions used by LTElog.pl and LTEextract.pl
# Decodes NMEA messages and returns stuff I though would be useful.
# 
# Revision history
#
#   Date     Version        Author           Notes
# ~~~~~~~~~~ ~~~~~~~ ~~~~~~~~~~~~~~~~~~~ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# 2016-01-13   1.0    Louis Marais       Initial version 
#
# Last modification date: 2016-06-09

# Export routines from the library

use Exporter;
use vars qw(@ISA @EXPORT);

@ISA = qw(Exporter);

@EXPORT = qw(decodeMsgGGA decodeMsgGLL decodeMsgGSA
             decodeMsgGSV decodeMsgRMC decodeMsgZDA 
             decodeHealth decodeLockStatus
             decodeMsgPJLTS decodeMsgPSTI);

# Switch library used for multiple selection constructs
use Switch;

# message decode routines

#----------------------------------------------------------------------------

sub ExtractPos
{
  my($s,$frmt) = (@_);
  my($pos) = "";
  $_ = $s;
  if (/\D+\s+(\D)\s+(\d+):(\d+):(\d+).(\d+)/)
  {
    # Extract hemisphere
    my($hemi) = $1;  
    # Change seconds and fractions of seconds to fractions of minutes
    my($fracmin)=($4 / 60 + $5 / 60000) * 1000000;
    $fracmin = int($fracmin);
    # put degrees and minutes into correct format
    my($dm)= sprintf($frmt,$2,$3);
    # put fractional minutes into correct format
    my($fms) = sprintf("%6s",$fracmin);
    # replace spaces in formatted strings with zeroes
    $fms =~ s/ /0/g; 
    $dm =~ s/ /0/g;
    # generate position result
    $pos =$dm.".".$fms." ".$hemi;
  }
  return $pos;
} # ExtractPos

#----------------------------------------------------------------------------

sub MonthNumber
{
  my($s) = @_;
  my($t);
  switch ($s)
  {
    case ("Jan") { $t = "1"; }
    case ("Feb") { $t = "2"; }
    case ("Mar") { $t = "3"; }
    case ("Apr") { $t = "4"; }
    case ("May") { $t = "5"; }
    case ("Jun") { $t = "6"; }
    case ("Jul") { $t = "7"; }
    case ("Aug") { $t = "8"; }
    case ("Sep") { $t = "9"; }
    case ("Oct") { $t = "10"; }
    case ("Nov") { $t = "11"; }
    case ("Dec") { $t = "12"; }
    else { $t = "0"; }
  }
  return $t;
} # MonthNumber

#----------------------------------------------------------------------------

sub stripSpaces
{
  my($s) = @_;
  # strip off leading spaces
  $s =~ s/^\s+//;
  # strip off trailing spaces
  $s =~ s/\s+$//;
  # return result
  return $s;
} # stripSpaces

#----------------------------------------------------------------------------

sub FindInfo
{
  my($s,$f) = (@_);
  chomp ($s);
  $s = stripSpaces($s); 
  my($t) = "";
  if ($s =~ /$f(.*)/)
  {
    $t = stripSpaces($1);
    #print "\$t: $t\n";
  }
  return $t;
} # FindInfo

#----------------------------------------------------------------------------

# To implement:
#
# decodeMsgGGA decodeMsgGLL decodeMsgGSA
# decodeMsgGSV decodeMsgRMC decodeMsgZDA

#----------------------------------------------------------------------------

sub decodeMsgGGA
{
  return 0;
} # decodeMsgGGA

#----------------------------------------------------------------------------

sub decodeMsgGLL
{
  return 0;
} # decodeMsgGLL

#----------------------------------------------------------------------------

sub decodeMsgGSA
{
  return 0;
} # decodeMsgGSA

#----------------------------------------------------------------------------


# Partially filled array of satellite parameters is passed to this subroutine,
# as well as the number of messages expected and the number of the previous 
# message. If both these are 0, it means that this is the first message 
# received.

sub decodeMsgGSV
{
  return 0;
} # decodeMsgGSV

#----------------------------------------------------------------------------

sub decodeMsgRMC
{
  return 0;
} # decodeMsgRMC

#----------------------------------------------------------------------------

sub decodeMsgZDA
{
  return 0;
} # decodeMsgZDA

#----------------------------------------------------------------------------

# Lock status values have the following meaning:
#
#  Value    Meaning
#  ~~~~~ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    0    TCXO warmup
#    1    Holdover
#    2    Locking (TCXO training)
#    4    [Value not defined]
#    5    Holdover, but still phase locked (within 100 s of losing GPS lock)
#    6    Locked, no pending events, and GPS is active

sub decodeLockStatus
{
  my($s) = @_;
  my($res) = "";
  switch($s)
  {
    case(0) { $res = "TCXO warmup"; }
    case(1) { $res = "Holdover"; }
    case(2) { $res = "Locking"; }
    case(4) { $res = "Undefined"; }
    case(5) { $res = "Holdover, phase locked"; }
    case(6) { $res = "Locked"; }
  }
  return ($res);
} # decodeLockStatus

#----------------------------------------------------------------------------

# Health status bits have the following meaning:
#
# Health status bit       What it means
# ~~~~~~~~~~~~~~~~~ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#         0x01       TCXO coarse-DAC is maxed out at 255
#         0x02       TCXO coarse-DAC is mined out at 0
#         0x04       Phase offset to UTC > 250 ns
#         0x08       Run time < 300 seconds
#         0x10       GPS is in holdover > 60 s
#         0x20       Frequency estimate is out of bounds
#         0x40       TCXO voltage is too high
#         0x80       TCXO voltage is too low
#         0x200      For first 2 minutes after phase-reset, or a coarse-DAC change
# 
# Example: 0x230 : Within 2 minutes of phase reset or coarse-DAC change, GPS in holdover > 60 s, Frequency estimate out of bounds.
#

sub decodeHealth
{
  # Take the string, and change it to a hex value. Test the hex value against
  # the possible conditions, and add each condition to the result string.
  # Return the result string.
  my($s) = @_;
  my($res) = "";
  my($h) = hex($s);
  if (($h & 0x1) == 0x1) { $res = "OCXO coarse DAC at 255"; }
  if (($h & 0x2) == 0x2) { if ($res ne "") { $res = $res.", "; } $res = $res."OCXO coarse DAC at 0"; }
  if (($h & 0x4) == 0x4) { if ($res ne "") { $res = $res.", "; } $res = $res."UTC phase offset > 250 ns"; }
  if (($h & 0x8) == 0x8) { if ($res ne "") { $res = $res.", "; } $res = $res."Run time < 300 s"; }
  if (($h & 0x10) == 0x10) { if ($res ne "") { $res = $res.", "; } $res = $res."GPS holdover > 60 s"; }
  if (($h & 0x20) == 0x20) { if ($res ne "") { $res = $res.", "; } $res = $res."Frequency estimate out of bounds"; }
  if (($h & 0x40) == 0x40) { if ($res ne "") { $res = $res.", "; } $res = $res."OCXO voltage too high"; }
  if (($h & 0x80) == 0x80) { if ($res ne "") { $res = $res.", "; } $res = $res."OCXO voltage too low"; }
  if (($h & 0x200) == 0x200) { if ($res ne "") { $res = $res.", "; } $res = $res."Within 2 min of phase reset OR coarse DAC change"; }
  if ($res eq "") { $res = "Completely healthy!"; }
  return ($res);
} # decodeHealth

#----------------------------------------------------------------------------

#$PJLTS,0.00,0.00,0,1,1.6732218,55.7740,1.9E-8,655,3,0x230*54
#   |     |    |  | |      |        |     |     |  |   |   |
#   |     |    |  | |      |        |     |     |  |   |   +-- Checksum
#   |     |    |  | |      |        |     |     |  |   +------ Health status
#   |     |    |  | |      |        |     |     |  +---------- Number of satellites tracked
#   |     |    |  | |      |        |     |     +------------- Seconds in holdover
#   |     |    |  | |      |        |     +------------------- Estimated frequency accuracy
#   |     |    |  | |      |        +------------------------- EFC percentage
#   |     |    |  | |      +---------------------------------- EFC voltage
#   |     |    |  | +----------------------------------------- Lock status
#   |     |    |  +------------------------------------------- Number of captured 1PPS pulses
#   |     |    +---------------------------------------------- Raw UTC offset in ns
#   |     +--------------------------------------------------- Filtered UTC offset in ns
#   +--------------------------------------------------------- Message ID

sub decodeMsgPJLTS
{
  my($s) = @_;
  my(@v) = breakNMEA($s);
  my($filteredUTC)        = $v[0];
  my($rawUTC)             = $v[1];
  my($noOfCapturedPulses) = $v[2];
  my($lockStatus)         = $v[3];
  my($EFCvoltage)         = $v[4];
  my($EFCpercentage)      = $v[5];
  my($estimatedFreqAcc)   = $v[6];
  my($secsInHoldover)     = $v[7];
  my($nmbrsats)           = $v[8];
  my($health)             = $v[9];
  return ($filteredUTC,$rawUTC,$noOfCapturedPulses,$lockStatus,$EFCvoltage,$EFCpercentage,
          $estimatedFreqAcc,$secsInHoldover,$nmbrsats,$health);
} # decodeMsgPJLTS

#----------------------------------------------------------------------------

#$PSTI,00,1,1985,-12.4*1E
#  |    | |   |    |   |
#  |    | |   |    |   +-- Checksum
#  |    | |   |    +------ 1PPS Quantisation error in ns
#  |    | |   +----------- Survey length in 1PPS fixes
#  |    | +--------------- Timing mode
#  |    +----------------- Fixed field, always [00]
#  +---------------------- Message ID
#
# Timing mode:
# ~~~~~~~~~~~~
#  0 = PVT mode
#  1 = Survey mode
#  2 = Static (Position Hold) mode

sub decodeMsgPSTI
{
  my($s) = @_;
  my(@v) = breakNMEA($s);
  my($tm) = $v[2];
  if ($tm == 0) { return "PVT"; }
  elsif ($tm == 1) { return "Survey"; }
  return "Position Hold";
} # decodeMsgPSTI

#----------------------------------------------------------------------------

sub breakNMEA
{
  my($s) = @_;
  # Strip off crlf
  $s =~ s/[\x0A\x0D]//g;
  # Remove message identifier and associated comma
  $s = substr $s,7; # could also use  $s =~ s/\$\w{5},//;
  # Remove "*" character and checksum
  $s = substr $s,0,-3; # could also use  $s =~ s/\*\w{2}$//;
  # Split the string and present the result
  my (@v) = split(',',$s);
  # For debugging, show output
  #foreach my $val (@v)
  #{
  #  print "$val\n";
  #}
  #print scalar @v,"\n";
  return(@v);
} # breakNMEA

#----------------------------------------------------------------------------
