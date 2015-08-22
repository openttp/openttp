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
#

$obs=$ARGV[0];
$fin=$ARGV[1];

if ($#ARGV!=1){
	print "Usage: xrnxobs.pl observation_name rinex_obs_file\n";
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
}

if ($rnxver == 0){
	print "Can't determine the RINEX version\n";
	exit;
}

while ($line=<IN>){
	if ($line=~/TYPES OF OBSERV/){
		if (!($line =~ /$obs/)){
			print "The observation $obs is not present\n";
			exit;
		}
		@obstypes= split " ",$line;
		$nobs=$obstypes[0];
		for ($i=1;$i<=$nobs;$i++){
			if ($obstypes[$i] eq $obs){
				$obscol=$i-1;
				last;
			}
		}
		printf "     1    %2s                                                # / TYPES OF OBSERV \n",$obs;
	}
	else{
		print $line;
	}
	last if ($line =~/END OF HEADER/);
}

while ($line=<IN>){
	if ($rnxver == 2){
		print $line;
		@obsinfo = split " ",$line;
		@svns = split /G|R/,$obsinfo[7];
		if ($svns[0] > 12){
			$line=<IN>; # lazy
			print $line;
		}
		for ($i=0;$i<$svns[0];$i++){
			$line=<IN>;
			chomp $line;
			@meas=split " ",$line;
			printf "%16s\n",$meas[$obscol];
		}
	}
	elsif ($rnxver == 3){
		next unless ($line=~/^>/);
		@obs=split /\s+/,$line;
		for ($i=0;$i<$obs[8];$i++){
			
		}
	}
}
close(IN);
