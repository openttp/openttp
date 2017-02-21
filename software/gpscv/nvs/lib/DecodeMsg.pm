package NV08C::DecodeMsg;
use strict;
use warnings;

# Start date: 2015-06-24
# Last modification date: 2016-06-22

# Export routines from the library

use Exporter;
use vars qw(@ISA @EXPORT);

@ISA = qw(Exporter);

@EXPORT = qw(decodeMsg46 decodeMsg4A decodeMsg4B decodeMsg50 decodeMsg51
             decodeMsg52 decodeMsg60 decodeMsg61 decodeMsg70 decodeMsg72
             decodeMsg73 decodeMsg74 decodeMsg88 decodeMsgE7 decodeMsgF5
             decodeMsgF6 decodeMsgF7 hexStr);

# Switch library used for multiple selection constructs
use Switch;

# message decode routines

# decode message 46h - Time, Date, Time Zone

sub decodeMsg46
{
  my($s)=@_;
  # hex to binary
  $s = hextobin($s);
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
  
  #print "\$dts: $dts\n";
  
  return ($res,$dts);
} # decodeMsg46

# decode message 4Ah - Ionospheric parameters

sub decodeMsg4A
{
  my($s)=@_;
  # hex to binary
  $s = hextobin($s);
  # Check the length of the message
  if(length($s) != 33) { return ""; }
  # First 4 bytes is alpha_0 (FP32)
  my($alpha_0) = unpack "f1",substr $s,0,4;
  # Next 4 bytes is alpha_1 (FP32)
  my($alpha_1) = unpack "f1",substr $s,4,4;
  # Next 4 bytes is alpha_2 (FP32)
  my($alpha_2) = unpack "f1",substr $s,8,4;
  # Next 4 bytes is alpha_3 (FP32)
  my($alpha_3) = unpack "f1",substr $s,12,4;
  # Next 4 bytes is beta_0 (FP32)
  my($beta_0) = unpack "f1",substr $s,16,4;
  # Next 4 bytes is beta_1 (FP32)
  my($beta_1) = unpack "f1",substr $s,20,4;
  # Next 4 bytes is beta_2 (FP32)
  my($beta_2) = unpack "f1",substr $s,24,4;
  # Next 4 bytes is beta_3 (FP32)
  my($beta_3) = unpack "f1",substr $s,28,4;
  # Last byte is reliability flag, 255 = reliable
  my($reliable) = ord(substr $s,32,1);
  return ($alpha_0,$alpha_1,$alpha_2,$alpha_3,$beta_0,$beta_1,$beta_2,$beta_3,$reliable);
}

# decode message 4Bh - GPS, GLONASS and UTC Time Scales parameters

sub decodeMsg4B
{
  my($s)=@_;
  # hex to binary
  $s = hextobin($s);
  # Check the length of the message
  if(length($s) != 42) { return ""; }
  # First 8 bytes is A1 (FP64)
  my($A1) = unpack "d1", substr $s,0,8;
  # Next 8 bytes is A0 (FP64)
  my($A0) = unpack "d1", substr $s,8,8;
  # Next 4 bytes is t_ot (INT32U)
  my($t_ot) = unpack "V1", substr $s,16,4;
  # Next 2 bytes in WN_t (INT16U)
  my($WN_t) = unpack "S1", substr $s,20,2;
  # Next 2 bytes is delta_t_LS (INT16S)
  my($delta_t_LS) = unpack "s1", substr $s,22,2;
  # Next 2 bytes is WN_LSF (INT16U)
  my($WN_LSF) = unpack "S1", substr $s,24,2;
  # Next 2 bytes is DN (INT16U)
  my($DN) = unpack "S1", substr $s,26,2;
  # Next 2 bytes is delta_t_LSF (INT16S)
  my($delta_t_LSF) = unpack "s1", substr $s,28,2;
  # Next byte is reliability of GPS and UTC scales (INT8U): 255 = reliable
  my($GPS_UTC_reliable) = ord(substr $s,30,1);
  # Next 2 bytes is N_A (INT16U)
  my($N_A) = unpack "S1", substr $s,31,2;
  # Next 8 bytes is tau_c (FP64)
  my($tau_c) = unpack "d1", substr $s,33,8;
  # Last byte is reliability of GLONASS and UTC(SU) scales (INT8U): 255 = reliable
  my($GLONASS_UTCSU_reliable) = ord(substr $s,41,1);
  return ($A1,$A0,$t_ot,$WN_t,$delta_t_LS,$WN_LSF,$DN,$delta_t_LSF,$GPS_UTC_reliable,$N_A,$tau_c,$GLONASS_UTCSU_reliable);
}

