#!/usr/bin/perl -w

#
# The MIT License (MIT)
#
# Copyright (c) 2017 Michael J. Wouters
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

$UPSTART="upstart";
$SYSTEMD="systemd";

# The first entry is OS-defined string, second is our name for tarballs etc,
# then init system
@os =(
	["Red Hat Enterprise Linux (WS|Workstation) release 6","rhel6",$UPSTART], 
	["CentOS release 6","centos6",$UPSTART],
	["CentOS Linux 7","centos7",$SYSTEMD],
	["Ubuntu 14.04","ubuntu14",$UPSTART],
	["Ubuntu 16.04","ubuntu16",$UPSTART],
	["BeagleBoard.org Debian","bbdebian8",$SYSTEMD]
	);

# Try for /etc/os-release first (systemd systems only)
if ((-e "/etc/os-release")){
	$thisos = `grep '^PRETTY_NAME' /etc/os-release | cut -f 2 -d "="`;
	$thisos =~ s/\"//g;
}
else{
	$thisos = `cat /etc/issue`;
	chomp $thisos;
}

for ($i=0;$i<=$#os;$i++){
	last if ($thisos =~/$os[$i][0]/);
}
$osid = $i;
if ($osid> $#os){
	printf "Unknown OS: $thisos\n";
	exit;
}

open(OUT,">Makefile");
open(IN, "<Makefile.template");
while ($l=<IN>){
	if ($l=~/^DEFINES/){
		print OUT "DEFINES = ";
		if ($thisos =~ /Beagleboard/){
			print OUT " -DOTTP -DSYSTEMD";
		}
		else{
			print OUT "-DTTS";
			if ($os[$osid][2] eq $SYSTEMD){
				print OUT " -DSYSTEMD";
			}
			elsif ($os[$osid][2] eq $UPSTART){
				print OUT " -DUPSTART";
			}
		}
		print OUT "\n";
	}
	else{
		print OUT $l;
	}
}
close IN;
close OUT;

