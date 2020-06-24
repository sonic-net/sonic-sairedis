#!/usr/bin/env bash
#
# Script to start syncd using supervisord
#

# Source the file that holds common code for systemd and supervisord
. /usr/bin/syncd_init_common.sh -p

config_syncd_phy

exec "/usr/bin/physyncd_startup.py"

