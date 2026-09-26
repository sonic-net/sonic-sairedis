#include "vpp/SwitchVpp.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>

#include <array>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace saivs;

/*
 * Recording stubs for the VPP API. The unit test build links the test binary
 * with the linker's --wrap option for each of them, so SwitchVpp calls land
 * here instead of on a VPP socket.
 */
namespace
{
    struct VppCall
    {
        std::string api;
        std::string name;   // interface or table name
        uint32_t id;        // table id, VNI or instance
        uint32_t sub_id;
        bool flag;          // is_ipv6 or is_add
    };

    std::vector<VppCall> g_vppCalls;

    uint32_t g_nextSwIfIndex = 100;

    std::string macStr(
            const uint8_t *mac)
    {
        SWSS_LOG_ENTER();

        char buf[18];

        snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        return buf;
    }

    // position of the first recorded call matching api and name, or -1
    int vppCallIndex(
            const std::string& api,
            const std::string& name)
    {
        SWSS_LOG_ENTER();

        for (size_t i = 0; i < g_vppCalls.size(); i++)
        {
            if (g_vppCalls[i].api == api && g_vppCalls[i].name == name)
            {
                return (int)i;
            }
        }

        return -1;
    }

    std::vector<VppCall> vppCallsTo(
            const std::string& api)
    {
        SWSS_LOG_ENTER();

        std::vector<VppCall> calls;

        for (auto& call: g_vppCalls)
        {
            if (call.api == api)
            {
                calls.push_back(call);
            }
        }

        return calls;
    }
}

extern "C" {

int __wrap_init_vpp_client() { return 0; }
int __wrap_vpp_sync_for_events() { return 0; }
vpp_event_info_t * __wrap_vpp_ev_dequeue() { return NULL; }
int __wrap_vpp_want_l2_macs_events2(bool enable, vpp_mac_event_cb_fn cb, void *ctx) { return 0; }
int __wrap_refresh_interfaces_list() { return 0; }
int __wrap_set_sw_interface_l2_bridge(const char *hwif_name, uint32_t bridge_id, bool l2_enable, uint32_t port_type)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"set_sw_interface_l2_bridge", hwif_name ? hwif_name : "", bridge_id, port_type, l2_enable});
    return 0;
}

int __wrap_set_sw_interface_l2_bridge_by_index(uint32_t sw_if_index, uint32_t bridge_id, bool l2_enable, uint32_t port_type) { return 0; }
int __wrap_interface_set_state(const char *hwif_name, bool is_up) { return 0; }
int __wrap_sw_interface_set_mac_by_index(uint32_t sw_if_index, uint8_t *mac_address) { return 0; }
int __wrap_configure_lcp_interface(const char *hwif_name, const char *hostif_name, bool is_add)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"configure_lcp_interface",
            std::string(hwif_name ? hwif_name : "") + " " + (hostif_name ? hostif_name : ""), 0, 0, is_add});
    return 0;
}

int __wrap_interface_ip_address_add_del(const char *hw_ifname, vpp_ip_route_t *prefix, bool is_add) { return 0; }
int __wrap_vpp_bridge_domain_add_del(uint32_t bridge_id, bool is_add) { return 0; }

int __wrap_ip4_nbr_add_del(const char *hwif_name, uint32_t sw_if_index, struct sockaddr_in *addr,
        bool is_static, bool no_fib_entry, uint8_t *mac, bool is_add)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"ip4_nbr_add_del", macStr(mac), sw_if_index, 0, is_add});
    return 0;
}

int __wrap_ip6_nbr_add_del(const char *hwif_name, uint32_t sw_if_index, struct sockaddr_in6 *addr,
        bool is_static, bool no_fib_entry, uint8_t *mac, bool is_add)
{
    SWSS_LOG_ENTER();

    return 0;
}

int __wrap_ip_vrf_add(uint32_t vrf_id, const char *vrf_name, bool is_ipv6)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"ip_vrf_add", vrf_name ? vrf_name : "", vrf_id, 0, is_ipv6});
    return 0;
}

