#!/usr/bin/perl -w

# Original version SQ, subsequently modified by PF and RBW
# 23.11.99 PF  Read parameter file from ../Parameter_Files/ directory 
# 22.11.99 RBW Only write to file if it was successfully opened; read
#                HPIB address from parameter file
# 17. 2.00 RBW Add Interface Clear and Device Clear commands to 488/p init
#                Check for null input on read; log SIGTERM
# 24-09-2002 MJW Modified for using the PRS10's time tagging facility
# 02-09-2002 MJW Whoops! Counter values should be in s, not ns !
# 07-02-2006 MJW Modified to periodically query the PRS10 for status information
# 28-02-2006 MJW File to flag power failure added. Note! This file is NOT removed by this program
# 21-03-2006 MJW Extra status file added for use by alerting programs
# 01-05-2006 MJW Attempt to fix serial port hangups by using the serial
#		             port in raw mode
# 29-05-2006 MJW Final cleanup
# 19-06-2006 MJW Changed serial port timeout logic so that a failure to
#		 repeated time interval measurements does not stop the
#		 status logging.
# 21-08-2006 MJW Bug fix. The PRS10 ouputs its name on powerup which confused the logging programme
#								if it was running while this happened. The solution is to check each response for the PRS10
#               string and read the serial port again if is found. 
# 20-05-2008 MJW Force readings into the range [-0.5,0.5]
#

use POSIX;
use Time::HiRes qw( usleep gettimeofday);
use TFLibrary;
use vars ( '$tmask' );

$debugOn=0;
$MAXTRIES=10; # maximum number of times to try to get a sensible response from the PRS10

$alarmTimeout = 120; # SIGAARLM timout - needs to be greater than status polling time

