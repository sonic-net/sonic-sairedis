#ifndef __SAI_META_H__
#define __SAI_META_H__

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <set>
#include <memory>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

#define MAX_LIST_COUNT 128

#define DEFAULT_LAYER2_FRAME_SIZE 1514

#define DEFAULT_VLAN_NUMBER 1
#define MINIMUM_VLAN_NUMBER 1
#define MAXIMUM_VLAN_NUMBER 4094

#define MINIMUM_L4_PORT_NUMBER 0x0
#define MAXIMUM_L4_PORT_NUMBER 0xFFFF

// should be 64 ?
#define MINIMUM_PACKET_SIZE 0
#define MAXIMUM_PACKET_SIZE 0x10000

#ifndef SAI_QUEUE_ATTR_INDEX
#define SAI_QUEUE_ATTR_INDEX 0x100 // TODO remove on new SAI
#endif

#define StringifyEnum(x) ((std::is_enum<x>::value) ? #x : 0)

typedef enum _sai_attr_serialization_type_t
{
    SAI_SERIALIZATION_TYPE_BOOL,
    SAI_SERIALIZATION_TYPE_CHARDATA,
    SAI_SERIALIZATION_TYPE_UINT8,
    SAI_SERIALIZATION_TYPE_INT8,
    SAI_SERIALIZATION_TYPE_UINT16,
    SAI_SERIALIZATION_TYPE_INT16,
    SAI_SERIALIZATION_TYPE_UINT32,
    SAI_SERIALIZATION_TYPE_INT32,
    SAI_SERIALIZATION_TYPE_UINT64,
    SAI_SERIALIZATION_TYPE_INT64,
    SAI_SERIALIZATION_TYPE_MAC,
    SAI_SERIALIZATION_TYPE_IP4,
    SAI_SERIALIZATION_TYPE_IP6,
    SAI_SERIALIZATION_TYPE_IP_ADDRESS,
    SAI_SERIALIZATION_TYPE_OBJECT_ID,
    SAI_SERIALIZATION_TYPE_OBJECT_LIST,
    SAI_SERIALIZATION_TYPE_UINT8_LIST,
    SAI_SERIALIZATION_TYPE_INT8_LIST,
    SAI_SERIALIZATION_TYPE_UINT16_LIST,
    SAI_SERIALIZATION_TYPE_INT16_LIST,
    SAI_SERIALIZATION_TYPE_UINT32_LIST,
    SAI_SERIALIZATION_TYPE_INT32_LIST,
    SAI_SERIALIZATION_TYPE_UINT32_RANGE,
    SAI_SERIALIZATION_TYPE_INT32_RANGE,
    SAI_SERIALIZATION_TYPE_VLAN_LIST,

    SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_BOOL,
    SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8,
    SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT8,
    SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16,
    SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT16,
    SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32,
    SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32,
    SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_MAC,
    SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP4,
    SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP6,
    SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID,
    SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST,
    SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8_LIST,

    SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8,
    SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT8,
    SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT16,
    SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT16,
    SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT32,
    SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32,
    SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_MAC,
    SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV4,
    SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV6,
    SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID,
    SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST,

    SAI_SERIALIZATION_TYPE_PORT_BREAKOUT,
    SAI_SERIALIZATION_TYPE_QOS_MAP_LIST,
    SAI_SERIALIZATION_TYPE_TUNNEL_MAP_LIST,
    SAI_SERIALIZATION_TYPE_ACL_CAPABILITY

} sai_attr_serialization_type_t;

typedef struct _sai_object_meta_key_t
{
    sai_object_type_t             object_type;

    union
    {
        sai_object_id_t           object_id;
        sai_vlan_id_t             vlan_id;
        sai_hostif_trap_id_t      trap_id;
        sai_fdb_entry_t           fdb_entry;
        sai_neighbor_entry_t      neighbor_entry;
        sai_unicast_route_entry_t route_entry;

    } key;

} sai_object_meta_key_t;

