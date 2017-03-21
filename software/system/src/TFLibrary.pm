#
#
# The MIT License (MIT)
#
# Copyright (c) 2015  R. Bruce Warrington, Michael J. Wouters
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

package TFLibrary;

# Library of Perl bits for Time and Frequency processing

# Modification history:
# 30. 4.99  RBW  Begun: %TFPath, &TFCheckClockName, &TFMJDRange
#  3. 5.99  RBW  &TFParseMonth
# 14. 5.99  RBW  &TFClockPath; local filehandles; croak; general &TFMakeHash
# 17. 5.99  RBW  &TFConnectSerial, &TFHPCounterCommand
# 18. 5.99  RBW  &TFTimeStamp
#  2. 6.99  RBW  &TFMicro488Command 
# 30. 6.99  RBW  Fixed pattern match in TFCheckClockName; don't complain if
#                  path file does not exist for %TFPath
# 17. 7.99  RBW  Fixed pattern match on clock names: don't only compile once,
#                  handle commented out clocks
# 21. 7.99  RBW  &TFClockList
# 11. 8.99  RBW  &TFMJD; miscellaneous tidying of error conditions
# 13. 3.01  RBW  &TFMakeHash: also handle equal signs
#                &TFConnectSerial: allow both "iflag=>" and "iflags=>"
#  7. 5.01  RBW  &TFMakeHash: provide option to map keys to lower case
#  6. 6.01  RBW  &TFPortInUse
# 13. 6.01  RBW  &TFPortInUse: look for /proc entry instead of piping from ps,
#                  because ps complains if running setuid
#  5. 6.03  RBW  &TFLock, &TFUnlock, &TFTestLock
# 27. 6.03  RBW  &TFLock, &TFTestLock: check /proc cmdline instead of kill $$,0
#                  and hope this is more reliable
# 23. 2.07  MJW &TFMakeHash2 added. This allows sectioning within a config file
# 22. 2.12  MJW &TFMakeHash2 merges with an optional default config file. Bug fix - chomp()
#								instead of chop
# 16-08-12 MJW &TFMakeHash2 removes leading whitespace from the token, comments etc
# 13-01-16  MJW &TFMakeHash2 allows values with multiple lines

use Exporter;
@ISA = qw(Exporter);

@EXPORT = qw(
  TFMakeHash %TFPath 
  TFMakeHash2
  TFCheckClockName TFClockPath TFClockList
  TFMJD TFMJDRange TFParseMonth TFTimeStamp
  TFConnectSerial TFPortInUse TFHPCounterCommand TFMicro488Command
  TFLock TFUnlock TFTestLock
  TFMakeAbsoluteFilePath TFMakeAbsolutePath
  TFCreateProcessLock TFRemoveProcessLock 
);

use Carp;
use FileHandle;
use POSIX;

# Local subroutines

sub MakeHash
{
	my ($inname,%options)=@_;
  my $infile=new FileHandle;
  my %hash=();
  my ($key,$value,$token);

  %options=() unless %options;
  unless (open $infile,$inname) {
		carp "Can't open hash data file $inname: $!";
    return %hash; # which is empty
  }
  
	$token="";
	$value="";
	$key="";
	
	while (<$infile>) {
		if (!/^\s*#/) { # skip comments
			chomp;
			s/#.*$//; # remove trailing comments
			s/\s+$//; # remove trailing whitespace
			s/^\s+//; # remove leading whitespace
			next unless $_; # undefined - must have been an empty line
			if (/^\s*\[(.+)\]/) {
				if (length($value)>0){ # new key so save any running hash value
						$hash{$token}=$value;
						$token="";$value="";
					}
					$key=$1;
				}
				elsif (/\s*=\s*/) {
					if (length($value)>0){ # new token/value so save any running hash value
						$hash{$token}=$value;
						$token="";$value="";
					}
					@_=split /\s*=\s*/; # this strips white space around the delimiter
					next unless @_==2;
					$token = $_[0];
					if (length($key)>0) {
						$token = $key.":".$token;
					}
					$token =~ tr/A-Z/a-z/ if defined $options{tolower};
					$value = $_[1];
			} 
			else{ # this is appended to the current value
				if (length($value)>0){
					$value .= $_;
				}
			}
		}
	}
	
	# Add the last token
	if (length($value)>0){
		$hash{$token}=$value;
	}
	
  close $infile;

  return %hash;
}

