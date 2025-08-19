#include <cstdint>

#include <memory>
#include <vector>
#include <array>

#include <gtest/gtest.h>
#include "Workaround.h"
#include "swss/logger.h"

#include <arpa/inet.h>

using namespace syncd;

TEST(Workaround, isSetAttributeWorkaround)
{
    ASSERT_EQ(Workaround::isSetAttributeWorkaround(SAI_OBJECT_TYPE_HOSTIF, SAI_HOSTIF_ATTR_QUEUE, SAI_STATUS_FAILURE), true);
    ASSERT_EQ(Workaround::isSetAttributeWorkaround(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_SRC_MAC_ADDRESS, SAI_STATUS_FAILURE), true);
    ASSERT_EQ(Workaround::isSetAttributeWorkaround(SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_TYPE, SAI_STATUS_FAILURE), false);
    ASSERT_EQ(Workaround::isSetAttributeWorkaround(SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_TYPE, SAI_STATUS_SUCCESS), false);
    ASSERT_EQ(Workaround::isSetAttributeWorkaround(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_VXLAN_DEFAULT_ROUTER_MAC, SAI_STATUS_FAILURE), true);
}

TEST(Workaround, isSetAttributeWorkaroundDuringApplyView) {

    sai_object_id_t ipInIpTunnelOid = 0x2a000000000001;
    sai_object_id_t vxlanTunnelOid = 0x2a000000000002;

    std::stringstream ipInIpTunnelOidSS, vxlanTunnelOidSS;
    ipInIpTunnelOidSS << "oid:0x" << std::hex << ipInIpTunnelOid;
    vxlanTunnelOidSS << "oid:0x" << std::hex << vxlanTunnelOid;

    auto ipInIpTunnelOidStr = ipInIpTunnelOidSS.str();
    auto vxlanTunnelOidStr = vxlanTunnelOidSS.str();

    swss::TableDump dump = {
        // Required Switch initialisation
        {"SAI_OBJECT_TYPE_SWITCH:oid:0x21000000000000", {
            {"SAI_SWITCH_ATTR_ECMP_DEFAULT_HASH_SEED", "0"},
            {"SAI_SWITCH_ATTR_FDB_AGING_TIME", "600"},
            {"SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY", "0x55a14f029dc0"},
            {"SAI_SWITCH_ATTR_INIT_SWITCH", "true"},
            {"SAI_SWITCH_ATTR_LAG_DEFAULT_HASH_SEED", "0"},
            {"SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY", "0x55a14f029dd0"},
            {"SAI_SWITCH_ATTR_QOS_DSCP_TO_TC_MAP", "oid:0x1400000004a417"},
            {"SAI_SWITCH_ATTR_SRC_MAC_ADDRESS", "02:00:00:00:00:01"},
            {"SAI_SWITCH_ATTR_SWITCH_SHUTDOWN_REQUEST_NOTIFY", "0x55a14f029df0"}
        }},
        // SAI_OBJECT_TUNNEL without type VXLAN
        {"SAI_OBJECT_TYPE_TUNNEL:" + ipInIpTunnelOidStr, {
            {"SAI_TUNNEL_ATTR_DECAP_DSCP_MODE", "SAI_TUNNEL_DSCP_MODE_PIPE_MODEL"},
            {"SAI_TUNNEL_ATTR_DECAP_ECN_MODE", "SAI_TUNNEL_DECAP_ECN_MODE_COPY_FROM_OUTER"},
            {"SAI_TUNNEL_ATTR_DECAP_TTL_MODE", "SAI_TUNNEL_TTL_MODE_PIPE_MODEL"},
            {"SAI_TUNNEL_ATTR_ENCAP_TTL_MODE", "SAI_TUNNEL_TTL_MODE_PIPE_MODEL"},
            {"SAI_TUNNEL_ATTR_OVERLAY_INTERFACE", "oid:0x2a000000000003"},
            {"SAI_TUNNEL_ATTR_TYPE", "SAI_TUNNEL_TYPE_IPINIP"},
            {"SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE", "oid:0x2a000000000004"},
        }},
        // SAI_OBJECT_TUNNEL with type VXLAN
        {"SAI_OBJECT_TYPE_TUNNEL:" + vxlanTunnelOidStr, {
            {"SAI_TUNNEL_ATTR_DECAP_MAPPERS", "2:oid:0x2a000000000005,oid:0x2a000000000006"},
            {"SAI_TUNNEL_ATTR_ENCAP_MAPPERS", "2:oid:0x2a000000000007,oid:0x2a000000000008"},
            {"SAI_TUNNEL_ATTR_ENCAP_SRC_IP", "192.0.2.1"},
            {"SAI_TUNNEL_ATTR_PEER_MODE", "SAI_TUNNEL_PEER_MODE_P2MP"},
            {"SAI_TUNNEL_ATTR_TYPE", "SAI_TUNNEL_TYPE_VXLAN"},
            {"SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE", "oid:0x2a000000000009"},
            {"SAI_TUNNEL_ATTR_ENCAP_TTL_MODE", "SAI_TUNNEL_TTL_MODE_PIPE_MODEL"},
            {"SAI_TUNNEL_ATTR_ENCAP_TTL_VAL", "255"}
        }}
    };
    AsicView currentView;
    currentView.fromDump(dump);

    ASSERT_EQ(Workaround::isSetAttributeWorkaroundDuringApplyView(currentView, ipInIpTunnelOid, SAI_TUNNEL_ATTR_DECAP_TTL_MODE, SAI_STATUS_SUCCESS), false);
    ASSERT_EQ(Workaround::isSetAttributeWorkaroundDuringApplyView(currentView, ipInIpTunnelOid, SAI_TUNNEL_ATTR_DECAP_TTL_MODE, SAI_STATUS_FAILURE), false);
    ASSERT_EQ(Workaround::isSetAttributeWorkaroundDuringApplyView(currentView, ipInIpTunnelOid, SAI_TUNNEL_ATTR_ENCAP_TTL_MODE, SAI_STATUS_SUCCESS), false);
    ASSERT_EQ(Workaround::isSetAttributeWorkaroundDuringApplyView(currentView, ipInIpTunnelOid, SAI_TUNNEL_ATTR_ENCAP_TTL_MODE, SAI_STATUS_FAILURE), false);

    ASSERT_EQ(Workaround::isSetAttributeWorkaroundDuringApplyView(currentView, vxlanTunnelOid, SAI_TUNNEL_ATTR_PEER_MODE, SAI_STATUS_SUCCESS), false);
    ASSERT_EQ(Workaround::isSetAttributeWorkaroundDuringApplyView(currentView, vxlanTunnelOid, SAI_TUNNEL_ATTR_PEER_MODE, SAI_STATUS_FAILURE), false);
    ASSERT_EQ(Workaround::isSetAttributeWorkaroundDuringApplyView(currentView, vxlanTunnelOid, SAI_TUNNEL_ATTR_ENCAP_TTL_MODE, SAI_STATUS_SUCCESS), false);
    ASSERT_EQ(Workaround::isSetAttributeWorkaroundDuringApplyView(currentView, vxlanTunnelOid, SAI_TUNNEL_ATTR_ENCAP_TTL_MODE, SAI_STATUS_FAILURE), true);

    // Non-existent OID
    ASSERT_EQ(Workaround::isSetAttributeWorkaroundDuringApplyView(currentView, 0x2a0000deadbeef, SAI_TUNNEL_ATTR_ENCAP_TTL_MODE, SAI_STATUS_FAILURE), false);
}