typedef enum _sai_attr_flags_t {

    /*
     * Attribute with this flags is mandatory when calling CREATE api, unless
     * this attribute is marked as conditional. Must be combined with
     * CREATE_ONLY or CREATE_AND_SET flag.
     */

    SAI_ATTR_FLAGS_MANDATORY_ON_CREATE = (1 << 0),

    /*
     * Attribute with this flag can only be created and it's value cannot be
     * changed by SET api. Can be combined with with MANDATORY flag. If
     * attribute is not combined with MANDATORY flag then DEFAULT value must be
     * provided for this attribute.
     */

    SAI_ATTR_FLAGS_CREATE_ONLY         = (1 << 1),

    /*
     * Attribute with this flag can be created and after creation value may be
     * modified using SET api. Can be bombined with MANDATORY flag. If
     * attribute is not combined with MANDATORY flag then DEFAULT value must be
     * provided for this attribute.
     */

    SAI_ATTR_FLAGS_CREATE_AND_SET      = (1 << 2),

    /*
     * Attribute with this flag can only be read using GET api. Creation and
     * modification is not possible. Can be combined with DYNAMIC flag for
     * example counter attribute.
     */

    SAI_ATTR_FLAGS_READ_ONLY           = (1 << 3),

    /*
     * Attribute with this flag is treated as unique key (can only be combined
     * with MANDATORY and CREATE_ONLY flags. This flag will indicate that
     * creating new object with the same key will fail (for example VLAN).
     * There may be more than one key in attributes when creating object. Key
     * should be used only on primitive attribute values (like enum or int).
     * In some cases it may be supported on list (for port lanes) but then
     * extra logic is needed to compute and handle that key.
     *
     * If multiple keys are provided, meta key is created as combination of
     * keys in order attribute id's are declared (internal details).
     */

    SAI_ATTR_FLAGS_KEY                 = (1 << 4),

    /*
     * Attribute with this flag indicates that value of the attribute is
     * dynamic and can change in time (like a attribute counter value, or port
     * operational status). Change may happen independently or when other
     * attribute was created or modified (creating vlan member will change vlan
     * member list). Can be combined with READ_ONLY flag. In future it maybe
     * possible to combine this attribute with other flags.
     */

    //SAI_ATTR_FLAGS_DYNAMIC             = (1 << 5),

    /* Below flags are subject to be removed */

    /*
     * Attribute with this flag will indicate that this attribute is special
     * and it needs extended logic to be handled. This flag can only be
     * standalone. This flag may be removed in the future.
     *
     * NOTE: we could assume that this logic will skip generic validation
     * on attribute that has this flag.
     */

    //SAI_ATTR_FLAGS_SPECIAL             = (1 << 6),

    /*
     * Attribute with this flag can be SET only. This flag is subject to be
     * removed since SAI api spec is changing. This attribute must be
     * standalone.
     */

    //SAI_ATTR_FLAGS_SET_ONLY            = (1 << 7),

    /*
     * Attribute with this flag will indicate that this attribute is acl entry
     * field. This flag is subject to be removed since it can be deduced from
     * serialization type.
     */

    //SAI_ATTR_FLAGS_ACL_FIELD           = (1 << 8),

    /*
     * Attribute with this flag will indicate that this attribute is acl entry
     * action. This flag is subject to be removed since it can be deduced from
     * serialization type.
     */
    //SAI_ATTR_FLAGS_ACL_ACTION          = (1 << 9),

} sai_attr_flags_t;

