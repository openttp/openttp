package NV08C::DecodeMsg;
use strict;
use warnings;

use Exporter qw(import);

# export routines from library
our @EXPORT_OK = qw(decodeMsg46 decodeMsg51 decodeMsg52 decodeMsg60 decodeMsg61 decodeMsg70 decodeMsg72 decodeMsg73 decodeMsg88 decodeMsgE7 hexStr);

# Switch library used for multiple selection constructs
use Switch;

# message decode routines

# decode message 46h - Time, Date, Time Zone

sub decodeMsg46
{
  my($s)=@_;
  # Check the length of the message
  if(length($s) != 10) { return ("","1980-01-01 00:00:00"); }
  # First 4 bytes is Time from beginning of week
  my($tfwbs) = substr $s,0,4;
  my($tfwb) = unpack "V1",$tfwbs;
  # Next byte is  Day
  my($dy) = ord(substr $s,4,1);
  # Next byte is Month
  my($mn) = ord(substr $s,5,1);
  # Next 2 bytes is year
  my($yr) = unpack "S1",substr $s,6,2;
  my($dt) = sprintf "%4d-%2d-%2d",$yr,$mn,$dy;
  # Replace spaces with zeroes
  $dt =~ s/ /0/g;
  # Next byte is time zone correction, hours
  my($tzh) = toINT8S(substr $s,8,1);
  # Next byte is time zone correction, minutes
  my($tzm) = toINT8S(substr $s,9,1);
  # decode time
  my($tms) = decodeTime($tfwb);
  # Return results
  my($res) = sprintf("Time zone offset: %d hours %d minutes",$tzh,$tzm);
  my($dts) = $dt." ".$tms;
  return ($res,$dts);
} # decodeMsg46

# decode message 51h - Query of receiver operating parameters

sub decodeMsg51
{
  my($s)=@_;
  # Check the length of the message
  if(length($s) != 11) { return ""; }
  # First byte is operational satellite navigation system
  my($gns) = ord(substr $s,0,1);
  my($gnss);
  switch ($gns) {
    case 0 { $gnss = "GPS, GLONASS"; }
    case 1 { $gnss = "GPS"; }
    case 2 { $gnss = "GLONASS"; }
    case 3 { $gnss = "GALILEO"; }
    case 10 { $gnss = "GPS, GLONASS, SBAS"; }
    case 11 { $gnss = "GPS, SBAS"; }
    case 12 { $gnss = "GLONASS, SBAS"; }
    else { $gnss = "UNDEFINED!"; }
  }
  # Second byte is reserved, always 0
  # 3rd byte is coordinate system, see table 8 page 20 in NV08C BINR protocol manual
  my($coord) = ord(substr $s,2,1);
  my($coords);
  switch ($coord) {
    case 0x00 { $coords = "WGS-84"; }
    case 0x01 { $coords = "PZ-90"; }
    case 0x02 { $coords = "SK-42"; }
    case 0x03 { $coords = "SK-95"; }
    case 0x04 { $coords = "PZ90.02"; }
    case 0xF9 { $coords = "User 1"; }
    case 0xFA { $coords = "User 2"; }
    case 0xFB { $coords = "User 3"; }
    case 0xFC { $coords = "User 4"; }
    case 0xFD { $coords = "User 5"; }
    case 0xFE { $coords = "GK-SK-42"; }
    else { $coords = "UNDEFINED!"; }
  } 
  # 4th byte is satellite height mask, degrees
  my($hmask) = toINT8S(substr $s,3,1);
  # 5th byte is minimum SNR to use for navigation
  my($minsnr) = ord(substr $s,4,1);
  # 6th and 7th bytes are maximum value of rms error at which navigation solution still deemed valid
  my($maxrms) = unpack "S1",substr $s,5,2;
  # bytes 8 to 11 is solution filtration degree
  my($sfd) = unpack "f1",substr $s,7,4;
  # Return result
  my($res) = sprintf("Sat system(s): %s, Coord syst: %s, Elev mask: %d degrees, Min SNR: %d dBHz, Max RMS: %d m, Solution filtration degree: %0.0f",$gnss,$coords,$hmask,$minsnr,$maxrms,$sfd);
  return $res;
} # decodeMsg51

# decode message 52h - Visible satellites

