#!/usr/bin/perl

use strict;
use warnings;
use diagnostics;

use utils;

sub test_brcm_start_empty
{
    fresh_start;

    play "empty_sw.rec";
}

sub test_brcm_start_empty_to_empty
{
    fresh_start;

    play "empty_sw.rec";
    play "empty_sw.rec";
}

sub test_brcm_full
{
    fresh_start;

    play "full.rec";
}

sub test_brcm_full_to_empty
{
    fresh_start;

    play "full.rec";
    play "empty_sw.rec";
}

sub test_brcm_empty_to_full
{
    fresh_start;

    play "empty_sw.rec";
    play "full.rec";
}

sub test_brcm_empty_restart_to_full
{
    fresh_start;

    play "empty_sw.rec";

    kill_syncd;
    start_syncd;

    play "full.rec";
}

sub test_brcm_empty_to_full_to_empty_to_full_to_full
{
    fresh_start;

    play "empty_sw.rec";
    play "full.rec";
    play "empty_sw.rec";
    play "full_second.rec";
    play "full_second.rec";
}

sub test_brcm_full_to_full
{
    fresh_start;

    play "full.rec";
    play "full_second.rec";
}

sub test_brcm_full_to_full_no_bridge
{
    fresh_start;

    play "full.rec";
    play "full_second_no_bridge.rec";
}

sub test_brcm_full_to_full_no_bridge_restart
{
    fresh_start;

    play "full.rec";
    play "full_second_no_bridge.rec";

    kill_syncd;
    start_syncd;

    play "full_second_no_bridge.rec";
}

sub test_brcm_empty_to_full_to_empty
{
    fresh_start;

    play "empty_sw.rec";
    play "full.rec";
    play "empty_sw.rec";
}

sub test_brcm_empty_to_full_nhg_bug
{
    fresh_start;

    play "empty_sw.rec";
    play "full_nhg_bug.rec";
}

sub test_brcm_empty_to_full_prio_flow_bug
{
    fresh_start;

    play "empty_sw.rec";
    play "full_nhg_bug_prio_flow_bug.rec";
}

sub test_brcm_empty_to_full_trap_group_bug
{
    fresh_start;

    play "empty_sw.rec";
    play "full_nhg_bug_trap_group_create_fail.rec";
}

sub test_brcm_queue_bug_null_buffer_profile
{
    fresh_start;

    play "full_queue_bug_null_buffer_profile.rec";
    play "empty_sw.rec";
}

sub test_brcm_full_to_empty_no_queue
{
    fresh_start;

    play "full_no_queue.rec";
    play "empty_sw.rec";
}

sub test_brcm_full_to_empty_no_queue_no_ipg
{
    fresh_start;

    play "full_no_queue_no_ipg.rec";
    play "empty_sw.rec";
}

sub test_brcm_full_to_empty_hostif_remove_segfault
{
    fresh_start;

    play "full_hostif_remove_segfault.rec";
    play "empty_sw.rec";
}

sub test_brcm_full_to_empty_no_queue_no_ipg_no_buffer_profile
{
    fresh_start;

    play "full_no_queue_no_ipg_no_buffer_pfofile.rec";
    play "empty_sw.rec";
}


# RUN TESTS

test_brcm_start_empty;
test_brcm_start_empty_to_empty;
test_brcm_full;
test_brcm_full_to_empty;
test_brcm_empty_to_full;
test_brcm_empty_restart_to_full;
test_brcm_empty_to_full_to_empty_to_full_to_full;
test_brcm_full_to_full;
test_brcm_full_to_full_no_bridge;
test_brcm_full_to_full_no_bridge_restart;
test_brcm_empty_to_full_to_empty;
test_brcm_empty_to_full_nhg_bug;
test_brcm_empty_to_full_prio_flow_bug;
test_brcm_empty_to_full_trap_group_bug;
test_brcm_queue_bug_null_buffer_profile;
test_brcm_full_to_empty_no_queue;
test_brcm_full_to_empty_no_queue_no_ipg;
test_brcm_full_to_empty_hostif_remove_segfault;
test_brcm_full_to_empty_no_queue_no_ipg_no_buffer_profile;
