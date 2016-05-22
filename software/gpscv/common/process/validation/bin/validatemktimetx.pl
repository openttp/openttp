#!/usr//bin/perl -w

#
# The MIT License (MIT)
#
# Copyright (c) 2016  Michael J. Wouters
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
# Modification history
#

use POSIX;

@receivers = ('javad','trimble');

$mktimetx = '../mktimetx/mktimetx';
if (!(-e $mktimetx)){
	print "Unable to find $mktimetx !\n";
	exit;
}

$rxdelaycal = '../../bin/rxdelaycal.pl';
if (!(-e $rxdelaycal)){
	print "Unable to find $rxdelaycal !\n";
	exit;
}

Log("Run at ".(strftime "%F %H:%M:%S",gmtime));

# Get versions etc
@ver = split /\n/, `$mktimetx --version`;
Log("Using $mktimetx ($ver[0])");

@ver = split /\n/, `$rxdelaycal -v`;
Log("Using $rxdelaycal ($ver[0])");

$mjdStart=-1;
$mjdStop=-1;
for ($r=0;$r<=$#receivers;$r++){
	Log("Validating $receivers[$r] CGGTTS output");
	# Determine the available MJDs
	@files = glob("$receivers[$r]/raw/*.rxrawdata");
	for ($f=0;$f<=$#files;$f++){
		$files[$f] =~ /(\d{5})\.rxrawdata/;
		$mjd=$1;
		if ($f==0){$mjdStart=$mjd;}
		if ($f==$#files){$mjdStop=$mjd;}
		Log("Processing $mjd");
		#`$mktimetx --configuration $receivers[$r]/etc/gpscv.conf -m $mjd`;
		# Get the processing log
		$log = `cat $receivers[$r]/tmp/mktimetx.log`;
		Log($log);
		
	}
	#`$rxdelaycal $receivers[$r]/cggtts-old $receivers[$r]/cggtts $mjdStart $mjdStop`;
}

sub Log
{
	print $_[0],"\n";
}