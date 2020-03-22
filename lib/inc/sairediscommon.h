#ifndef __SAIREDISCOMMON__
#define __SAIREDISCOMMON__

// TODO those defines are shared via syncd and sairedis

#define SYNCD_INIT_VIEW     "INIT_VIEW"
#define SYNCD_APPLY_VIEW    "APPLY_VIEW"
#define SYNCD_INSPECT_ASIC  "SYNCD_INSPECT_ASIC"
#define ASIC_STATE_TABLE    "ASIC_STATE"
#define TEMP_PREFIX         "TEMP_"

// Messages for processing queries from libsairedis to syncd
#define STRING_ATTR_ENUM_VALUES_CAPABILITY_QUERY        "attr_enum_values_capability_query"
#define STRING_ATTR_ENUM_VALUES_CAPABILITY_RESPONSE     "attr_enum_values_capability_response"
#define STRING_OBJECT_TYPE_GET_AVAILABILITY_QUERY       "object_type_get_availability_query"
#define STRING_OBJECT_TYPE_GET_AVAILABILITY_RESPONSE    "object_type_get_availability_response"

#endif // __SAIREDISCOMMON__
