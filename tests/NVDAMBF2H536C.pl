#!/usr/bin/perl

BEGIN { push @INC,'.'; }

use strict;
use warnings;
use diagnostics;

use utils;

sub test_dashapi_replay
{
    fresh_start;

    play "dashapis.rec";
}

sub test_dashapi_replay_bulk
{
    fresh_start_bulk;

    play "dashapis.rec";
}

# RUN

test_dashapi_replay;
test_dashapi_replay_bulk;

kill_syncd;
