#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_HASH

DEFINE_ENUM_VALUES(sai_native_hash_field_t)
{
    SAI_NATIVE_HASH_FIELD_SRC_IP,
    SAI_NATIVE_HASH_FIELD_DST_IP,
    SAI_NATIVE_HASH_FIELD_INNER_SRC_IP,
    SAI_NATIVE_HASH_FIELD_INNER_DST_IP,
    SAI_NATIVE_HASH_FIELD_VLAN_ID,
    SAI_NATIVE_HASH_FIELD_IP_PROTOCOL,
    SAI_NATIVE_HASH_FIELD_ETHERTYPE,
    SAI_NATIVE_HASH_FIELD_L4_SRC_PORT,
    SAI_NATIVE_HASH_FIELD_L4_DST_PORT,
    SAI_NATIVE_HASH_FIELD_SRC_MAC,
    SAI_NATIVE_HASH_FIELD_DST_MAC,
    SAI_NATIVE_HASH_FIELD_IN_PORT
};

const char metadata_sai_native_hash_field_t_enum_name[] = "sai_native_hash_field_t";
const sai_native_hash_field_t metadata_sai_native_hash_field_t_enum_values[] = {
    SAI_NATIVE_HASH_FIELD_SRC_IP,
    SAI_NATIVE_HASH_FIELD_DST_IP,
    SAI_NATIVE_HASH_FIELD_INNER_SRC_IP,
    SAI_NATIVE_HASH_FIELD_INNER_DST_IP,
    SAI_NATIVE_HASH_FIELD_VLAN_ID,
    SAI_NATIVE_HASH_FIELD_IP_PROTOCOL,
    SAI_NATIVE_HASH_FIELD_ETHERTYPE,
    SAI_NATIVE_HASH_FIELD_L4_SRC_PORT,
    SAI_NATIVE_HASH_FIELD_L4_DST_PORT,
    SAI_NATIVE_HASH_FIELD_SRC_MAC,
    SAI_NATIVE_HASH_FIELD_DST_MAC,
    SAI_NATIVE_HASH_FIELD_IN_PORT,
};
const char* metadata_sai_native_hash_field_t_enum_values_names[] = {
    "SAI_NATIVE_HASH_FIELD_SRC_IP",
    "SAI_NATIVE_HASH_FIELD_DST_IP",
    "SAI_NATIVE_HASH_FIELD_INNER_SRC_IP",
    "SAI_NATIVE_HASH_FIELD_INNER_DST_IP",
    "SAI_NATIVE_HASH_FIELD_VLAN_ID",
    "SAI_NATIVE_HASH_FIELD_IP_PROTOCOL",
    "SAI_NATIVE_HASH_FIELD_ETHERTYPE",
    "SAI_NATIVE_HASH_FIELD_L4_SRC_PORT",
    "SAI_NATIVE_HASH_FIELD_L4_DST_PORT",
    "SAI_NATIVE_HASH_FIELD_SRC_MAC",
    "SAI_NATIVE_HASH_FIELD_DST_MAC",
    "SAI_NATIVE_HASH_FIELD_IN_PORT",
    NULL
};
const char* metadata_sai_native_hash_field_t_enum_values_short_names[] = {
    "SRC_IP",
    "DST_IP",
    "INNER_SRC_IP",
    "INNER_DST_IP",
    "VLAN_ID",
    "IP_PROTOCOL",
    "ETHERTYPE",
    "L4_SRC_PORT",
    "L4_DST_PORT",
    "SRC_MAC",
    "DST_MAC",
    "IN_PORT",
    NULL
};
const size_t metadata_sai_native_hash_field_t_enum_values_count = 12;
DEFINE_ENUM_METADATA(sai_native_hash_field_t, 12);

const sai_attr_metadata_t sai_hash_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_HASH,
        .attrid                 = SAI_HASH_ATTR_NATIVE_FIELD_LIST,
        .attridname             = "SAI_HASH_ATTR_NATIVE_FIELD_LIST",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_native_hash_field_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_native_hash_field_t ),
        .enummetadata           = &metadata_enum_sai_native_hash_field_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_HASH,
        .attrid                 = SAI_HASH_ATTR_UDF_GROUP_LIST,
        .attridname             = "SAI_HASH_ATTR_UDF_GROUP_LIST",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_UDF_GROUP },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },
};

const size_t sai_hash_attr_metadata_count = sizeof(sai_hash_attr_metadata)/sizeof(sai_attr_metadata_t);
