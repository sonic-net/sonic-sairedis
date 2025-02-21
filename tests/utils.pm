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

    `killall -9 syncd vssyncd lt-vssyncd 2>/dev/null`;
}

sub flush_redis
{
    print color('bright_blue') . "Flushing redis" . color('reset') . "\n";

    my @ret = `redis-cli flushall`;

    $ret[0] = "failed" if not defined $ret[0];

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
    `./vssyncd -SUu -p "$DIR/vsprofile.ini" @_ >/dev/null 2>/dev/null &`;
}

sub start_syncd_bulk
{
    print color('bright_blue') . "Starting syncd bulk" . color('reset') . "\n";
    `./vssyncd -SUul -p "$DIR/vsprofile.ini" @_ >/dev/null 2>/dev/null &`;
}

sub start_syncd_warm
{
    print color('bright_blue') . "Starting syncd warm" . color('reset') . "\n";
    `./vssyncd -SUu -t warm -p "$DIR/vsprofile.ini" >/dev/null 2>/dev/null &`;

    sleep 1;
}

sub sync_start_syncd
{
    print color('bright_blue') . "Starting syncd" . color('reset') . "\n";
    `./vssyncd -s -SUu -p "$DIR/vsprofile.ini" >/dev/null 2>/dev/null &`;
}

sub sync_start_syncd_warm
{
    print color('bright_blue') . "Starting syncd warm" . color('reset') . "\n";
    `./vssyncd -s -SUu -t warm -p "$DIR/vsprofile.ini" >/dev/null 2>/dev/null &`;

    sleep 1;
}

sub request_warm_shutdown
{
    print color('bright_blue') . "Requesting syncd warm shutdown" . color('reset') . "\n";
    `../syncd/syncd_request_shutdown -w`;

    sleep 2;
}

sub play_common
{
    my @params = @_;

    my $len = @params;

    my $asicop = $params[$len-1];

    if ($asicop =~ /^\d+$/)
    {
        pop@params;
        $len = @params;
    }
    else
    {
        undef $asicop;
    }

    for (my $i = 0; $i < $len; $i++)
    {
        $params[$i] = "$DIR/$params[$i]" if $params[$i] =~ /\.rec$/;
    }

    print color('bright_blue') . "Replay @params" . color('reset') . "\n";

    my @ret = `../saiplayer/saiplayer -u @params`;

    if ($? != 0)
    {
        print color('red') . "player @params: exitcode: $?:" . color('reset') . "\n";
        exit 1;
    }

    return if not defined $asicop;

    # asic operation count is defined
    # we need to check log for that

    open (my $H, "<", "applyview.log") or die "failed to open applyview.log $!";

    my $line = <$H>;

    close ($H);

    chomp$line;

    if (not $line =~ /ASIC_OPERATIONS: (\d+)/)
    {
        print color('red') . "expected ASIC_OPERATIONS count on first line, but got: '$line'" . color('reset') . "\n";
        exit 1;
    }

    if ($1 > $asicop)
    {
        print color('red') . "expected ASIC_OPERATIONS count is $asicop but got $1" . color('reset') . "\n";
        exit 1;
    }
}

sub play
{
    play_common @_;
}

sub sync_play
{
    play_common "-m", @_;
}

sub fresh_start
{
    my $caller = GetCaller();

    `rm -f applyview.log`;

    print "$caller: " . color('bright_blue') . "Fresh start" . color('reset') . "\n";

    kill_syncd;
    flush_redis;
    start_syncd @_;
}

sub fresh_start_bulk
{
    my $caller = GetCaller();

    `rm -f applyview.log`;

    print "$caller: " . color('bright_blue') . "Fresh start with bulk" . color('reset') . "\n";

    kill_syncd;
    flush_redis;
    start_syncd_bulk @_;
}

sub sync_fresh_start
{
    my $caller = GetCaller();

    `rm -f applyview.log`;

    print "$caller: " . color('bright_blue') . "Fresh start" . color('reset') . "\n";

    kill_syncd;
    flush_redis;
    sync_start_syncd;
}


BEGIN
{
    our @ISA    = qw(Exporter);
    our @EXPORT = qw/ color
    kill_syncd flush_redis start_syncd play fresh_start fresh_start_bulk start_syncd_warm request_warm_shutdown
    sync_start_syncd sync_fresh_start sync_start_syncd_warm sync_start_syncd sync_play
    /;

    my $script = $0;

    $DIR = $1 if $script =~ /(\w+)\.pl/;

    print "Using scripts dir '$DIR'\n";
}

1;
