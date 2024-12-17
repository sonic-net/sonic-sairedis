#include "SwitchVpp.h"
#include "EventPayloadNotification.h"
#include "SwitchVppUtils.h"

#include "meta/sai_serialize.h"
#include "meta/NotificationBfdSessionStateChange.h"

#include "swss/logger.h"
#include "swss/select.h"

#include "vppxlate/SaiVppXlate.h"

#include <arpa/inet.h>

using namespace saivs;

sai_status_t SwitchVpp::bfd_session_add(
    _In_ const std::string &serializedObjectId,
    _In_ sai_object_id_t switch_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(vpp_bfd_session_add(serializedObjectId, switch_id, attr_count, attr_list));
    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_BFD_SESSION, serializedObjectId, switch_id, attr_count, attr_list));

    return SAI_STATUS_SUCCESS;

}

sai_status_t SwitchVpp::vpp_bfd_session_add(
    _In_ const std::string &serializedObjectId,
    _In_ sai_object_id_t switch_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    int ret = 0;

    sai_object_id_t bfd_oid;
    sai_deserialize_object_id(serializedObjectId, bfd_oid);

    /* Attribute#1 */
    auto attr = sai_metadata_get_attr_by_id(SAI_BFD_SESSION_ATTR_MIN_RX, attr_count, attr_list);
    if (!attr)
    {
        SWSS_LOG_ERROR("attr SAI_BFD_SESSION_ATTR_MIN_RX was not passed");
        return SAI_STATUS_FAILURE;
    }

    uint32_t required_min_rx = attr->value.u32;

    /* Attribute#2 */
    attr = sai_metadata_get_attr_by_id(SAI_BFD_SESSION_ATTR_MIN_TX, attr_count, attr_list);
    if (!attr)
    {
        SWSS_LOG_ERROR("attr SAI_BFD_SESSION_ATTR_MIN_TX was not passed");
        return SAI_STATUS_FAILURE;
    }

    uint32_t required_min_tx = attr->value.u32;

    /* Attribute#3 */
    attr = sai_metadata_get_attr_by_id(SAI_BFD_SESSION_ATTR_MULTIPLIER, attr_count, attr_list);
    if (!attr)
    {
        SWSS_LOG_ERROR("attr SAI_BFD_SESSION_ATTR_MULTIPLIER was not passed");
        return SAI_STATUS_FAILURE;
    }

    uint8_t detect_mult = attr->value.u8;

    /* Attribute#4 */
    attr = sai_metadata_get_attr_by_id(SAI_BFD_SESSION_ATTR_SRC_IP_ADDRESS, attr_count, attr_list);
    if (!attr)
    {
        SWSS_LOG_ERROR("attr SAI_BFD_SESSION_ATTR_SRC_IP_ADDRESS was not passed");
        return SAI_STATUS_FAILURE;
    }

    /*local addr */
    sai_ip_address_t local_addr = attr->value.ipaddr;
    vpp_ip_addr_t vpp_local_addr;
    sai_ip_address_t_to_vpp_ip_addr_t(local_addr, vpp_local_addr);

    /* Attribute#5 */
    attr = sai_metadata_get_attr_by_id(SAI_BFD_SESSION_ATTR_DST_IP_ADDRESS, attr_count, attr_list);
    if (!attr)
    {
        SWSS_LOG_ERROR("attr SAI_BFD_SESSION_ATTR_DST_IP_ADDRESS was not passed");
        return SAI_STATUS_FAILURE;
    }

    /* Peer Addr*/
    sai_ip_address_t peer_addr = attr->value.ipaddr;
    vpp_ip_addr_t vpp_peer_addr;
    sai_ip_address_t_to_vpp_ip_addr_t(peer_addr, vpp_peer_addr);

    /* Attribute#6 - multihop is optional */
    bool multihop = false;
    attr = sai_metadata_get_attr_by_id(SAI_BFD_SESSION_ATTR_MULTIHOP, attr_count, attr_list);
    if (attr)
    {
        multihop = attr->value.booldata;
    }

    const char *hwif_name = NULL;
    if (!multihop) {
        /* Attribute#7 */
        attr = sai_metadata_get_attr_by_id(SAI_BFD_SESSION_ATTR_PORT, attr_count, attr_list);
        if (!attr)
        {
            SWSS_LOG_ERROR("attr SAI_BFD_SESSION_ATTR_PORT was not passed");
            return SAI_STATUS_FAILURE;
        }

        sai_object_id_t port_id = attr->value.oid;
        sai_object_type_t obj_type = objectTypeQuery(port_id);
        if (obj_type != SAI_OBJECT_TYPE_PORT)
        {
            SWSS_LOG_ERROR("SAI_BRIDGE_PORT_ATTR_PORT_ID=%s expected to be PORT but is: %s",
                    sai_serialize_object_id(port_id).c_str(),
                    sai_serialize_object_type(obj_type).c_str());
            return SAI_STATUS_FAILURE;
        }
        std::string ifname = "";
        if (vpp_get_hwif_name(port_id, 0, ifname) == true)
        {
            hwif_name = ifname.c_str();
        }
        else
        {
            SWSS_LOG_ERROR("BFD session create request FAILED due to invalid hwif name");

            return SAI_STATUS_FAILURE;
        }
    }

    if (multihop || hwif_name) {
        SWSS_LOG_NOTICE("BFD session create request sent to VS, hwif: %s, multihop: %d, oid: %ld",  hwif_name, multihop, bfd_oid);
        /*vpp call to add bfd session*/
        ret = bfd_udp_add(multihop, hwif_name, &vpp_local_addr, &vpp_peer_addr, detect_mult, required_min_tx, required_min_rx);
        if (ret >= 0)
        {
            BFD_MUTEX;

            // store bfd attributes to sai oid mapping
            vpp_bfd_info_t bfd_info;
            bfd_info.multihop = multihop;
            memcpy(&bfd_info.local_addr, &local_addr, sizeof(local_addr));
            memcpy(&bfd_info.peer_addr, &peer_addr, sizeof(peer_addr));

            m_bfd_info_map[bfd_info] = bfd_oid;
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::bfd_session_del(
    _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    vpp_bfd_session_del(serializedObjectId);
    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_BFD_SESSION, serializedObjectId));

    return SAI_STATUS_SUCCESS;

}
sai_status_t SwitchVpp::vpp_bfd_session_del(
    _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    int ret = 0;

    sai_object_id_t bfd_oid;
    sai_deserialize_object_id(serializedObjectId, bfd_oid);
    sai_attribute_t attr;

    /* Attribute#1 */
    attr.id = SAI_BFD_SESSION_ATTR_SRC_IP_ADDRESS;
    CHECK_STATUS(get(SAI_OBJECT_TYPE_BFD_SESSION, bfd_oid, 1, &attr));
    sai_ip_address_t local_addr = attr.value.ipaddr;
    vpp_ip_addr_t vpp_local_addr;
    /*local addr */
    sai_ip_address_t_to_vpp_ip_addr_t(local_addr, vpp_local_addr);

    /* Attribute#2 */
    attr.id = SAI_BFD_SESSION_ATTR_DST_IP_ADDRESS;
    CHECK_STATUS(get(SAI_OBJECT_TYPE_BFD_SESSION, bfd_oid, 1, &attr));
    sai_ip_address_t peer_addr = attr.value.ipaddr;
    vpp_ip_addr_t vpp_peer_addr;
    /* Peer Addr*/
    sai_ip_address_t_to_vpp_ip_addr_t(peer_addr, vpp_peer_addr);

    /* Attribute#3 - multihop is optional */
    bool multihop = false;
    attr.id = SAI_BFD_SESSION_ATTR_MULTIHOP;
    if(get(SAI_OBJECT_TYPE_BFD_SESSION, bfd_oid, 1, &attr) == SAI_STATUS_SUCCESS)
    {
        multihop = attr.value.booldata;
    }

    const char *hwif_name = NULL;
    if (!multihop) {
        /* Attribute#4 */
        attr.id = SAI_BFD_SESSION_ATTR_PORT;
        CHECK_STATUS(get(SAI_OBJECT_TYPE_BFD_SESSION, bfd_oid, 1, &attr));
        sai_object_id_t port_id = attr.value.oid;
        sai_object_type_t obj_type = objectTypeQuery(port_id);
        if (obj_type != SAI_OBJECT_TYPE_PORT)
        {
            SWSS_LOG_ERROR("SAI_BRIDGE_PORT_ATTR_PORT_ID=%s expected to be PORT but is: %s",
                    sai_serialize_object_id(port_id).c_str(),
                    sai_serialize_object_type(obj_type).c_str());
            return SAI_STATUS_FAILURE;
        }

        std::string ifname = "";
        if (vpp_get_hwif_name(port_id, 0, ifname) == true)
        {
            hwif_name = ifname.c_str();
        }
        else
        {
            SWSS_LOG_ERROR("BFD session delete request FAILED due to invalid hwif name");

            return SAI_STATUS_FAILURE;
        }
    }

    if (multihop || hwif_name) {
        SWSS_LOG_NOTICE("BFD session delete request sent to VS, hwif: %s, multihop: %d, oid: %ld",  hwif_name, multihop, bfd_oid);
        /*vpp call to add bfd session*/
        ret = bfd_udp_del(multihop, hwif_name, &vpp_local_addr, &vpp_peer_addr);
        if (ret >= 0)
        {
            BFD_MUTEX;

            // remove bfd attributes to sai oid mapping
            vpp_bfd_info_t bfd_info;
            bfd_info.multihop = multihop;
            memcpy(&bfd_info.local_addr, &local_addr, sizeof(local_addr));
            memcpy(&bfd_info.peer_addr, &peer_addr, sizeof(peer_addr));

            auto it = m_bfd_info_map.find(bfd_info);

            // Check if the key exists and erase by iterator
            if (it != m_bfd_info_map.end()) {
                m_bfd_info_map.erase(it);
            }
        }
    }

    return SAI_STATUS_SUCCESS;
}

void SwitchVpp::update_bfd_session_state(
        _In_ sai_object_id_t bfd_oid,
        _In_ sai_bfd_session_state_t state)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_BFD_SESSION_ATTR_STATE;
    attr.value.s32 = state;

    sai_status_t status = set(SAI_OBJECT_TYPE_BFD_SESSION, bfd_oid, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to update bfd state %s: %d",
                sai_serialize_object_id(bfd_oid).c_str(),
                state);
    }
}

