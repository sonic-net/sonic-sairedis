#!/usr/bin/perl

BEGIN { push @INC,'.'; }

use strict;
use warnings;
use diagnostics;

use utils;

sub test_brcm_acl_limit
{
    fresh_start("-b", "$utils::DIR/break.ini", "-p", "$utils::DIR/vsprofile.ini");

    play "acl_limit.rec";
}

test_brcm_acl_limit;

kill_syncd;