#define HAS_FLAG_MANDATORY_ON_CREATE(x)   ((x & SAI_ATTR_FLAGS_MANDATORY_ON_CREATE) == SAI_ATTR_FLAGS_MANDATORY_ON_CREATE)
#define HAS_FLAG_CREATE_ONLY(x)           ((x & SAI_ATTR_FLAGS_CREATE_ONLY) == SAI_ATTR_FLAGS_CREATE_ONLY)
#define HAS_FLAG_CREATE_AND_SET(x)        ((x & SAI_ATTR_FLAGS_CREATE_AND_SET) == SAI_ATTR_FLAGS_CREATE_AND_SET)
#define HAS_FLAG_READ_ONLY(x)             ((x & SAI_ATTR_FLAGS_READ_ONLY) == SAI_ATTR_FLAGS_READ_ONLY)
#define HAS_FLAG_KEY(x)                   ((x & SAI_ATTR_FLAGS_KEY) == SAI_ATTR_FLAGS_KEY)
#define HAS_FLAG_DYNAMIC(x)               ((x & SAI_ATTR_FLAGS_DYNAMIC) == SAI_ATTR_FLAGS_DYNAMIC)
#define HAS_FLAG_SPECIAL(x)               ((x & SAI_ATTR_FLAGS_SPECIAL) == SAI_ATTR_FLAGS_SPECIAL)
#define HAS_FLAG_SET_ONLY(x)              ((x & SAI_ATTR_FLAGS_SET_ONLY) == SAI_ATTR_FLAGS_SET_ONLY)
#define HAS_FLAG_ACL_FIELD(x)             ((x & SAI_ATTR_FLAGS_ACL_FIELD) == SAI_ATTR_FLAGS_ACL_FIELD)
#define HAS_FLAG_ACL_ACTION(x)            ((x & SAI_ATTR_FLAGS_ACL_ACTION) == SAI_ATTR_FLAGS_ACL_ACTION)

/*
 * Inline operator is required to not cause error when combining flags.
 * Error is raised by compiler flags: -Werror -fpermissive
 */

constexpr inline sai_attr_flags_t operator|(sai_attr_flags_t a, sai_attr_flags_t b)
{
    return static_cast<sai_attr_flags_t>(static_cast<int>(a) | static_cast<int>(b));
}

typedef enum _sai_default_value_type_t {

    /*
     * There is no default value.
     * This must be assigned on MANDATORY_ON_CREATE
     * attributes.
     */

    SAI_DEFAULT_VALUE_TYPE_NONE = 0,

    /*
     * Default value is just a const value.
     */

    SAI_DEFAULT_VALUE_TYPE_CONST,

    /*
     * Value must be in range provided by other attribute
     * (usually by SWITCH). Range can be obtined by GET api.
     * Usually default value is minimum of range.
     */

    SAI_DEFAULT_VALUE_TYPE_ATTR_RANGE,

    /*
     * Default value in this case is equal to other
     * attribute value (usually SWITCH attribute).
     */

    SAI_DEFAULT_VALUE_TYPE_ATTR_VALUE,

    /*
     * Default value is just empty list.
     * We introduce this type to not declare empty list each time.
     * TODO this can be obvious if attribute is
     * serialization type list, and pointer in list is NULL
     */

    SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,

    /*
     * This value is assigned by switch vendor
     * like default switch MAC address.
     *
     * It can also be default created object
     * like default hash.
     *
     * TODO vendor specific should be different
     * than default objects that are created
     * by default.
     */

    SAI_DEFAULT_VALUE_TYPE_VENDOR_SPECIFIC,

    /*
     * This object is created by default
     * inside switch (hidden object).
     *
     * Should be used only on object id types.
     */
    SAI_DEFAULT_VALUE_TYPE_SWITCH_INTERNAL,

    /*
     * If attribute is not specified, then default value
     * is inherited from other object (type + attr).
     */

    SAI_DEFAULT_VALUE_TYPE_INHERIT,

    // TODO
    // - const assigned by switch
    // - min in range assigned by switch
    // - max in range assigned by switch
    // - default created object assigned by switch

} sai_default_value_type_t;

typedef enum _sai_attr_condition_type_t {

    /*
     * This attribute is not conditional atttribute
     */

    SAI_ATTR_CONDITION_TYPE_NONE = 0,

    /*
     * Any condition that will be true will make
     * this attribute mandatory.
     */

    SAI_ATTR_CONDITION_TYPE_OR,

    /*
     * All condictions must meet for this attribute
     * to be mandatory on create.
     */

    SAI_ATTR_CONDITION_TYPE_AND,

} sai_attr_condition_type_t;

