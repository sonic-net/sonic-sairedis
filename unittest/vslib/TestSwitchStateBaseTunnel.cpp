#include "SwitchBCM56850.h"
#include "EventPayloadNotification.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>

#include <memory>
#include <utility>
#include <vector>

using namespace saivs;

static void dummyPortStateChangeNotify(
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();
}

// port state changes sent since the last call
static std::vector<std::pair<sai_object_id_t, sai_port_oper_status_t>> portStateChanges(
        _In_ std::shared_ptr<EventQueue> eventQueue)
{
    SWSS_LOG_ENTER();

    std::vector<std::pair<sai_object_id_t, sai_port_oper_status_t>> changes;

    while (eventQueue->size())
    {
        auto payload = std::dynamic_pointer_cast<EventPayloadNotification>(eventQueue->dequeue()->getPayload());

        if (!payload || payload->getNotification()->getNotificationType() != SAI_SWITCH_NOTIFICATION_TYPE_PORT_STATE_CHANGE)
        {
            continue;
        }

        uint32_t count;
        sai_port_oper_status_notification_t* data = nullptr;

        sai_deserialize_port_oper_status_ntf(payload->getNotification()->getSerializedNotification(), count, &data);

        for (uint32_t i = 0; i < count; i++)
        {
            changes.emplace_back(data[i].port_id, data[i].port_state);
        }

        sai_deserialize_free_port_oper_status_ntf(count, data);
    }

    return changes;
}

TEST(SwitchStateBaseTunnel, p2pVxlanTunnelReportsOperUp)
{
    auto eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

    auto sc = std::make_shared<SwitchConfig>(0, "");

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_BCM56850;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
    sc->m_eventQueue = eventQueue;

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    const sai_object_id_t switchId = 0x2100000000;

    SwitchBCM56850 sw(switchId, std::make_shared<RealObjectIdManager>(0, scc), sc);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    ASSERT_EQ(sw.initialize_default_objects(1, &attr), SAI_STATUS_SUCCESS);

    attr.id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;
    attr.value.ptr = (void*)&dummyPortStateChangeNotify;

    ASSERT_EQ(sw.set(SAI_OBJECT_TYPE_SWITCH, switchId, &attr), SAI_STATUS_SUCCESS);

    portStateChanges(eventQueue);

    sai_ip_address_t dip;

    dip.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    dip.addr.ip4 = htonl(0x0a000002);

    sai_attribute_t attrs[3];

    attrs[0].id = SAI_TUNNEL_ATTR_TYPE;
    attrs[0].value.s32 = SAI_TUNNEL_TYPE_VXLAN;
    attrs[1].id = SAI_TUNNEL_ATTR_PEER_MODE;
    attrs[1].value.s32 = SAI_TUNNEL_PEER_MODE_P2MP;

    auto p2mp = sw.m_realObjectIdManager->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL, switchId);

    ASSERT_EQ(sw.create(SAI_OBJECT_TYPE_TUNNEL, sai_serialize_object_id(p2mp), switchId, 2, attrs), SAI_STATUS_SUCCESS);

    EXPECT_TRUE(portStateChanges(eventQueue).empty());

    attrs[1].value.s32 = SAI_TUNNEL_PEER_MODE_P2P;
    attrs[2].id = SAI_TUNNEL_ATTR_ENCAP_DST_IP;
    attrs[2].value.ipaddr = dip;

    auto p2p = sw.m_realObjectIdManager->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL, switchId);

    ASSERT_EQ(sw.create(SAI_OBJECT_TYPE_TUNNEL, sai_serialize_object_id(p2p), switchId, 3, attrs), SAI_STATUS_SUCCESS);

    auto changes = portStateChanges(eventQueue);

    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes[0].first, p2p);
    EXPECT_EQ(changes[0].second, SAI_PORT_OPER_STATUS_UP);
}