sub decodeMsg52
{
  my($s) = @_;
  # Check length of message - it can contain multiples of 7 byte segments, or nothing
  if(length($s) < 7) { return ("",""); }
  # Define variables for result
  my($res,$sats);
  $sats = "";
  # Loop through the data in 7 byte blocks
  while(length($s) > 0) {
    # First byte: Satellite system
    my($satsys) = ord(substr $s,0,1);
    my($satsyss);
    switch($satsys) { 
      case 1 { $satsyss = "GPS"; }
      case 2 { $satsyss = "GLONASS"; }
      case 4 { $satsyss = "SBAS"; }
      else { $satsyss = "UNDEFINED!"; }
    }        
    # Second byte: Satellite on-board number
    my($sobno) = ord(substr $s,1,1);
    # Third byte: Carrier frequency slot for GLONASS only 
    my($svn,$glofs);
    if ($satsys == 2) {
      #$svn = 64 + $sobno; # NMEA 0183 says GLONASS svn is 64 + slot no
      #                    # https://www.nmea.org/content/nmea_standards/nmea_0183_v_410.asp
      #                    # or http://www.shipops.oregonstate.edu/data/wecoma/cruise/2010/w1004b/documentation_2010/Instrument_Documentation/NMEA_0183_defn.pdf
      $glofs = toINT8S(substr $s,2,1);
      $svn = 45 + $glofs; # This is the way TOPCON receivers calculate the USI which is 
                          # similar to the PRN for GPS satellites.
      $sats = $sats.$svn.",";
      #if ($glofs > 128) { $glofs = $glofs - 256; }  
    } else {
      $svn = $sobno;
      $glofs = 255;
    }
    # Fourth byte: Satellite height mask, degrees
    my($hm) = ord(substr $s,3,1);
    # Fifth and Sixth bytes: Satellite azimuth, degrees
    my($azs) = substr $s,4,2;
    my($az) = unpack "S1",$azs;
    # Seventh byte: Signal to noise ratio
    my($sn) = ord(substr $s,6,1);
    # Show result
    $res = $res.sprintf("%8s %8d  %7d %9d %9d %10d %8d\n",$satsyss,$svn,$sobno,$glofs,$hm,$az,$sn);
    # Reduce string length by 7 bytes
    $s = substr $s,7;
  }
  # Strip off last "," from $sats (if any)
  $sats =~ s/,$//; 
  # Return result
  return ($res,$sats);
} # decodeMsg52

# decode message 60h - Number of Satellites used and DOP

sub decodeMsg60
{
  my($s) = @_;
  # Check the length of the string passed to the sub routine
  if (length($s) != 10) { return ("",0,999.9,999.9); }
  # First byte: Number of GPS satellites
  my($GPSsats)=ord(substr $s,0,1);
  # Second byte: Number of GLONASS satellites
  my($GLONASSsats)=ord(substr $s,1,1);
  # Next 4 bytes: HDOP
  my($HDOP) = unpack "f1",substr $s,2,4;
  # Next 4 bytes: VDOP
  my($VDOP) = unpack "f1",substr $s,6,4;
  # Return result
  my($res) = sprintf "\nNumber of sats - GPS: %2d GLONASS: %2d\nDilution of precision - HDOP: %5.1f VDOP: %5.1f",$GPSsats,$GLONASSsats,$HDOP,$VDOP;
  return ($res,$GLONASSsats,$HDOP,$VDOP);
} # decodeMsg60 

# decode message 61h - DOP and RMS for calculated PVT

