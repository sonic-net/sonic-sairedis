#!/usr/bin/perl

use strict;
use warnings;
use diagnostics;

use Term::ANSIColor;

our $errors = 0;
our $warnings = 0;

sub LogInfo
{
    print color('bright_green') . "@_" . color('reset') . "\n";
}

sub LogWarning
{
    $warnings++;
    print color('bright_yellow') . "WARNING: @_" . color('reset') . "\n";
}

sub LogError
{
    $errors++;
    print color('bright_red') . "ERROR: @_" . color('reset') . "\n";
}

sub GetSourceFilesAndHeaders
{
    my @files = `find .. -name "*.cpp" -o -name "*.h"`;
    #my @files = `find .. -name "syncd.cpp"`;

    return @files;
}

sub ReadFile
{
    my $filename = shift;

    local $/ = undef;

    # first search file in meta directory

    open FILE, $filename or die "Couldn't open file $filename: $!";

    binmode FILE;

    my $string = <FILE>;

    close FILE;

    return $string;
}

sub ExtractComments
{
    my $input = shift;

    my $comments = "";

    # good enough comments extractor C/C++ source

    while ($input =~ m!(".*?")|//.*?[\r\n]|/\*.*?\*/!s)
    {
        $input = $';

        $comments .= $& if not $1;
    }

    return $comments;
}

sub RunAspell
{
    my $hash = shift;

    my %wordsToCheck = %{ $hash };

    if (not -e "/usr/bin/aspell")
    {
        LogError "ASPELL IS NOT PRESENT, please install aspell";
        return;
    }

    LogInfo "Running Aspell";

    my @keys = sort keys %wordsToCheck;

    my $count = @keys;

    my $all = "@keys";

    LogInfo "Words to check: $count";

    my @result = `echo "$all" | /usr/bin/aspell -l en -a -p ./aspell.en.pws 2>&1`;

    for my $res (@result)
    {
        if ($res =~ /error/i)
        {
            LogError "aspell error: $res";
            last;
        }

        chomp $res;
        next if $res =~ /^\*?$/;

        print "$res\n";
        next if not $res =~ /^\s*&\s*(\S+)/;

        my $word = $1;

        next if $word =~ /^wred$/i;

        chomp $res;

        my $where = "??";

        if (not defined $wordsToCheck{$word})
        {
            for my $k (@keys)
            {
                if ($k =~ /(^$word|$word$)/)
                {
                    $where = $wordsToCheck{$k};
                    last;
                }

                $where = $wordsToCheck{$k} if ($k =~ /$word/);
            }
        }
        else
        {
            $where = $wordsToCheck{$word};
        }

        LogWarning "Word '$word' is misspelled $where";
    }
}

my @files = GetSourceFilesAndHeaders();

my %wordsToCheck = ();

for my $file (@files)
{
    chomp $file;
    next if $file =~ m!/SAI/!;
    next if $file =~ m!/config.h!;

    my $data = ReadFile $file;

    my $comments = ExtractComments $data;

    $comments =~ s!github.com\S+! !g;
    $comments =~ s!l2mc! !g;
    $comments =~ s!\s+\d+(th|nd) ! !g;
    $comments =~ s!(/\*|\*/)! !g;
    $comments =~ s!//! !g;
    $comments =~ s!\s+\*\s+! !g;
    $comments =~ s![^a-zA-Z0-9_]! !g;

    my @words = split/\s+/,$comments;

    for my $w (@words)
    {
        next if $w =~ /_/;
        next if $w =~ /xYYY+/;

        $wordsToCheck{$w} = $file;
    }
}

RunAspell(\%wordsToCheck);

exit 1 if ($warnings > 0 or $errors > 0);
