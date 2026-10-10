#include "Sai.h"
#include "saivs.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>

#include <chrono>
#include <mutex>
#include <set>
#include <thread>

#include <memory>

using namespace saivs;

static const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
        return NULL;

    if (strcmp(variable, SAI_KEY_VS_SWITCH_TYPE) == 0)
        return SAI_VALUE_VS_SWITCH_TYPE_BCM56850;

    return nullptr;
}

static int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
        return 0;

    return -1;
}

static sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

static std::mutex g_operUpMutex;
static std::set<sai_object_id_t> g_operUpPortIds;

static bool isReportedOperUp(
        _In_ sai_object_id_t portId)
{
    SWSS_LOG_ENTER();

    std::lock_guard<std::mutex> lock(g_operUpMutex);

    return g_operUpPortIds.find(portId) != g_operUpPortIds.end();
}

static void onPortStateChange(
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();

    std::lock_guard<std::mutex> lock(g_operUpMutex);

    for (uint32_t i = 0; i < count; i++)
    {
        if (data[i].port_state == SAI_PORT_OPER_STATUS_UP)
        {
            g_operUpPortIds.insert(data[i].port_id);
        }
    }
}

TEST(Sai, bulkGet)
{
    Sai sai;

    sai.apiInitialize(0, &test_services);

    sai_attribute_t attr;

    sai_object_id_t switch_id;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(sai.create(SAI_OBJECT_TYPE_SWITCH, &switch_id, SAI_NULL_OBJECT_ID, 1, &attr), SAI_STATUS_SUCCESS);

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    EXPECT_EQ(sai.get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr), SAI_STATUS_SUCCESS);

    auto portNum = attr.value.u32;

    std::vector<sai_object_id_t> oids(portNum);

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = portNum;
    attr.value.objlist.list = oids.data();
    EXPECT_EQ(sai.get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr), SAI_STATUS_SUCCESS);

    std::vector<sai_attribute_t> attrs(portNum);
    std::vector<uint32_t> attrCounts(portNum, 1);
    std::vector<sai_status_t> statuses(portNum);
    std::vector<sai_attribute_t*> pattrs(portNum);
    for (size_t i = 0; i < portNum; i++)
    {
        attrs[i].id = SAI_PORT_ATTR_ADMIN_STATE;
        pattrs[i] = &attrs[i];
    }

    EXPECT_EQ(SAI_STATUS_SUCCESS,
            sai.bulkGet(
                SAI_OBJECT_TYPE_PORT,
                portNum,
                oids.data(),
                attrCounts.data(),
                pattrs.data(),
                SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR,
                statuses.data()));
}
TEST(Sai, fdbAgingWakeEvent)
{
    // Smoke test: verify the FDB aging thread starts and shuts down cleanly
    // without deadlocking. The aging thread's normal wake path (MAC event
    // delivery via m_fdbAgingWakeEvent) is NOT exercised here — this only
    // confirms the start/stop life cycle completes without hanging.
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    sai_attribute_t attr;
    sai_object_id_t switch_id = SAI_NULL_OBJECT_ID;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(sai.create(SAI_OBJECT_TYPE_SWITCH, &switch_id, SAI_NULL_OBJECT_ID, 1, &attr), SAI_STATUS_SUCCESS);
    EXPECT_NE(switch_id, SAI_NULL_OBJECT_ID);

    // Allow the aging thread to run at least one cycle.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // remove(SWITCH) triggers stopFdbAgingThread() which joins the thread.
    // If the wake/shutdown paths are broken this would hang; the test passing
    // confirms the aging thread terminates correctly.
    EXPECT_EQ(sai.remove(SAI_OBJECT_TYPE_SWITCH, switch_id), SAI_STATUS_SUCCESS);
}

