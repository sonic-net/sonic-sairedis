#!/usr/bin/perl

BEGIN { push @INC,'.'; }

use strict;
use warnings;
use diagnostics;

use utils;

sub test_mrvl_query_stats_capability
{
    fresh_start;

    play "query_stats_capability.rec";
}


# RUN TESTS

test_mrvl_query_stats_capability;

kill_syncd;
