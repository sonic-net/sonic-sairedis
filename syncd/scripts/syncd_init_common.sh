#!/usr/bin/env bash

CMD_SYNCD=/usr/bin/syncd

# dsserve: domain socket server for stdio
CMD_DSSERVE=/usr/bin/dsserve
CMD_DSSERVE_ARGS="$CMD_SYNCD --diag"

ENABLE_SAITHRIFT=0

TEMPLATES_DIR=/usr/share/sonic/templates
PLATFORM_DIR=/usr/share/sonic/platform
HWSKU_DIR=/usr/share/sonic/hwsku
SAI_PROFILE_DIR=/etc/sai.d

VARS_FILE=$TEMPLATES_DIR/swss_vars.j2

# Retrieve vars from sonic-cfggen
SYNCD_VARS=$(sonic-cfggen -d -y /etc/sonic/sonic_version.yml -t $VARS_FILE) || exit 1
SONIC_ASIC_TYPE=$(echo $SYNCD_VARS | jq -r '.asic_type')

if [ -x $CMD_DSSERVE ]; then
    CMD=$CMD_DSSERVE
    CMD_ARGS=$CMD_DSSERVE_ARGS
else
    CMD=$CMD_SYNCD
    CMD_ARGS=
fi

# Use temporary view between init view and apply view
CMD_ARGS+=" -u"

# Create a folder for SAI failure dump files
mkdir -p /var/log/sai_failure_dump/

# Use bulk APIs in SAI
# currently disabled since most vendors don't support that yet
# CMD_ARGS+=" -l"

# Set zmq mode by default for smartswitch DPU
# Otherwise, set synchronous mode if it is enabled in CONFIG_DB
SYNC_MODE=$(echo $SYNCD_VARS | jq -r '.synchronous_mode')
SWITCH_TYPE=$(echo $SYNCD_VARS | jq -r '.switch_type')
if [ "$SWITCH_TYPE" == "dpu" ]; then
    CMD_ARGS+=" -z zmq_sync -x /usr/share/sonic/hwsku/context_config.json"
elif [ "$SYNC_MODE" == "enable" ]; then
    CMD_ARGS+=" -s"
fi

SUPPORTING_BULK_COUNTER_GROUPS=$(echo $SYNCD_VARS | jq -r '.supporting_bulk_counter_groups')
if [ "$SUPPORTING_BULK_COUNTER_GROUPS" != "" ]; then
    CMD_ARGS+=" -B $SUPPORTING_BULK_COUNTER_GROUPS"
fi

case "$(cat /proc/cmdline)" in
  *SONIC_BOOT_TYPE=fastfast*)
    if [ -e /var/warmboot/warm-starting ]; then
        FASTFAST_REBOOT='yes'
    fi
    ;;
  *SONIC_BOOT_TYPE=express*)
    if [ -e /var/warmboot/warm-starting ]; then
        EXPRESS_REBOOT='yes'
    fi
    ;;
  *SONIC_BOOT_TYPE=fast*|*fast-reboot*)
    # check that the key exists
    SYSTEM_FAST_REBOOT=`sonic-db-cli STATE_DB hget "FAST_RESTART_ENABLE_TABLE|system" enable`
    if [[ x"${SYSTEM_FAST_REBOOT}" == x"true" ]]; then
       FAST_REBOOT='yes'
    else
       FAST_REBOOT='no'
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
    elif [ x"$FAST_REBOOT" == x"yes" ]; then
        CMD_ARGS+=" -t fast"
    elif [ x"$FASTFAST_REBOOT" == x"yes" ]; then
        CMD_ARGS+=" -t fastfast"
    elif [ x"$EXPRESS_REBOOT" == x"yes" ]; then
        CMD_ARGS+=" -t express"
    fi
}

config_syncd_pensando()
{
    CMD_ARGS+=" -l"
}

