#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t is_header_version_ok(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    const sai_attribute_t* attr_iphdr_version = get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_IPHDR_VERSION, attr_count, attr_list);

    if (attr_iphdr_version != NULL)
    {
        uint8_t iphdr_version = attr_iphdr_version->value.u8;

        switch (iphdr_version)
        {
            case 4:
            case 6:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("invalid ip header version value: %u", iphdr_version);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_create_mirror_session(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return is_header_version_ok(attr_count, attr_list);
}

sai_status_t meta_pre_remove_mirror_session(
        _In_ sai_object_id_t session_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_mirror_session_attribute(
        _In_ sai_object_id_t session_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    // TODO we need to type to decide which parameters are safe to set
    // SAI_MIRROR_SESSION_ATTR_MONITOR_PORT:
    // TODO should changing port during mirror session should be possible ?
    // what if new port is somehow incompattible with current session?

    // SAI_MIRROR_SESSION_ATTR_GRE_PROTOCOL_TYPE: // TODO validate GRE protocol
    return is_header_version_ok(1, attr);
}

sai_status_t meta_pre_get_mirror_session_attribute(
        _In_ sai_object_id_t session_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}
