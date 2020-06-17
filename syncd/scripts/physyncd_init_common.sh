#!/usr/bin/env bash

CMD_PHYSYNCD=/usr/bin/physyncd

# dsserve: domain socket server for stdio
CMD_DSSERVE=/usr/bin/dsserve
CMD_DSSERVE_ARGS="$CMD_PHYSYNCD --diag"

ENABLE_SAITHRIFT=0

PLATFORM_DIR=/usr/share/sonic/platform
HWSKU_DIR=/usr/share/sonic/hwsku

SONIC_ASIC_TYPE=$(sonic-cfggen -y /etc/sonic/sonic_version.yml -v asic_type)

if [ -x $CMD_DSSERVE ]; then
    CMD=$CMD_DSSERVE
    CMD_ARGS=$CMD_DSSERVE_ARGS
else
    CMD=$CMD_PHYSYNCD
    CMD_ARGS=
fi

# Use temporary view between init and apply
CMD_ARGS+=" -u"

BOOT_TYPE="$(cat /proc/cmdline | grep -o 'SONIC_BOOT_TYPE=\S*' | cut -d'=' -f2)"

case "$BOOT_TYPE" in
  fast-reboot)
     FAST_REBOOT='yes'
    ;;
  fastfast)
    if [ -e /var/warmboot/warm-starting ]; then
        FASTFAST_REBOOT='yes'
    fi
    ;;
  *)
     FAST_REBOOT='no'
     FASTFAST_REBOOT='no'
    ;;
esac


function check_warm_boot()
{
    # FIXME: if we want to continue start option approach, then we need to add
    #        code here to support redis database query.
    # SYSTEM_WARM_START=`/usr/bin/redis-cli -n 6 hget "WARM_RESTART_ENABLE_TABLE|system" enable`
    # SERVICE_WARM_START=`/usr/bin/redis-cli -n 6 hget "WARM_RESTART_ENABLE_TABLE|${SERVICE}" enable`
    # SYSTEM_WARM_START could be empty, always make WARM_BOOT meaningful.
    # if [[ x"$SYSTEM_WARM_START" == x"true" ]] || [[ x"$SERVICE_WARM_START" == x"true" ]]; then
    #     WARM_BOOT="true"
    # else
        WARM_BOOT="false"
    # fi
}


function set_start_type()
{
    if [ x"$WARM_BOOT" == x"true" ]; then
        CMD_ARGS+=" -t warm"
    elif [ $FAST_REBOOT == "yes" ]; then
        CMD_ARGS+=" -t fast"
    elif [ $FASTFAST_REBOOT == "yes" ]; then
        CMD_ARGS+=" -t fastfast"
    fi
}


config_syncd_bcm()
{
    :;
}


config_physyncd()
{

    set_start_type

    if [ ${ENABLE_SAITHRIFT} == 1 ]; then
        CMD_ARGS+=" -r -m $HWSKU_DIR/port_config.ini"
    fi

    [ -r $PLATFORM_DIR/physyncd.conf ] && . $PLATFORM_DIR/physyncd.conf
}