config_syncd_cisco_8000()
{
    export BASE_OUTPUT_DIR=/opt/cisco/silicon-one
    CMD_ARGS+=" -p $HWSKU_DIR/sai.profile"
    CMD_ARGS+=" -l"

    # Cisco SDK debug shell support
    version=$(python3 -V 2>&1 | sed 's/.* \([0-9]\).\([0-9]\).*/\1\.\2/')
    if [ ! -z "$version" ]; then
        export SAI_SHELL_ENABLE=1
        export SAI_DEBUG_PYTHON_SO_PATH=/usr/lib/python${version}/config-${version}m-x86_64-linux-gnu/libpython${version}m.so
        export PYTHONPATH=/usr/lib/cisco/pylib
    fi
}

function merge_config_bcm_files()
{
    to_file=$1
    from_file=$2
    message=$3
    override=false
    echo "" >> $to_file
    echo "# Start of $message" >> $to_file
    while read line
    do
        line=$( echo $line | xargs )
        if [ ! -z "$line" ];then
            if [ "${line::1}" == '#' ];then
                echo $line >> $to_file
            elif [ "$line" == "[Low Inheritance Precedence]" ];then
                override=false
                echo "# $line" >> $to_file
            elif [ "$line" == "[High Inheritance Precedence]" ];then
                override=true
                echo "# $line" >> $to_file
                echo "Merge properties with override $override"
            else
                sedline=${line%=*}
                if grep -q $sedline $to_file ;then
                   if $override ;then
                      echo "Override the config $(grep $sedline $to_file) with $line in $to_file"
                      prop=${line:0:`expr index $line =`}
                      sed -i "/$prop/d" $to_file
                      echo $line >> $to_file
                   else
                      grepline=$(grep $sedline $to_file)
                      if [ "${grepline::1}" == '#' ];then
                         echo $line >> $to_file
                      else
                         echo "Keep the config $(grep $sedline $to_file) in $to_file"
                      fi
                   fi
                else
                   echo $line >> $to_file
                fi
            fi
        fi
    done < $from_file
    echo "# End of $message" >> $to_file
    echo "Merged $from_file to $to_file"
}

function merge_config_yml_files()
{
    to_file=$1
    from_file=$2
    message=$3
    override=false
    merged_cnt=0
    echo "" >> $to_file
    echo "# Start of $message" >> $to_file
    while read line
    do
        line=$( echo $line | xargs )
        if [ ! -z "$line" ];then
            if [ "${line::1}" == '#' ];then
                echo "        $line" >> $to_file
            elif [ "$line" == "[Low Inheritance Precedence]" ];then
                override=false
                echo "        # $line" >> $to_file
            elif [ "$line" == "[High Inheritance Precedence]" ];then
                override=true
                echo "        # $line" >> $to_file
                echo "Merge properties with override $override"
            else
                sedline=${line%:*}
                if grep -q $sedline $to_file ;then
                   if $override ;then
                      echo "Override the config $(grep $sedline $to_file) with $line in $to_file"
                      prop=${line:0:`expr index "$line" :`}
                      sed -i "/$prop/d" $to_file
                      echo "        $line" >> $to_file
                      merged_cnt+=1
                   else
                      grepline=$(grep $sedline $to_file)
                      grepline="${grepline#"${grepline%%[![:space:]]*}"}"
                      if [ "${grepline::1}" == '#' ];then
                         echo "        $line" >> $to_file
                         merged_cnt+=1
                      else
                         echo "Keep the config $(grep $sedline $to_file) in $to_file"
                      fi
                   fi
                else
                   echo "        $line" >> $to_file
                   merged_cnt+=1
                fi
            fi
        fi
    done < $from_file

    if [ $merged_cnt -gt 0 ]; then
         sed -i "/# Start of/a \    global:" $to_file
         sed -i "/# Start of/a \  0:" $to_file
         sed -i "/# Start of/a \bcm_device:" $to_file
         sed -i "/# Start of/a \---" $to_file
    fi
    echo "# End of $message" >> $to_file
    if [ $merged_cnt -gt 0 ]; then
       sed -i "/# End of/i \..." $to_file
    fi
    echo "Merged $from_file to $to_file"
}

