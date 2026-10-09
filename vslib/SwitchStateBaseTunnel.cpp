#include "SwitchStateBase.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"

using namespace saivs;

sai_status_t SwitchStateBase::createBridgePort(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_BRIDGE_PORT, sid, switch_id, attr_count, attr_list));

    auto type = sai_metadata_get_attr_by_id(SAI_BRIDGE_PORT_ATTR_TYPE, attr_count, attr_list);
    auto tunnel = sai_metadata_get_attr_by_id(SAI_BRIDGE_PORT_ATTR_TUNNEL_ID, attr_count, attr_list);

    if (type == nullptr || type->value.s32 != SAI_BRIDGE_PORT_TYPE_TUNNEL || tunnel == nullptr)
    {
        return SAI_STATUS_SUCCESS;
    }

    sai_attribute_t attr;

    attr.id = SAI_TUNNEL_ATTR_TYPE;

    if (get(SAI_OBJECT_TYPE_TUNNEL, tunnel->value.oid, 1, &attr) != SAI_STATUS_SUCCESS ||
            attr.value.s32 != SAI_TUNNEL_TYPE_VXLAN)
    {
        return SAI_STATUS_SUCCESS;
    }

    attr.id = SAI_TUNNEL_ATTR_PEER_MODE;

    if (get(SAI_OBJECT_TYPE_TUNNEL, tunnel->value.oid, 1, &attr) != SAI_STATUS_SUCCESS ||
            attr.value.s32 != SAI_TUNNEL_PEER_MODE_P2P)
    {
        return SAI_STATUS_SUCCESS;
    }

    // The operational status of a VXLAN tunnel to a single peer is reported
    // as a port state change on the tunnel's bridge port (the notification
    // carries a port, a lag or a bridge port, never a tunnel); the user tracks
    // the remote endpoint state from it. The virtual tunnel is always up.

    send_port_oper_status_notification(object_id, SAI_PORT_OPER_STATUS_UP, true);

    return SAI_STATUS_SUCCESS;
}
