#!/usr/bin/env bash
#
# Script to start syncd using supervisord
#

# Source the file that holds common code for systemd and supervisord
. /usr/bin/physyncd_init_common.sh

config_physyncd

exec "/usr/bin/physyncd_startup.py"