# BEGIN is at end of file :-)

#----------------------------------------------------------------------------

sub TFMakeHash {
# Read a text file to set up a hash for configuration information
# Comments with leading # will be ignored (including trailing on a line)
# Otherwise, the first field is taken as the key and the second as the value
# For example, this mechanism is used for the %TFPath hash.
  my ($inname,%options)=@_;
  my $infile=new FileHandle;
  my %hash;
  
  %options=() unless %options;
  unless (open $infile,$inname) {
    carp "Can't open hash data file $inname: $!";	
    return undef;
  }
  while (<$infile>) {
    if (!/^\s*#/) {
      chop;
      s/#.*$//;
      s/\s+$//;
			s/^\s+//; 
      next unless $_;
      if (/\s*=\s*/) {@_=split /\s*=\s*/} else {@_=split}
      next unless @_==2;
      $_[0]=~tr/A-Z/a-z/ if defined $options{tolower};
      $hash{$_[0]}=$_[1];
    }
  }
  close $infile;

  return %hash;
} # TFMakeHash

#----------------------------------------------------------------------------

sub TFMakeHash2 {
# Read a text file to set up a hash for configuration information
# Comments with leading # will be ignored (including trailing on a line)
# Otherwise, the first field is taken as the key and the second as the value
# For example, this mechanism is used for the %TFPath hash.
  my ($inname,%options)=@_;
  my %defhash;
	my %lochash;
	my %hash;
	my $defaults;
	my $local;
	my $key;

  %options=() unless %options;

  # Defaults 
	if (defined $options{defaults})
	{
		$defaults = $options{defaults};
		%defhash = &MakeHash($defaults,%options);
		if (!(%defhash))
		{
			return %defhash;
		}
	}

	%lochash = &MakeHash($inname,%options);
	if (! (%lochash))
	{
		return %lochash;
	}

	# Merge the two hashes
	%hash = %defhash;
	foreach $key (keys %lochash)
	{
		$hash{$key} = $lochash{$key}; # this will override the default values
	}

  return %hash;

} # TFMakeHash2

#----------------------------------------------------------------------------

sub TFCheckClockName {
# check that clock name is legal by searching in logger table
  my $name=shift;
  my $found=0;
  my $infile=new FileHandle;
  my $inname=$TFPath{logger_table};

  if (!defined($name)) {return undef}

  unless (open $infile,$inname) {
    carp "Can't open logger table $inname: $!";
    return undef;
  }
  while (<$infile>) {
    chop;
    s/^#\s*//;
    next unless ($_);
    @field=split;
    if ($field[0] =~ /^$name$/i) {
      $found=1;
      $name=$field[0]; # take clock name as it appears in logger config file
      last;
    }
  }
  close $infile;
  
  if ($found) {$_[0]=$name;} # update with text as it appears in the file
  return $found;
} # TFCheckClockName

#----------------------------------------------------------------------------

sub TFClockPath {
# Read path to clock data from logger config file and return with extension
  my $name=shift;
  my @field;
  my $infile=new FileHandle;
  my $inname=$TFPath{logger_table};

  unless (open $infile,$inname) {
    carp "Can't open hash logger table $inname: $!";
    return undef;
  }
  while (<$infile>) {
    chop;
    s/^#\s*//;
    next unless ($_);
    @field=split;
    if ($field[0] =~ /$name/i) 
      {return ($TFPath{logger_data}.$field[4],$field[5]);}
  }
  return undef;
} # TFClockPath

#-----------------------------------------------------------------------

