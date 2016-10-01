#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_QOS_MAPS

DEFINE_ENUM_VALUES(sai_qos_map_type_t)
{
    SAI_QOS_MAP_DOT1P_TO_TC,
    SAI_QOS_MAP_DOT1P_TO_COLOR,
    SAI_QOS_MAP_DSCP_TO_TC,
    SAI_QOS_MAP_DSCP_TO_COLOR,
    SAI_QOS_MAP_TC_TO_QUEUE,
    SAI_QOS_MAP_TC_AND_COLOR_TO_DSCP,
    SAI_QOS_MAP_TC_AND_COLOR_TO_DOT1P,
    SAI_QOS_MAP_TC_TO_PRIORITY_GROUP,
    SAI_QOS_MAP_PFC_PRIORITY_TO_PRIORITY_GROUP,
    SAI_QOS_MAP_PFC_PRIORITY_TO_QUEUE
};

const sai_attr_metadata_t sai_qos_maps_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_QOS_MAPS,
        .attrid                 = SAI_QOS_MAP_ATTR_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_qos_map_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_qos_map_type_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_QOS_MAPS,
        .attrid                 = SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST,
        .serializationtype      = SAI_SERIALIZATION_TYPE_QOS_MAP_LIST,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO all defaults? what means all?
        // TODO special handling is needed
        //  * Defaults:
        //  *   All Dot1p/DSCP maps to traffic class 0
        //  *   All Dot1p/DSCP maps to color SAI_PACKET_COLOR_GREEN
        //  *   All traffic class maps to queue 0.
    },
};

const size_t sai_qos_maps_attr_metadata_count = sizeof(sai_qos_maps_attr_metadata)/sizeof(sai_attr_metadata_t);
