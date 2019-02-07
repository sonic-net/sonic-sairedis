#!/usr/bin/env bash

CMD_SYNCD=/usr/bin/syncd

# dsserve: domain socket server for stdio
CMD_DSSERVE=/usr/bin/dsserve
CMD_DSSERVE_ARGS="$CMD_SYNCD --diag"

ENABLE_SAITHRIFT=0

PLATFORM_DIR=/usr/share/sonic/platform
HWSKU_DIR=/usr/share/sonic/hwsku

SONIC_ASIC_TYPE=$(sonic-cfggen -y /etc/sonic/sonic_version.yml -v asic_type)

if [ -x $CMD_DSSERVE ]; then
    CMD=$CMD_DSSERVE
    CMD_ARGS=$CMD_DSSERVE_ARGS
else
    CMD=$CMD_SYNCD
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
    if [ -f "/etc/sai.d/sai.profile" ]; then
        CMD_ARGS+=" -p /etc/sai.d/sai.profile"
    else
        CMD_ARGS+=" -p $HWSKU_DIR/sai.profile"
    fi

    [ -e /dev/linux-bcm-knet ] || mknod /dev/linux-bcm-knet c 122 0
    [ -e /dev/linux-user-bde ] || mknod /dev/linux-user-bde c 126 0
    [ -e /dev/linux-kernel-bde ] || mknod /dev/linux-kernel-bde c 127 0
}

config_syncd_mlnx()
{
    CMD_ARGS+=" -p /tmp/sai.profile"

    [ -e /dev/sxdevs/sxcdev ] || ( mkdir -p /dev/sxdevs && mknod /dev/sxdevs/sxcdev c 231 193 )

    # Read MAC address and align the last 6 bits.
    MAC_ADDRESS=$(sonic-cfggen -d -v DEVICE_METADATA.localhost.mac)
    last_byte=$(python -c "print '$MAC_ADDRESS'[-2:]")
    aligned_last_byte=$(python -c "print format(int(int('$last_byte', 16) & 0b11000000), '02x')")  # put mask and take away the 0x prefix
    ALIGNED_MAC_ADDRESS=$(python -c "print '$MAC_ADDRESS'[:-2] + '$aligned_last_byte'")          # put aligned byte into the end of MAC

    # Write MAC address into /tmp/profile file.
    cat $HWSKU_DIR/sai.profile > /tmp/sai.profile
    echo "DEVICE_MAC_ADDRESS=$ALIGNED_MAC_ADDRESS" >> /tmp/sai.profile
    echo "SAI_WARM_BOOT_WRITE_FILE=/var/warmboot/" >> /tmp/sai.profile
}

config_syncd_centec()
{
    CMD_ARGS+=" -p $HWSKU_DIR/sai.profile"

    [ -e /dev/linux_dal ] || mknod /dev/linux_dal c 198 0
    [ -e /dev/net/tun ] || ( mkdir -p /dev/net && mknod /dev/net/tun c 10 200 )
}

config_syncd_cavium()
{
    CMD_ARGS+=" -p $HWSKU_DIR/sai.profile -d"

    export XP_ROOT=/usr/bin/

    # Wait until redis-server starts
    until [ $(redis-cli ping | grep -c PONG) -gt 0 ]; do
        sleep 1
    done
}

config_syncd_marvell()
{
    CMD_ARGS+=" -p $HWSKU_DIR/sai.profile"

    [ -e /dev/net/tun ] || ( mkdir -p /dev/net && mknod /dev/net/tun c 10 200 )
}

config_syncd_barefoot()
{
    # Check and load SDE profile
    P4_PROFILE=$(sonic-cfggen -d -v 'DEVICE_METADATA["localhost"]["p4_profile"]')
    if [[ -n "$P4_PROFILE" ]]; then
        if [[ ( -d /opt/bfn/install_${P4_PROFILE} ) && ( -L /opt/bfn/install || ! -e /opt/bfn/install ) ]]; then
            ln -srfn /opt/bfn/install_${P4_PROFILE} /opt/bfn/install
        fi
    fi
    export ONIE_PLATFORM=`grep onie_platform /etc/machine.conf | awk 'BEGIN { FS = "=" } ; { print $2 }'`
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/bfn/install/lib/platform/$ONIE_PLATFORM:/opt/bfn/install/lib:/opt/bfn/install/lib/tofinopd/switch
    ./opt/bfn/install/bin/dma_setup.sh
    export LD_PRELOAD=libswitchapi.so:libswitchsai.so:libpd.so:libpdcli.so:libdriver.so:libbfsys.so:libbfutils.so:libbf_switchd_lib.so:libtofinopdfixed_thrift.so:libpdthrift.so
}

config_syncd_nephos()
{
    CMD_ARGS+=" -p $HWSKU_DIR/sai.profile"
}

config_syncd_vs()
{
    CMD_ARGS+=" -p $HWSKU_DIR/sai.profile"
}

config_syncd()
{
    check_warm_boot

    if [ "$SONIC_ASIC_TYPE" == "broadcom" ]; then
        config_syncd_bcm
    elif [ "$SONIC_ASIC_TYPE" == "mellanox" ]; then
        config_syncd_mlnx
    elif [ "$SONIC_ASIC_TYPE" == "cavium" ]; then
        config_syncd_cavium
    elif [ "$SONIC_ASIC_TYPE" == "centec" ]; then
        config_syncd_centec
    elif [ "$SONIC_ASIC_TYPE" == "marvell" ]; then
        config_syncd_marvell
     elif [ "$SONIC_ASIC_TYPE" == "barefoot" ]; then
         config_syncd_barefoot
    elif [ "$SONIC_ASIC_TYPE" == "nephos" ]; then
        config_syncd_nephos
    elif [ "$SONIC_ASIC_TYPE" == "vs" ]; then
        config_syncd_vs
    else
        echo "Unknown ASIC type $SONIC_ASIC_TYPE"
        exit 1
    fi

    set_start_type

    if [ ${ENABLE_SAITHRIFT} == 1 ]; then
        CMD_ARGS+=" -r -m $HWSKU_DIR/port_config.ini"
    fi

    [ -r $PLATFORM_DIR/syncd.conf ] && . $PLATFORM_DIR/syncd.conf
}

