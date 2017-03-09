#!/usr/bin/perl -w

#
# The MIT License (MIT)
#
# Copyright (c) 2016 Michael J. Wouters
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

# This string identifies one of the TTSV2 systems
# Host bridge: Intel Corporation Mobile 945GME Express Memory Controller Hub (rev 03)
# This identifies the TTSV3 motherboard
# Host bridge: Intel Corporation Atom Processor D4xx/D5xx/N4xx/N5xx DMI Bridge (rev 02)

$mobo = `lspci | grep 'Host bridge'`;
if ($mobo =~ /Intel Corporation Mobile 945GME/){
	$Makefile='Makefile.parport';
}
elsif ($mobo =~ /Intel Corporation Atom Processor D4xx\/D5xx\/N4xx\/N5xx/){
	$Makefile='Makefile.sio8186x';
}
else{
	print "Can't identify the motherboard : assuming parallel port for ppsd\n";
	$Makefile='Makefile.parport';
}
  
`cp $Makefile Makefile`;