int __wrap_ip_vrf_del(uint32_t vrf_id, const char *vrf_name, bool is_ipv6)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"ip_vrf_del", vrf_name ? vrf_name : "", vrf_id, 0, is_ipv6});
    return 0;
}

int __wrap_vpp_ip_flow_hash_set(uint32_t vrf_id, uint32_t mask, int addr_family)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"vpp_ip_flow_hash_set", "", vrf_id, 0, addr_family == AF_INET6});
    return 0;
}

int __wrap_set_interface_vrf(const char *hwif_name, uint32_t sub_id, uint32_t vrf_id, bool is_ipv6)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"set_interface_vrf", hwif_name ? hwif_name : "", vrf_id, sub_id, is_ipv6});
    return 0;
}

int __wrap_create_bvi_interface(uint8_t *mac_address, uint32_t instance)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"create_bvi_interface", macStr(mac_address), instance, 0, true});
    return 0;
}

int __wrap_delete_bvi_interface(const char *hwif_name)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"delete_bvi_interface", hwif_name ? hwif_name : "", 0, 0, false});
    return 0;
}

int __wrap_vpp_vxlan_tunnel_add_del(vpp_vxlan_tunnel_t *tunnel, bool is_add, uint32_t *sw_if_index)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"vpp_vxlan_tunnel_add_del", "", tunnel->vni, 0, is_add});

    if (is_add)
    {
        *sw_if_index = g_nextSwIfIndex++;
    }

    return 0;
}

}

TEST(SwitchVpp, getLagMemberEgressDisableAction)
{
    using Action = SwitchVpp::LagMemberEgressDisableAction;

    EXPECT_EQ(Action::NONE, SwitchVpp::getLagMemberEgressDisableAction(false, true, false));
    EXPECT_EQ(Action::DISABLE, SwitchVpp::getLagMemberEgressDisableAction(true, true, false));
    EXPECT_EQ(Action::ENABLE, SwitchVpp::getLagMemberEgressDisableAction(false, true, true));
    EXPECT_EQ(Action::NONE, SwitchVpp::getLagMemberEgressDisableAction(true, true, true));
    EXPECT_EQ(Action::NONE, SwitchVpp::getLagMemberEgressDisableAction(false, false, false));
    EXPECT_EQ(Action::DISABLE, SwitchVpp::getLagMemberEgressDisableAction(true, false, false));
}

/*
 * The unit tests are built with access control checking disabled, so the
 * fixture reaches the protected and private SwitchVpp members directly.
 */
class SwitchVppVrf : public ::testing::Test
{
    protected:

        void SetUp() override
        {
            auto sc = std::make_shared<SwitchConfig>(0, "");

            sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
            sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
            sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
            sc->m_useTapDevice = true;
            sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
            sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

            auto scc = std::make_shared<SwitchConfigContainer>();

            scc->insert(sc);

            m_mgr = std::make_shared<RealObjectIdManager>(0, scc);
            m_sw = std::make_shared<SwitchVpp>(m_switchId, m_mgr, sc);

            ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_default_virtual_router());

            sai_attribute_t attr;

            attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;

            ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), 1, &attr));

            m_defaultVr = attr.value.oid;

            g_vppCalls.clear();
        }

        void TearDown() override
        {
            m_sw.reset();
        }

        sai_object_id_t createVr()
        {
            SWSS_LOG_ENTER();

            auto vr = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, m_switchId);

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, sai_serialize_object_id(vr), m_switchId, 0, nullptr));

            return vr;
        }

        // A virtual router that exists in SAI but was never given a VPP table.
        sai_object_id_t createVrWithoutTable()
        {
            SWSS_LOG_ENTER();

            auto vr = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, m_switchId);

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, sai_serialize_object_id(vr), m_switchId, 0, nullptr));

            return vr;
        }

        uint32_t tableOf(
                sai_object_id_t vr)
        {
            SWSS_LOG_ENTER();

            auto vrf = m_sw->vpp_get_ip_vrf(vr);

            EXPECT_NE(nullptr, vrf);

            return vrf ? vrf->m_vrf_id : UINT32_MAX;
        }

        sai_status_t createVlanRif(
                sai_object_id_t vr,
                uint16_t vlanId)
        {
            SWSS_LOG_ENTER();

            auto vlan = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, m_switchId);

            sai_attribute_t vattr;

            vattr.id = SAI_VLAN_ATTR_VLAN_ID;
            vattr.value.u16 = vlanId;

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_VLAN, sai_serialize_object_id(vlan), m_switchId, 1, &vattr));

            sai_attribute_t attrs[4];
            sai_mac_t mac = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };

            attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
            attrs[0].value.s32 = SAI_ROUTER_INTERFACE_TYPE_VLAN;
            attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
            attrs[1].value.oid = vr;
            attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;
            attrs[2].value.oid = vlan;
            attrs[3].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;
            memcpy(attrs[3].value.mac, mac, sizeof(sai_mac_t));

            auto rif = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_switchId);

            return m_sw->create(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(rif), m_switchId, 4, attrs);
        }

        const sai_object_id_t m_switchId = 0x2100000000;

        std::shared_ptr<RealObjectIdManager> m_mgr;

        std::shared_ptr<SwitchVpp> m_sw;

        sai_object_id_t m_defaultVr = SAI_NULL_OBJECT_ID;
};

TEST_F(SwitchVppVrf, DefaultVirtualRouterUsesTableZero)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, createVlanRif(m_defaultVr, 100));

    EXPECT_TRUE(vppCallsTo("ip_vrf_add").empty());
    EXPECT_TRUE(vppCallsTo("set_interface_vrf").empty());

    EXPECT_EQ(0u, tableOf(m_defaultVr));

    // registering the default table is what sets its flow hash
    auto hashes = vppCallsTo("vpp_ip_flow_hash_set");

    ASSERT_EQ(2u, hashes.size());
    EXPECT_EQ(0u, hashes[0].id);
    EXPECT_EQ(0u, hashes[1].id);
}

TEST_F(SwitchVppVrf, VirtualRouterGetsItsOwnTable)
{
    const uint32_t base = SwitchVpp::vrf_table_id_base;

    auto vr1 = createVr();
    auto vr2 = createVr();

    EXPECT_EQ(base, tableOf(vr1));
    EXPECT_EQ(base + 1, tableOf(vr2));

    // both address families, named after the table id
    auto adds = vppCallsTo("ip_vrf_add");

    ASSERT_EQ(4u, adds.size());
    EXPECT_EQ(base, adds[0].id);
    EXPECT_FALSE(adds[0].flag);
    EXPECT_EQ("vrf_" + std::to_string(base), adds[0].name);
    EXPECT_EQ(base, adds[1].id);
    EXPECT_TRUE(adds[1].flag);
    EXPECT_EQ("vrf_" + std::to_string(base), m_sw->vpp_get_ip_vrf(vr1)->m_vrf_name);
}

TEST_F(SwitchVppVrf, VlanRouterInterfaceBindsBviToItsVirtualRoutersTable)
{
    auto vr = createVr();
    uint32_t table = tableOf(vr);

    g_vppCalls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, createVlanRif(vr, 100));

    ASSERT_EQ(1u, vppCallsTo("create_bvi_interface").size());

    auto binds = vppCallsTo("set_interface_vrf");

    ASSERT_EQ(2u, binds.size());

    for (auto& bind: binds)
    {
        EXPECT_EQ("bvi100", bind.name);
        EXPECT_EQ(0u, bind.sub_id);
        EXPECT_EQ(table, bind.id);
    }

    EXPECT_FALSE(binds[0].flag);
    EXPECT_TRUE(binds[1].flag);

    // the table was created with the virtual router, not with the interface
    EXPECT_TRUE(vppCallsTo("ip_vrf_add").empty());
}

