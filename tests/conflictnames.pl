#!/usr/bin/perl

use strict;
use warnings;
use diagnostics;

my @files = `find ../ -type f -name "sai*.h"`;

my %H = ();

my $exit = 0;

for my $file (@files)
{
    chomp $file;
    next if $file =~ m!SAI/flex!;
    next if $file =~ m!/debian/!;
    next if not $file =~ m!/(sai[^/]*.h)$!;

    if (defined $H{$1})
    {
        printf "SAME file name CONFLICT: $H{$1} and $file\n";
        printf "files will be copied to /usr/include/sai/ during package install and will override, change name of $1\n";
        $exit = 1;
        next;
    }

    $H{$1} = $file;
}

exit $exit;