sub decodeMsg61
{
  my($s) = @_;
  # Check the length of the string passed to the sub routine
  if (length($s) != 36) { return ("",999.9); }
  # First 4 bytes: HDOP
  my($HDOP)= unpack "f1",substr $s,0,4;
  # Next 4 bytes: VDOP
  my($VDOP)= unpack "f1",substr $s,4,4;
  # Next 4 bytes: TDOP
  my($TDOP)= unpack "f1",substr $s,8,4;
  # Next 4 bytes: Latitude error estimate
  my($lat_err)= unpack "f1",substr $s,12,4;
  # Next 4 bytes: Longitude error estimate
  my($lon_err)= unpack "f1",substr $s,16,4;
  # Next 4 bytes: Altitude error estimate
  my($alt_err)= unpack "f1",substr $s,20,4;
  # Next 4 bytes: Latitude speed error estimate
  my($lat_spd_err)= unpack "f1",substr $s,24,4;
  # Next 4 bytes: Longitude speed error estimate
  my($lon_spd_err)= unpack "f1",substr $s,28,4;
  # Next 4 bytes: Altitude speed error estimate
  my($alt_spd_err)= unpack "f1",substr $s,32,4;
  # Create result string
  my($res) = sprintf ("\nHDOP: %0.1f, VDOP: %0.1f, TDOP: %0.2f,\nLat err: %0.1f m, Lon err: %0.1f m, Alt err: %0.1f m,\nLat spd err: %0.2f m/s, Lon spd err: %0.2f m/s, Alt spd err: %0.2f m/s\n",
                       $HDOP,$VDOP,$TDOP,$lat_err,$lon_err,$alt_err,$lat_spd_err,$lon_spd_err,$alt_spd_err);
  # For debugging:
  #print $res;
  return ($res,$TDOP);
} # decodeMsg61 

# decode message 70h - Software version

sub decodeMsg70
{
  my($s) = @_;
  # Check the length of the string passed to the sub routine
  if (length($s) != 76) { return ""; }
  # First byte: Number of channels
  my($noChnls) = ord(substr $s,0,1);
  # Next 21 bytes: Equipment and software version number (text string)
  my($devID) = substr $s,1,21;
  # Next 4 bytes: Serial number (device ID)
  my($sn) = unpack "I1",substr $s,22,4;
  # Rest of the message is made up of reserved fields - not decoded
  # Return result
  my($res) = sprintf ("NV08C Device ID: %s, Serial number: %d, No of channels: %d",$devID,$sn,$noChnls);
  return $res;
} # decodeMsg70

# decode message 72h - Time and Frequency Parameters

sub decodeMsg72
{
  my($s) = @_;
  # Check the length of the string passed to the sub routine
  if (length($s) != 34) { return ("",999.9); }
  # Define result variable
  my($res);
  # First 10 bytes: current time from beginning of week, in milliseconds, 80 bit value - not decoded
  # Next 2 bytes: GPS week number
  my($gpsweek) = unpack "v1",substr $s,10,2;
  $res = sprintf("GPS week number: %d, ",$gpsweek);
  # Next byte: Time scale type - 0 GLONASS, 1 GPS, 2 UTC(SU), 3 UTC
  my($timescale) = ord(substr $s,12,1);
  my($timescales);
  switch ($timescale) {
    case 0 { $timescales = "GLONASS"; }  
    case 1 { $timescales = "GPS"; }  
    case 2 { $timescales = "UTC(SU)"; }  
    case 3 { $timescales = "UTC"; }  
    else { $timescales = "UNDEFINED!"; }
  }
  $res = $res . sprintf("Time scale: %s, ",$timescales);
  # Next 8 bytes (FP64): Deviation of reference generator period
  my($dev) = unpack "d1",substr $s,13,8;
  $res = $res . sprintf("Deviation from reference generator period: %0.1f s, ",$dev);
  # Next 8 bytes (FP64): Current deviation of time stamp from true scale, ns
  my($saw) = unpack "d1",substr $s,21,8;
  $res = $res . sprintf("Deviation of time stamp from time scale: %0.1f ns, ",$saw);
  # Next 2 bytes: GPS timescale deviation from UTC, seconds
  my($gpsd) = unpack "S1",substr $s,29,2;
  $res = $res . sprintf("Time scale deviation from UTC: %d s",$gpsd);
  # Last 3 bytes: not used - not decoded
  # Return result
  return ($res,$saw);
} # decodeMsg72

# from http://www.perlmonks.org/?node_id=180320
# Code for days, hours, minutes and seconds and "div_mod" from same source

sub decodeTime
{
  my($s)=@_;
  # Find number of days
  my($sc, $mn, $hr, $dy );
  ($mn,$sc) = div_mod($s, 60);
  ($hr,$mn) = div_mod($mn, 60);
  ($dy,$hr) = div_mod($hr, 24);
  # Construct result
  my($res) = sprintf ("%2d:%2d:%2d",$hr,$mn,$sc);
  $res =~ s/ /0/g; 
  # return result
  return $res;
} # decodeTime

sub div_mod { return int( $_[0]/$_[1]) , ($_[0] % $_[1]); }

# decode message 73h - Time synchronisation operating mode

