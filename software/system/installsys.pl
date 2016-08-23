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

#
# System software installation script
#
# Modification history
# 2016-06-19 MJW First version
# 

use English;
use Getopt::Std;
use Carp;
use FileHandle;
use POSIX;

use vars qw($opt_h $opt_m $opt_t $opt_v);

$0=~s#.*/##;	# strip path

$VERSION = "version 0.1";
$ECHO=1;

if (!getopts('hmtv') || $opt_h){
	ShowHelp();	
	exit;
}

if ($opt_v){
	print "$0 $VERSION\n";
	exit;
}

if ($EFFECTIVE_USER_ID >0){
	print "$0 must be run as superuser!\n";
	exit;
}

open (LOG,">./installsys.log");

CompileTarget('libconfigurator','src/libconfigurator','install');
CompileTarget('dioctrl','src/dioctrl','install');
CompileTarget('lcdmon','src/lcdmon','install');
#CompileTarget('okbitloader','src/okbitloader','install');
#CompileTarget('okcounterd','src/okcounterd','install');
CompileTarget('ppsd','src/ppsd','install');
close LOG;

# ------------------------------------------------------------------------
sub ShowHelp
{
	print "$0 $VERSION\n";
	print "\t-h print help\n";
	print "\t-m minimal installation\n";
	print "\t-t verbose\n";
	print "\t-v print version\n";
	print "\nMust be run as superuser\n";
}

# ------------------------------------------------------------------------
sub Log
{
	my ($msg,$flags)=@_;
	if (defined $flags){
		if ($flags == $ECHO) {print $msg;}
	}
	print LOG $msg;
}

#-----------------------------------------------------------------------------------
sub CompileTarget
{
	my ($target,$targetDir)= @_;
	my $makeargs = "";
	if (defined $_[2]){$makeargs=$_[2];}
	
	Log ("\nCompiling $target ...\n",$ECHO);
	my $cwd = getcwd();
	chdir $targetDir;
	
	if (-e 'configure.pl'){
		my $configout = `./configure.pl`;
		if ($opt_t) {print $configout;}
	}
	
	my $makeout = `make clean && make $makeargs 2>&1`;
	if ($opt_t) {print $makeout;}
	if ($? >> 8){
		Log ("\nCompilation/installation of $target failed :-(\n",$ECHO);
		exit(1);
	}
	chdir $cwd;
}
