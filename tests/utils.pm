#!/usr/bin/perl

package utils;

use strict;
use warnings;
use diagnostics;

use Term::ANSIColor;

require Exporter;

our $DIR;

sub GetCaller
{
    my ($package, $filename, $line, $subroutine) = caller(2);

    return $subroutine;
}

sub kill_syncd
{
    my $caller = GetCaller();

    print color('bright_blue') . "Killing syncd" . color('reset') . "\n";

    `/usr/bin/killall -9 vssyncd lt-vssyncd 2>/dev/null`;
}

sub flush_redis
{
    print color('bright_blue') . "Flushing redis" . color('reset') . "\n";

    my @ret = `redis-cli flushall`;

    chomp $ret[0];

    if ($ret[0] ne "OK")
    {
        print color('red') . "failed to flush redis: @ret" . color('reset') . "\n";
        exit 1;
    }
}

sub start_syncd
{
    print color('bright_blue') . "Starting syncd" . color('reset') . "\n";
    `./vssyncd -NSu -p "$DIR/vsprofile.ini" >/dev/null 2>/dev/null &`;
}

sub play
{
    my $file = shift;

    print color('bright_blue') . "Replay $file" . color('reset') . "\n";

    my @ret = `../saiplayer/saiplayer -u "$DIR/$file"`;

    if ($? != 0)
    {
        print color('red') . "player $DIR/$file: exitcode: $?:" . color('reset') . "\n";
        exit 1;
    }
}

sub fresh_start
{
    my $caller = GetCaller();

    print "$caller: " . color('bright_blue') . "Fresh start" . color('reset') . "\n";

    kill_syncd;
    flush_redis;
    start_syncd;
}

BEGIN
{
    our @ISA    = qw(Exporter);
    our @EXPORT = qw/
    kill_syncd flush_redis start_syncd play fresh_start
    /;

    my $script = $0;

    $DIR = $1 if $script =~ /(\w+)\.pl/;

    print "Using scripts dir '$DIR'\n";
}

END
{
    kill_syncd;
}

1;