TEST_F(SwitchVppVrf, SubPortBindsToTheAllocatedTable)
{
    auto vr = createVr();
    uint32_t table = tableOf(vr);

    g_vppCalls.clear();

    // the port and sub-port router interface path
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_router_interface_set_vrf(vr, "GigabitEthernet0/8/0", 100, "Ethernet0.100"));

    auto binds = vppCallsTo("set_interface_vrf");

    ASSERT_EQ(2u, binds.size());
    EXPECT_EQ("GigabitEthernet0/8/0", binds[0].name);
    EXPECT_EQ(100u, binds[0].sub_id);
    EXPECT_EQ(table, binds[0].id);
    EXPECT_EQ(table, binds[1].id);
}

TEST_F(SwitchVppVrf, UnknownVirtualRouterIsNotCachedAsTableZero)
{
    // No VPP table and no kernel VRF to fall back to (the host interface does
    // not exist here): the interface stays in table 0, but the virtual router
    // must not be remembered as table 0 for later interfaces.
    auto vr = createVrWithoutTable();

    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_router_interface_set_vrf(vr, "bvi200", 0, "Vlan200"));

    EXPECT_TRUE(vppCallsTo("set_interface_vrf").empty());
    EXPECT_EQ(nullptr, m_sw->vpp_get_ip_vrf(vr));
}

TEST_F(SwitchVppVrf, RemovingVirtualRouterReleasesItsTable)
{
    auto vr1 = createVr();
    auto vr2 = createVr();
    uint32_t table1 = tableOf(vr1);

    g_vppCalls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, sai_serialize_object_id(vr1)));

    auto dels = vppCallsTo("ip_vrf_del");

    ASSERT_EQ(2u, dels.size());
    EXPECT_EQ(table1, dels[0].id);
    EXPECT_FALSE(dels[0].flag);
    EXPECT_EQ(table1, dels[1].id);
    EXPECT_TRUE(dels[1].flag);

    EXPECT_EQ(nullptr, m_sw->vpp_get_ip_vrf(vr1));
    EXPECT_NE(nullptr, m_sw->vpp_get_ip_vrf(vr2));

    // the released id is handed out again
    auto vr3 = createVr();

    EXPECT_EQ(table1, tableOf(vr3));
}

TEST_F(SwitchVppVrf, LinuxNlKeepsKernelTableIds)
{
    // NO_LINUX_NL=n: the linux control plane plugin syncs routes into tables
    // keyed by the kernel table id, so saivpp must not allocate one of its own.
    m_sw->nbr_env_read = true;
    m_sw->nbr_active = false;

    auto vr = createVr();

    EXPECT_TRUE(vppCallsTo("ip_vrf_add").empty());
    EXPECT_EQ(nullptr, m_sw->vpp_get_ip_vrf(vr));
}

/*
 * One VXLAN tunnel with a VR->VNI encap mapper holding three entries: two
 * tenant virtual routers with tables, and one virtual router without.
 */
class SwitchVppTunnelNexthop : public SwitchVppVrf
{
    protected:

        void SetUp() override
        {
            SwitchVppVrf::SetUp();

            if (HasFatalFailure())
            {
                return;
            }

            m_vrA = createVr();
            m_vrB = createVr();
            m_vrUnknown = createVrWithoutTable();

            auto map = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL_MAP, m_switchId);

            sai_attribute_t mattr;

