#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_FDB

DEFINE_ENUM_VALUES(sai_fdb_entry_type_t)
{
    SAI_FDB_ENTRY_DYNAMIC,
    SAI_FDB_ENTRY_STATIC
};

const char metadata_sai_fdb_event_t_enum_name[] = "sai_fdb_event_t";
const sai_fdb_event_t metadata_sai_fdb_event_t_enum_values[] = {
    SAI_FDB_EVENT_LEARNED,
    SAI_FDB_EVENT_AGED,
    SAI_FDB_EVENT_FLUSHED,
};
const char* metadata_sai_fdb_event_t_enum_values_names[] = {
    "SAI_FDB_EVENT_LEARNED",
    "SAI_FDB_EVENT_AGED",
    "SAI_FDB_EVENT_FLUSHED",
    NULL
};
const char* metadata_sai_fdb_event_t_enum_values_short_names[] = {
    "LEARNED",
    "AGED",
    "FLUSHED",
    NULL
};
const size_t metadata_sai_fdb_event_t_enum_values_count = 3;
DEFINE_ENUM_METADATA(sai_fdb_event_t, 3);

const char metadata_sai_fdb_entry_type_t_enum_name[] = "sai_fdb_entry_type_t";
const sai_fdb_entry_type_t metadata_sai_fdb_entry_type_t_enum_values[] = {
    SAI_FDB_ENTRY_DYNAMIC,
    SAI_FDB_ENTRY_STATIC,
};
const char* metadata_sai_fdb_entry_type_t_enum_values_names[] = {
    "SAI_FDB_ENTRY_TYPE_DYNAMIC",
    "SAI_FDB_ENTRY_TYPE_STATIC",
    NULL
};
const char* metadata_sai_fdb_entry_type_t_enum_values_short_names[] = {
    "DYNAMIC",
    "STATIC",
    NULL
};
const size_t metadata_sai_fdb_entry_type_t_enum_values_count = 2;
DEFINE_ENUM_METADATA(sai_fdb_entry_type_t, 2);

const sai_attr_metadata_t sai_fdb_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_FDB,
        .attrid                 = SAI_FDB_ENTRY_ATTR_TYPE,
        .attridname             = "SAI_FDB_ENTRY_ATTR_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_fdb_entry_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_fdb_entry_type_t ),
        .enummetadata           = &metadata_enum_sai_fdb_entry_type_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_FDB,
        .attrid                 = SAI_FDB_ENTRY_ATTR_PORT_ID,
        .attridname             = "SAI_FDB_ENTRY_ATTR_PORT_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT, SAI_OBJECT_TYPE_LAG, SAI_OBJECT_TYPE_TUNNEL },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // TODO some extra check may be required on tunnel
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_FDB,
        .attrid                 = SAI_FDB_ENTRY_ATTR_PACKET_ACTION,
        .attridname             = "SAI_FDB_ENTRY_ATTR_PACKET_ACTION",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PACKET_ACTION_FORWARD },
        .enumtypestr            = StringifyEnum ( sai_packet_action_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_action_t ),
        .enummetadata           = &metadata_enum_sai_packet_action_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_FDB,
        .attrid                 = SAI_FDB_ENTRY_ATTR_META_DATA,
        .attridname             = "SAI_FDB_ENTRY_ATTR_META_DATA",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_ATTR_RANGE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // TODO valid range is SAI_SWITCH_ATTR_FDB_DST_USER_META_DATA_RANGE
    },
};

const size_t sai_fdb_attr_metadata_count = sizeof(sai_fdb_attr_metadata)/sizeof(sai_attr_metadata_t);
