#!/usr/bin/perl -w

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

# Extracts a single RINEX observation type from a RINEX file

# Modification history
# 2015-08-20 MJW First version
# 2016-08-09 MJW Handle more than 12 observations properly. Handle blank entries in RINEX observation records
# 2017-02-07 MJW Bug fixes for multiline observations with missing observations.
# 2018-05-15 MJW Extract more than one observation

use POSIX qw(floor ceil);

$fin=$ARGV[1];

if ($#ARGV!=1){
	print "Usage: xrnxobs.pl observation_name[,observation_name,...] rinex_obs_file\n";
	exit;
}

open(IN,"<$fin");

$line=<IN>;
print $line;

@verinfo = split /\s+/,$line;
$rnxver=0;

if ($verinfo[1] =~ /2\.\d+/){
	$rnxver=2;
}
elsif ($verinfo[1] =~ /3\.\d+/){
	$rnxver=3;
	print "Sorry,RINEX V3 is not supported yet.\n";
	exit;
}

if ($rnxver == 0){
	print "Can't determine the RINEX version.\n";
	exit;
}

@obs = split /,/,$ARGV[0];

while ($line=<IN>){
	if ($line=~/TYPES OF OBSERV/){
		for ($i=0;$i<=$#obs;$i++){
			if (!($line =~ /$obs[$i]/)){
				print "The observation $obs[$i] is not present\n";
				exit;
			}
		}
		@obstypes= split " ",( substr $line,0,60);
		$nobs=$obstypes[0];
		for ($i=0;$i<=$#obs;$i++){
			for ($n=1;$n<=$nobs;$n++){
				if ($obstypes[$n] eq $obs[$i]){
					$obscol[$i]=$n-1;
					last;
				}
			}
		}
		printf("%6d",$#obs + 1);
		for ($i=0;$i<=$#obs;$i++){
			printf("%4s%2s"," ",$obs[$i]);
		}
		printf(" " x (60 - ($#obs+1+1)*6));
		printf("# / TYPES OF OBSERV \n");
	}
	else{
		print $line;
	}
	last if ($line =~/END OF HEADER/);
}


while ($line=<IN>){
	if ($rnxver == 2){
		print $line;
		next if $line=~/COMMENT/;
		next if (!($line=~/G|R/));
		$line =~ s/^\s+//;
		chomp $line;
		@obsinfo = split " ",$line;
		$svnlist = $obsinfo[7];
		@svns = split /G|R/,$svnlist;
		if ($svns[0] > 12){
			$todo= floor($svns[0]/12);
			for ($i=0;$i<$todo;$i++){
				$line=<IN>; # lazy
				print $line;
				chomp $line;
				$line =~ s/^\s+//;
				$svnlist .= $line;
			}
			@svns = split /G|R/,$svnlist;
		}
		for ($i=0;$i<$svns[0];$i++){
			$nlines = ceil($nobs/5); # could be more than one line of measurements per SV
			$line = "";
			for ($l=0;$l<$nlines;$l++){ # so read lines, concatenating them
				$ll=<IN>;
				chomp $ll;
				# To simplify parsing, pad each line out to 80 characters
				$ll = sprintf("%-80s",$ll);
				$line .= $ll;
			}
		
			# Decompose the string, FORTRAN-like
			$len = length($line);
			# Each field should be 16 characters long
			# Round up the length to the next multiple of 16
			$nfields = ceil($len/16);
			for ($o=0;$o<=$#obs;$o++){
				for ($f=0;$f<$nfields;$f++){
					$val = substr $line, $f*16, (($f+1)*16<=$len?16:$len-$f*16);
					if ($f == $obscol[$o]){
						# Is it a number ?
						if ($val =~/\d+\.\d+/){
							printf "%16s",$val;
						}
						else{ # nahh
							printf "%16s"," "; # blank it
						}
						last;
					}
				}
			}
			printf("\n");
		}
	}
	elsif ($rnxver == 3){
	}
}
close(IN);