typedef struct _sai_attr_condition_t {

    /*
     * Specifies valid attribute id for this object type.
     * Attribute is for the same object type.
     */

    const sai_attr_id_t                       attrid;

    /*
     * Condition value that attribute will be mandatory
     * then default value must be provided for attribute.
     *
     * We would like to use sai_attribute_value_t here
     * but there is gcc bug:
     *
     * internal compiler error: in reshape_init_class, at cp/decl.c:5208
     *
     * when union is used, so we declare our own value class
     * withoud uniotn. Unfortunetly this will lead to populate
     * all fields in struct even if not used.
     *
     */

    const sai_attribute_value_t               condition;

    const char * const                        enumstr;

    // TODO for enums we could have a

} sai_attr_condition_t;

// TODO we need string representations of:
// - object types
// - attributes id's
// - enum types
// - enum values

// TODO add extra fields that maybe useful
// bool allowrepetitiononlist; (false by default)
// bool allowmixedobjecttypes; (false by default)
// bool allowemptylist; (false by default)
// bool hasMandatoryAttribytes (auto set)
// bool hasContitionalAttributes (auto set)
// bool hasValidOnlyAttributes (auto set)
// bool snoopsave (for readonly attributes it will be stored in local db
// also must be ignored on post set/remve (check flags) so
// we could validate ranges for example what are valid when doing query

/**
 * @brief Defines enum metadata information.
 */
typedef struct _sai_enum_metadata_t {

    /**
     * @brief String representation of enum typedef.
     */
    const char*     name;

    /**
     * @brief Values count in enum.
     */
    const size_t    valuescount;

    /**
     * @brief Array of enum values.
     */
    const int*      values;

    /**
     * @brief Array of enum values string names.
     */
    const char**    valuesnames;

    /**
     * @brief Array of enum values string short names.
     */
    const char**    valuesshortnames;

} sai_enum_metadata_t;

