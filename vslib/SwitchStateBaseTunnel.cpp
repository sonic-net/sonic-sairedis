#include "SwitchStateBase.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"

using namespace saivs;

sai_status_t SwitchStateBase::createTunnel(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_TUNNEL, sid, switch_id, attr_count, attr_list));

    auto type = sai_metadata_get_attr_by_id(SAI_TUNNEL_ATTR_TYPE, attr_count, attr_list);
    auto peerMode = sai_metadata_get_attr_by_id(SAI_TUNNEL_ATTR_PEER_MODE, attr_count, attr_list);

    if (type && type->value.s32 == SAI_TUNNEL_TYPE_VXLAN &&
            peerMode && peerMode->value.s32 == SAI_TUNNEL_PEER_MODE_P2P)
    {
        // ASIC SAI implementations report the operational status of a VXLAN
        // tunnel to a single peer as a port state change keyed by the tunnel;
        // the user tracks the remote endpoint state from it

        send_port_oper_status_notification(object_id, SAI_PORT_OPER_STATUS_UP, true);
    }

    return SAI_STATUS_SUCCESS;
}
