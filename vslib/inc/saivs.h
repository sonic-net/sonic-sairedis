#pragma once

extern "C" {
#include "sai.h"
}

#define SAI_KEY_VS_SWITCH_TYPE              "SAI_VS_SWITCH_TYPE"
#define SAI_KEY_VS_SAI_SWITCH_TYPE          "SAI_VS_SAI_SWITCH_TYPE"

#define SAI_VALUE_SAI_SWITCH_TYPE_NPU       "SAI_SWITCH_TYPE_NPU"
#define SAI_VALUE_SAI_SWITCH_TYPE_PHY       "SAI_SWITCH_TYPE_PHY"

/**
 * @def SAI_KEY_VS_INTERFACE_LANE_MAP_FILE
 *
 * If specified in profile.ini it should point to eth interface to lane map.
 *
 * Example:
 * eth0:1,2,3,4
 * eth1:5,6,7,8
 *
 * TODO must support hardware info for multiple switches
 */
#define SAI_KEY_VS_INTERFACE_LANE_MAP_FILE  "SAI_VS_INTERFACE_LANE_MAP_FILE"

/**
 * @def SAI_KEY_VS_RESOURCE_LIMITER_FILE
 *
 * File with resource limitations for object type create.
 *
 * Example:
 * SAI_OBJECT_TYPE_ACL_TABLE=3
 */
#define SAI_KEY_VS_RESOURCE_LIMITER_FILE    "SAI_VS_RESOURCE_LIMITER_FILE"

/**
 * @def SAI_KEY_VS_HOSTIF_USE_TAP_DEVICE
 *
 * Bool flag, (true/false). If set to true, then during create host interface
 * sai object also tap device will be created and mac address will be assigned.
 * For this operation root privileges will be required.
 *
 * By default this flag is set to false.
 */
#define SAI_KEY_VS_HOSTIF_USE_TAP_DEVICE      "SAI_VS_HOSTIF_USE_TAP_DEVICE"

/**
 * @def SAI_KEY_VS_CORE_PORT_INDEX_MAP_FILE
 *
 * For VOQ systems if specified in profile.ini it should point to eth interface to
 * core and core port index map as port name:core_index,core_port_index
 *
 * Example:
 * eth1:0,1
 * eth17:1,1
 *
 */
#define SAI_KEY_VS_CORE_PORT_INDEX_MAP_FILE  "SAI_VS_CORE_PORT_INDEX_MAP_FILE"

#define SAI_VALUE_VS_SWITCH_TYPE_BCM56850     "SAI_VS_SWITCH_TYPE_BCM56850"
#define SAI_VALUE_VS_SWITCH_TYPE_BCM81724     "SAI_VS_SWITCH_TYPE_BCM81724"
#define SAI_VALUE_VS_SWITCH_TYPE_MLNX2700     "SAI_VS_SWITCH_TYPE_MLNX2700"

/*
 * Values for SAI_KEY_BOOT_TYPE (defined in saiswitch.h)
 */

#define SAI_VALUE_VS_BOOT_TYPE_COLD "0"
#define SAI_VALUE_VS_BOOT_TYPE_WARM "1"
#define SAI_VALUE_VS_BOOT_TYPE_FAST "2"

/**
 * @def SAI_VS_UNITTEST_CHANNEL
 *
 * Notification channel for redis database.
 */
#define SAI_VS_UNITTEST_CHANNEL     "SAI_VS_UNITTEST_CHANNEL"

/**
 * @def SAI_VS_UNITTEST_SET_RO_OP
 *
 * Notification operation for "SET" READ_ONLY attribute.
 */
#define SAI_VS_UNITTEST_SET_RO_OP   "set_ro"

/**
 * @def SAI_VS_UNITTEST_SET_STATS
 *
 * Notification operation for "SET" stats on specific object.
 */
#define SAI_VS_UNITTEST_SET_STATS_OP      "set_stats"

/**
 * @def SAI_VS_UNITTEST_ENABLE
 *
 * Notification operation for enabling unittests.
 */
#define SAI_VS_UNITTEST_ENABLE_UNITTESTS  "enable_unittests"

typedef enum _sai_vs_switch_attr_t
{
    /**
     * @brief Will enable metadata unittests.
     *
     * @type bool
     * @flags CREATE_AND_SET
     * @default false
     */
    SAI_VS_SWITCH_ATTR_META_ENABLE_UNITTESTS = SAI_SWITCH_ATTR_CUSTOM_RANGE_START,

    /**
     * @brief Will allow to set value that is read only.
     *
     * Unittests must be enabled.
     *
     * Value is the attribute to be allowed.
     *
     * @type sai_int32_t
     * @flags CREATE_AND_SET
     */
    SAI_VS_SWITCH_ATTR_META_ALLOW_READ_ONLY_ONCE,

} sau_vs_switch_attr_t;
