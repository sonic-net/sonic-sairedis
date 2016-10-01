#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_STP_INSTANCE

// TODO QUAD API needed, need STP_MEMBER object

const sai_attr_metadata_t sai_stp_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_STP_INSTANCE,
        .attrid                 = SAI_STP_ATTR_VLAN_LIST,
        .serializationtype      = SAI_SERIALIZATION_TYPE_VLAN_LIST,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

};

const size_t sai_stp_attr_metadata_count = sizeof(sai_stp_attr_metadata)/sizeof(sai_attr_metadata_t);
