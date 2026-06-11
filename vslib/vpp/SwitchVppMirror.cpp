#include "SwitchVpp.h"
#include "SwitchVppUtils.h"

#include "meta/sai_serialize.h"
#include "swss/logger.h"

using namespace saivs;

sai_status_t SwitchVpp::createMirrorSession(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    const sai_attribute_value_t *value;
    uint32_t attr_index;
    MirrorSessionInfo info{};

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(find_attrib_in_list(attr_count, attr_list, SAI_MIRROR_SESSION_ATTR_TYPE, &value, &attr_index));
    int32_t mirror_type = value->s32;

    if(mirror_type == SAI_MIRROR_SESSION_TYPE_LOCAL) {
        CHECK_STATUS(find_attrib_in_list(attr_count, attr_list, SAI_MIRROR_SESSION_ATTR_MONITOR_PORT, &value, &attr_index));
        sai_object_id_t monitor_port = value->oid;

        std::string hwif_name;
        if(!vpp_get_hwif_name(monitor_port, 0, hwif_name)) {
            SWSS_LOG_ERROR("Failed to get hwif name for monitor port %s", sai_serialize_object_id(monitor_port).c_str());
            return SAI_STATUS_FAILURE;
        }

        int sw_idx = get_sw_if_idx(hwif_name.c_str());
        if(sw_idx < 0) {
            SWSS_LOG_ERROR("Failed to get sw_if_index for hwif %s", hwif_name.c_str());
            return SAI_STATUS_FAILURE;
        }

        SWSS_LOG_INFO("SPAN mirror session info: monitor_port=%s, hwif_name=%s, sw_if_index=%d",
            sai_serialize_object_id(monitor_port).c_str(), hwif_name.c_str(), sw_idx);
        
        info.sw_if_index = (uint32_t)sw_idx;
        info.is_erspan = false;
    } else {
        SWSS_LOG_ERROR("Unsupported mirror session type %d", mirror_type);
        return SAI_STATUS_FAILURE;
    }

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_MIRROR_SESSION, sid, switch_id, attr_count, attr_list));

    m_mirror_sessions[object_id] = info;
    m_mirror_session_count++;

    SWSS_LOG_NOTICE("Created mirror session %s, type=%d, sw_if_index=%u, is_erspan=%d, mirror session count: %d",
        sid.c_str(), mirror_type, info.sw_if_index, info.is_erspan, m_mirror_session_count);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeMirrorSession(
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    auto it = m_mirror_sessions.find(object_id);
    if(it == m_mirror_sessions.end()) {
        SWSS_LOG_ERROR("Mirror session %s not found", sai_serialize_object_id(object_id).c_str());
        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_MIRROR_SESSION, sai_serialize_object_id(object_id)));

    m_mirror_sessions.erase(it);
    m_mirror_session_count--;

    SWSS_LOG_NOTICE("Removed mirror session %s, mirror session count: %d", sai_serialize_object_id(object_id).c_str(), m_mirror_session_count);

    return SAI_STATUS_SUCCESS;
}