typedef struct _sai_attr_metadata_t {

    /*
     * Specifies valid SAI object type.
     */

    const sai_object_type_t                   objecttype;

    /*
     * Specifies valid attribute id for this object type.
     */

    const sai_attr_id_t                       attrid;

    /**
     * Specifies valid attribute id name for this object type.
     */
    const char*                               attridname;

    /*
     * Specifies serialization type for this attribute.
     */

    const sai_attr_serialization_type_t       serializationtype;

    /*
     * Specifies flags for this attribute.
     */

    const sai_attr_flags_t                    flags;

    // TODO we should have flag - allow list duplicates

    /*
     * If object serialization type is OBJECT_ID
     * this list specifies what object type can be used.
     *
     * TODO if list contains SAI_NULL_OBJECT_ID then
     * it can be used as actual object.
     */

    //const std::vector<sai_object_type_t>&allowedobjecttypes;
    const std::set<sai_object_type_t>         allowedobjecttypes;

    /*
     * If object serialization type is OBJECT_ID
     * it tells whether SAI_NULL_OBJECT_ID can be used
     * as actual id.
     *
     * TODO could be passed as SAI_NULL_OBJECT_ID to list
     * instead of this flag but then we would need extra
     * check on checking types.
     */

    const bool                                allownullobjectid;

    /*
     * Specifies default value type. Since default value
     * can be a const value, a const assigned by switch
     * (which is not know at compile time, but can be
     * obtained by GET api, or a min/max value in specific
     * range also assigned by switch at runtime.
     * Also default value can be an object
     */

    const sai_default_value_type_t            defaultvaluetype;

    /*
     * TODO if value type is ATTR type, then we need to
     * specify object_id and attr_id from which we need
     * to get value.
     */

    /*
     * If creation flag is CREATE_ONLY or CREATE_AND_SET
     * then default value must be provided for attribute.
     *
     * NOTE: default value may not apply for:
     *  - ACL ENTRY FIELD or ACL ENTRY ACTION
     *  we need to take special care about those
     */

    const sai_attribute_value_t               defaultvalue;

    /*
     * Indicates wheter attribute is enum value.
     * Serialization type must be INT32.
     *
     * NOTE: could be deduced from enum type string or
     * enum vector values and serialization type.
     */

    // const bool                                isenum;

    /*
     * Indicates wheter attribute is enum list value.
     * Serialization type must be INT32_LIST.
     *
     * NOTE: could be deduced from enum type string or
     * enum vector values and serialization type.
     */

    // const bool                                isenumlist;

    /*
     * If attribute is enum, this entry should provide
     * a string name of enum type.
     */

    const char* const                         enumtypestr;

    /*
     * If attribute is enum this set will specify
     * all possible values for this enum type.
     *
     * NOTE: reference here is important since enum
     * sets are declared in only one cpp file and
     * init is done only at compile time.
     */

    const std::set<int32_t>&                  enumallowedvalues;

    /*
     * If attribute is enum value, then this will
     * hold enum metadata.
     */
    const sai_enum_metadata_t* const          enummetadata;

    /*
     * Specifies condition type of attribute.
     *
     * NOTE: currently all conditions are "OR" conditions
     * so we can deduce if this is conditional type
     * if any conditions are defined.
     */

    //const sai_attr_condition_type_t           conditiontype;

    /*
     * If condition type is specified, this vector will
     * describe all conditions.
     */

    const std::vector<sai_attr_condition_t>   conditions;

    /*
     * If conditions are specified (OR condition assumed)
     * then this attribute is only valid when different
     * atribute has condition value set. Valid only
     * attribute (against we check) can be dynamic so
     * this attribute can't be marked as MANDATORY on
     * create since default value will be required.
     *
     * NOTE: There is only handful of attributes with
     * valid only mark. For now we will check that in
     * specific attribute logic.
     */

    // const std::vector<sai_attr_condition_t>   validonlywhen;

    // HELPER METHODS

    bool isconditional() const
    {
        return !conditions.empty();
    }

    bool isenum() const
    {
        return (serializationtype == SAI_SERIALIZATION_TYPE_INT32 ||
                serializationtype == SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32 ||
                serializationtype == SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32) &&
               (!enumallowedvalues.empty()) &&
               (enumtypestr != NULL);
    }

    bool isenumlist() const
    {
        return (serializationtype == SAI_SERIALIZATION_TYPE_INT32_LIST) &&
               (!enumallowedvalues.empty()) &&
               (enumtypestr != NULL);
    }

    bool isvlan() const
    {
        // TODO would be better to be a field

        if (objecttype == SAI_OBJECT_TYPE_VLAN_MEMBER &&
                attrid == SAI_VLAN_MEMBER_ATTR_VLAN_ID)
        {
            return true;
        }

        if (objecttype == SAI_OBJECT_TYPE_ROUTER_INTERFACE &&
                attrid == SAI_ROUTER_INTERFACE_ATTR_VLAN_ID)
        {
            return true;
        }


        if (objecttype == SAI_OBJECT_TYPE_PORT &&
                attrid == SAI_PORT_ATTR_PORT_VLAN_ID)
        {
            return true;
        }

        if (objecttype == SAI_OBJECT_TYPE_MIRROR &&
                attrid == SAI_MIRROR_SESSION_ATTR_VLAN_ID)
        {
            return true;
        }

        // has different serialization than u16
        // SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_ID
        // SAI_ACL_ENTRY_ATTR_FIELD_INNER_VLAN_ID
        // SAI_ACL_ENTRY_ATTR_ACTION_SET_INNER_VLAN_ID
        // SAI_ACL_ENTRY_ATTR_ACTION_SET_OUTER_VLAN_ID

        return false;
    }

    ~_sai_attr_metadata_t() { }

} sai_attr_metadata_t;

#define ENUM_VALUES(x) enum_ ## x ## _values
#define DEFINE_ENUM_VALUES(x) const std::set<int32_t> ENUM_VALUES(x)
#define EXTERN_ENUM_VALUES(x) extern DEFINE_ENUM_VALUES(x)