sub decodeMsg73
{
  my($s)=@_;
  # Check the length of the message
  if(length($s) != 11) { return ""; }
  # First byte is mode of operation with coordinates
  my($opermode) = toINT8S(substr $s,0,1);
  my($opermodes);
  switch ($opermode) {
    case 0 { $opermodes = "Dynamic"; }
    case 1 { $opermodes = "With fixed coordinates"; }
    case 2 { $opermodes = "With averaging of coordinates"; }
    else { $opermodes = "UNDEFINED!"; }
  }
  # Following 8 bytes is time stamp formation delay, ms (equal to antenna cable delay)
  my($tf) = (unpack "d1",substr $s,1,8) * 1E6; # convert ms to ns
  # last 2 bytes is coordinate averaging time, minutes
  my($aver)= unpack "s1",substr $s,9,2;
  # Return result
  my($res) = sprintf("Operating mode: %s, Antenna cable delay: %0.0f ns, Coordinates averaging time: %d minutes",$opermodes,$tf,$aver);
  return $res;
} # decodeMsg73

# decode message 88h - PVT vector

sub decodeMsg88
{
  my($s)=@_;
  # Check the length of the message
  if(length($s) != 69) { return ""; }
  # First 8 bytes is latitude in rad
  my($lat) = unpack "d1",substr $s,0,8;
  #$lat = rad2deg($lat);
  # Next 8 bytes is longitude in rad
  my($lon) = unpack "d1",substr $s,8,8; 
  #$lon = rad2deg($lon);
  # Next 8 bytes is height in meter
  my($hgt) = unpack "d1",substr $s,16,8;
  my($hgts) = sprintf("%0.2f",$hgt);
  # Next 4 bytes is rms error of plain coordinates - not decoded
  # Next 10 bytes is time from beginning of week - not decoded
  # Next 2 bytes is GPS week number - not decoded
  # Next 8 bytes is speed in terms of latitude - not decoded
  # Next 8 bytes is speed in terms of longitude - not decoded
  # Next 8 bytes is speed in terms of altitude - not decoded
  # Next 4 bytes is deviation from reference generator period, in ms per s - not decoded
  # last byte is solutions status, each bit carrying value - not decoded
  # Return result
  my($res) = sprintf("Position - lat(DDMM.mm...): %s, lon(DDDMM.mm...): %s, height: %s meter",radtolat($lat),radtolon($lon),$hgts);
  return $res;
} # decodeMsg88

# return latitude in format DDMM.MMMMM N/S given a value in radians as input

sub radtolat
{
  my($s)=@_;
  my($lat) = radtocoord(abs($s));
  if ($s > 0) { $lat = $lat." N"; } else { $lat = $lat." S"}
  return $lat;
} # radtolat

# return longitude in format DDDMM.MMMMM E/W given a value in radians as input

sub radtolon
{
  my($s)=@_;
  my($lon) = radtocoord(abs($s));
  if ($s > 0) { $lon = $lon." E"; } else { $lon = $lon." W"}
  return $lon;
} # radtolon

# return a value in format DDDMM.MMMMM given an input in radians

sub radtocoord
{
  my($s)=@_;
  my($deg) = $s / 3.1415926535898 * 180; 
  my($degs) = POSIX::floor($deg);
  my($min) = ($deg - $degs) * 60;
  my($mins) = POSIX::floor($min);
  my($minss) = sprintf("%2s",$mins);
  $minss =~ s/ /0/g; 
  my($sub) = ($min - $mins);
  my($subs) = sprintf("%0.6f",$sub);
  $subs =~ s/0\./\./;  
  return $degs.$minss.$subs;
} # radtocoord

# decode message E7h - Additional operating parameters (see message D7h <table 31> for format)

