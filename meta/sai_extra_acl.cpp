#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_create_acl_table(
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    const sai_attribute_t* attr_priority = get_attribute_by_id(SAI_ACL_TABLE_ATTR_PRIORITY, attr_count, attr_list);

    if (attr_priority != NULL)
    {
        uint32_t priority = attr_priority->value.u32;

        // TODO range is in
        // SAI_SWITCH_ATTR_ACL_TABLE_MINIMUM_PRIORITY .. SAI_SWITCH_ATTR_ACL_TABLE_MAXIMUM_PRIORITY
        // extra validation will be needed here and snoop

        SWSS_LOG_DEBUG("acl priority: %d", priority);
    }

    const sai_attribute_t* attr_size = get_attribute_by_id(SAI_ACL_TABLE_ATTR_SIZE, attr_count, attr_list);

    if (attr_size != NULL)
    {
        uint32_t size = attr_size->value.u32; // default value is zero

        SWSS_LOG_DEBUG("size %u", size);

        // TODO this attribute is special, since it can change dynamically, but it can be
        // set on creation time and grow when entries are added
    }

    // TODO group ID is special, since when not specified it's created automatically
    // by switch and user can obtain it via GET api and group tables.
    // This behaviour should be changed and so no object would be created internally
    // there is a tricky way to track this object usage

    int fields = 0;

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        if (attr_list[i].id >= SAI_ACL_TABLE_ATTR_FIELD_START &&
            attr_list[i].id <= SAI_ACL_TABLE_ATTR_FIELD_END)
        {
            fields++;
        }
    }

    if (fields == 0)
    {
        SWSS_LOG_ERROR("at least one acl table field must present");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO another special attribute depending on switch SAI_SWITCH_ATTR_ACL_CAPABILITY
    // it may be mandatory on create, why we need to query attributes and then pass them here?
    // sdk logic can figure this out anyway

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_acl_table(
    _In_ sai_object_id_t acl_table_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_acl_table_attribute(
    _In_ sai_object_id_t acl_table_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_acl_table_attribute(
    _In_ sai_object_id_t acl_table_id,
    _In_ uint32_t attr_count,
    _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_create_acl_entry(
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    const sai_attribute_t* attr_priority = get_attribute_by_id(SAI_ACL_ENTRY_ATTR_PRIORITY, attr_count, attr_list);

    if (attr_priority != NULL)
    {
        SWSS_LOG_DEBUG("priority %u", attr_priority->value.u32);

        // TODO special, range in  SAI_SWITCH_ATTR_ACL_ENTRY_MINIMUM_PRIORITY .. SAI_SWITCH_ATTR_ACL_ENTRY_MAXIMUM_PRIORITY
        // TODO we need to check if priority was snooped and saved so we could validate it here
    }

    // FIELDS

    int fields = 0;

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        if (attr_list[i].id >= SAI_ACL_ENTRY_ATTR_FIELD_START &&
                attr_list[i].id <= SAI_ACL_ENTRY_ATTR_FIELD_END)
        {
            fields++;
        }
    }

    if (fields == 0)
    {
        SWSS_LOG_ERROR("at least one acl entry field must present");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // PORTS, some fields here may be CPU_PORT
    // TODO where we use port also lag or tunnel ?
    // TODO are actions read/write?
    // TODO extra validation is needed since we must check if type of mirror session is ingress
    // TODO extra validation is needed since we must check if type of mirror session is egress
    // TODO extra validation may be needed in policer
    // TODO extra validation may be needed on of sample packet
    // TODO extra validation may be needed on CPU_QUEUE
    // TODO extra validation may be needed on EGRESS_BLOCK_PORT_LIST

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_delete_acl_entry(
    _In_ sai_object_id_t acl_entry_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_acl_entry_attribute(
    _In_ sai_object_id_t acl_entry_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_acl_entry_attribute(
    _In_ sai_object_id_t acl_entry_id,
    _In_ uint32_t attr_count,
    _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // TODO for get we need to check if attrib was set previously ?
    // like field or action, or can we get action that was not set?

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_create_acl_counter(
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_acl_counter(
    _In_ sai_object_id_t acl_counter_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_acl_counter_attribute(
    _In_ sai_object_id_t acl_counter_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_acl_counter_attribute(
    _In_ sai_object_id_t acl_counter_id,
    _In_ uint32_t attr_count,
    _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_create_acl_range(
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    const sai_attribute_t* attr_type = get_attribute_by_id(SAI_ACL_RANGE_ATTR_TYPE, attr_count, attr_list);
    const sai_attribute_t* attr_limit = get_attribute_by_id(SAI_ACL_RANGE_ATTR_LIMIT, attr_count, attr_list);

    sai_acl_range_type_t type = (sai_acl_range_type_t)attr_type->value.s32;
    sai_u32_range_t range = attr_limit->value.u32range;

    SWSS_LOG_DEBUG("acl range <%u..%u> of type %d", range.min, range.max, type);

    switch (type)
    {
        // layer 4 port range
        case SAI_ACL_RANGE_L4_SRC_PORT_RANGE:
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

            SWSS_LOG_ERROR("FATAL: inalid type %d", type);

            return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_acl_range(
    _In_ sai_object_id_t acl_range_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_acl_range_attribute(
    _In_ sai_object_id_t acl_range_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_acl_range_attribute(
    _In_ sai_object_id_t acl_range_id,
    _In_ uint32_t attr_count,
    _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}
