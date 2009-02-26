#!/usr/bin/perl -w

use POSIX qw(strftime);

# stat Version.h
@statinfo = stat "Version.h";
$vermdate  = $statinfo[9];
`mv Version.h Version.h.old`;
@files = glob ("*.cpp");
push @files,glob("*.h");

$newest = 0;
for ($i=0;$i<=$#files;$i++)
{
	@statinfo = stat $files[$i];
	if ($statinfo[9] > $newest)
	{
		$newest = $statinfo[9];
	}
}

if ($newest > $vermdate)
{

	$verstr = strftime "%a %e %b %Y %T",gmtime($newest);
	
	open  (OUT, ">Version.h");
	print OUT "#ifndef __VERSION_H_\n";
	print OUT "#define __VERSION_H_\n";
	print OUT "#define LAST_MODIFIED \"$verstr\"\n";
	print OUT "#endif\n";
}
else
{
	`mv Version.h.old Version.h`;
}
# Date info is fields 4-8

