#include "sai_redis.h"

#define MINIMUM_L4_PORT_NUMBER 0x0
#define MAXIMUM_L4_PORT_NUMBER 0xFFFF

// shoulg be 64 ?
#define MINIMUM_PACKET_SIZE 0
#define MAXIMUM_PACKET_SIZE 0x10000

std::set<sai_object_id_t> local_acl_tables_set;
std::set<sai_object_id_t> local_acl_entries_set;
std::set<sai_object_id_t> local_acl_counters_set;
std::set<sai_object_id_t> local_acl_ranges_set;
std::set<sai_object_id_t> local_acl_table_groups_set;

/**
 * Routine Description:
 *   @brief Create an ACL table
 *
 * Arguments:
 *   @param[out] acl_table_id - the the acl table id
 *   @param[in] attr_count - number of attributes
 *   @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_create_acl_table(
    _Out_ sai_object_id_t* acl_table_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 2)
    {
        SWSS_LOG_ERROR("attribute count must be at least 2");

        // SAI_ACL_TABLE_ATTR_STAGE
        // SAI_ACL_TABLE_ATTR_PRIORITY

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_stage = redis_get_attribute_by_id(SAI_ACL_TABLE_ATTR_STAGE, attr_count, attr_list);
    const sai_attribute_t* attr_priority = redis_get_attribute_by_id(SAI_ACL_TABLE_ATTR_PRIORITY, attr_count, attr_list);

    if (attr_stage == NULL)
    {
        SWSS_LOG_ERROR("missing stage attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_acl_stage_t stage = (sai_acl_stage_t)attr_stage->value.s32;

    switch (stage)
    {
        case SAI_ACL_STAGE_INGRESS:
        case SAI_ACL_STAGE_EGRESS:
        case SAI_ACL_SUBSTAGE_INGRESS_PRE_L2:
        case SAI_ACL_SUBSTAGE_INGRESS_POST_L3:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid acl stage value: %d", stage);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_priority == NULL)
    {
        SWSS_LOG_ERROR("missing priority attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    uint32_t priority = attr_priority->value.u32;

    // TODO range is in
    // SAI_SWITCH_ATTR_ACL_TABLE_MINIMUM_PRIORITY .. SAI_SWITCH_ATTR_ACL_TABLE_MAXIMUM_PRIORITY
    // which is obtained from swich as RO attribute
    // extra validation will be needed here

    SWSS_LOG_DEBUG("acl priority: %d", priority);

    const sai_attribute_t* attr_size = redis_get_attribute_by_id(SAI_ACL_TABLE_ATTR_SIZE, attr_count, attr_list);

    if (attr_size != NULL)
    {
        uint32_t size = attr_size->value.u32; // default value is zero

        SWSS_LOG_DEBUG("size %u", size);

        // TODO this attribute is special, since it can change dynamically, but it can be
        // set on creation time and grow when entries are added
        // why to allow to create it ?
    }

    const sai_attribute_t* attr_group_id = redis_get_attribute_by_id(SAI_ACL_TABLE_ATTR_GROUP_ID, attr_count, attr_list);

    // TODO group ID is special, since when not specified it's created automatically
    // by switch and user can obtain it via GET api and group tables.
    // This behaviour should be changed and so no object would be created internally
    // there is a tricky way to track this object usage

    sai_object_id_t group_id = SAI_NULL_OBJECT_ID; // default value

    if (attr_group_id != NULL)
    {
        group_id = attr_group_id->value.oid;
    }

    if (group_id != SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("currently not supported to set group ID");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // ACL fields - at least one field must be passed - all fields are bool (on/off)
    // except user defined field (which is null by default)

    uint32_t fields[] = {
            SAI_ACL_TABLE_ATTR_FIELD_SRC_IPv6,
            SAI_ACL_TABLE_ATTR_FIELD_DST_IPv6,
            SAI_ACL_TABLE_ATTR_FIELD_SRC_MAC,
            SAI_ACL_TABLE_ATTR_FIELD_DST_MAC,
            SAI_ACL_TABLE_ATTR_FIELD_SRC_IP,
            SAI_ACL_TABLE_ATTR_FIELD_DST_IP,
            SAI_ACL_TABLE_ATTR_FIELD_IN_PORTS,
            SAI_ACL_TABLE_ATTR_FIELD_OUT_PORTS,
            SAI_ACL_TABLE_ATTR_FIELD_IN_PORT,
            SAI_ACL_TABLE_ATTR_FIELD_OUT_PORT,
            SAI_ACL_TABLE_ATTR_FIELD_SRC_PORT,
            SAI_ACL_TABLE_ATTR_FIELD_OUTER_VLAN_ID,
            SAI_ACL_TABLE_ATTR_FIELD_OUTER_VLAN_PRI,
            SAI_ACL_TABLE_ATTR_FIELD_OUTER_VLAN_CFI,
            SAI_ACL_TABLE_ATTR_FIELD_INNER_VLAN_ID,
            SAI_ACL_TABLE_ATTR_FIELD_INNER_VLAN_PRI,
            SAI_ACL_TABLE_ATTR_FIELD_INNER_VLAN_CFI,
            SAI_ACL_TABLE_ATTR_FIELD_L4_SRC_PORT,
            SAI_ACL_TABLE_ATTR_FIELD_L4_DST_PORT,
            SAI_ACL_TABLE_ATTR_FIELD_ETHER_TYPE,
            SAI_ACL_TABLE_ATTR_FIELD_IP_PROTOCOL,
            SAI_ACL_TABLE_ATTR_FIELD_DSCP,
            SAI_ACL_TABLE_ATTR_FIELD_ECN,
            SAI_ACL_TABLE_ATTR_FIELD_TTL,
            SAI_ACL_TABLE_ATTR_FIELD_TOS,
            SAI_ACL_TABLE_ATTR_FIELD_IP_FLAGS,
            SAI_ACL_TABLE_ATTR_FIELD_TCP_FLAGS,
            SAI_ACL_TABLE_ATTR_FIELD_IP_TYPE,
            SAI_ACL_TABLE_ATTR_FIELD_IP_FRAG,
            SAI_ACL_TABLE_ATTR_FIELD_IPv6_FLOW_LABEL,
            SAI_ACL_TABLE_ATTR_FIELD_TC,
            SAI_ACL_TABLE_ATTR_FIELD_ICMP_TYPE,
            SAI_ACL_TABLE_ATTR_FIELD_ICMP_CODE,
            SAI_ACL_TABLE_ATTR_FIELD_VLAN_TAGS,
            SAI_ACL_TABLE_ATTR_FIELD_FDB_DST_USER_META,
            SAI_ACL_TABLE_ATTR_FIELD_ROUTE_DST_USER_META,
            SAI_ACL_TABLE_ATTR_FIELD_NEIGHBOR_DST_USER_META,
            SAI_ACL_TABLE_ATTR_FIELD_PORT_USER_META,
            SAI_ACL_TABLE_ATTR_FIELD_VLAN_USER_META,
            SAI_ACL_TABLE_ATTR_FIELD_ACL_USER_META,
            SAI_ACL_TABLE_ATTR_FIELD_FDB_NPU_META_DST_HIT,
            SAI_ACL_TABLE_ATTR_FIELD_NEIGHBOR_NPU_META_DST_HIT,
            SAI_ACL_TABLE_ATTR_FIELD_ROUTE_NPU_META_DST_HIT };

    // when acl table is created there is a requirement
    // that at least one (1) field should be present

    bool fields_present = false;

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        for (size_t j = 0; j < sizeof(fields)/sizeof(uint32_t); ++j)
        {
            if (attr_list[i].id == fields[j])
            {
               if (attr_list[i].value.booldata)
               {
                   fields_present = true;

                   i = attr_count; // break all loops
                   break;
               }
            }
        }
    }

    if (fields_present == false)
    {
        SWSS_LOG_ERROR("at least one field must present");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO support user defined field group (sai_object_id_t - what object type can be assigned here?)
    // TODO - also this should be converted to list, instead of range

    // TODO is this mandatory on create? what is the default value
    const sai_attribute_t* attr_range = redis_get_attribute_by_id(SAI_ACL_TABLE_ATTR_FIELD_RANGE, attr_count, attr_list);

    // TODO should this come from acl Field data ?
    if (attr_range != NULL)
    {
        sai_acl_range_type_t range = (sai_acl_range_type_t)attr_range->value.s32;

        switch (range)
        {
            case SAI_ACL_RANGE_L4_SRC_PORT_RANGE:
            case SAI_ACL_RANGE_L4_DST_PORT_RANGE:
            case SAI_ACL_RANGE_OUTER_VLAN:
            case SAI_ACL_RANGE_INNER_VLAN:
            case SAI_ACL_RANGE_PACKET_LENGTH:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("invalid range type value: %d", range);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    // TODO another special attribute depending on switch SAI_SWITCH_ATTR_ACL_CAPABILITY
    // it may be mandatory on create, why we need to query attributes and then pass them here?
    // sdk logic can figure this out anyway

    // TODO const sai_attribute_t* attr_action_list = redis_get_attribute_by_id(SAI_ACL_TABLE_ATTR_ACTION_LIST, attr_count, attr_list);

    sai_status_t status = redis_generic_create(
        SAI_OBJECT_TYPE_ACL_TABLE,
        acl_table_id,
        attr_count,
        attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting acl table %llx to local state", *acl_table_id);

        local_acl_tables_set.insert(*acl_table_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Delete an ACL table
 *
 * Arguments:
 *    @param[in] acl_table_id - the acl table id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_delete_acl_table(
    _In_ sai_object_id_t acl_table_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove acl table (+table group)

    if (local_acl_tables_set.find(acl_table_id) == local_acl_tables_set.end())
    {
        SWSS_LOG_ERROR("acl table %llx is missing", acl_table_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
        SAI_OBJECT_TYPE_ACL_TABLE,
        acl_table_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing acl table %llx from local state", acl_table_id);

        local_acl_tables_set.erase(acl_table_id);
    }

    return status;
}

/**
 * Routine Description:
 *   @brief Set ACL table attribute
 *
 * Arguments:
 *    @param[in] acl_table_id - the acl table id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_set_acl_table_attribute(
    _In_ sai_object_id_t acl_table_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_acl_tables_set.find(acl_table_id) == local_acl_tables_set.end())
    {
        SWSS_LOG_ERROR("acl table %llx is missing", acl_table_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        // case SAI_ACL_TABLE_ATTR_SIZE: // can size be changed dynamically ?
        // currently next hop don't have attributes that can be set
        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_ACL_TABLE,
            acl_table_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *   @brief Get ACL table attribute
 *
 * Arguments:
 *    @param[in] acl_table_id - acl table id
 *    @param[in] attr_count - number of attributes
 *    @param[out] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_get_acl_table_attribute(
    _In_ sai_object_id_t acl_table_id,
    _In_ uint32_t attr_count,
    _Out_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 1)
    {
        SWSS_LOG_ERROR("attribute count must be at least 1");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_acl_tables_set.find(acl_table_id) == local_acl_tables_set.end())
    {
        SWSS_LOG_ERROR("acl table %llx is missing", acl_table_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_ACL_TABLE_ATTR_STAGE:
            case SAI_ACL_TABLE_ATTR_PRIORITY:
            case SAI_ACL_TABLE_ATTR_SIZE:
            case SAI_ACL_TABLE_ATTR_GROUP_ID:

            case SAI_ACL_TABLE_ATTR_FIELD_SRC_IPv6:
            case SAI_ACL_TABLE_ATTR_FIELD_DST_IPv6:
            case SAI_ACL_TABLE_ATTR_FIELD_SRC_MAC:
            case SAI_ACL_TABLE_ATTR_FIELD_DST_MAC:
            case SAI_ACL_TABLE_ATTR_FIELD_SRC_IP:
            case SAI_ACL_TABLE_ATTR_FIELD_DST_IP:
            case SAI_ACL_TABLE_ATTR_FIELD_IN_PORTS:
            case SAI_ACL_TABLE_ATTR_FIELD_OUT_PORTS:
            case SAI_ACL_TABLE_ATTR_FIELD_IN_PORT:
            case SAI_ACL_TABLE_ATTR_FIELD_OUT_PORT:
            case SAI_ACL_TABLE_ATTR_FIELD_SRC_PORT:
            case SAI_ACL_TABLE_ATTR_FIELD_OUTER_VLAN_ID:
            case SAI_ACL_TABLE_ATTR_FIELD_OUTER_VLAN_PRI:
            case SAI_ACL_TABLE_ATTR_FIELD_OUTER_VLAN_CFI:
            case SAI_ACL_TABLE_ATTR_FIELD_INNER_VLAN_ID:
            case SAI_ACL_TABLE_ATTR_FIELD_INNER_VLAN_PRI:
            case SAI_ACL_TABLE_ATTR_FIELD_INNER_VLAN_CFI:
            case SAI_ACL_TABLE_ATTR_FIELD_L4_SRC_PORT:
            case SAI_ACL_TABLE_ATTR_FIELD_L4_DST_PORT:
            case SAI_ACL_TABLE_ATTR_FIELD_ETHER_TYPE:
            case SAI_ACL_TABLE_ATTR_FIELD_IP_PROTOCOL:
            case SAI_ACL_TABLE_ATTR_FIELD_DSCP:
            case SAI_ACL_TABLE_ATTR_FIELD_ECN:
            case SAI_ACL_TABLE_ATTR_FIELD_TTL:
            case SAI_ACL_TABLE_ATTR_FIELD_TOS:
            case SAI_ACL_TABLE_ATTR_FIELD_IP_FLAGS:
            case SAI_ACL_TABLE_ATTR_FIELD_TCP_FLAGS:
            case SAI_ACL_TABLE_ATTR_FIELD_IP_TYPE:
            case SAI_ACL_TABLE_ATTR_FIELD_IP_FRAG:
            case SAI_ACL_TABLE_ATTR_FIELD_IPv6_FLOW_LABEL:
            case SAI_ACL_TABLE_ATTR_FIELD_TC:
            case SAI_ACL_TABLE_ATTR_FIELD_ICMP_TYPE:
            case SAI_ACL_TABLE_ATTR_FIELD_ICMP_CODE:
            case SAI_ACL_TABLE_ATTR_FIELD_VLAN_TAGS:
            case SAI_ACL_TABLE_ATTR_FIELD_FDB_DST_USER_META:
            case SAI_ACL_TABLE_ATTR_FIELD_ROUTE_DST_USER_META:
            case SAI_ACL_TABLE_ATTR_FIELD_NEIGHBOR_DST_USER_META:
            case SAI_ACL_TABLE_ATTR_FIELD_PORT_USER_META:
            case SAI_ACL_TABLE_ATTR_FIELD_VLAN_USER_META:
            case SAI_ACL_TABLE_ATTR_FIELD_ACL_USER_META:
            case SAI_ACL_TABLE_ATTR_FIELD_FDB_NPU_META_DST_HIT:
            case SAI_ACL_TABLE_ATTR_FIELD_NEIGHBOR_NPU_META_DST_HIT:
            case SAI_ACL_TABLE_ATTR_FIELD_ROUTE_NPU_META_DST_HIT:

            case SAI_ACL_TABLE_ATTR_FIELD_RANGE:
                // ok
                break;

            // TODO what about action list ? how range is specified anyway?

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
        SAI_OBJECT_TYPE_ACL_TABLE,
        acl_table_id,
        attr_count,
        attr_list);

    return status;
}

/**
 * Routine Description:
 *   @brief Create an ACL entry
 *
 * Arguments:
 *   @param[out] acl_entry_id - the acl entry id
 *   @param[in] attr_count - number of attributes
 *   @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_create_acl_entry(
    _Out_ sai_object_id_t *acl_entry_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 1)
    {
        SWSS_LOG_ERROR("attribute count must be at least 1");

        // SAI_ACL_ENTRY_ATTR_TABLE_ID

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_table_id = redis_get_attribute_by_id(SAI_ACL_ENTRY_ATTR_TABLE_ID, attr_count, attr_list);

    if (attr_table_id == NULL)
    {
        SWSS_LOG_ERROR("missing table id attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t table_id = attr_table_id->value.oid;

    if (local_acl_tables_set.find(table_id) == local_acl_tables_set.end())
    {
        SWSS_LOG_ERROR("acl table %llx is missing", table_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_priority = redis_get_attribute_by_id(SAI_ACL_ENTRY_ATTR_PRIORITY, attr_count, attr_list);

    // TODO special, since range is obtained from switch from other attributes

    if (attr_priority != NULL)
    {
        // SAI_SWITCH_ATTR_ACL_ENTRY_MINIMUM_PRIORITY .. SAI_SWITCH_ATTR_ACL_ENTRY_MAXIMUM_PRIORITY
        // TODO validate priority
        SWSS_LOG_DEBUG("priority %u", attr_priority->value.u32);
    }

    // TODO admin state

    // TODO should fields on acl entry match fields in acl table ?

    // FIELDS

    uint32_t fields[] = {
            SAI_ACL_ENTRY_ATTR_FIELD_SRC_IPv6,
            SAI_ACL_ENTRY_ATTR_FIELD_DST_IPv6,
            SAI_ACL_ENTRY_ATTR_FIELD_SRC_MAC,
            SAI_ACL_ENTRY_ATTR_FIELD_DST_MAC,
            SAI_ACL_ENTRY_ATTR_FIELD_SRC_IP,
            SAI_ACL_ENTRY_ATTR_FIELD_DST_IP,
            SAI_ACL_ENTRY_ATTR_FIELD_IN_PORTS,
            SAI_ACL_ENTRY_ATTR_FIELD_OUT_PORTS,
            SAI_ACL_ENTRY_ATTR_FIELD_IN_PORT,
            SAI_ACL_ENTRY_ATTR_FIELD_OUT_PORT,
            SAI_ACL_ENTRY_ATTR_FIELD_SRC_PORT,
            SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_ID,
            SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_PRI,
            SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_CFI,
            SAI_ACL_ENTRY_ATTR_FIELD_INNER_VLAN_ID,
            SAI_ACL_ENTRY_ATTR_FIELD_INNER_VLAN_PRI,
            SAI_ACL_ENTRY_ATTR_FIELD_INNER_VLAN_CFI,
            SAI_ACL_ENTRY_ATTR_FIELD_L4_SRC_PORT,
            SAI_ACL_ENTRY_ATTR_FIELD_L4_DST_PORT,
            SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE,
            SAI_ACL_ENTRY_ATTR_FIELD_IP_PROTOCOL,
            SAI_ACL_ENTRY_ATTR_FIELD_DSCP,
            SAI_ACL_ENTRY_ATTR_FIELD_ECN,
            SAI_ACL_ENTRY_ATTR_FIELD_TTL,
            SAI_ACL_ENTRY_ATTR_FIELD_TOS,
            SAI_ACL_ENTRY_ATTR_FIELD_IP_FLAGS,
            SAI_ACL_ENTRY_ATTR_FIELD_TCP_FLAGS,
            SAI_ACL_ENTRY_ATTR_FIELD_IP_TYPE,
            SAI_ACL_ENTRY_ATTR_FIELD_IP_FRAG,
            SAI_ACL_ENTRY_ATTR_FIELD_IPv6_FLOW_LABEL,
            SAI_ACL_ENTRY_ATTR_FIELD_TC,
            SAI_ACL_ENTRY_ATTR_FIELD_ICMP_TYPE,
            SAI_ACL_ENTRY_ATTR_FIELD_ICMP_CODE,
            SAI_ACL_ENTRY_ATTR_FIELD_VLAN_TAGS,
            SAI_ACL_ENTRY_ATTR_FIELD_FDB_DST_USER_META,
            SAI_ACL_ENTRY_ATTR_FIELD_ROUTE_DST_USER_META,
            // SAI_ACL_ENTRY_ATTR_FIELD_NEIGHBOR_DST_USER_META,
            SAI_ACL_ENTRY_ATTR_FIELD_PORT_USER_META,
            SAI_ACL_ENTRY_ATTR_FIELD_VLAN_USER_META,
            SAI_ACL_ENTRY_ATTR_FIELD_ACL_USER_META,
            SAI_ACL_ENTRY_ATTR_FIELD_FDB_NPU_META_DST_HIT,
            SAI_ACL_ENTRY_ATTR_FIELD_NEIGHBOR_NPU_META_DST_HIT,
            SAI_ACL_ENTRY_ATTR_FIELD_ROUTE_NPU_META_DST_HIT };

    // when acl entry is created there is a requirement
    // that at least one (1) field should be present

    // TODO should those fields match with table enabled fields? is this requirement?
    bool fields_present = false;

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        for (size_t j = 0; j < sizeof(fields)/sizeof(uint32_t); ++j)
        {
            if (attr_list[i].id == fields[j])
            {
               fields_present = true;

               i = attr_count; // break all loops
               break;
            }
        }
    }

    if (fields_present == false)
    {
        SWSS_LOG_ERROR("at least one field must present");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // PORTS, some fields here may be CPU_PORT
    // TODO also ports here maybe ports, lag or tunnel ?

    const sai_attribute_t* attr_field_in_ports = redis_get_attribute_by_id(SAI_ACL_ENTRY_ATTR_FIELD_IN_PORTS, attr_count, attr_list);

    if (attr_field_in_ports != NULL)
    {
        sai_object_list_t objlist = attr_field_in_ports->value.aclfield.data.objlist;

        if (objlist.list == NULL)
        {
            SWSS_LOG_ERROR("acl entry field in ports is NULL");

            return SAI_STATUS_INVALID_PARAMETER;
        }

        for (uint32_t i = 0; i < objlist.count; ++i)
        {
            sai_object_id_t obj = objlist.list[i];

            if (local_ports_set.find(obj) == local_ports_set.end())
            {
                SWSS_LOG_ERROR("port %llx is missing", obj);

                return SAI_STATUS_INVALID_PARAMETER;
            }
        }
    }

    const sai_attribute_t* attr_field_out_ports = redis_get_attribute_by_id(SAI_ACL_ENTRY_ATTR_FIELD_OUT_PORTS, attr_count, attr_list);

    if (attr_field_out_ports != NULL)
    {
        sai_object_list_t objlist = attr_field_out_ports->value.aclfield.data.objlist;

        if (objlist.list == NULL)
        {
            SWSS_LOG_ERROR("acl entry field in ports is NULL");

            return SAI_STATUS_INVALID_PARAMETER;
        }

        for (uint32_t i = 0; i < objlist.count; ++i)
        {
            sai_object_id_t obj = objlist.list[i];

            if (local_ports_set.find(obj) == local_ports_set.end())
            {
                SWSS_LOG_ERROR("port %llx is missing", obj);

                return SAI_STATUS_INVALID_PARAMETER;
            }
        }
    }

    const sai_attribute_t* attr_field_in_port = redis_get_attribute_by_id(SAI_ACL_ENTRY_ATTR_FIELD_IN_PORT, attr_count, attr_list);

    if (attr_field_in_port != NULL)
    {
        sai_object_id_t obj = attr_field_in_ports->value.aclfield.data.oid;

        // TODO lag ?
        if (local_ports_set.find(obj) == local_ports_set.end())
        {
            SWSS_LOG_ERROR("port %llx is missing", obj);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    const sai_attribute_t* attr_field_out_port = redis_get_attribute_by_id(SAI_ACL_ENTRY_ATTR_FIELD_OUT_PORT, attr_count, attr_list);

    if (attr_field_out_port != NULL)
    {
        sai_object_id_t obj = attr_field_out_port->value.aclfield.data.oid;

        // TODO lag ?
        if (local_ports_set.find(obj) == local_ports_set.end())
        {
            SWSS_LOG_ERROR("port %llx is missing", obj);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    const sai_attribute_t* attr_field_src_port = redis_get_attribute_by_id(SAI_ACL_ENTRY_ATTR_FIELD_SRC_PORT, attr_count, attr_list);

    if (attr_field_src_port != NULL)
    {
        sai_object_id_t obj = attr_field_src_port->value.aclfield.data.oid;

        // TODO lag ?
        if (local_ports_set.find(obj) == local_ports_set.end())
        {
            SWSS_LOG_ERROR("port %llx is missing", obj);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    // FIELD RANGE

    const sai_attribute_t* attr_field_range = redis_get_attribute_by_id(SAI_ACL_ENTRY_ATTR_FIELD_RANGE, attr_count, attr_list);

    if (attr_field_range != NULL)
    {
        // TODO field object id ?
        sai_object_list_t objlist = attr_field_range->value.aclfield.data.objlist;

        if (objlist.list == NULL)
        {
            SWSS_LOG_ERROR("field range list is NULL");

            return SAI_STATUS_INVALID_PARAMETER;
        }

        for (uint32_t i = 0; i < objlist.count; ++i)
        {
            sai_object_id_t obj = objlist.list[i];

            if (local_acl_ranges_set.find(obj) == local_acl_ranges_set.end())
            {
                SWSS_LOG_ERROR("acl range %llx is missing", obj);

                return SAI_STATUS_INVALID_PARAMETER;
            }
        }
    }

    // ACTIONS
    // TODO are actions read/write ?

    const sai_attribute_t* attr_action_redirect = redis_get_attribute_by_id(SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT, attr_count, attr_list);

    if (attr_action_redirect != NULL)
    {
        sai_object_id_t obj = attr_action_redirect->value.aclaction.parameter.oid;

        // port, lag, next hop, next hop group (cpu port?)
        if (local_ports_set.find(obj) == local_ports_set.end() &&
            local_lags_set.find(obj) == local_lags_set.end() &&
            local_next_hops_set.find(obj) == local_next_hops_set.end() &&
            local_next_hop_groups_set.find(obj) == local_next_hop_groups_set.end())
        {
            SWSS_LOG_ERROR("port %llx is missing", obj); // TODO add type ?

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    const sai_attribute_t* attr_action_redirect_list = redis_get_attribute_by_id(SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT_LIST, attr_count, attr_list);

    if (attr_action_redirect_list != NULL)
    {
        sai_object_list_t objlist = attr_action_redirect_list->value.aclaction.parameter.objlist;

        if (objlist.list == NULL)
        {
            SWSS_LOG_ERROR("acl action redirect list is NULL");

            return SAI_STATUS_INVALID_PARAMETER;
        }

        for (uint32_t i = 0; i < objlist.count; ++i)
        {
            sai_object_id_t obj = objlist.list[i];

            // port, lag, next hop, next hop group (cpu port?)
            if (local_ports_set.find(obj) == local_ports_set.end() &&
                local_lags_set.find(obj) == local_lags_set.end() &&
                local_next_hops_set.find(obj) == local_next_hops_set.end() &&
                local_next_hop_groups_set.find(obj) == local_next_hop_groups_set.end())
            {
                SWSS_LOG_ERROR("port %llx is missing", obj); // TODO add type ?

                return SAI_STATUS_INVALID_PARAMETER;
            }
        }
    }

    const sai_attribute_t* attr_action_counter = redis_get_attribute_by_id(SAI_ACL_ENTRY_ATTR_ACTION_COUNTER, attr_count, attr_list);

    if (attr_action_counter != NULL)
    {
        sai_object_id_t obj = attr_action_counter->value.aclaction.parameter.oid;

        if (local_acl_counters_set.find(obj) == local_acl_counters_set.end())
        {
            SWSS_LOG_ERROR("acl counter %llx is missing", obj);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    const sai_attribute_t* attr_action_mirror_ingress = redis_get_attribute_by_id(SAI_ACL_ENTRY_ATTR_ACTION_MIRROR_INGRESS, attr_count, attr_list);

    // TODO extra validation is needed since we must check if type of mirror session is ingress

    if (attr_action_mirror_ingress != NULL)
    {
        sai_object_id_t obj = attr_action_mirror_ingress->value.aclaction.parameter.oid;

        if (local_mirror_sessions_set.find(obj) == local_mirror_sessions_set.end())
        {
            SWSS_LOG_ERROR("mirror session %llx is missing", obj);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    const sai_attribute_t* attr_action_mirror_egress = redis_get_attribute_by_id(SAI_ACL_ENTRY_ATTR_ACTION_MIRROR_EGRESS, attr_count, attr_list);

    // TODO extra validation is needed since we must check if type of mirror session is egress

    if (attr_action_mirror_egress != NULL)
    {
        sai_object_id_t obj = attr_action_mirror_egress->value.aclaction.parameter.oid;

        if (local_mirror_sessions_set.find(obj) == local_mirror_sessions_set.end())
        {
            SWSS_LOG_ERROR("mirror session %llx is missing", obj);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    const sai_attribute_t* attr_action_policer = redis_get_attribute_by_id(SAI_ACL_ENTRY_ATTR_ACTION_SET_POLICER, attr_count, attr_list);

    // TODO extra validation may be needed in policer

    if (attr_action_policer != NULL)
    {
        sai_object_id_t obj = attr_action_policer->value.aclaction.parameter.oid;

        if (local_policers_set.find(obj) == local_policers_set.end())
        {
            SWSS_LOG_ERROR("policer %llx is missing", obj);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    // TODO for now we skip validation of sample packet

    // TODO we skip for now CPU_QUEUE

    // TODO we skip for now EGRESS_BLOCK_PORT_LIST

    sai_status_t status = redis_generic_create(
        SAI_OBJECT_TYPE_ACL_ENTRY,
        acl_entry_id,
        attr_count,
        attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting acl entry %llx to local state", *acl_entry_id);

        local_acl_entries_set.insert(*acl_entry_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *   @brief Delete an ACL entry
 *
 * Arguments:
 *   @param[in] acl_entry_id - the acl entry id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_delete_acl_entry(
    _In_ sai_object_id_t acl_entry_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove acl entry
    // it should be always safe since acl entry is leaf

    if (local_acl_entries_set.find(acl_entry_id) == local_acl_entries_set.end())
    {
        SWSS_LOG_ERROR("acl entry %llx is missing", acl_entry_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
        SAI_OBJECT_TYPE_ACL_ENTRY,
        acl_entry_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing acl entry %llx from local state", acl_entry_id);

        local_acl_entries_set.erase(acl_entry_id);

        // TODO decrease reference count on used objects
    }

    return status;
}

/**
 * Routine Description:
 *   @brief Set ACL entry attribute
 *
 * Arguments:
 *    @param[in] acl_entry_id - the acl entry id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_set_acl_entry_attribute(
    _In_ sai_object_id_t acl_entry_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_acl_entries_set.find(acl_entry_id) == local_acl_entries_set.end())
    {
        SWSS_LOG_ERROR("acl entry %llx is missing", acl_entry_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        // TODO check which attributes we can set on acl entry
        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_ACL_ENTRY,
            acl_entry_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *   @brief Get ACL entry attribute
 *
 * Arguments:
 *    @param[in] acl_entry_id - acl entry id
 *    @param[in] attr_count - number of attributes
 *    @param[out] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_get_acl_entry_attribute(
    _In_ sai_object_id_t acl_entry_id,
    _In_ uint32_t attr_count,
    _Out_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 1)
    {
        SWSS_LOG_ERROR("attribute count must be at least 1");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_acl_entries_set.find(acl_entry_id) == local_acl_entries_set.end())
    {
        SWSS_LOG_ERROR("acl entry %llx is missing", acl_entry_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO for get we need to check if attrib was set previously ?
    // like field or action, or can we get action that was not set ?

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_ACL_ENTRY_ATTR_TABLE_ID:
            case SAI_ACL_ENTRY_ATTR_PRIORITY:
            case SAI_ACL_ENTRY_ATTR_ADMIN_STATE:

            case SAI_ACL_ENTRY_ATTR_FIELD_SRC_IPv6:
            case SAI_ACL_ENTRY_ATTR_FIELD_DST_IPv6:
            case SAI_ACL_ENTRY_ATTR_FIELD_SRC_MAC:
            case SAI_ACL_ENTRY_ATTR_FIELD_DST_MAC:
            case SAI_ACL_ENTRY_ATTR_FIELD_SRC_IP:
            case SAI_ACL_ENTRY_ATTR_FIELD_DST_IP:
            case SAI_ACL_ENTRY_ATTR_FIELD_IN_PORTS:
            case SAI_ACL_ENTRY_ATTR_FIELD_OUT_PORTS:
            case SAI_ACL_ENTRY_ATTR_FIELD_IN_PORT:
            case SAI_ACL_ENTRY_ATTR_FIELD_OUT_PORT:
            case SAI_ACL_ENTRY_ATTR_FIELD_SRC_PORT:
            case SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_ID:
            case SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_PRI:
            case SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_CFI:
            case SAI_ACL_ENTRY_ATTR_FIELD_INNER_VLAN_ID:
            case SAI_ACL_ENTRY_ATTR_FIELD_INNER_VLAN_PRI:
            case SAI_ACL_ENTRY_ATTR_FIELD_INNER_VLAN_CFI:
            case SAI_ACL_ENTRY_ATTR_FIELD_L4_SRC_PORT:
            case SAI_ACL_ENTRY_ATTR_FIELD_L4_DST_PORT:
            case SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE:
            case SAI_ACL_ENTRY_ATTR_FIELD_IP_PROTOCOL:
            case SAI_ACL_ENTRY_ATTR_FIELD_DSCP:
            case SAI_ACL_ENTRY_ATTR_FIELD_ECN:
            case SAI_ACL_ENTRY_ATTR_FIELD_TTL:
            case SAI_ACL_ENTRY_ATTR_FIELD_TOS:
            case SAI_ACL_ENTRY_ATTR_FIELD_IP_FLAGS:
            case SAI_ACL_ENTRY_ATTR_FIELD_TCP_FLAGS:
            case SAI_ACL_ENTRY_ATTR_FIELD_IP_TYPE:
            case SAI_ACL_ENTRY_ATTR_FIELD_IP_FRAG:
            case SAI_ACL_ENTRY_ATTR_FIELD_IPv6_FLOW_LABEL:
            case SAI_ACL_ENTRY_ATTR_FIELD_TC:
            case SAI_ACL_ENTRY_ATTR_FIELD_ICMP_TYPE:
            case SAI_ACL_ENTRY_ATTR_FIELD_ICMP_CODE:
            case SAI_ACL_ENTRY_ATTR_FIELD_VLAN_TAGS:
            case SAI_ACL_ENTRY_ATTR_FIELD_FDB_DST_USER_META:
            case SAI_ACL_ENTRY_ATTR_FIELD_ROUTE_DST_USER_META:
            //case SAI_ACL_ENTRY_ATTR_FIELD_NEIGHBOR_DST_USER_META:
            case SAI_ACL_ENTRY_ATTR_FIELD_PORT_USER_META:
            case SAI_ACL_ENTRY_ATTR_FIELD_VLAN_USER_META:
            case SAI_ACL_ENTRY_ATTR_FIELD_ACL_USER_META:
            case SAI_ACL_ENTRY_ATTR_FIELD_FDB_NPU_META_DST_HIT:
            case SAI_ACL_ENTRY_ATTR_FIELD_NEIGHBOR_NPU_META_DST_HIT:
            case SAI_ACL_ENTRY_ATTR_FIELD_ROUTE_NPU_META_DST_HIT:

            case SAI_ACL_ENTRY_ATTR_FIELD_RANGE:
                // ok
                break;

            // TODO what about action list ? how range is specified anyway?

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
        SAI_OBJECT_TYPE_ACL_ENTRY,
        acl_entry_id,
        attr_count,
        attr_list);

    return status;
}

/**
 * Routine Description:
 *   @brief Create an ACL counter
 *
 * Arguments:
 *   @param[out] acl_counter_id - the acl counter id
 *   @param[in] attr_count - number of attributes
 *   @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_create_acl_counter(
    _Out_ sai_object_id_t *acl_counter_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 1)
    {
        SWSS_LOG_ERROR("attribute count must be at least 1");

        // SAI_ACL_COUNTER_ATTR_TABLE_ID

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_table_id = redis_get_attribute_by_id(SAI_ACL_COUNTER_ATTR_TABLE_ID, attr_count, attr_list);

    if (attr_table_id == NULL)
    {
        SWSS_LOG_ERROR("missing table id attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t table_id = attr_table_id->value.oid;

    if (local_acl_tables_set.find(table_id) == local_acl_tables_set.end())
    {
        SWSS_LOG_ERROR("acl table %llx is missing", table_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_create(
        SAI_OBJECT_TYPE_ACL_COUNTER,
        acl_counter_id,
        attr_count,
        attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting acl countr %llx to local state", *acl_counter_id);

        local_acl_counters_set.insert(*acl_counter_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *   @brief Delete an ACL counter
 *
 * Arguments:
 *   @param[in] acl_counter_id - the acl counter id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_delete_acl_counter(
    _In_ sai_object_id_t acl_counter_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove acl counter (+table group)

    if (local_acl_counters_set.find(acl_counter_id) == local_acl_counters_set.end())
    {
        SWSS_LOG_ERROR("acl counter %llx is missing", acl_counter_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
        SAI_OBJECT_TYPE_ACL_COUNTER,
        acl_counter_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing acl counter %llx from local state", acl_counter_id);

        local_acl_counters_set.erase(acl_counter_id);
    }

    return status;
}

/**
 * Routine Description:
 *   @brief Set ACL counter attribute
 *
 * Arguments:
 *    @param[in] acl_counter_id - the acl counter id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_acl_counter_attribute(
    _In_ sai_object_id_t acl_counter_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_acl_counters_set.find(acl_counter_id) == local_acl_counters_set.end())
    {
        SWSS_LOG_ERROR("acl counter %llx is missing", acl_counter_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        //case SAI_ACL_COUNTER_ATTR_TABLE_ID: // TODO can this be changed ?
        case SAI_ACL_COUNTER_ATTR_ENABLE_PACKET_COUNT:
        case SAI_ACL_COUNTER_ATTR_ENABLE_BYTE_COUNT:
        case SAI_ACL_COUNTER_ATTR_PACKETS:
        case SAI_ACL_COUNTER_ATTR_BYTES:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_ACL_COUNTER,
            acl_counter_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *   @brief Get ACL counter attribute
 *
 * Arguments:
 *    @param[in] acl_counter_id - acl counter id
 *    @param[in] attr_count - number of attributes
 *    @param[out] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_acl_counter_attribute(
    _In_ sai_object_id_t acl_counter_id,
    _In_ uint32_t attr_count,
    _Out_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 1)
    {
        SWSS_LOG_ERROR("attribute count must be at least 1");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_acl_counters_set.find(acl_counter_id) == local_acl_counters_set.end())
    {
        SWSS_LOG_ERROR("acl counter %llx is missing", acl_counter_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_ACL_COUNTER_ATTR_TABLE_ID:
            case SAI_ACL_COUNTER_ATTR_ENABLE_PACKET_COUNT:
            case SAI_ACL_COUNTER_ATTR_ENABLE_BYTE_COUNT:
            case SAI_ACL_COUNTER_ATTR_PACKETS:
            case SAI_ACL_COUNTER_ATTR_BYTES:
                // ok
                break;

            // TODO what about action list ? how range is specified anyway?

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_ACL_COUNTER,
            acl_counter_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *   @brief Create an ACL Range
 *
 * Arguments:
 *   @param[out] acl_range_id - the acl range id
 *   @param[in] attr_count - number of attributes
 *   @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_create_acl_range(
    _Out_ sai_object_id_t* acl_range_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 2)
    {
        SWSS_LOG_ERROR("attribute count must be at least 2");

        // SAI_ACL_RANGE_ATTR_TYPE
        // SAI_ACL_RANGE_ATTR_LIMIT

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_type = redis_get_attribute_by_id(SAI_ACL_RANGE_ATTR_TYPE, attr_count, attr_list);
    const sai_attribute_t* attr_limit = redis_get_attribute_by_id(SAI_ACL_RANGE_ATTR_LIMIT, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("missing type attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_acl_range_type_t type = (sai_acl_range_type_t)attr_type->value.s32;

    switch (type)
    {
        case SAI_ACL_RANGE_L4_SRC_PORT_RANGE:   // layer 4 port range
        case SAI_ACL_RANGE_L4_DST_PORT_RANGE:
        case SAI_ACL_RANGE_OUTER_VLAN:
        case SAI_ACL_RANGE_INNER_VLAN:
        case SAI_ACL_RANGE_PACKET_LENGTH:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid range type value: %d", type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_limit == NULL)
    {
        SWSS_LOG_ERROR("missing limit attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_u32_range_t range = attr_limit->value.u32range;

    SWSS_LOG_DEBUG("acl range <%u..%u> of type %d", range.min, range.max, type);

    // TODO validate range add helper method for that
    // TODO should we allow min == max ?

    switch (type)
    {
        case SAI_ACL_RANGE_L4_SRC_PORT_RANGE:   // layer 4 port range
        case SAI_ACL_RANGE_L4_DST_PORT_RANGE:

            // we allow 0
            if (range.min >= range.max  ||
                //range.min < MINIMUM_L4_PORT_NUMBER ||
                //range.max < MINIMUM_L4_PORT_NUMBER ||
                range.min > MAXIMUM_L4_PORT_NUMBER ||
                range.max > MAXIMUM_L4_PORT_NUMBER)
            {
                SWSS_LOG_ERROR("invalid acl port range <%u..%u> in <%u..%u>", range.min, range.max, MINIMUM_L4_PORT_NUMBER, MAXIMUM_L4_PORT_NUMBER);

                return SAI_STATUS_INVALID_PARAMETER;
            }

            break;

        case SAI_ACL_RANGE_OUTER_VLAN:
        case SAI_ACL_RANGE_INNER_VLAN:

            if (range.min >= range.max  ||
                range.min < MINIMUM_VLAN_NUMBER ||
                range.min > MAXIMUM_VLAN_NUMBER ||
                range.max < MINIMUM_VLAN_NUMBER ||
                range.max > MAXIMUM_VLAN_NUMBER)
            {
                SWSS_LOG_ERROR("invalid acl vlan range <%u..%u> in <%u..%u>", range.min, range.max, MINIMUM_VLAN_NUMBER, MAXIMUM_VLAN_NUMBER);

                return SAI_STATUS_INVALID_PARAMETER;
            }

            break;

        case SAI_ACL_RANGE_PACKET_LENGTH:

            if (range.min >= range.max ||
                range.min > MAXIMUM_PACKET_SIZE ||
                range.max > MAXIMUM_PACKET_SIZE)
            {
                SWSS_LOG_ERROR("invalid acl vlan range <%u..%u> in <%u..%u>", range.min, range.max, MINIMUM_PACKET_SIZE, MAXIMUM_PACKET_SIZE);

                return SAI_STATUS_INVALID_PARAMETER;
            }

            break;

        default:

            SWSS_LOG_ERROR("invalid range type value: %d", type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_create(
        SAI_OBJECT_TYPE_ACL_RANGE,
        acl_range_id,
        attr_count,
        attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting acl countr %llx to local state", *acl_range_id);

        local_acl_ranges_set.insert(*acl_range_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *   @brief Remove an ACL Range
 *
 * Arguments:
 *   @param[in] acl_range_id - the acl range id
 *
 * Return Values:
 *   @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_remove_acl_range(
    _In_ sai_object_id_t acl_range_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove acl range

    if (local_acl_ranges_set.find(acl_range_id) == local_acl_ranges_set.end())
    {
        SWSS_LOG_ERROR("acl range %llx is missing", acl_range_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
        SAI_OBJECT_TYPE_ACL_RANGE,
        acl_range_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing acl range %llx from local state", acl_range_id);

        local_acl_ranges_set.erase(acl_range_id);
    }

    return status;
}

/**
 * Routine Description:
 *   @brief Set ACL range attribute
 *
 * Arguments:
 *    @param[in] acl_range_id - the acl range id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_set_acl_range_attribute(
    _In_ sai_object_id_t acl_range_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_acl_ranges_set.find(acl_range_id) == local_acl_ranges_set.end())
    {
        SWSS_LOG_ERROR("acl range %llx is missing", acl_range_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_ACL_RANGE,
            acl_range_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *   @brief Get ACL range attribute
 *
 * Arguments:
 *    @param[in] acl_range_id - acl range id
 *    @param[in] attr_count - number of attributes
 *    @param[out] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_get_acl_range_attribute(
    _In_ sai_object_id_t acl_range_id,
    _In_ uint32_t attr_count,
    _Out_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 1)
    {
        SWSS_LOG_ERROR("attribute count must be at least 1");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_acl_ranges_set.find(acl_range_id) == local_acl_ranges_set.end())
    {
        SWSS_LOG_ERROR("acl range %llx is missing", acl_range_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_ACL_RANGE_ATTR_TYPE:
            case SAI_ACL_RANGE_ATTR_LIMIT:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_ACL_RANGE,
            acl_range_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief acl methods table retrieved with sai_api_query()
 */
const sai_acl_api_t redis_acl_api = {
    redis_create_acl_table,
    redis_delete_acl_table,
    redis_set_acl_table_attribute,
    redis_get_acl_table_attribute,

    redis_create_acl_entry,
    redis_delete_acl_entry,
    redis_set_acl_entry_attribute,
    redis_get_acl_entry_attribute,

    redis_create_acl_counter,
    redis_delete_acl_counter,
    redis_set_acl_counter_attribute,
    redis_get_acl_counter_attribute,

    redis_create_acl_range,
    redis_remove_acl_range,
    redis_set_acl_range_attribute,
    redis_get_acl_range_attribute,
};
