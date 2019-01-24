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

sub test_brcm_speed_init_apply
{
    fresh_start;

    play "speed_init.rec";
    play "speed_apply.rec";
}

sub test_brcm_lag_comparison_logic
{
    fresh_start;

    # we expect no asic operations on this test
    # even do lags looks the same, but are matched
    # using lag member and port

    play "lag_comparison_logic.rec";
    play "lag_comparison_logic.rec", 0;
}

sub test_brcm_nhg_comparison_logic
{
    fresh_start;

    # we expect no asic operations on this test
    # even do next hop groups looks the same, but are
    # matched using route destination prefix

    play "nhg_comparison_logic.rec";
    play "nhg_comparison_logic.rec", 0;
    play "nhg_comparison_logic.rec", 0;
}

sub test_brcm_remove_next_hop
{
    fresh_start;

    # we expect this will succeed, test checks
    # whether coparison logis on REMOVE operation
    # is working correctly and reference count will
    # be correct for each obejct when performing REMOVE

    play "remove_next_hop.rec";
    play "remove_next_hop.rec";
    play "remove_next_hop.rec";
    play "remove_next_hop.rec";
    play "remove_next_hop.rec";
}

sub test_brcm_hostif
{
    fresh_start;

    # we expect no asic operation on policer, hostif trap group, router interface

    play "hostif.rec";
    play "hostif.rec", 0;
    play "hostif.rec", 0;
}

sub test_brcm_full_testbed_config
{
    fresh_start;

    # we expect no asic operation on policer, hostif trap group, router interface

    play "full_testbed_config.rec";
    play "full_testbed_config.rec", 0;
    play "full_testbed_config.rec", 0;
}

sub test_brcm_rif_loopback
{
    fresh_start;

    # we expect no asic operation on router interface

    play "rif_loopback.rec";
    play "rif_loopback.rec", 0;
    play "rif_loopback.rec", 0;
    play "rif_loopback.rec", 0;
    play "rif_loopback.rec", 0;
    play "rif_loopback.rec", 0;
}

sub test_brcm_lag_no_members
{
    fresh_start;

    # we expect no asic operation on lag's that has no members

    play "lag_no_members.rec";
    play "lag_no_members.rec", 0;
    play "lag_no_members.rec", 0;
    play "lag_no_members.rec", 0;
    play "lag_no_members.rec", 0;
    play "lag_no_members.rec", 0;
}

sub test_brcm_qos_map_order
{
    fresh_start;

    # we expect no asic operation on qos maps that are the same but different order

    play "qos_map_order.rec", 0;
}

sub test_brcm_acl_tables
{
    fresh_start;

    # we expect no asic operation on lag's that has no members

    play "acl_tables.rec";
    play "acl_tables.rec", 0;
    play "acl_tables.rec", 0;
    play "acl_tables.rec", 0;
    play "acl_tables.rec", 0;
    play "acl_tables.rec", 0;
}

sub test_brcm_buffer_pool
{
    fresh_start;

    # we expect no operations on asic, and all buffer pools will be matched correctly

    play "full_buffer.rec";
    play "full_buffer_second.rec",0;
}

sub test_brcm_warm_boot_full
{
    fresh_start;

    play "full.rec";

    request_warm_shutdown;
    start_syncd_warm;

    play "full_second.rec";

    request_warm_shutdown;
}

sub test_brcm_warm_boot_empty
{
    fresh_start;

    play "empty_sw.rec";

    request_warm_shutdown;
    start_syncd_warm;

    play "empty_sw.rec", 0;

    request_warm_shutdown;
}

sub test_brcm_warm_boot_small_buffer
{
    fresh_start;

    play "small_buffer.rec";

    request_warm_shutdown;
    start_syncd_warm;

    play "small_buffer.rec", 0;

    request_warm_shutdown;
}

sub test_brcm_warm_boot_full_empty
{
    fresh_start;

    play "full.rec";

    request_warm_shutdown;
    start_syncd_warm;

    play "empty_sw.rec";
    play "empty_sw.rec", 0;
    play "full_second.rec";
    play "empty_sw.rec";

    request_warm_shutdown;
}

sub test_brcm_config_acl
{
    fresh_start;

    play "config_acl.rec";
    play "config_acl.rec", 0;

    fresh_start;

    play "config_acl2.rec";
    play "config_acl2.rec", 0;
}

sub test_brcm_warm_wred_queue
{
    fresh_start;

    play "wred_queue.rec";
    play "wred_queue.rec", 0;
    play "wred_queue.rec", 0;
    play "wred_queue.rec", 0;
    play "wred_queue.rec", 0;
    play "wred_queue.rec", 0;
    play "wred_queue.rec", 0;

    fresh_start;

    play "wred_queue.rec";

    request_warm_shutdown;
    start_syncd_warm;

    play "wred_queue.rec", 0;

    request_warm_shutdown;

    start_syncd_warm;

    play "wred_queue.rec", 0;
}

# RUN TESTS

test_brcm_config_acl;
test_brcm_warm_wred_queue;
test_brcm_warm_boot_full_empty;
test_brcm_warm_boot_small_buffer;
test_brcm_warm_boot_empty;
test_brcm_warm_boot_full;

test_brcm_buffer_pool;
test_brcm_acl_tables;
test_brcm_qos_map_order;
test_brcm_lag_no_members;
test_brcm_rif_loopback;
test_brcm_hostif;
test_brcm_full_testbed_config;
test_brcm_remove_next_hop;
test_brcm_nhg_comparison_logic;
test_brcm_lag_comparison_logic;
test_brcm_speed_init_apply;
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

kill_syncd;