            mattr.id = SAI_TUNNEL_MAP_ATTR_TYPE;
            mattr.value.s32 = SAI_TUNNEL_MAP_TYPE_VIRTUAL_ROUTER_ID_TO_VNI;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_TUNNEL_MAP, sai_serialize_object_id(map), m_switchId, 1, &mattr));

            addMapEntry(map, m_vrA, 1000);
            addMapEntry(map, m_vrB, 2000);
            addMapEntry(map, m_vrUnknown, 3000);

            m_tunnel = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL, m_switchId);

            sai_attribute_t tattrs[3];

            tattrs[0].id = SAI_TUNNEL_ATTR_TYPE;
            tattrs[0].value.s32 = SAI_TUNNEL_TYPE_VXLAN;
            tattrs[1].id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
            tattrs[1].value.ipaddr = ipv4("10.0.0.1");
            tattrs[2].id = SAI_TUNNEL_ATTR_ENCAP_MAPPERS;
            tattrs[2].value.objlist.count = 1;
            tattrs[2].value.objlist.list = &map;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_TUNNEL, sai_serialize_object_id(m_tunnel), m_switchId, 3, tattrs));

            // stand-ins for the kernel side of the decap host path
            m_sw->m_tunnel_mgr.m_netdev_master = [this](const std::string& netdev)
            {
                SWSS_LOG_ENTER();

                auto it = m_kernelMasters.find(netdev);

                return it == m_kernelMasters.end() ? std::string() : it->second;
            };

            m_sw->m_tunnel_mgr.m_add_tc_redirect = [this](const std::string& tap, const std::string& dev)
            {
                SWSS_LOG_ENTER();

                m_redirects.push_back(tap + " " + dev);

                return SAI_STATUS_SUCCESS;
            };

            g_vppCalls.clear();
        }

        void setSwitchMac(
                const std::array<uint8_t, 6>& mac)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;
            memcpy(attr.value.mac, mac.data(), mac.size());

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->set_internal(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), &attr));
        }

        // name of the one decap BVI created since the last clear
        std::string decapBvi()
        {
            SWSS_LOG_ENTER();

            auto bvis = vppCallsTo("create_bvi_interface");

            EXPECT_EQ(1u, bvis.size());

            return bvis.empty() ? std::string() : "bvi" + std::to_string(bvis[0].id);
        }

        static sai_ip_address_t ipv4(
                const char *addr)
        {
            SWSS_LOG_ENTER();

            sai_ip_address_t ip;

            memset(&ip, 0, sizeof(ip));
            ip.addr_family = SAI_IP_ADDR_FAMILY_IPV4;

            EXPECT_EQ(1, inet_pton(AF_INET, addr, &ip.addr.ip4));

            return ip;
        }

        void addMapEntry(
                sai_object_id_t map,
                sai_object_id_t vr,
                uint32_t vni)
        {
            SWSS_LOG_ENTER();

            auto entry = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, m_switchId);

            sai_attribute_t eattrs[4];

            eattrs[0].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP_TYPE;
            eattrs[0].value.s32 = SAI_TUNNEL_MAP_TYPE_VIRTUAL_ROUTER_ID_TO_VNI;
            eattrs[1].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP;
            eattrs[1].value.oid = map;
            eattrs[2].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VIRTUAL_ROUTER_ID_KEY;
            eattrs[2].value.oid = vr;
            eattrs[3].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VNI_ID_VALUE;
            eattrs[3].value.u32 = vni;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, sai_serialize_object_id(entry), m_switchId, 4, eattrs));
        }

        sai_status_t createNexthop(
                uint32_t vni,
                sai_object_id_t& nh,
                const uint8_t *remoteRouterMac = nullptr)
        {
            SWSS_LOG_ENTER();

            nh = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_NEXT_HOP, m_switchId);

            sai_attribute_t attrs[5];
            uint32_t count = 4;

            attrs[0].id = SAI_NEXT_HOP_ATTR_TYPE;
            attrs[0].value.s32 = SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP;
            attrs[1].id = SAI_NEXT_HOP_ATTR_IP;
            attrs[1].value.ipaddr = ipv4("10.0.0.2");
            attrs[2].id = SAI_NEXT_HOP_ATTR_TUNNEL_ID;
            attrs[2].value.oid = m_tunnel;
            attrs[3].id = SAI_NEXT_HOP_ATTR_TUNNEL_VNI;
            attrs[3].value.u32 = vni;

            if (remoteRouterMac)
            {
                attrs[4].id = SAI_NEXT_HOP_ATTR_TUNNEL_MAC;
                memcpy(attrs[4].value.mac, remoteRouterMac, sizeof(sai_mac_t));
                count = 5;
            }

            return m_sw->create(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh), m_switchId, count, attrs);
        }

        sai_object_id_t m_vrA = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_vrB = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_vrUnknown = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_tunnel = SAI_NULL_OBJECT_ID;

        std::map<std::string, std::string> m_kernelMasters;

        std::vector<std::string> m_redirects;
};