sub TFClockList {
  my ($spec,$logger)=@_;
  my ($user,$i,$add,@list,@clocks);
 
  # split clock list into individual patterns, and convert to regexp wildcards
  @list=split /,/,$spec;
  for ($i=0; $i<=$#list; $i++) {
    $list[$i]=~s/\./\\\./g;
    $list[$i]=~s/\?/\./g;
    $list[$i]=~s/\*/\.\*/g;
  }

  # retrieve a logger table, from a remote logger if necessary
  $user="";
  if ($logger=~/\@/) {
    ($user,$logger)=split /\@/,$logger;
    $user.="\@";
  } 
  chop($host=`hostname`);
  if ($host=~/\./) {@_=split /\./,$host; $host=$_[0];} # discard domain
  $INFILE = "$0.tmp.log";
  if ($logger eq $host)
    {`cp $TFPath{"logger_table"} $INFILE`;}
  else 
    {`scp $user$logger:$TFPath{"logger_table"} $INFILE`;}

  # see which clocks in the file match against the list
  unless (open INFILE) {
    carp "Can't open logger table file $INFILE: $!";
    return undef;
  }
  while (<INFILE>) {
    next if /^\s*#/; # skip comments
    @_=split;
    $add=0;
    foreach $spec (@list) {
      if ($spec=~/^[!~^]$_[0]/i) {$add=0; last}
      elsif ($_[0]=~/^$spec$/i) {$add=1}
    }
    if ($add) {push @clocks,$_[0]}
  }
  close INFILE;
  `rm -f $INFILE`;

  return @clocks;
} # TFGetClockList

#----------------------------------------------------------------------------