#define COND_ENUM(id,value) { .attrid = id, .condition = sai_attribute_value_t { .s32 = value }, .enumstr = #value }
#define COND_BOOL(id,value) { .attrid = id, .condition = sai_attribute_value_t { .booldata = value }, .enumstr = NULL }

EXTERN_ENUM_VALUES(sai_packet_action_t);
EXTERN_ENUM_VALUES(sai_meter_type_t);
EXTERN_ENUM_VALUES(sai_packet_color_t);
EXTERN_ENUM_VALUES(sai_hostif_trap_id_t);

#define EXTERN_METADATA(x)\
extern const sai_attr_metadata_t sai_ ## x ## _attr_metadata[];\
extern const size_t sai_ ## x ## _attr_metadata_count;

EXTERN_METADATA(acl_counter);
EXTERN_METADATA(acl_entry);
EXTERN_METADATA(acl_range);
EXTERN_METADATA(acl_table);
EXTERN_METADATA(buffer_pool);
EXTERN_METADATA(buffer_profile);
EXTERN_METADATA(fdb);
EXTERN_METADATA(hash);
EXTERN_METADATA(hostintf);
EXTERN_METADATA(hostintf_trap);
EXTERN_METADATA(hostintf_trap_group);
EXTERN_METADATA(lag);
EXTERN_METADATA(lag_member);
EXTERN_METADATA(mirror);
EXTERN_METADATA(neighbor);
EXTERN_METADATA(nexthop);
EXTERN_METADATA(nexthopgroup);
EXTERN_METADATA(policer);
EXTERN_METADATA(port);
EXTERN_METADATA(priority_group);
EXTERN_METADATA(qos_maps);
EXTERN_METADATA(queue);
EXTERN_METADATA(route);
EXTERN_METADATA(router);
EXTERN_METADATA(routerintf);
EXTERN_METADATA(samplepacket);
EXTERN_METADATA(scheduler);
EXTERN_METADATA(scheduler_group);
EXTERN_METADATA(stp);
EXTERN_METADATA(switch);
EXTERN_METADATA(tunnel);
EXTERN_METADATA(tunnel_map);
EXTERN_METADATA(tunnel_table_entry);
EXTERN_METADATA(udf);
EXTERN_METADATA(udf_group);
EXTERN_METADATA(udf_match);
EXTERN_METADATA(vlan);
EXTERN_METADATA(vlan_member);
EXTERN_METADATA(wred);

#define DEFINE_ENUM_METADATA(x,count)\
const sai_enum_metadata_t metadata_enum_ ## x = {\
    .name              = metadata_ ## x ## _enum_name,\
    .valuescount       = count,\
    .values            = (const int*)metadata_ ## x ## _enum_values,\
    .valuesnames       = metadata_ ## x ## _enum_values_names,\
    .valuesshortnames  = metadata_ ## x ## _enum_values_short_names,\
};


extern const sai_enum_metadata_t metadata_enum_sai_switch_oper_status_t;
extern const sai_enum_metadata_t metadata_enum_sai_port_oper_status_t;
extern const sai_enum_metadata_t metadata_enum_sai_status_t;
extern const sai_enum_metadata_t metadata_enum_sai_fdb_event_t;
extern const sai_enum_metadata_t metadata_enum_sai_object_type_t;
extern const sai_enum_metadata_t metadata_enum_sai_port_event_t;
extern const sai_enum_metadata_t metadata_enum_sai_packet_color_t;
extern const sai_enum_metadata_t metadata_enum_sai_packet_action_t;
extern const sai_enum_metadata_t metadata_enum_sai_next_hop_group_type_t;
extern const sai_enum_metadata_t metadata_enum_sai_meter_type_t;
extern const sai_enum_metadata_t metadata_enum_sai_hostif_trap_type_t;
extern const sai_enum_metadata_t metadata_enum_sai_port_stat_t;

struct HashForEnum
{
    template <typename T> std::size_t operator()(T t) const
    {
        return static_cast<std::size_t>(t);
    }
};

