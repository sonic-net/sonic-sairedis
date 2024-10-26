#!/bin/bash 
###BRCM Common config UT####
PLATFORM_COMMON_DIR=/usr/share/sonic/device/x86_64-broadcom_common
PLT_CONFIG_BCM=$(find $HWSKU_DIR -name '*.bcm' -not -path "*/phy/*")
PLT_CONFIG_YML=$(find $HWSKU_DIR -name '*.yml')
PLATFORM_DIR=/usr/share/sonic/platform

if [ ! -z "$PLT_CONFIG_BCM" ] && [ -f $PLATFORM_DIR/common_config_support ] ; then
    CONFIG_BCM=$(find /tmp -name '*.bcm')
    
    #Get first three characters of chip id
    readline=$(grep '0x14e4' /proc/linux-kernel-bde)
    chip_id=${readline#*0x14e4:0x}
    chip_id=${chip_id::3}
    COMMON_CONFIG_BCM=$(find $PLATFORM_COMMON_DIR/x86_64-broadcom_${chip_id} -maxdepth 1 -name '*.bcm')
    check_override=false
    check_pass=false
    
    #Check if common config does apply to config bcm correctly
    while read line
    do
        line=$( echo $line | xargs )
        if [ ! -z "$line" ];then
            if [ "${line::1}" == '#' ];then
                echo "Skip checking line starting with #"
            elif [ "$line" == "[High Inheritance Precedence]" ];then
                echo "Checking High Inheritance property..."
                check_override=true
            elif [ "$line" == "[Low Inheritance Precedence]" ];then
                echo "Checking Low Inheritance property..."
                check_override=false
            else
                if $check_override ;then
                   if grep -q $line $CONFIG_BCM ;then
                      check_pass=true
                   else
                      echo "Fail: Checking overwite properties not existing.."
                      return
                   fi
                else
                   sedline=${line%=*}
                   if grep -q $sedline $CONFIG_BCM ;then
                      check_pass=true
                   else
                      echo "Fail: Checking properties not existing.."
                      return
                   fi
                fi
            fi
        fi
    done < $COMMON_CONFIG_BCM
    if $check_pass ;then
       echo "PASS: Checking Common config merged Success"
    fi
fi

if [ ! -z "$PLT_CONFIG_YML" ] && [ -f $PLATFORM_DIR/common_config_support ]; then
    CONFIG_YML=$(find /tmp -name '*.yml')
    
    #Get first three characters of chip id
    readline=$(grep '0:14e4' /proc/linux_ngbde)
    chip_id=${readline#*0:14e4:}
    chip_id=${chip_id::3}
    COMMON_CONFIG_BCM=$(find $PLATFORM_COMMON_DIR/x86_64-broadcom_${chip_id} -maxdepth 1 -name '*.bcm')
    check_override=false
    check_pass=false
    
    #Check if common config does apply to config bcm correctly
    while read line
    do
        line=$( echo $line | xargs )
        if [ ! -z "$line" ];then
            if [ "${line::1}" == '#' ];then
                echo "Skip checking line starting with #"
            elif [ "$line" == "[High Inheritance Precedence]" ];then
                echo "Checking High Inheritance property..."
                check_override=true
            elif [ "$line" == "[Low Inheritance Precedence]" ];then
                echo "Checking Low Inheritance property..."
                check_override=true
            else
                if $check_override ;then
                   if grep -q $line $CONFIG_BCM ;then
                      check_pass=true
                   else
                      echo "Fail: Checking overwite properties not existing.."
                      return
                   fi
                else
                   sedline=${line%=*}
                   if grep -q $sedline $CONFIG_BCM ;then
                      check_pass=true
                   else
                      echo "Fail: Checking properties not existing.."
                      return
                   fi
                fi
            fi
        fi
    
    if $check_pass ;then
       echo "PASS: Checking Common config merged Success"
    fi
    
    done < $COMMON_CONFIG_BCM

fi
    