namespace
{
    // what an EVPN type-5 next hop carries: the remote VTEP's router MAC
    const uint8_t REMOTE_ROUTER_MAC[6] = { 0x22, 0x36, 0x29, 0xc2, 0x14, 0xf1 };
}

TEST_F(SwitchVppTunnelNexthop, UsesTheMapEntryOfItsVni)
{
    sai_object_id_t nh;

    // the entry of a virtual router without a table is present, and ignored
    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(2000, nh));

    auto tunnels = vppCallsTo("vpp_vxlan_tunnel_add_del");

    ASSERT_EQ(1u, tunnels.size());
    EXPECT_EQ(2000u, tunnels[0].id);
    EXPECT_TRUE(tunnels[0].flag);

    // the decap BVI routes in the table of the VNI's virtual router
    uint32_t table = tableOf(m_vrB);
    auto binds = vppCallsTo("set_interface_vrf");

    ASSERT_EQ(2u, binds.size());
    EXPECT_EQ(table, binds[0].id);
    EXPECT_FALSE(binds[0].flag);
    EXPECT_EQ(table, binds[1].id);
    EXPECT_TRUE(binds[1].flag);

    uint32_t sw_if_index = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh, sw_if_index));
}

TEST_F(SwitchVppTunnelNexthop, RemoveTearsDownTheTunnelOfItsVni)
{
    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(1000, nh));

    g_vppCalls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh)));

    auto tunnels = vppCallsTo("vpp_vxlan_tunnel_add_del");

    ASSERT_EQ(1u, tunnels.size());
    EXPECT_EQ(1000u, tunnels[0].id);
    EXPECT_FALSE(tunnels[0].flag);

    EXPECT_EQ(1u, vppCallsTo("delete_bvi_interface").size());

    uint32_t sw_if_index = 0;

    EXPECT_NE(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh, sw_if_index));
}

TEST_F(SwitchVppTunnelNexthop, FailsWhenItsVniHasNoUsableVirtualRouter)
{
    sai_object_id_t nh;

    // the VNI's virtual router has no table yet
    EXPECT_NE(SAI_STATUS_SUCCESS, createNexthop(3000, nh));

    // no map entry carries the VNI at all
    EXPECT_NE(SAI_STATUS_SUCCESS, createNexthop(4000, nh));

    EXPECT_TRUE(vppCallsTo("vpp_vxlan_tunnel_add_del").empty());
}

TEST_F(SwitchVppTunnelNexthop, EncapsulatesToTheRouterMacOfTheRemoteVtep)
{
    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(2000, nh, REMOTE_ROUTER_MAC));

    auto nbrs = vppCallsTo("ip4_nbr_add_del");

    ASSERT_EQ(1u, nbrs.size());
    EXPECT_EQ("22:36:29:c2:14:f1", nbrs[0].name);
    EXPECT_TRUE(nbrs[0].flag);

    g_vppCalls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh)));

    // the remove takes out the neighbour the create added
    nbrs = vppCallsTo("ip4_nbr_add_del");

    ASSERT_EQ(1u, nbrs.size());
    EXPECT_EQ("22:36:29:c2:14:f1", nbrs[0].name);
    EXPECT_FALSE(nbrs[0].flag);
}