// TODO those should be internal only
extern std::unordered_map<sai_object_type_t,std::unordered_map<sai_attr_id_t, const sai_attr_metadata_t*>, HashForEnum> AttributesMetadata;
extern std::unordered_map<std::string,const sai_attr_metadata_t*> AttributesIdMetadata;

extern std::string get_attr_info(const sai_attr_metadata_t& md);
extern const char* get_object_type_name(sai_object_type_t o);
extern const char* get_attr_name(sai_object_type_t o, sai_attr_id_t a);

extern const sai_attribute_t* get_attribute_by_id(
        _In_ sai_attr_id_t id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

extern const sai_attr_metadata_t* get_attribute_metadata(
        _In_ sai_object_type_t objecttype,
        _In_ sai_attr_id_t attrid);

extern const sai_attribute_t* get_object_previous_attr(
        _In_ const sai_object_meta_key_t meta_key,
        _In_ const sai_attr_metadata_t& md);

extern sai_status_t meta_init_db();
extern void meta_init();


// GENERIC FUNCTION POINTERS

typedef sai_status_t (*sai_create_generic_fn)(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t* object_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

typedef sai_status_t (*sai_remove_generic_fn)(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id);

typedef sai_status_t (*sai_set_generic_attribute_fn)(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_attribute_t *attr);

typedef sai_status_t (*sai_get_generic_attribute_fn)(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list);

// META GENERIC

extern sai_status_t meta_sai_create_oid(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t* object_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_create_generic_fn create);

extern sai_status_t meta_sai_remove_oid(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ sai_remove_generic_fn remove);

extern sai_status_t meta_sai_set_oid(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_generic_attribute_fn set);

extern sai_status_t meta_sai_get_oid(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_generic_attribute_fn get);

// META TRAP

extern sai_status_t meta_sai_set_trap(
        _In_ sai_hostif_trap_id_t hostif_trapid,
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_hostif_trap_attribute_fn set);

extern sai_status_t meta_sai_get_trap(
        _In_ sai_hostif_trap_id_t hostif_trapid,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_hostif_trap_attribute_fn get);

// META FDB

extern sai_status_t meta_sai_create_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_create_fdb_entry_fn create);

extern sai_status_t meta_sai_remove_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ sai_remove_fdb_entry_fn remove);

extern sai_status_t meta_sai_set_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_fdb_entry_attribute_fn set);

extern sai_status_t meta_sai_get_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_fdb_entry_attribute_fn get);

// META NEIGHBOR

extern sai_status_t meta_sai_create_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_create_neighbor_entry_fn create);

extern sai_status_t meta_sai_remove_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ sai_remove_neighbor_entry_fn remove);

extern sai_status_t meta_sai_set_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_neighbor_attribute_fn set);

extern sai_status_t meta_sai_get_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_neighbor_attribute_fn get);

// META SWITCH

extern sai_status_t meta_sai_set_switch(
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_switch_attribute_fn set);

extern sai_status_t meta_sai_get_switch(
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_switch_attribute_fn get);

// META VLAN

extern sai_status_t meta_sai_create_vlan(
        _In_ sai_vlan_id_t vlan_id,
        _In_ sai_create_vlan_fn create);

extern sai_status_t meta_sai_remove_vlan(
        _In_ sai_vlan_id_t vlan_id,
        _In_ sai_remove_vlan_fn remove);

extern sai_status_t meta_sai_set_vlan(
        _In_ sai_vlan_id_t vlan_id,
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_vlan_attribute_fn set);

extern sai_status_t meta_sai_get_vlan(
        _In_ sai_vlan_id_t vlan_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_vlan_attribute_fn get);

// META ROUTE

extern sai_status_t meta_sai_create_route_entry(
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_create_route_fn create);

extern sai_status_t meta_sai_remove_route_entry(
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ sai_remove_route_fn remove);

extern sai_status_t meta_sai_set_route_entry(
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ const sai_attribute_t *attr,
        _In_ sai_set_route_attribute_fn set);

extern sai_status_t meta_sai_get_route_entry(
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list,
        _In_ sai_get_route_attribute_fn get);

#endif // __SAI_META_H__