config_syncd_bcm()
{
    PLATFORM_COMMON_DIR=/usr/share/sonic/device/x86_64-broadcom_common
    PLT_CONFIG_BCM=""
    PLT_CONFIG_YML=""
    PLT_SAI_PROFILE=$(find $SAI_PROFILE_DIR -name 'sai.profile')
    readline=$(grep SAI_INIT_CONFIG_FILE $PLT_SAI_PROFILE)
    if [ ${readline: -3} == "bcm" ]; then
       PLT_CONFIG_BCM=${readline#*=}
    elif [ ${readline: -3} == "yml" ]; then
       PLT_CONFIG_YML=${readline#*=}
    fi

    if [ ! -z "$PLT_CONFIG_BCM" ] && [ -f $PLATFORM_DIR/common_config_support ] ; then
       cp -f $PLT_CONFIG_BCM /tmp
       cp -f /etc/sai.d/sai.profile /tmp
       CONFIG_BCM=$(find /tmp -name '*.bcm')
       SAI_PROFILE=$(find /tmp -name 'sai.profile')
       sed -i 's+/usr/share/sonic/hwsku+/tmp+g' $SAI_PROFILE

       #Get first three characters of chip id
       readline=$(grep '0x14e4' /proc/linux-kernel-bde)
       chip_id=${readline#*0x14e4:0x}
       chip_id=${chip_id::3}
       COMMON_CONFIG_BCM=$(find $PLATFORM_COMMON_DIR/x86_64-broadcom_${chip_id} -maxdepth 1 -name '*.bcm')

       if [ -f $PLATFORM_COMMON_DIR/x86_64-broadcom_${chip_id}/*.bcm ]; then
          for file in $CONFIG_BCM; do
             merge_config_bcm_files $file $COMMON_CONFIG_BCM "chip common properties"
          done
          echo "Merging $PLT_CONFIG_BCM with $COMMON_CONFIG_BCM, merge files stored in $CONFIG_BCM"
       fi
       #sync the file system
       sync

       # copy the final config.bcm and sai.profile to the shared folder for 'show tech'
       cp -f /tmp/sai.profile /var/run/sswsyncd/
       cp -f /tmp/*.bcm /var/run/sswsyncd/
    fi

    if [ ! -z "$PLT_CONFIG_YML" ] && [ -f $PLATFORM_DIR/common_config_support ]; then
       cp -f $PLT_CONFIG_YML /tmp
       cp -f /etc/sai.d/sai.profile /tmp
       CONFIG_YML=$(find /tmp -name '*.yml')
       SAI_PROFILE=$(find /tmp -name 'sai.profile')
       sed -i 's+/usr/share/sonic/hwsku+/tmp+g' $SAI_PROFILE

       #Get first three characters of chip id
       readline=$(grep '0:14e4' /proc/linux_ngbde)
       chip_id=${readline#*0:14e4:}
       chip_id=${chip_id::3}
       COMMON_CONFIG_BCM=$(find $PLATFORM_COMMON_DIR/x86_64-broadcom_${chip_id} -maxdepth 1 -name '*.bcm')

       if [ -f $PLATFORM_COMMON_DIR/x86_64-broadcom_${chip_id}/*.bcm ]; then
          for file in $CONFIG_YML; do
             merge_config_yml_files $file $COMMON_CONFIG_BCM "chip common properties"
          done
          echo "Merging $PLT_CONFIG_YML with $COMMON_CONFIG_BCM, merge files stored in $CONFIG_YML "
       fi
       #sync the file system
       sync

       # copy the final config.bcm and sai.profile to the shared folder for 'show tech'
       cp -f /tmp/sai.profile /var/run/sswsyncd/
       cp -f /tmp/*.yml /var/run/sswsyncd/
    fi

    if [ -f "/tmp/sai.profile" ]; then
        CMD_ARGS+=" -p /tmp/sai.profile"
	elif [ -f "/etc/sai.d/sai.profile" ]; then
        CMD_ARGS+=" -p /etc/sai.d/sai.profile"
    else
        CMD_ARGS+=" -p $HWSKU_DIR/sai.profile"
    fi

    if [ -f "$HWSKU_DIR/context_config.json" ]; then
        CMD_ARGS+=" -x $HWSKU_DIR/context_config.json -g 0"
    fi

    echo "SAI_OBJECT_TYPE_ACL_TABLE" >> /tmp/break_before_make_objects
    echo "SAI_OBJECT_TYPE_TUNNEL" >> /tmp/break_before_make_objects
    CMD_ARGS+=" -b /tmp/break_before_make_objects"

    [ -e /dev/linux-bcm-knet ] || mknod /dev/linux-bcm-knet c 122 0
    [ -e /dev/linux-user-bde ] || mknod /dev/linux-user-bde c 126 0
    [ -e /dev/linux-kernel-bde ] || mknod /dev/linux-kernel-bde c 127 0

}

config_syncd_mlnx()
{
    CMD_ARGS+=" -l -p /tmp/sai.profile"

    [ -e /dev/sxdevs/sxcdev ] || ( mkdir -p /dev/sxdevs && mknod /dev/sxdevs/sxcdev c 231 193 )

    # Read MAC address
    MAC_ADDRESS="$(echo $SYNCD_VARS | jq -r '.mac')"

    # Read dual ToR and DSCP remapping information
    DUAL_TOR="$(echo $SYNCD_VARS | jq -r '.dual_tor')"
    DSCP_REMAPPING="$(echo $SYNCD_VARS | jq -r '.dscp_remapping')"

    SAI_COMMON_FILE_PATH=/etc/mlnx/sai-common.profile

    if [[ -f $HWSKU_DIR/sai.profile.j2 ]]; then
        export RESOURCE_TYPE="$(echo $SYNCD_VARS | jq -r '.resource_type')"
        j2 -e RESOURCE_TYPE $HWSKU_DIR/sai.profile.j2 -o /tmp/sai-temp.profile
    else
        cat $HWSKU_DIR/sai.profile > /tmp/sai-temp.profile
    fi

    echo >> /tmp/sai-temp.profile

    DEVICE_TYPE=$(/usr/bin/asic_detect/asic_detect.sh)
    if [[ $? -eq 0 ]]; then
        ASIC_PROFILE_FILE="sai-${DEVICE_TYPE}.profile"
        ASIC_PROFILE_PATH="/etc/mlnx/${ASIC_PROFILE_FILE}"
        if [ -f "$ASIC_PROFILE_PATH" ]; then
            cat "$ASIC_PROFILE_PATH" >> /tmp/sai-temp.profile
            echo >> /tmp/sai-temp.profile
        fi
    else
        echo "Warning: ASIC is not detected..."
    fi

    if [[ -f $SAI_COMMON_FILE_PATH ]]; then
        cat $SAI_COMMON_FILE_PATH >> /tmp/sai-temp.profile
    fi

    # keep only the first occurence of each prefix with '=' sign, and remove the others.
    awk -F= '!seen[$1]++' /tmp/sai-temp.profile > /tmp/sai.profile
    rm -f /tmp/sai-temp.profile

    # Update sai.profile with MAC_ADDRESS and WARM_BOOT settings
    echo "DEVICE_MAC_ADDRESS=$MAC_ADDRESS" >> /tmp/sai.profile
    echo "SAI_WARM_BOOT_WRITE_FILE=/var/warmboot/" >> /tmp/sai.profile

    if [[ "$DUAL_TOR" == "enable" ]] && [[ "$DSCP_REMAPPING" == "enable" ]]; then
       echo "SAI_DSCP_REMAPPING_ENABLED=1" >> /tmp/sai.profile
    fi

    if [[ "$DUAL_TOR" == "enable" ]]; then
       echo "SAI_ADDITIONAL_MAC_ENABLED=1" >> /tmp/sai.profile
       echo "SAI_ACL_MULTI_BINDING_ENABLED=1" >> /tmp/sai.profile
    fi

    SDK_DUMP_PATH=`cat /tmp/sai.profile|grep "SAI_DUMP_STORE_PATH"|cut -d = -f2`
    if [ ! -d "$SDK_DUMP_PATH" ]; then
        mkdir -p "$SDK_DUMP_PATH"
    fi

    # Pick extra SAI profile parameters generated by other applications
    if [[ -f /tmp/sai_extra.profile ]]; then
        cat /tmp/sai_extra.profile >> /tmp/sai.profile
    fi

    # Ensure no redundant newlines
    sed -i '/^$/d' /tmp/sai.profile

    # As long as sonic does not support PTP which can be enabled/disabled, Nvidia platforms enables
    # phcsync for all systems. If HW does not support it, it will do nothing.
    supervisorctl start phcsync
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
    until [ $(sonic-db-cli PING | grep -c PONG) -gt 0 ]; do
        sleep 1
    done
}

config_syncd_marvell_prestera()
{
    CMD_ARGS+=" -p $HWSKU_DIR/sai.profile"

    export MRVL_PSAI_SONIC=1

    [ -e /dev/net/tun ] || ( mkdir -p /dev/net && mknod /dev/net/tun c 10 200 )
}

config_syncd_barefoot()
{
    PROFILE_FILE="$HWSKU_DIR/sai.profile"
    if [ ! -f $PROFILE_FILE ]; then
        # default profile file
        PROFILE_FILE="/tmp/sai.profile"
        echo "SAI_KEY_WARM_BOOT_WRITE_FILE=/var/warmboot/sai-warmboot.bin" > $PROFILE_FILE
        echo "SAI_KEY_WARM_BOOT_READ_FILE=/var/warmboot/sai-warmboot.bin" >> $PROFILE_FILE
    fi
    CMD_ARGS+=" -l -p $PROFILE_FILE"
    # Check if SDE profile is configured
    P4_PROFILE=$(sonic-cfggen -d -v 'DEVICE_METADATA["localhost"]["p4_profile"]')
    if [[ -n "$P4_PROFILE" ]]; then
        if [[ ( -d /opt/bfn/install_${P4_PROFILE} ) && ( -L /opt/bfn/install || ! -e /opt/bfn/install ) ]]; then
            ln -srfn /opt/bfn/install_${P4_PROFILE} /opt/bfn/install
        fi
    else
        CHIP_FAMILY_INFO="$(cat $HWSKU_DIR/switch-tna-sai.conf | grep chip_family | awk -F : '{print $2}' | cut -d '"'  -f 2)"
        CHIP_FAMILY=${CHIP_FAMILY_INFO,,}
        [[ "$CHIP_FAMILY" == "tofino" ]] && P4_PTYPE="x" || P4_PTYPE="y"
        # Check if the current profile fits the ASIC family
        PROFILE_DEFAULT=$(readlink /opt/bfn/install)
        if [[ "$PROFILE_DEFAULT" != "install_$P4_PTYPE"*"_profile" && "$PROFILE_DEFAULT" != *"_$CHIP_FAMILY"  ]]; then
            # Find suitable profile
            PROFILE=$(ls -d /opt/bfn/install_$P4_PTYPE*_profile -d  /opt/bfn/install_*_$CHIP_FAMILY 2> /dev/null | head -1)
            if [[ ! -z $PROFILE  ]]; then
                ln -srfn $PROFILE /opt/bfn/install
            fi
        fi
    fi
    export PYTHONHOME=/opt/bfn/install/
    export PYTHONPATH=/opt/bfn/install/
    export ONIE_PLATFORM=`grep onie_platform /etc/machine.conf | awk 'BEGIN { FS = "=" } ; { print $2 }'`
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/bfn/install/lib/platform/$ONIE_PLATFORM:/opt/bfn/install/lib:/opt/bfn/install/lib/tofinopd/switch
    ./opt/bfn/install/bin/dma_setup.sh
}

config_syncd_nephos()
{
    CMD_ARGS+=" -p $HWSKU_DIR/sai.profile"
}

config_syncd_vs()
{
    if [[ $(sonic-db-cli CONFIG_DB hget 'DEVICE_METADATA|localhost' switch_type) == 'dpu' ]]; then
        if [[ -f /usr/bin/syncd_dash ]]; then
            CMD_SYNCD=/usr/bin/syncd_dash
            CMD=$CMD_SYNCD
        fi
    fi

    CMD_ARGS+=" -l -p $HWSKU_DIR/sai.profile"
}

vpp_api_check()
{
   VPP_API_SOCK=$1
   while true
   do
      [ -S "$VPP_API_SOCK" ] && vpp_api_test socket-name $VPP_API_SOCK <<< "show_version" 2>/dev/null | grep "version:" && break
      sleep 1
   done
}

config_syncd_vpp()
{
    CMD_ARGS+=" -p $HWSKU_DIR/sai_vpp.profile"
    vpp_api_check "/run/vpp/api.sock"
    source /etc/sonic/vpp/syncd_vpp_env
    export NO_LINUX_NL
}

config_syncd_soda()
{
    # Add support for SAI bulk operations
    CMD_ARGS+=" -l -p $HWSKU_DIR/sai.profile"
}

config_syncd_marvell_teralynx()
{
    CMD_ARGS+=" -p $HWSKU_DIR/sai.profile"
    ulimit -s 65536
    export II_ROOT="/var/log/mrvl_teralynx"
    export II_APPEND_LOG=1
    mkdir -p $II_ROOT
}

config_syncd_nvidia_bluefield()
{
    # Read MAC addresses
    base_mac="$(echo $SYNCD_VARS | jq -r '.mac')"
    hwsku=$(sonic-cfggen -d -v 'DEVICE_METADATA["localhost"]["hwsku"]')
    single_port=$([[ $hwsku == *"-com-dpu" ]] && echo true || echo false)

    eth0_mac=$(cat /sys/class/net/Ethernet0/address)

    cp $HWSKU_DIR/sai.profile /tmp/sai-temp.profile

    echo >> /tmp/sai-temp.profile

    DEVICE_TYPE=$(/usr/bin/asic_detect/asic_detect.sh)
    if [[ $? -eq 0 ]]; then
        ASIC_PROFILE_FILE="sai-${DEVICE_TYPE}.profile"
        ASIC_PROFILE_PATH="/etc/mlnx/${ASIC_PROFILE_FILE}"
        if [ -f "$ASIC_PROFILE_PATH" ]; then
            cat "$ASIC_PROFILE_PATH" >> /tmp/sai-temp.profile
            echo >> /tmp/sai-temp.profile
        fi
    else
        echo "Warning: ASIC is not detected..."
    fi

    # keep only the first occurence of each prefix with '=' sign, and remove the others.
    awk -F= '!seen[$1]++' /tmp/sai-temp.profile > /tmp/sai.profile
    rm -f /tmp/sai-temp.profile

    # Update sai.profile with MAC_ADDRESS
    echo "DEVICE_MAC_ADDRESS=$base_mac" >> /tmp/sai.profile
    echo "PORT_1_MAC_ADDRESS=$eth0_mac" >> /tmp/sai.profile

    CMD_ARGS+=" -l -p /tmp/sai.profile -w 180000000"

    SDK_DUMP_PATH=$(cat /tmp/sai.profile | grep "SAI_DUMP_STORE_PATH" | cut -d = -f2)
    if [ ! -d "$SDK_DUMP_PATH" ]; then
        mkdir -p "$SDK_DUMP_PATH"
    fi

    echo 11700 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
    mkdir -p /mnt/huge
    mount -t hugetlbfs pagesize=1GB /mnt/huge

    ethtool -A Ethernet0 rx off tx off
}

config_syncd_xsight()
{
    SYS_MODE="asic"
    CFG_FILE="/etc/sonic/xlink.cfg"
    LABEL_REVISION_FILE="/etc/sonic/hw_revision"
    ONIE_MACHINE=`sed -n -e 's/^.*onie_machine=//p' /etc/machine.conf`

    /usr/bin/init_xsai.sh

    #export XLOG_DEBUG="XSW SAI SAI-HOST XHAL-TBL XHAL-LKP XHAL-LPM XHAL-TCAM XHAL-DTE XHAL-RNG XHAL-SP XHAL-RPC"
    export XLOG_SYSLOG=ALL
    export XLOG_LEVEL=ERROR
    #export XLOG_FILE="/tmp/xsai.log"

    #ports for XCLI Thrift client
    export SAI_RPC_PORT=31000
    export XSW_RPC_PORT=31001
    export XHAL_RPC_PORT=31002

    if [[ ${ONIE_MACHINE,,} != *"kvm"* ]]; then
        # Working on HW box. Determine what to run XBM/ASIC
        if [[ -f ${CFG_FILE} ]]; then
            SYS_MODE=`sed -n -e 's/^.*sys_mode[[:blank:]]*=[[:blank:]]*//p' ${CFG_FILE}`
        fi
    else
        SYS_MODE="xbm"
    fi

    if [[ ${SYS_MODE,,} == "xbm" ]]; then
        rm -f /xbm/log/*
        /xbm/run_xbm.sh &
    else
        export XDRV_PLUGIN_SO=libxpci_drv_plugin.so
    fi

    CMD_ARGS+=" -p $HWSKU_DIR/sai.profile"
}

config_syncd_clounix()
{
    CMD_ARGS+=" -p $HWSKU_DIR/sai.profile"
}

config_syncd()
{
    check_warm_boot


    if [ "$SONIC_ASIC_TYPE" == "cisco-8000" ]; then
        config_syncd_cisco_8000
    elif [ "$SONIC_ASIC_TYPE" == "broadcom" ]; then
        config_syncd_bcm
    elif [ "$SONIC_ASIC_TYPE" == "mellanox" ]; then
        config_syncd_mlnx
    elif [ "$SONIC_ASIC_TYPE" == "cavium" ]; then
        config_syncd_cavium
    elif [ "$SONIC_ASIC_TYPE" == "centec" ]; then
        config_syncd_centec
    elif [ "$SONIC_ASIC_TYPE" == "marvell-prestera" ]; then
        config_syncd_marvell_prestera
     elif [ "$SONIC_ASIC_TYPE" == "barefoot" ]; then
         config_syncd_barefoot
    elif [ "$SONIC_ASIC_TYPE" == "nephos" ]; then
        config_syncd_nephos
    elif [ "$SONIC_ASIC_TYPE" == "vs" ]; then
        config_syncd_vs
    elif [ "$SONIC_ASIC_TYPE" == "vpp" ]; then
        config_syncd_vpp
    elif [ "$SONIC_ASIC_TYPE" == "marvell-teralynx" ]; then
        config_syncd_marvell_teralynx
    elif [ "$SONIC_ASIC_TYPE" == "alpinevs" ]; then
        config_syncd_vs
    elif [ "$SONIC_ASIC_TYPE" == "soda" ]; then
        config_syncd_soda
    elif [ "$SONIC_ASIC_TYPE" == "nvidia-bluefield" ]; then
        config_syncd_nvidia_bluefield
    elif [ "$SONIC_ASIC_TYPE" == "xsight" ]; then
        config_syncd_xsight
    elif [ "$SONIC_ASIC_TYPE" == "pensando" ]; then
	config_syncd_pensando
    elif [ "$SONIC_ASIC_TYPE" == "clounix" ]; then
        config_syncd_clounix
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