TEST(Sai, p2pVxlanTunnelBridgePortOperUp)
{
    // The bridge port is created through the metadata, as syncd does: the
    // metadata must accept the create and the state change is delivered.
    Sai sai;

    ASSERT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    sai_attribute_t attrs[4];
    sai_object_id_t switch_id = SAI_NULL_OBJECT_ID;

    attrs[0].id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attrs[0].value.booldata = true;
    attrs[1].id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;
    attrs[1].value.ptr = (void*)&onPortStateChange;

    ASSERT_EQ(sai.create(SAI_OBJECT_TYPE_SWITCH, &switch_id, SAI_NULL_OBJECT_ID, 2, attrs), SAI_STATUS_SUCCESS);

    attrs[0].id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;
    attrs[1].id = SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID;

    ASSERT_EQ(sai.get(SAI_OBJECT_TYPE_SWITCH, switch_id, 2, attrs), SAI_STATUS_SUCCESS);

    auto vr = attrs[0].value.oid;
    auto bridge = attrs[1].value.oid;

    attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
    attrs[0].value.oid = vr;
    attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    attrs[1].value.s32 = SAI_ROUTER_INTERFACE_TYPE_LOOPBACK;

    sai_object_id_t rif = SAI_NULL_OBJECT_ID;

    ASSERT_EQ(sai.create(SAI_OBJECT_TYPE_ROUTER_INTERFACE, &rif, switch_id, 2, attrs), SAI_STATUS_SUCCESS);

    attrs[0].id = SAI_TUNNEL_ATTR_TYPE;
    attrs[0].value.s32 = SAI_TUNNEL_TYPE_VXLAN;
    attrs[1].id = SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE;
    attrs[1].value.oid = rif;
    attrs[2].id = SAI_TUNNEL_ATTR_PEER_MODE;
    attrs[2].value.s32 = SAI_TUNNEL_PEER_MODE_P2P;
    attrs[3].id = SAI_TUNNEL_ATTR_ENCAP_DST_IP;
    attrs[3].value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    attrs[3].value.ipaddr.addr.ip4 = htonl(0x0a000002);

    sai_object_id_t tunnel = SAI_NULL_OBJECT_ID;

    ASSERT_EQ(sai.create(SAI_OBJECT_TYPE_TUNNEL, &tunnel, switch_id, 4, attrs), SAI_STATUS_SUCCESS);

    attrs[0].id = SAI_BRIDGE_PORT_ATTR_TYPE;
    attrs[0].value.s32 = SAI_BRIDGE_PORT_TYPE_TUNNEL;
    attrs[1].id = SAI_BRIDGE_PORT_ATTR_TUNNEL_ID;
    attrs[1].value.oid = tunnel;
    attrs[2].id = SAI_BRIDGE_PORT_ATTR_BRIDGE_ID;
    attrs[2].value.oid = bridge;

    sai_object_id_t bridgePort = SAI_NULL_OBJECT_ID;

    ASSERT_EQ(sai.create(SAI_OBJECT_TYPE_BRIDGE_PORT, &bridgePort, switch_id, 3, attrs), SAI_STATUS_SUCCESS);

    EXPECT_FALSE(isReportedOperUp(tunnel));

    for (int i = 0; i < 500 && !isReportedOperUp(bridgePort); i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(isReportedOperUp(bridgePort));

    // the metadata knows the bridge port exactly once

    attrs[0].id = SAI_BRIDGE_PORT_ATTR_TUNNEL_ID;

    EXPECT_EQ(sai.get(SAI_OBJECT_TYPE_BRIDGE_PORT, bridgePort, 1, attrs), SAI_STATUS_SUCCESS);
    EXPECT_EQ(attrs[0].value.oid, tunnel);

    EXPECT_EQ(sai.remove(SAI_OBJECT_TYPE_BRIDGE_PORT, bridgePort), SAI_STATUS_SUCCESS);
    EXPECT_EQ(sai.remove(SAI_OBJECT_TYPE_TUNNEL, tunnel), SAI_STATUS_SUCCESS);
    EXPECT_EQ(sai.remove(SAI_OBJECT_TYPE_SWITCH, switch_id), SAI_STATUS_SUCCESS);
}