TEST(Workaround,convertPortOperStatusNotification)
{
    sai_port_oper_status_notification_t data[2];

    ASSERT_EQ(Workaround::convertPortOperStatusNotification(0, nullptr, SAI_API_VERSION).size(), 0);
    ASSERT_EQ(Workaround::convertPortOperStatusNotification(5000, data, SAI_API_VERSION).size(), 0);

    ASSERT_EQ(Workaround::convertPortOperStatusNotification(2, data, SAI_VERSION(1,15,0)).size(), 2);
    ASSERT_EQ(Workaround::convertPortOperStatusNotification(2, data, SAI_VERSION(1,14,1)).size(), 2);

    // check new structure notifications

    data[0].port_id = 12;
    data[0].port_state = SAI_PORT_OPER_STATUS_DOWN;
    data[0].port_error_status = SAI_PORT_ERROR_STATUS_HIGH_BER;
    data[1].port_id = 22;
    data[1].port_state = SAI_PORT_OPER_STATUS_UP;
    data[1].port_error_status = SAI_PORT_ERROR_STATUS_DATA_UNIT_MISALIGNMENT_ERROR;

    auto ntf = Workaround::convertPortOperStatusNotification(2, data, SAI_VERSION(1,14,1));

    ASSERT_EQ(ntf[0].port_id, 12);
    ASSERT_EQ(ntf[0].port_state, SAI_PORT_OPER_STATUS_DOWN);
    ASSERT_EQ(ntf[0].port_error_status, SAI_PORT_ERROR_STATUS_HIGH_BER);
    ASSERT_EQ(ntf[1].port_id, 22);
    ASSERT_EQ(ntf[1].port_state, SAI_PORT_OPER_STATUS_UP);
    ASSERT_EQ(ntf[1].port_error_status, SAI_PORT_ERROR_STATUS_DATA_UNIT_MISALIGNMENT_ERROR);

    // check old structure notification
    Workaround::sai_port_oper_status_notification_v1_14_0_t old[2];

    old[0].port_id = 42;
    old[0].port_state = SAI_PORT_OPER_STATUS_UP;
    old[1].port_id = 43;
    old[1].port_state = SAI_PORT_OPER_STATUS_DOWN;

    auto ntf2 = Workaround::convertPortOperStatusNotification(2, reinterpret_cast<sai_port_oper_status_notification_t*>(old), SAI_VERSION(1,14,0));

    ASSERT_EQ(ntf.size(), 2);

    ASSERT_EQ(ntf2[0].port_id, 42);
    ASSERT_EQ(ntf2[0].port_state, SAI_PORT_OPER_STATUS_UP);
    ASSERT_EQ(ntf2[0].port_error_status, 0);
    ASSERT_EQ(ntf2[1].port_id, 43);
    ASSERT_EQ(ntf2[1].port_state, SAI_PORT_OPER_STATUS_DOWN);
    ASSERT_EQ(ntf2[1].port_error_status, 0);
}