# decode message 50h - Query of current port status

sub decodeMsg50
{
  my($s)=@_;
  # hex to binary
  $s = hextobin($s);
  # Check the length of the message
  if(length($s) != 6) { return ""; }
  # First byte is port number
  my($portnumber) = ord(substr $s,0,1);
  # Next 4 bytes is baud rate
  my($baudrate) = unpack "V1", substr $s,1,4;
  # Last byte is protocol type
  #  1 - No protocol
  #  2 - NMEA protocol
  #  3 - DIFF protocol
  #  4 - BINR protocol
  #  5 - BINR2 protocol
  my($prot_no) = ord(substr $s,5,1);
  my(@protocol) = qw(NONE NMEA DIFF BINR BINR2);
  return ($portnumber,$baudrate,$protocol[$prot_no - 1]);
}

# decode message 51h - Query of receiver operating parameters

sub decodeMsg51
{
  my($s)=@_;
  # hex to binary
  $s = hextobin($s);
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
# return ($ret,$sats)

sub decodeMsg52
{
  my($s) = @_;
  # hex to binary
  $s = hextobin($s);
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
      #$sats = $sats.$svn.",";
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
    # Add satellite to list if signal to noise is ok
    if($sn > 0) { $sats = $sats.$svn.","; }
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
  # hex to binary
  $s = hextobin($s);
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
  return ($res,$GPSsats,$GLONASSsats,$HDOP,$VDOP);
} # decodeMsg60 

# decode message 61h - DOP and RMS for calculated PVT

sub decodeMsg61
{
  my($s) = @_;
  # hex to binary
  $s = hextobin($s);
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
  # hex to binary
  $s = hextobin($s);
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
  # hex to binary
  $s = hextobin($s);
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
  # hex to binary
  $s = hextobin($s);
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

# decode message 74h - Time scale parameters

sub decodeMsg74
{
  my($s)=@_;
  # hex to binary
  $s = hextobin($s);
  # Check the length of the message
  if(length($s) != 51) { return ""; }
  # First 10 bytes is GPS scale shift from receiver internal time scale (ms)
  my($GPSscaleShift) = toFP80(substr $s,0,10);
  # Next 10 bytes is GLONASS scale shift from receiver internal time scale (ms)
  my($GLONASSscaleShift) = toFP80(substr $s,10,10);
  # Next 10 bytes is GPS time scale shift from UTC (ms)
  my($GPSvUTC) = toFP80(substr $s,20,10);
  # Next 10 bytes is GLONASS scale shift from UTC(SU) (ms)
  my($GLONASSvUTCSU) = toFP80(substr $s,30,10);
  # Next 10 bytes is GPS scale shift from GLONASS (ignoring 3 hour difference) (ms)
  my($GPSvGLONASS) = toFP80(substr $s,40,10);
  # Last byte is Data validity flags
  #   Bit    Value
  #    1   GPS time is valid
  #    2   GLONASS time is valid
  #    3   UTC time is valid
  #    4   UTC(SU) time is valid
  my($datavalid) = ord(substr $s,50,1);
  return ($GPSscaleShift,$GLONASSscaleShift,$GPSvUTC,$GLONASSvUTCSU,$GPSvGLONASS,$datavalid);
}

# decode message 88h - PVT vector

sub decodeMsg88
{
  my($s)=@_;
  # hex to binary
  $s = hextobin($s);
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
  # hex to binary
  $s = hextobin($s);
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

# decode message F5h - Raw data

# Sample message:
# F5 00:00:02 
# Preamble:  
#             8190b2ba83999441
#             6d03
#             ade6d4ffff99d040
#             0000000070996441
#             00
# Channel 1:
#             02
#             0f
#             0f
#             2a
#             90eb09292b4c7fc1
#             004805809f0e5340
#             0000f0d83e038dc0
#             3f
#             00
# Channel 2:
#             02
#             1b
#             1b
#             2f
#             6ca136b46fbd73c1
#             0048951d39d45240
#             000080f3fc52ae40
#             3f
#             00
# Channels 3 - 14:
#             0218182400b8973281cd5ec1004865c481ab5440000020e5205da8403f00
#             020d0d2a1883adbddf3670c100489580a45355400000c052dd9b9ec03f00
#             021d1d26e6a23eddd19182c10048a5c36f19544000008cefab6aa3c03f00
#             021414283029acb8c6c880c10048e5283bc1534000008893ba9195c03f00
#             020a0a2c3c9560ae674281c1004885a1b28e51400000e422a9d8a2403f00
#             02202030d02041be5e786ec100481501c285534000001c60b62db3403f00
#             0215152dec3131f0906d87c1004815f5431f5140000000fad87061403f00
#             02121233463ac41affd686c10048d5d0199550400000d096ba3c80403f00
#             021a1a2678b0987f210c77c100485521678b54400000d86bbc0c91c03f00
#             0210102cf0bfb9bd1c8671c100481555f1e653400000c05ec5b164c03f00
#             020e0e28105bedbb26c65cc10048d5ebca81544000007e828d4eb4403f00
#             0208081a0000000000000000004845f41732554000007051d29ab2401300

sub decodeMsgF5
{
  my($s)=@_;
  # hex to binary
  $s = hextobin($s);
  # Check the length of the message - it should contain at least 1 channel's data
  if (length($s) < 57) { return ""; }
  # Decode preamble (first 27 bytes)
  # First 8 bytes is time of measurement in ms (UTC)
  my($timemeas) = unpack "d1", substr $s,0,8;
  # Next 2 bytes is week number
  my($wn) = unpack "S1", substr $s,8,2;
  # Next 8 bytes is GPS-UTC shift, ms
  my($gps_UTC) = unpack "d1", substr $s,10,8;
  # Next 8 bytes is GLONASS-UTC shift, ms
  my($glonass_UTC) = unpack "d1", substr $s,18,8;
  # Next byte is Receiver time scale correction, ms
  my($rxtscorr) = toINT8S(substr $s,26,1);
  # For each channel decode raw data
  my(@signaltype,@satno,@glonasscarrierno,@snr,@carrierphase,@psuedorange,@dopplerfreq,@rawdataflags);
  my($channel) = 0;
  while(length($s) >= (27+30*($channel+1)))
  {
    # First byte is signal type
    #   0x01 - GLONASS
    #   0x02 - GPS
    #   0x04 - SBAS
    $signaltype[$channel] = ord(substr $s,(27+(30*$channel)),1);
    # Next byte is satellite number
    $satno[$channel] = ord(substr $s,(28+(30*$channel)),1);
    # Next byte is GLONASS carrier number
    $glonasscarrierno[$channel] = ord(substr $s,(29+(30*$channel)),1);
    # Next byte is signal to noise ratio, db-Hz
    $snr[$channel] = ord(substr $s,(30+(30*$channel)),1);
    # Next 8 bytes is carrier phase, cycles
    $carrierphase[$channel] = unpack "d1", substr $s,(31+(30*$channel)),8;
    # Next 8 bytes is psuedo range, ms
    $psuedorange[$channel] =  unpack "d1", substr $s,(39+(30*$channel)),8;
    # Next 8 bytes is Doppler frequency, Hz
    $dopplerfreq[$channel] =  unpack "d1", substr $s,(47+(30*$channel)),8;
    # Next byte is Raw data flags
    #   0x01 - Signal present (tracking)
    #   0x02 - Millisecond psuedorange / Doppler frequency measurements present
    #   0x04 - Psuedorange measurements are smoothed
    #   0x08 - Carrier phase measurements present
    #   0x10 - Signal time is available (full psuedo range)
    #   0x20 - Preamble not detected (half-cycle ambiguity for carrier phase)
    $rawdataflags[$channel] = ord(substr $s,(55+(30*$channel)),1);
    # Last byte is reserved (usually 0x00) We won't bother deconding that!
    $channel++;
  }
  # Construct results table
  my($table) =    "Time                    : $timemeas ms(UTC)\n";
  $table = $table."Week number             : $wn\n";
  $table = $table."GPS-UTC time shift      : $gps_UTC ms\n";
  $table = $table."GLONASS-UTC time shift  : $glonass_UTC ms\n";
  $table = $table."Rx time scale correction: $rxtscorr ms\n\n";
  $table = $table."                                        Carrier       Psuedo      Doppler   Raw\n";
  $table = $table."Signal Satellite   GLONASS    SNR        phase        range      Frequency Data\n";
  $table = $table." type   number   carrier no (dB-Hz)     (cycles)       (ms)         (Hz)   Flags\n\n";
  #                  02      99         99        99   1234.134 12345. 342342343 0xFF
  #                  02      99         --        99   1234.134 12345. 
  for(my $i = 0; $i < $channel; $i++)
  {
    $table = $table.sprintf("  %02d %7d         ",$signaltype[$i],$satno[$i]);
    if($signaltype[$i] != 1){
      $table = $table."--"; 
    } else { 
      $table = $table.sprintf("%2d",$glonasscarrierno[$i]); 
    }
    $table = $table.sprintf("%10d  ",$snr[$i]);
    $table = $table.sprintf("%15.5f ",$carrierphase[$i]);
    $table = $table.sprintf("%10.5f ",$psuedorange[$i]);
    $table = $table.sprintf("%12.5f 0x",$dopplerfreq[$i]);
    $table = $table.sprintf("%02x\n",$rawdataflags[$i]);
  }
  return ($table);
}

# decode message F6h - Geocentric coordinates of antenna in WGS-84 system

sub decodeMsgF6
{
  my($s)=@_;
  # hex to binary
  $s = hextobin($s);
  # Check the length of the message
  if (length($s) != 49) { return ""; }
  # First 8 bytes is X in metres
  my($X) = unpack "d1", substr $s,0,8;
  # Next 8 bytes is Y in metres
  my($Y) = unpack "d1", substr $s,8,8;
  # Next 8 bytes is Z in metres
  my($Z) = unpack "d1", substr $s,16,8;
  # Next 8 bytes is RMS error of X in metres
  my($Xrms) = unpack "d1", substr $s,24,8;
  # Next 8 bytes is RMS error of Y in metres
  my($Yrms) = unpack "d1", substr $s,32,8;
  # Next 8 bytes is RMS error of Z in metres
  my($Zrms) = unpack "d1", substr $s,40,8;
  # Last byte indicates Static (0) or Dynamic (1) mode
  my($mode) = ord(substr $s,48,1);
  return ($X,$Y,$Z,$Xrms,$Yrms,$Zrms,$mode);
}

# decode message F7h - Extended Ephemeris of Satellites

sub decodeMsgF7
{
  my($s)=@_;
  # hex to binary
  $s = hextobin($s);
  # Check the length of the message - this message can have 2 different lengths,
  # depending on whether it is providing GPS or GLONASS ephemeris
  if(length($s) < 2) { return ""; }
  my($satsys) = ord(substr $s,0,1);
  my($satno) = ord(substr $s,1,1);
  # Decode GPS data ($satsys = 1)
  if($satsys == 1)
  {
    # First make sure message is long enough
    if(length($s) != 138) { return ""; }
    # First 4 bytes is Crs in metres
    my($Crs) = unpack "f1", substr $s,2,4;
    # Next 4 bytes is delta_n in radians/ms
    my($delta_n) = unpack "f1", substr $s,6,4;
    # Next 8 bytes is M0 in radians
    my($M0) = unpack "d1", substr $s,10,8;
    # Next 4 bytes is Cuc in radians
    my($Cuc) = unpack "f1", substr $s,18,4;
    # Next 8 bytes is e
    my($e) = unpack "d1", substr $s,22,8;
    # Next 4 bytes is $Cus in radians
    my($Cus) = unpack "f1", substr $s,30,4;
    # Next 8 bytes is sqrt of A in m^1/2
    my($sqrtA) = unpack "d1", substr $s,34,8;
    # Next 8 bytes is toe in ms
    my($toe) = unpack "d1", substr $s,42,8;
    # Next 4 bytes is C_ic in radians
    my($Cic) = unpack "f1", substr $s,50,4;
    # Next 8 bytes is Omega 0 in radians
    my($Omega0) = unpack "d1", substr $s,54,8;
    # Next 4 bytes is Cis in radians
    my($Cis) = unpack "f1", substr $s,62,4;
    # Next 8 bytes is i0 in radians
    my($i0) = unpack "d1", substr $s,66,8;
    # Next 4 bytes is Crc in metres
    my($Crc) = unpack "f1", substr $s,74,4;
    # Next 8 bytes is omega in radians
    my($omega) = unpack "d1", substr $s,78,8;
    # Next 8 bytes is dot Omega (OmegaR) in radians/ms
    my($OmegaR) = unpack "d1", substr $s,86,8;
    # Next 8 bytes is IDOT in radians/ms
    my($IDOT) = unpack "d1", substr $s,94,8;
    # Next 4 bytes is Tgd in ms
    my($Tgd) = unpack "f1", substr $s,102,4;
    # Next 8 bytes is toc in ms
    my($toc) = unpack "d1", substr $s,106,8;
    # Next 4 bytes is Af2 in ms/ms^2
    my($Af2) = unpack "f1", substr $s,114,4;
    # Next 4 bytes is Af1 in ms/ms
    my($Af1) = unpack "f1", substr $s,118,4;
    # Next 4 bytes is Af0 in ms
    my($Af0) = unpack "f1", substr $s,122,4;
    # Next 2 bytes is URA
    my($URA) = unpack "S1", substr $s,126,2;
    # Next 2 bytes is IODE
    my($IODE) = unpack "S1", substr $s,128,2;
    # Next 2 bytes is IODC
    my($IODC) = unpack "S1", substr $s,130,2;
    # Next 2 bytes is C/A or P on L2
    my($codeL2) = unpack "S1", substr $s,132,2;
    # Next 2 bytes is L2 P data flag
    my($L2Pdata) = unpack "S1", substr $s,134,2;
    # Last 2 bytes is week number
    my($WN) = unpack "S1", substr $s,136,2;
    # Construct ephemeris table
    my($table) = "GPS satellite ".$satno." ephemeris:\n";
    $table = $table.sprintf("   C_rs: %13.6e       delta_n: %13.6e      M0: %13.6e\n",$Crs,$delta_n,$M0);
    $table = $table.sprintf("   C_uc: %13.6e             e: %13.6e    C_us: %13.6e\n",$Cus,$e,$Cus);
    $table = $table.sprintf("  sqrtA: %13.6e          t_oe: %13.6e    C_ic: %13.6e\n",$sqrtA,$toe,$Cic);
    $table = $table.sprintf(" Omega0: %13.6e          C_is: %13.6e     i_0: %13.6e\n",$Omega0,$Cis,$i0);
    $table = $table.sprintf("   C_rc: %13.6e         omega: %13.6e  OmegaR: %13.6e\n",$Crc,$omega,$OmegaR);
    $table = $table.sprintf("   IDOT: %13.6e          T_gd: %13.6e    t_oc: %13.6e\n",$IDOT,$Tgd,$toc);
    $table = $table.sprintf("   a_f2: %13.6e          a_f1: %13.6e    a_f0: %13.6e\n",$Af2,$Af1,$Af0);
    $table = $table.sprintf("    URA: %13d          IODE: %13d    IODC: %13d\n",$URA,$IODE,$IODC);
    $table = $table.sprintf("Code L2: %13d L2P data flag: %13d Week no: %13d\n",$codeL2,$L2Pdata,$WN);
    return ($table);
  }
  elsif ($satsys == 2)
  {
    # First make sure message is long enough
    if(length($s) != 93) { return ""; }
    # First byte is carrier number
    my($carrierno) = toINT8S(substr $s,2,1);
    # next 8 bytes is X in metres
    my($X) = unpack "d1", substr $s,3,8;
    # Next 8 bytes is Y in metres
    my($Y) = unpack "d1", substr $s,11,8;
    # Next 8 bytes is Z in metres
    my($Z) = unpack "d1", substr $s,19,8;
    # Next 8 bytes is velocity in X in metre/ms
    my($vX) = unpack "d1", substr $s,27,8;
    # Next 8 bytes is velocity in Y in metre/ms
    my($vY) = unpack "d1", substr $s,35,8;
    # Next 8 bytes is velocity in Z in metre/ms
    my($vZ) = unpack "d1", substr $s,43,8;
    # Next 8 bytes is acceleration in X in metre/ms^2
    my($aX) = unpack "d1", substr $s,51,8;
    # Next 8 bytes is acceleration in Y in metre/ms^2
    my($aY) = unpack "d1", substr $s,59,8;
    # Next 8 bytes is acceleration in Z in metre/ms^2
    my($aZ) = unpack "d1", substr $s,67,8;
    # Next 8 bytes is t_b in ms
    my($tb) = unpack "d1", substr $s,75,8;
    # Next 4 bytes is gamma_n of t_b
    my($gn) = unpack "f1", substr $s,83,4;
    # Next 4 bytes is tau_n of t_b in ms
    my($tn) = unpack "f1", substr $s,87,4;
    # Last 2 bytes is En
    my($En) = unpack "S1", substr $s,91,2;
    # Construct ephemeris table
    my($table) = "GLONASS satellite ".$satno." ephemeris:\n";
    $table = $table.sprintf("    X: %13.6e       Y: %13.6e     Z: %13.6e\n",$X,$Y,$Z);
    $table = $table.sprintf("vel X: %13.6e   vel Y: %13.6e vel Z: %13.6e\n",$vX,$vY,$vZ);
    $table = $table.sprintf("acc X: %13.6e   acc Y: %13.6e acc Z: %13.6e\n",$aX,$aY,$aZ);
    $table = $table.sprintf("tau b: %13.6e gamma n: %13.6e tau n: %13.6e\n",$tb,$gn,$tn);
    $table = $table.sprintf("  E_n: %3d\n",$En);
    return ($table);
  }
  else
  {
    # There is a small chance that $satsys is invalid. In that case, we return 
    # nothing.
    return "";
  }
}

# End of message deconding subroutines

# Convert from a character to a signed 8 bit value

sub toINT8S
{
  my($s) = @_;
  # Check length of incoming string
  if (length($s) > 1) { return 0; }
  my($i)=ord($s);
  if ($i > 128) { $i = $i - 256; }
  return $i;
} # toINT8S

# Return space separated hex representation of binary data

sub hexStr
{
  my($str) = @_;
  if (length($str) > 0)
  {
    # convert string to hexadecimal ascii string:
    $str = uc(unpack "H*",$str);
    # insert a space between every 2 characters
    $str =~ s/(..)/sprintf("%2s ",$1)/eg;
    return $str;
  } 
  else 
  {
    return "";
  }
} # hexStr

sub hextobin
{
  my($s) =@_;
  $s = pack "H*",$s;
  return $s;
} #hextobin

sub toFP80
{
  # For debugging:
  #print "\nIn toFP80 subroutine\n";

  # takes an FP80 and returns a number in FP64 format.
  # based on FP80toFP64 routine in NVS.cpp in the TTS data processing chain
  my($s) = @_;

  # For debugging:
  #print "Input: ",hexStr($s),"\n";
  
  my($sign) = 1;
  my($chk) = ord(substr $s,9,1);
  if(($chk & 0x80) != 0x00) { $sign = -1; }
  
  # For debugging:
  #print "\$sign: $sign\n";
  
  my($buf8) = ord(substr $s,8,1);
  my($exponent) = (($chk & 0x7f) << 8) + $buf8;
  
  # For debugging:
  #print "\$exponent: $exponent\n";
  
  # We do not have a uint64 - construct by hand, little endian style!
  my($mantissa) = 0;
  for(my $i = 0;$i < 7; $i++)
  {
    $mantissa = $mantissa + ord(substr $s,$i,1) * 2**(8*$i);
  }
  # handle MSB separately - we cannot do this: $mantissa &= 0x7FFFFFFFFFFFFFFF
  # after calculuting the initial value, as it throws a non-protability warning.
  $mantissa = $mantissa + (ord(substr $s,7,1) & 0x7F) * 2**(8*7);
  
  # For debugging:
  #print "\$mantissa: $mantissa\n";

  my($normaliseCorrection);
  if((ord(substr $s,7,1) & 0x80) != 0x00)
  {
    $normaliseCorrection = 1;
  }
  else
  {
    $normaliseCorrection = 0;
  }
  
  # For debugging:
  #print "\$normaliseCorrection: $normaliseCorrection\n";
    
  # This works, but throws a warning: 
  #     Hexadecimal number > 0xffffffff non-portable at testFP80.pl line 63.
  # How can we do this differently to avoid it? See above, it is solved.
  
  #$mantissa &= 0x7FFFFFFFFFFFFFFF;
  
  # For debugging:
  #print "\$mantissa: $mantissa\n";

  # For debugging:
  #my($tempVar) = $mantissa/(1 << 63);
  #print "\$tempVar : $tempVar\n";
  
  return ($sign * 2**($exponent-16383) * ($normaliseCorrection + $mantissa/(1 << 63)));
} # toFP80

# Sample of message 74, hex encoded
# 74 00:00:02 006835a7feffcf840d4000000000000000000000004036b3ffffcf840d40000000000000a08ced3f006835a7feffcf840d401f
# GPS time scale shift from Rx time, ms    : 00 68 35 a7 fe ff cf 84 0d 40 
# GLONASS time scale shift from Rx time, ms: 00 00 00 00 00 00 00 00 00 00 
# GPS scale shift from UTC scale, ms       : 00 40 36 b3 ff ff cf 84 0d 40 
# GLONASS scale shift from UTC(SU), ms     : 00 00 00 00 00 00 a0 8c ed 3f 
# GPS scale shift from GLONASS scale, ms   : 00 68 35 a7 fe ff cf 84 0d 40
# Data validity flag                       : 1f

#double FP80toFP64(uint8_t *buf)
#{
#  // little - endian ....
#  double sign=1.0;
#  if ((buf[9] & 0x80) != 0x00)
#    sign=-1.0;
#  uint32_t exponent = ((buf[9]& 0x7f)<<8) + buf[8];
#  //printf("sign = %i raw exp = %i exp = %i \n",(int) sign, (int) exponent, (int) exponent - 16383);
#  uint64_t mantissa;
#  memcpy(&mantissa,buf,sizeof(uint64_t));
#  // Is this a normalized number ?
#  double normalizeCorrection;
#  if ((mantissa & 0x8000000000000000) != 0x00)
#    normalizeCorrection = 1;
#  else
#    normalizeCorrection = 0;
#  mantissa &= 0x7FFFFFFFFFFFFFFF;
#  
#  return sign * pow(2,(int) exponent - 16383) * (normalizeCorrection + (double) mantissa/((uint64_t)1 << 63));
#}


1;