#!/usr/bin/perl -w

# restplayer.pl plays back Resolution T GPS data files to a serial port
# This is useful for testing ntpd
# It waits in a loop 

use Getopt::Std;
use Time::HiRes qw( usleep gettimeofday);
use TFLibrary;
use POSIX;
use vars qw();

$port="/dev/ttyS2";

$startMJD=$ARGV[0];
$startTime=$ARGV[1]; # seconds into day
$stopMJD=$ARGV[2];
$stopTime=$ARGV[3]; # seconds into the day
$home=$ENV{HOME};

if (-d "$home/raw")  {$raw="$home/raw";}  else {$raw="$home/cv_rawdata";}


for ($mjd=$startMJD;$mjd<=$stopMJD;$mjd++) # decompress
{
	$infile="$raw/$mjd.rxrawdata";
	$zipfile=$infile.".gz";
	if (-e $zipfile)
	{
		`gunzip $zipfile`;
		push @zipped,$infile;
	}
}

print "Opening\n";

$rx=&TFConnectSerial($port,
      (ispeed=>0010002,ospeed=>0010002,iflag=>IGNBRK,
       oflag=>0,lflag=>0,cflag=>CS8|CREAD|HUPCL|CLOCAL));
#vec($rxmask,fileno $rx,1)=1;
print "Port open\n";
	 
for ($mjd=$startMJD;$mjd<=$stopMJD;$mjd++)
{
	$lastTime=86399;
	if ($mjd==$stopMJD)
	{
		$lastTime=$stopTime;
	}
	
	$infile="$raw/$mjd.rxrawdata";
	open (IN,"<$infile");
		
	if ($mjd==$startMJD) # skip to startTime
	{
		
		while ($msg=<IN>)
		{
			if ($msg =~ /^# /) { next;}
			if ($msg =~ /^@ /) { next;}
			($msgid,$tod,$msgdata) = split " ",$msg;
			chomp $msgdata;
			$tod =~ /(\d{2}):(\d{2}):(\d{2})/;
			unless ((defined $1) && (defined $2) && (defined $3)) {next;}
			$tod = $1*3600 + $2*60 + $3;
			if ($tod >= $startTime)
			{
				last;
			}
		}
	}
	else
	{
		while ($msg=<IN>)
		{
			$msg=<IN>;
			if ($msg =~ /^# /) { next;}
			if ($msg =~ /^@ /) { next;}
			($msgid,$tod,$msgdata) = split " ",$msg;
			chomp $msgdata;
			$tod =~ /(\d{2}):(\d{2}):(\d{2})/;
			unless ((defined $1) && (defined $2) && (defined $3)) {next;}
			$tod = $1*3600 + $2*60 + $3;
			last;
		}
	}
	
	# don't worry about the first message
	$gotFAB=0;
	while (($tod <= $lastTime) && !eof(IN))
	{
		($tv_sec,$tv_usec) = gettimeofday();
		usleep(1000000-$tv_usec);
		print STDERR "#### $tod $tv_usec \n";
		Translate($msgdata); # spit out wot we last got - should be the first message
		$gotFAB=($msgdata=~/^8fab/);
		while ($msg=<IN>)
		{
			if ($msg =~ /^# /) { next;}
			if ($msg =~ /^@ /) { next;}
			($msgid,$newtod,$msgdata) = split " ",$msg;
			chomp $msgdata;
			$newtod =~ /(\d{2}):(\d{2}):(\d{2})/;
			unless ((defined $1) && (defined $2) && (defined $3)) {next;}
			$newtod = $1*3600 + $2*60 + $3;
			if ($newtod == $tod)
			{
				if (($tod==86399) && ($msgdata=~/^8fab/) && ($gotFAB)) # this is a leap second
				{
					last; # do it again
				}
				
				Translate($msgdata);
			}
			elsif ($newtod == $tod + 1)
			{
				$tod=$newtod;
				last;
			}
			elsif ($newtod > $tod+1)
			{
				$secstosleep = $newtod - $tod - 1;
				sleep($secstosleep);
				$tod=$newtod;
				last;
			}
			else
			{
				
			}
		}
	}
	
	close(IN);	
}

for ($i=0;$i<=$#zipped;$i++)
{
	`gzip $zipped[$i]`;
}

# -----------------------------------------------------------------------------------------
sub Translate
{
	# Turn the hexified, unstuffed message back into TSIP
	# TSIP starts with <DLE> and terminates with <DLE><ETX>
	# The message is stuffed by doubling any DLE bytes
	my $tmp=shift;
	#print $tmp,"\n";
	$packed = pack "H*",$tmp;
	#print length($tmp)," ",length($packed);
	$packed=~s/\x10/\x10\x10/g;		# 'stuff' (double) DLE in data
	#print " ",length($packed),"\n";
  print $rx "\x10".$packed."\x10\x03";
}
