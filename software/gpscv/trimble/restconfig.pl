#!/usr/bin/perl -w



use Time::HiRes qw( gettimeofday);
use TFLibrary;
use POSIX;
use Getopt::Std;
use POSIX qw(strftime);
use vars  qw($tmask $opt_r);

$DEBUG=0;


$home=$ENV{HOME};
if (-d "$home/etc")  {$configpath="$home/etc";}  else 
	{$configpath="$home/Parameter_Files";}

if (-d "$home/logs")  {$logpath="$home/logs";} else 
	{$logpath="$home/Log_Files";}


$0=~s#.*/##;
$Init{version}="1.0";
$Init{file}=$configpath."/cctf.setup";


# Check for an existing lock file
$lockfile = $logpath."/rx.lock";
if (-e $lockfile)
{
	open(LCK,"<$lockfile");
	$pid = <LCK>;
	chomp $pid;
	if (-e "/proc/$pid")
	{
		printf STDERR "Process $pid already running\n";
		exit;
	}
	close LCK;
}


#Initialise parameters

print "Configuring the serial port for 115200, no parity\n";
print "Note! Default serial port settings for the Resolution T are assumed:\n";
print " 9600 baud, odd parity, 8 data bits, 1 stop bit.\n";

&Initialise(@ARGV==1? $ARGV[0] : $Init{file});

sleep 3;


print "Configuring ...\n";

&SendCommand("\xBC\x00\x0b\x0b\x03\x00\x00\x00\x02\x02\x00");

sleep 3;

print "Closing the serial port ...\n";
close $rx;
sleep 3;

print "Reopening the serial port at new speed\n";

# open the serial ports to the receiver
$rxmask="";
$port=$Init{"gps port"};
$port="/dev/$port" unless $port=~m#/#;

$rx=&TFConnectSerial($port,
      (ispeed=>0010002,ospeed=>0010002,iflag=>IGNBRK,
       oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|CLOCAL));

# set up a mask which specifies this port for select polling later on
vec($rxmask,fileno $rx,1)=1;

print "Writing settings to FLASH\n";
&SendCommand("\x8E\x26");
# This will cause a reset
sleep 3;

print "Done! New port settings are 115200 baud, no parity\n";

# end of main 

#----------------------------------------------------------------------------

sub Initialise 
{
  my $name=shift;
  my @required=("gps port");
  my ($tag,$err);
  
  open IN,$name or die "Could not open initialisation file $name: $!";
  while (<IN>) {
    s/#.*//;            # delete comments
    s/\s+$//;           # and trailing whitespace
    if (/(.+)=(.+)/) {  # got a parameter?
      $tag=$1;
      $tag=~tr/A-Z/a-z/;        
      $Init{$tag}=$2;
    }
  }

  # check that all required information is present
  $err=0;
  foreach (@required) {
    unless (defined $Init{$_}) {
      print STDERR "! No value for $_ given in $name\n";
      $err=1;
    }
  }
  exit if $err;

  # open the serial ports to the receiver
  $rxmask="";
  $port=$Init{"gps port"};
  unless (`/usr/local/bin/lockport $port $0`==1) {
    printf "! Could not obtain lock on $port. Exiting.\n";
    exit;
  }
  $port="/dev/$port" unless $port=~m#/#;
  #$rx=&TFConnectSerial($port,
  #    (ispeed=>0010002,ospeed=>0010002,iflag=>IGNBRK,
  #     oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|PARENB|PARODD|CLOCAL));

	$rx=&TFConnectSerial($port,
      (ispeed=>B9600,ospeed=>B9600,iflag=>IGNBRK,
       oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|PARENB|PARODD|CLOCAL));
  # set up a mask which specifies this port for select polling later on
  vec($rxmask,fileno $rx,1)=1;

  print "> Port $port to Rx is open\n";
} # Initialise

sub Debug
{
	$now = strftime "%H:%M:%S",gmtime;
	if ($DEBUG) {print "$now $_[0] \n";}
}

#----------------------------------------------------------------------------

#----------------------------------------------------------------------------

sub SendCommand
{
  my $cmd=shift;
  $cmd=~s/\x10/\x10\x10/g;		# 'stuff' (double) DLE in data
  print $rx "\x10".$cmd."\x10\x03";	# DLE at start, DLE/ETX at end
} # SendCommand
