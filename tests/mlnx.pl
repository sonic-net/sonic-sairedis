#!/usr/bin/perl

BEGIN { push @INC,'.'; }

use strict;
use warnings;
use diagnostics;

use utils;

sub test_mlnx_nhg_member
{
    fresh_start;

    play "full_nhg_member.rec";
    play "empty_sw.rec";
}

sub test_mlnx_full_to_empty
{
    fresh_start;

    play "full_no_hostif_entry.rec";
    play "empty_sw.rec";
}

sub test_mlnx_empty_to_full_to_empty
{
    fresh_start;

    play "empty_sw.rec";
    play "full_no_hostif_entry.rec";
    play "empty_sw.rec";
}

sub test_mlnx_full_to_crash
{
    fresh_start;

    play "full_to_crash.rec";
}

sub test_mlnx_empty_to_full_and_again
{
    fresh_start;

    play "empty_sw.rec";
    play "full_no_hostif_entry.rec";
    play "empty_sw.rec";
    play "full_no_hostif_entry_second.rec";
    play "full_no_hostif_entry_second.rec";
}

sub test_mlnx_full_to_full
{
    fresh_start;

    play "full_no_hostif_entry.rec";
    play "full_no_hostif_entry_second.rec";
}

# RUN

test_mlnx_nhg_member;
test_mlnx_full_to_empty;
test_mlnx_empty_to_full_to_empty;
test_mlnx_full_to_crash;
test_mlnx_empty_to_full_and_again;
test_mlnx_full_to_full;

kill_syncd;
