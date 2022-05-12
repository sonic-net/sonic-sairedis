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

sub test_brcm_acl_prio
{
    fresh_start("-b", "$utils::DIR/break.ini", "-p", "$utils::DIR/vsprofile.ini");

    play "acl_prio.rec";
    play "acl_prio.rec", 0;
    play "acl_prio.rec", 0;
    play "acl_prio.rec", 0;
    play "acl_prio.rec", 0;
    play "acl_prio.rec", 0;
}

test_brcm_acl_prio;
test_brcm_acl_limit;

kill_syncd;