sub TFMJD {
  my $now;
  if (@_==3) {
     unless (defined $TFPath{"mjd"}) {
       carp "Path to mjd not defined";
       return undef;
     }
     unless (-e $TFPath{"mjd"}) {
       carp "$TFPath{mjd} not found";
       return undef;
     }
     return `echo "$_[0] $_[1] $_[2]" | $TFPath{"mjd"}`;
  }
  elsif (@_<=1) {
    $now=($#_==1? $_[0] : time());
    return int($now/86400) + 40587;
  }
  else {
    carp "TFMJD: wrong number of arguments (0, 1 or 3 expected)";
    return undef;
  }
} # TFMJD

#----------------------------------------------------------------------------

sub TFMJDRange {
  my ($year,$month)=@_;
  my ($m,$first,$last);
  my $mjd=$TFPath{mjd};
  
  unless (defined $TFPath{"mjd"}) {
    carp "Path to mjd not defined";
    return undef;
  }
  unless (-e $TFPath{"mjd"}) {
    carp "$TFPath{mjd} not found";
    return undef;
  }

  $m=(defined($month)? $month : 1);
  $first=`$mjd -d 1 $m $year`;
  chop $first;
  $m+=(defined($month)? 1 : 12);
  $last=`$mjd -d 1 $m $year`;
  chop $last;
  $last--;
  return ($first,$last);
} # TFMJDRange 

#----------------------------------------------------------------------------

sub TFParseMonth {
# Parse a month specifier command line option: mm:yyyy or +-n for relative
  my $option=$_[0];
  my ($date,$month,$year,$remainder,$step);
  my $ok=1;

 if ($option =~ /^([-+])/) {
    # relative
    $step=$1."1";
    $date=`date -u +"%m %Y"`;
    chop($date);
    ($month,$year)=split /\s+/,$date;
    while ($option) {
      $month+=$step;
      if ($month<1) {$year--; $month=12;}
      if ($month>12) {$year++; $month=1;}
      $option-=$step;
    }
  }
  elsif ($option =~ /:/) {
    # absolute
    ($month,$year,$remainder)=split /:/,$option;
    if (!defined($month) || !defined($year) || defined($remainder) ||
      ($month<1) || ($month>12)) {$ok=0;}
  }
  else {$ok=0;}

  if ($ok) {$_[1]=$month; $_[2]=$year;}
  return $ok;
} # TFParseMonth

#-----------------------------------------------------------------------------

sub TFTimeStamp {
  my ($s,$m,$h,$md,$mn,$y,$wd,$yd,$dst)=gmtime;
  return sprintf "%02d:%02d:%02d",$h,$m,$s;
} # TFTimeStamp

#-----------------------------------------------------------------------------

sub TFConnectSerial {
# initialise serial port
  my ($port,%config)=@_;
  my $handle=new FileHandle;
  my $fd;

  # default configuration
  %config=() unless %config;
  $config{ispeed}=B9600 unless defined $config{ispeed}; 
  $config{ospeed}=B9600 unless defined $config{ospeed};
  # while (($tag,$val)=each(%config)) {print "> $tag -> $val\n";}

  sysopen $handle,$port,O_RDWR or croak "! Could not open $port: $!";
  $fd=fileno $handle;

  $termios=POSIX::Termios->new(); 
  # See Camel book pp 471 and termios man page

  $termios->getattr($fd);

  if (defined $config{iflag})  {$termios->setiflag($config{iflag})}
  if (defined $config{iflags}) {$termios->setiflag($config{iflags})}
  if (defined $config{oflag})  {$termios->setoflag($config{oflag})}
  if (defined $config{oflags}) {$termios->setoflag($config{oflags})}
  if (defined $config{lflag})  {$termios->setlflag($config{lflag})}
  if (defined $config{lflags}) {$termios->setlflag($config{lflags})}
  if (defined $config{cflag})  {$termios->setcflag($config{cflag})}
  if (defined $config{cflags}) {$termios->setcflag($config{cflags})}

  if (defined $config{ispeed}) {$termios->setispeed($config{ispeed})}
  if (defined $config{ospeed}) {$termios->setospeed($config{ospeed})}

  $termios->setattr($fd,TCSANOW);

  autoflush $handle 1;  # use unbuffered IO

  return $handle;
} # TFConnectSerial

#----------------------------------------------------------------------------

sub TFPortInUse {
# Check for a current lockfile on a given port: return 0 if free, 1 if in
# use, or -1 if we are unsure (various error conditions)
  my $port=shift;
  my $lockfile;

  $port=~s/(.*)tty//;				# strip any leading guff
  $port=~tr/a-z/A-Z/;
  $lockfile="/var/lock/LCK..tty$port";

  return 0 unless -e $lockfile;			# does lockfile exist?
  return -1 unless (open IN,"<$lockfile");
  $_=<IN>;
  close IN;
  return -1 unless $_ && /^\s*(\d+)/ && $1;	# extract PID

  return -e "/proc/$1/";			# look for PID in /proc
} # TFPortInUse

#----------------------------------------------------------------------------

sub TFHPCounterCommand {
# Send a command to a HP53131/132 counter, and optionally return a response
# Assumes that Micro 488/p has been set up with LF as both the serial 
# terminator and the GPIB terminator
  my ($handle,$cmd,$listen)=@_;
  my $lf=chr(10);

  if (!defined($cmd) || ($cmd eq "")) {$cmd="READ?";}
  print $handle "OA;03;".$cmd.$lf;
  if ((defined($listen) && $listen) || ($cmd=~/\?$/)) {
    print $handle "EN;03".$lf;
    $_=<$handle>;
  }
} # TFHPCounterCommand

#----------------------------------------------------------------------------

sub TFMicro488Command {
# Send a command to a Micro 488/p serial/GPIB converter
  my ($handle,$cmd)=@_;
  
  print $handle $cmd;
} # TFMicro488Command

#----------------------------------------------------------------------------
# DEPRECATED
#
sub TFLock {
  my ($file,$pid,$cmd);
  my $name=@_? shift : $0;

  # Note: I'm sure it's possible (and desirable) to call TFTestLock
  # and then just handle the creation of a new lockfile afterwards... but
  # I can't be bothered fixing this just yet.

  $name=~s/.*\///;
  $file="/usr/local/lock/LCK..$name";
  if (-e $file) {
    if (open LOCK,$file) {
      chomp($pid=<LOCK>);
      close LOCK;
      # Initially tried "kill 0,$pid" to check whether the process is
      # still around, but this appears unreliable. Instead we look for 
      # an entry in the /proc directory, and (at the risk of being too
      # careful) also check the cmdline to make sure it matches.
      if (($pid>0) && (-e "/proc/$pid") && (open CMD,"/proc/$pid/cmdline")) {
        $cmd=<CMD>;
	close CMD;
	return 0 if $cmd=~/$name/;
      }
      else {unlink $file}
    }
    else {return 1}
  }

  if (open LOCK,">$file") {
    print LOCK "$$\n";
    close LOCK;
    chmod 0666,$file;
    return 3;
  }
  else {return 2}
} # TFLock

#----------------------------------------------------------------------------
# DEPRECATED
#
sub TFUnlock {
# Note: strictly we should check that either we made the lock or that any
# existing lock is stale, but we don't make this check.
  my $file;
  my $name=@_? shift : $0;

  $name=~s/.*\///;
  $file="/usr/local/lock/LCK..$name";
  return unlink $file;
} # TFUnlock

#----------------------------------------------------------------------------
# DEPRECATED
#
sub TFTestLock {
  my ($result,$pid,$file);
  my $name=@_? shift : $0;

  $name=~s/.*\///;
  $file="/usr/local/lock/LCK..$name";
  if (-e $file) {
    if (open LOCK,$file) {
      chomp($pid=<LOCK>);
      close LOCK;
      # Initially tried "kill 0,$pid" to check whether the process is
      # still around, but this appears unreliable. Instead we look for 
      # an entry in the /proc directory, and (at the risk of being too
      # careful) also check the cmdline to make sure it matches.
      if (($pid>0) && (-e "/proc/$pid") && (open CMD,"/proc/$pid/cmdline")) {
        $_=<CMD>;
	$result=(/$name/? 0 : 4);
	close CMD;
      }
      else {$result=3}
    }
    else {$result=2}
  }
  else {$result=1}

  return $result;
} # TFTestLock

#----------------------------------------------------------------------------
sub TFMakeAbsoluteFilePath {
	my ($fname,$homedir,$defaultpath)=@_;
	$ret = $fname;
	if ($fname=~/^\//){
		# absolute path already - nothing to do
	}
	elsif ($fname=~/\//){ # relative to HOME
		$ret = $homedir."/".$fname;
	}
	else{
		# No path so add the default path
		$ret = $defaultpath. "/" . $fname;
	}
	return $ret;
}

#----------------------------------------------------------------------------
sub TFMakeAbsolutePath {
	my ($path,$rootpath)=@_;
	if (!($path=~/^\//)){
		$path =$rootpath."/".$path;
	}
	if (!($path=~/\/$/)){
		$path .= "/";
	}
	return $path;
}


#----------------------------------------------------------------------------
# Returns 0 on fail, 1 on success
#
sub TFCreateProcessLock{
	my ($lockFile)=@_;
	my (@info);
	if (-e $lockFile){
		open(LCK,"<$lockFile");
		@info = split ' ', <LCK>;
		close LCK;
		if (-e "/proc/$info[1]"){
			return 0;
		}
		else{ # write over it
			open(LCK,">$lockFile");
			print LCK "$0 $$\n";
			close LCK;
		}
	}
	else{
		open(LCK,">$lockFile");
		print LCK "$0 $$\n";
		close LCK;
	}
	return 1;
}

#----------------------------------------------------------------------------
sub TFRemoveProcessLock{
	my ($lockFile)=@_;
	if (-e $lockFile) {unlink $lockFile;}
}


#----------------------------------------------------------------------------

BEGIN {
  %TFPath=&TFMakeHash("/usr/local/etc/PATHS") if -e "/usr/local/etc/PATHS";
}

#----------------------------------------------------------------------------