TEST_F(SwitchVppTunnelNexthop, DecapBviAnswersToTheSwitchMac)
{
    setSwitchMac({ 0x22, 0xfd, 0xe1, 0x99, 0x49, 0x9e });

    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(2000, nh, REMOTE_ROUTER_MAC));

    auto bvis = vppCallsTo("create_bvi_interface");

    ASSERT_EQ(1u, bvis.size());
    EXPECT_EQ("22:fd:e1:99:49:9e", bvis[0].name);
}

TEST_F(SwitchVppTunnelNexthop, WithoutARemoteRouterMacBothSidesKeepTheSharedOne)
{
    setSwitchMac({ 0x22, 0xfd, 0xe1, 0x99, 0x49, 0x9e });

    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(2000, nh));

    auto nbrs = vppCallsTo("ip4_nbr_add_del");

    ASSERT_EQ(1u, nbrs.size());
    EXPECT_EQ("00:00:00:00:00:01", nbrs[0].name);

    auto bvis = vppCallsTo("create_bvi_interface");

    ASSERT_EQ(1u, bvis.size());
    EXPECT_EQ("00:00:00:00:00:01", bvis[0].name);
}

TEST_F(SwitchVppTunnelNexthop, DecapBviGetsAHostPathIntoItsKernelVrf)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, createVlanRif(m_vrB, 3000));

    m_kernelMasters["Vlan3000"] = "Vrft";

    g_vppCalls.clear();

    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(2000, nh, REMOTE_ROUTER_MAC));

    auto bvi = decapBvi();

    int pair = vppCallIndex("configure_lcp_interface", bvi + " tap_" + bvi);
    int bridge = vppCallIndex("set_sw_interface_l2_bridge", bvi);

    ASSERT_NE(-1, pair);
    ASSERT_NE(-1, bridge);
    EXPECT_TRUE(g_vppCalls[pair].flag);

    // paired before it becomes its bridge domain's BVI, or sonic_ext steers
    // the punts to the underlay port's tap
    EXPECT_LT(pair, bridge);

    ASSERT_EQ(1u, m_redirects.size());
    EXPECT_EQ("tap_" + bvi + " Vrft", m_redirects[0]);
}

TEST_F(SwitchVppTunnelNexthop, NoKernelVrfMeansNoHostPath)
{
    // the VR has a VLAN router interface, but its SVI is in no kernel VRF
    ASSERT_EQ(SAI_STATUS_SUCCESS, createVlanRif(m_vrB, 3000));

    g_vppCalls.clear();

    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(2000, nh, REMOTE_ROUTER_MAC));

    EXPECT_TRUE(vppCallsTo("configure_lcp_interface").empty());
    EXPECT_TRUE(m_redirects.empty());
}

TEST_F(SwitchVppTunnelNexthop, DoesNotBorrowTheKernelVrfOfAnotherVirtualRouter)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, createVlanRif(m_vrA, 100));

    m_kernelMasters["Vlan100"] = "VrfA";

    g_vppCalls.clear();

    sai_object_id_t nh;

    // VNI 2000 belongs to VR B
    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(2000, nh, REMOTE_ROUTER_MAC));

    EXPECT_TRUE(vppCallsTo("configure_lcp_interface").empty());
    EXPECT_TRUE(m_redirects.empty());
}

TEST_F(SwitchVppTunnelNexthop, RemoveTearsDownTheHostPath)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, createVlanRif(m_vrB, 3000));

    m_kernelMasters["Vlan3000"] = "Vrft";

    g_vppCalls.clear();

    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(2000, nh, REMOTE_ROUTER_MAC));

    auto bvi = decapBvi();

    g_vppCalls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh)));

    int unpair = vppCallIndex("configure_lcp_interface", bvi + " tap_" + bvi);
    int del = vppCallIndex("delete_bvi_interface", bvi);

    ASSERT_NE(-1, unpair);
    ASSERT_NE(-1, del);
    EXPECT_FALSE(g_vppCalls[unpair].flag);
    EXPECT_LT(unpair, del);
}
