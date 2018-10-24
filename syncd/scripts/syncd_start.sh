#!/usr/bin/env bash
#
# Script to start syncd using supervisord
#

# Source the file that holds common code for systemd and supervisord
. /usr/bin/syncd_init_common.sh

config_syncd

if [ "$SONIC_ASIC_TYPE" == "mellanox" ] && [ -d /var/warmboot/syncd ]; then
    export platform=$(sonic-cfggen -y /etc/sonic/sonic_version.yml -v asic_type)
    export mlnx_warm_recover=''
    /usr/bin/restore.sh
else
    exec ${CMD} ${CMD_ARGS}
fi