sub decodeMsgE7
{
  my($s)=@_;
  # Check the length of the message - message size depends on data
  # type, but should be at least 1 byte long 
  if(length($s) < 1) { return ""; }
  # first byte is data type
  my($datatype) = ord (substr $s,0,1);
  my($res);
  switch ($datatype) {
    case  1 { $res = E7maxaccel($s); } # User's maximum acceleration
    case  2 { $res = E7navrate($s); } # Navigation rate
    case  3 { $res = E7psuedorange($s); } # Psuedo range smoothing interval
    case  4 { $res = "msgE7: Data type 4 not defined"; } # not defined
    case  5 { $res = E7ppscntl($s); } # PPS control
    case  6 { $res = E7antcabdly($s); } # Antenna cable delay, milliseconds
    case  7 { $res = E7navopermode($s); } # Navigation operating mode
    case  8 { $res = E7operdiffcor($s); } # Mode of operation with differential corrections
    case  9 { $res = E7chnldist($s); } # Reception channel distribution among satellite systems
    case 10 { $res = "msgE7: Data type 10 not defined"; } # not defined
    case 11 { $res = E7sbasmode($s); } # Use of SBAS satellites operating in test mode
    case 12 { $res = E7apgs($s); } # Assisted data request on / off
    case 13 { $res = E7rtcmlife($s); } # RTCM corrections lifetime (RTCM has priority over SBAS)
    else { $res = "msgE7: Invalid data type"; }
  }
  # Show result
  return $res;
} # decodeMsgE7

# Sub routines for decoding different parts of the E7h message

# decode message E7 data type 1: User's maximum acceleration

sub E7maxaccel
{
  my($s)=@_;
  # Check the length of the message
  if(length($s) < 5) { return "User's maximum acceleration: No result"; }
  # first byte is data type, ignore
  # next four bytes is maximum allowed acceleration, 0.1 to 100 m/s/s
  my($maxaccel) = unpack "f1",substr $s,1,4;
  my($maxaccels) = sprintf ("User's maximum acceleration: %0.1f m/s/s",$maxaccel);
  return $maxaccels;
} # E7maxaccel

# decode message E7 data type 2: Navigation rate

sub E7navrate
{
  my($s)=@_;
  # Check the length of the message
  if(length($s) < 2) { return "Navigation rate: No result"; }
  # first byte is data type, ignore
  # next byte is navigation rate
  my($navrate) = sprintf("Navigation rate: %1d Hz",ord(substr $s,1,1));
  return $navrate;
} # E7navrate

# decode message E7 data type 3: Psuedo range smoothing interval 

sub E7psuedorange
{
  my($s)=@_;
  # Check the length of the message
  if(length($s) < 3) { return "Psuedo range smoothing interval: No result"; }
  # first byte is data type, ignore
  # next 2 bytes is psuedo range smoothing interval, seconds
  my($psuedo) = unpack "S1",substr $s,1,2;
  my($psuedos) = sprintf("Psuedo range smoothing interval: %0d seconds",$psuedo);
  return $psuedos;
} # E7psuedorange

# decode message E7 data type 5: PPS control

sub E7ppscntl
{
  my($s)=@_;
  # Check the length of the message
  if(length($s) < 7) { return "PPS control: No result"; }
  # first byte is data type, ignore
  # next byte is PPS control, individual bits have meaning
  my($pps) = unpack "B8",substr $s,1,1;
  my($ppstype) = substr $pps,7,1; #lsb
  my($sfttype) = substr $pps,6,1;
  my($ppsver)  = substr $pps,5,1;
  my($ppssgn)  = substr $pps,4,1;
  my($ppssync) = ord(pack "B8",("0000".substr $pps,0,4));
  my($ppstypes);
  my($sfttypes);
  my($ppsvers);
  my($ppssgns);
  my($ppssyncs);
  if($ppstype == "1") { $ppstypes = "Hardware"; } else { $ppstypes = "Software"; }
  if($sfttype == "1") { $sfttypes = "Every second"; } else { $sfttypes = "Navigation rate interval"; }
  if($ppsver == "1") { $ppsvers = "Verified"; } else { $ppsvers = "Not verified"; }
  if($ppssgn == "1") { $ppssgns = "Direct"; } else { $ppssgns = "Inverted"; }
  switch ($ppssync) {
    case (1) {$ppssyncs = "GPS"; }
    case (2) {$ppssyncs = "GLONASS"; }
    case (3) {$ppssyncs = "UTC"; }
    case (4) {$ppssyncs = "UTC(SU)"; }
    else {$ppssyncs = "unsynchronised"; }
  }
  # Next byte: Internal time scale synchronisation
  my($tssync) = ord(substr $s,2,1);
  my($tssyncs);
  if($tssync == 0) { $tssyncs = "Off"; } else { $tssyncs = "On"; } 
  # Next 4 bytes: PPS length, nanoseconds
  my($ppslen) = (unpack "I1",substr $s,3,4)/1000.0; # microseconds
  # Construct response string
  my($res) = sprintf("PPS control: type - %s, ",$ppstypes);
  $res = $res.sprintf("rate - %s, ",$sfttypes);
  $res = $res.sprintf("verification - %s, ",$ppsvers);
  $res = $res.sprintf("presentation - %s, ",$ppssgns);
  $res = $res.sprintf("synchronised to - %s, ",$ppssyncs);
  $res = $res.sprintf("internal timescale sync - %s, ",$tssyncs);
  $res = $res.sprintf("1PPS pulse length is %0.3f microseconds.",$ppslen);  
  return $res;
} # E7ppscntl