# check command line
if ($0=~m#^(.*/)#) {$path=$1} else {$path="./"}	# read path info
$0=~s#.*/##;					# then strip it
unless (@ARGV==1) {
  print "Usage: $0 filename\n";
  print "  where <filename> is the name of the setup file\n";
  exit;
}

$count=0;
open (PS, "ps x|");
while (<PS>) {
  @_=split;
  $count++ if ($_[4]=~/^[\w\/]*$0$/);
}
close PS;
if ($count>1) {ErrorExit("$0 process already present")}



# Read setup info from file
$filename=$ARGV[0];
unless ($filename=~m#/#) {$filename=$path.$filename}

%Init = &TFMakeHash($filename,(tolower=>1));

# Check we got the info we need from the setup file
@check=("counter Port","data path",
  	"counter data extension",
	"log rb status",
	"rb logging interval",
	"rb logging extension",
	"rb status",
	"rb power failure");
foreach (@check) {
  $tag=$_;
  $tag=~tr/A-Z/a-z/;	
  unless (defined $Init{$tag}) {ErrorExit("No entry for $_ found in $filename")}
}

$logPRS10    = $Init{"log rb status"};
$logInterval = $Init{"rb logging interval"};
$lastLog     = -1;
$oldPRS10mjd = 0;
$pwrflag     = $Init{"rb power failure"};
$prs10StatusFile = $Init{"rb status"};

# Create a lock on the port - this will stop other UUCP lock file
# aware applications from using the port
$port = $Init{"counter port"};
unless (`/usr/local/bin/lockport -p $$ $port $0`==1) 
{
	ErrorExit("Failed to lock port");
}

# Other initialisation
unless ($Init{"data path"}=~m#/$#) {$Init{"data path"}.="/"}

$oldmjd=0;

# set up a timeout in case we get stuck beacuse of a dead serial port
$SIG{ALRM} = sub {ErrorExit("Timed out - exiting.")};
alarm $alarmTimeout;

# catch kill signal so we can log that we were killed
$SIG{TERM} = sub {ErrorExit("Received SIGTERM - exiting.")};

# initialise the port

$PORT = &TFConnectSerial($Init{"counter port"},
	(ispeed=>B9600,ospeed=>B9600,
	 iflag=>IGNPAR,oflag=>0,lflag=>0,
	 cflag=>CS8|CLOCAL|CREAD));

# set up a mask which specifies this port for select polling later on
$rxmask="";
vec($rxmask,fileno $PORT,1)=1;

# flush receiver, and wait a moment just in case
tcflush($PORT,TCIOFLUSH);

$lastData=-1; # time at which we last got TT data
$gotData=0;

while (1) 
{
		
  # query PRS10
  $data=GetTimeTag();
	# if we couldn't get a response then serial comms is dead 
	# and there's no point trying to get the status
  next unless (length($data) > 0);
	alarm $alarmTimeout; # got a response so reset the timeout
	
  # Why do we do this ? TT does not arm the PRS10 for a time
  # interval measurement. It simply queries. After a query it sets
  # the result to -1 until a new value is available. Unfortunately,
  # sleep()ing for a second doesn't seem to be the best idea because
  # readings get lost. So we'll sleep for most of a second
  # print "$data";
	
	
	
  if ($data > -1)
	{	 
		Debug("Got TT data $data"); 
    # get MJD and time
    $tt=time();
		$lastData=$tt;
		$gotData=1;
    $mjd=int($tt / 86400)+40587;
    ($sec,$min,$hour)=gmtime($tt);

    # check for new day and create new filename if necessary
    if ($mjd!=$oldPRS10mjd) {
      $oldPRS10mjd=$mjd;
      $file_out=$Init{"data path"} . $mjd . $Init{"counter data extension"};
    }

    # write time-stamped data to file
    $time=sprintf("%02d:%02d:%02d",$hour,$min,$sec);
    if (open OUT,">>$file_out")
		{
			# Force TT reading into the range [-0.5,0.5]
			if ($data > 5.0E8) {$data-=1.0E9;}
			print OUT $time,"  ",$data*1.0E-9,"\n";
			close OUT;
		}
		
	} # of ($data > -1)
	
	# Do status polling after we have got the current measurement
	# (to avoid possibly losing a measurement because something was slow)
	# unless it's more than 2 s since the last time we got data (indicating
	# no 1 pps)
	if ($gotData || time() - $lastData > 2)
	{
		
		$tt=time;
		if ($logPRS10 &&( $tt- $lastLog >= $logInterval))
		{
			
			$mjd=int($tt / 86400)+40587;
			if ($mjd!=$oldmjd)
			{
     	 	$oldmjd=$mjd;
      	$prs10LogFile=$Init{"data path"} . $mjd .  $Init{"rb logging extension"};
			}
			if (!(-e $prs10LogFile))
			{
				open (OUT,">>$prs10LogFile");
				# query the PRS10 for its id - we'll dump this at the
				# top of the status file
				$prs10id = GetID();
				print OUT "# PRS10 ID $prs10id $ENV{HOSTNAME}\n";
			}
			else
			{
				open (OUT,">>$prs10LogFile");
			}
			($sec,$min,$hour)=gmtime($tt);
			$time=sprintf("%02d:%02d:%02d",$hour,$min,$sec);
			$lastLog=$tt;
			print OUT "$time ";
			$status = GetStatus();
			next unless (length($status) > 0);
			@statusbytes=split ",",$status;
			# If the PRS10 has lost power then most flags will be set and we ought
			# to clear them
			open(STATUS,">$prs10StatusFile");
			if (129 <= $statusbytes[5])
			{
				# Print flags line so we know about the power loss
				for ($i=0; $i<= $#statusbytes;$i++)
				{
					print OUT "$statusbytes[$i]  ";
				}
				for ($i=0;$i<16;$i++)
				{
					$volts = GetAD($i);
					print OUT "$volts ";
				}	
				print OUT "\n";
				print OUT "$time "; # Next line will have updated flags
				# Make the power failure file
				open (PWRFLAG,">$pwrflag");
				print PWRFLAG "$mjd $time Power loss detected\n"; # contents not used
				close PWRFLAG;
				$status = GetStatus();
				next unless (length($status) > 0);
				@statusbytes=split ",",$status;
			}
			for ($i=0; $i<= $#statusbytes;$i++)
			{
				print OUT "$statusbytes[$i]  ";
				print STATUS "$statusbytes[$i]  ";
			}
			for ($i=0;$i<16;$i++)
			{
				$volts = GetAD($i);
				print OUT "$volts ";
				print STATUS "$volts ";
			}
			print OUT "\n";
			close OUT;
			print STATUS "\n";
			close STATUS;
		}
		
  }
	$gotData = 0;
	# Sleep a bit
	#usleep(100000);
}
print "Unexpected temination\n";
close($PORT);

# End of main program
#-----------------------------------------------------------------------


#-----------------------------------------------------------------------

sub ErrorExit {
  my $message=shift;
  @_=gmtime(time());
  printf "%02d/%02d/%02d %02d:%02d:%02d $message\n",
    $_[3],$_[4]+1,$_[5]%100,$_[2],$_[1],$_[0];
  exit;
}

#-----------------------------------------------------------------------

sub GetResponse {
	$done =0;
	$input="";
	#print "GetResponse\n";
	while (!$done)
	{
		$nfound=select $tmask=$rxmask,undef,undef,1.0;
		if ($nfound==0) {return "";}
  		next unless $nfound;
		sysread $PORT,$input,$nfound,length $input;
		
		# if last character is newline then we've got the message
		$lastch=substr $input,length($input)-1,1;
		if ($lastch eq chr(13) )
		{
			# Test for PRS_10 in the response. This is automatically output 
			# by the PRS10 on reset and may be lurking to confuse us.
			$done = !($input =~/PRS_10/);
		}
	}
	return $input;
}

# ------------------------------------------------------------------------
sub PRS10Cmd
{
	$tries=0;
	while ($tries < 5)
	{
		print $PORT "$_[0]\r";
		$response = GetResponse();
		last if (length($response) > 0);
		$tries++;
	}
	return $response;
}

# -------------------------------------------------------------------------
sub Debug
{
	if ($debugOn)
	{
		($sec,$min,$hour)=gmtime(time());
		printf STDERR "%02d:%02d:%02d $_[0]\n",$hour,$min,$sec;
	}
}

# -------------------------------------------------------------------------
sub GetID
{
	my ($r,$i);
	for ($i=0;$i<$MAXTRIES;$i++)
	{
		$r=PRS10Cmd("ID?");
		chop $r; # remove terminator
		# valid response begins with PRS10
		if ($r=~/^PRS10/)
		{
			Debug("Got PRS10 ID $r");
			return $r;
		}
	}
	return "";
}

# -------------------------------------------------------------------------
sub GetTimeTag
{
	my ($r,$i);
	for ($i=0;$i<$MAXTRIES;$i++)
	{
		$r=PRS10Cmd("tt?");
		chop $r; # remove terminator
		# valid responses are -1 and a +ve integer
		if (($r eq -1) || ($r=~/^\d+$/))
		{
			return $r;
		}
	}
	return "";
}

# -------------------------------------------------------------------------
sub GetStatus
{
	
	my ($r,$i);
	for ($i=0;$i<$MAXTRIES;$i++)
	{
		$r=PRS10Cmd("ST?");
		chop $r; # remove terminator
		# valid response is six comma-separated integers
		if ($r=~/^\d+,\d+,\d+,\d+,\d+,\d+/)
		{
			Debug("Status $i $r");  
			return $r;
		}
	}
	return "";
}


# -------------------------------------------------------------------------
sub GetAD
{
	my ($r,$i);
	my $cmd = "AD " . $_[0] . "?";
	for ($i=0;$i<$MAXTRIES;$i++)
	{
		$r=PRS10Cmd("$cmd");
		chop $r; # remove terminator
		# valid response is a floating point number of the form d.dddd
		if ($r=~/^\d{1}\.\d{3}/)
		{
			return $r;
		}
	}
	return "-1"; # this flags no sensible response
}