void SwitchVpp::send_bfd_state_change_notification(
        _In_ sai_object_id_t bfd_oid,
        _In_ sai_bfd_session_state_t state,
        _In_ bool force)
{
    SWSS_LOG_ENTER();

    char state_buf[64] = {0};
    sai_bfd_session_state_notification_t data;

    data.bfd_session_id = bfd_oid;
    data.session_state = state;

    sai_serialize_bfd_session_state(state_buf, state);

    auto meta = getMeta();

    if (meta)
    {
        meta->meta_sai_on_bfd_session_state_change(1, &data);
    }

    auto objectType = objectTypeQuery(bfd_oid);

    if (objectType != SAI_OBJECT_TYPE_BFD_SESSION)
    {
        SWSS_LOG_ERROR("object type %s not supported for %s",
                sai_serialize_object_type(objectType).c_str(),
                sai_serialize_object_id(bfd_oid).c_str());
        return;
    }

    sai_attribute_t attr;

    attr.id = SAI_BFD_SESSION_ATTR_STATE;

    if (get(objectType, bfd_oid, 1, &attr) != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get bfd session attribute SAI_BFD_SESSION_ATTR_STATE");
    }
    else
    {
        if (force)
        {
            SWSS_LOG_NOTICE("explicitly send SAI_SWITCH_ATTR_BFD_SESSION_STATE_CHANGE_NOTIFY for bfd session %s: %s",
                    sai_serialize_object_id(data.bfd_session_id).c_str(),
                    state_buf);

        }
        else if ((sai_bfd_session_state_t)attr.value.s32 == data.session_state)
        {
            SWSS_LOG_INFO("bfd session state didn't change, will not send notification");
            return;
        }
    }

    attr.id = SAI_SWITCH_ATTR_BFD_SESSION_STATE_CHANGE_NOTIFY;

    if (get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &attr) != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get SAI_SWITCH_ATTR_BFD_SESSION_STATE_CHANGE_NOTIFY for switch %s",
                sai_serialize_object_id(m_switch_id).c_str());
        return;
    }

    if (attr.value.ptr == NULL)
    {
        SWSS_LOG_INFO("SAI_SWITCH_ATTR_BFD_SESSION_STATE_CHANGE_NOTIFY callback is NULL");
        return;
    }

    sai_switch_notifications_t sn = { };

    sn.on_bfd_session_state_change = (sai_bfd_session_state_change_notification_fn)attr.value.ptr;

    update_bfd_session_state(bfd_oid, data.session_state);

    SWSS_LOG_NOTICE("send event SAI_SWITCH_ATTR_BFD_SESSION_STATE_CHANGE_NOTIFY for bfd session %s: %s",
            sai_serialize_object_id(bfd_oid).c_str(),
            state_buf);

    auto str = sai_serialize_bfd_session_state_ntf(1, &data);

    auto ntf = std::make_shared<sairedis::NotificationBfdSessionStateChange>(str);

    auto payload = std::make_shared<EventPayloadNotification>(ntf, sn);

    m_switchConfig->m_eventQueue->enqueue(std::make_shared<Event>(EVENT_TYPE_NOTIFICATION, payload));
}
