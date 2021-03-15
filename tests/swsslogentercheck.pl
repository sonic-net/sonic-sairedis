#!/usr/bin/perl

# purpose of this script is to check if all sources
# declared SWSS_LOG_ENTER on all methods

use diagnostics;
use strict;
use warnings;
use utf8;

sub ReadFile
{
    my $file = shift;
    local $/ = undef;

    open FILE, "<", $file or die "Couldn't open file $file: $!";
    my $string = <FILE>;
    close FILE;

    return $string;
}

my $exitCode = 0;
for my $arg (@ARGV)
{
    my $s = ReadFile($arg);

    next if $arg =~ m!/SAI/!;
    next if $arg =~ m!/debian/!;
    next if $arg =~ m!/pyext/!;
    next if $arg =~ m!/python/!;

    my $returnType = '([:_a-z0-9<>]+)(?:\s*[*&]\s*|\s+)';
    my $methodName = '(~?\w+|\w+::\w+)';
    my $methodParams = '\(([ ,:<>&a-z0-9_*\r\n]*?)\)';
    my $const = '(?:\s*const\s*)?';

    while ($s =~ m/($returnType$methodName\s*$methodParams\s*$const\{\n+)([^\n]+).*?\n+([^\n;]+)/gis)
    {
        my $method = $1;
        my $rt = $2;
        my $name = $3;
        my $line1 = $5;
        my $line2 = $6;

        next if $name =~ /^(if|else|while|for|switch|\d+)$/;
        next if $rt =~ /^(if|else|while|for)$/;
        next if $line1 =~ /^\s+SWSS_LOG_ENTER/;
        next if $line1 =~ /mutex/i and $line2 =~ /^\s*SWSS_LOG_ENTER/;
        next if $line1 =~ /setMinPrio/ and $line2 =~ /^\s*SWSS_LOG_ENTER/;
        next if $line1 =~ /^\s+\/\/ SWSS_LOG_ENTER.+(omitted|disabled)/;

        print " *** $arg : method is missing SWSS_LOG_ENTER():\n $method$line1\n$line2\n\n";

        $exitCode = 1;
    }
}

exit $exitCode;