# decode message E7 data type 6: Antenna cable delay, milliseconds

sub E7antcabdly
{
  my($s)=@_;
  # Check the length of the message
  if(length($s) < 9) { return "Antenna cable delay: No result"; }
  # first byte is data type, ignore
  # next 8 bytes is antenna cable delay in milliseconds
  my($antdel) = fromFP64(substr $s,1,4);
  my($res) = sprintf("Antenna cable delay: %0.7f milliseconds",$antdel);
  return $res; 
} # E7antcabdly

# decode message E7 data type 7: Navigation operating mode

sub E7navopermode
{
  my($s)=@_;
  # Check the length of the message
  if(length($s) < 3) { return "Navigation operating mode: No result"; }
  # first byte is data type, ignore
  # We will not use this parameter, so it does not make sense to decode it
  return "Navigation operating mode: Not decoded";
} # E7navopermode

# decode message E7 data type 8: Mode of operation with differential corrections

sub E7operdiffcor
{
  my($s)=@_;
  # Check the length of the message
  if(length($s) < 3) { return "Mode of operation with differential corrections: No result"; }
  # first byte is data type, ignore
  # We do not use the receiver with differential corrections, so it does not make sense to decode it
  return "Mode of operation with differential corrections: Not decoded";
} # E7operdiffcor

# decode message E7 data type 9: Reception channel distribution among satellite systems

sub E7chnldist
{
  my($s)=@_;
  # Check the length of the message
  if(length($s) < 4) { return "Reception channel distribution among satellite systems: No result"; }
  # first byte is data type, ignore
  # Don't really care about this at present, will not decode it
  return "Reception channel distribution among satellite systems: Nor decoded";
} # E7chnldist

# decode message E7 data type 11 : Use of SBAS satellites operating in test mode

sub E7sbasmode
{
  my($s)=@_;
  # Check the length of the message
  if(length($s) < 2) { return "Use of SBAS satellites operating in test mode: No result"; }
  # first byte is data type, ignore
  # We do not use SBAS, so it does not make sense to decode this message
  return "Use of SBAS satellites operating in test mode: Not decoded";
} # E7sbasmode

# decode message E7 data type 12: Assisted data request on / off

sub E7apgs
{
  my($s)=@_;
  # Check the length of the message
  if(length($s) < 2) { return "Assisted data request on / off: No result"; }
  # first byte is data type, ignore
  # We do not use AGPS, so it does not make sense to decode this message
  "Assisted data request on / off: Not decoded";
} # E7apgs

# decode message E7 data type 13: RTCM corrections lifetime

sub E7rtcmlife
{
  my($s)=@_;
  # Check the length of the message - message size depends on data
  # type, but should be at least  bytes long 
  if(length($s) < 3) { return "RTCM corrections lifetime: No result"; }
  # first byte is data type, ignore
  # We do not use RTCM, so it does not make sense to decode this message
  return "RTCM corrections lifetime: Not decoded";
} # E7rtcmlife

# End of message deconding subroutines

# Convert from a character to a signed 8 bit value

sub toINT8S{
  my($s) = @_;
  # Check length of incoming string
  if (length($s) > 1) { return 0; }
  my($i)=ord($s);
  if ($i > 128) { $i = $i - 256; }
  return $i;
} # toINT8S

# Return space separated hex representation of binary data

sub hexStr{
  my($str) = @_;
  if (length($str) > 0){
    # convert string to hexadecimal ascii string:
    $str = uc(unpack "H*",$str);
    # insert a space between every 2 characters
    $str =~ s/(..)/sprintf("%2s ",$1)/eg;
    return $str;
  } else {
    return "";
  }
} # hexStr


1;