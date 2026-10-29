#include "SwitchVppTestHarness.h"

#include "meta/sai_serialize.h"

#include <arpa/inet.h>

#include <cstring>
#include <string>
#include <vector>

using namespace saivs;
using namespace saivs_test;

/*
 * A static NAT of a host. Host 10.0.1.3 sits behind a zone 0 router
 * interface, and the VLAN 200 router interface is zone 1. A static NAT of the
 * host to 192.168.1.100 makes orchagent create a SOURCE_NAT entry (source
 * 10.0.1.3 to .100), a DESTINATION_NAT entry (destination .100 to 10.0.1.3)
 * and a DESTINATION_NAT_POOL entry (.100).
 */
class SwitchVppNatTest : public SwitchVppTest
{
    protected:

        void SetUp() override
        {
            SWSS_LOG_ENTER();

            vppMock().natSupported = true;
            vppMock().natSessions.clear();

            SwitchVppTest::SetUp();
        }

        static uint32_t ip(
                const char *addr)
        {
            SWSS_LOG_ENTER();

            struct in_addr a;

            EXPECT_EQ(1, inet_pton(AF_INET, addr, &a));

            return a.s_addr;
        }

        sai_status_t enableNat(
                bool enable)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = SAI_SWITCH_ATTR_NAT_ENABLE;
            attr.value.booldata = enable;

            return setSwitch(attr);
        }

        sai_status_t createZonedVlanRif(
                uint16_t vlanId,
                uint32_t zone,
                sai_object_id_t *rifOut)
        {
            SWSS_LOG_ENTER();

            auto vlan = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, m_switchId);

            sai_attribute_t vattr;

            vattr.id = SAI_VLAN_ATTR_VLAN_ID;
            vattr.value.u16 = vlanId;

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_VLAN, sai_serialize_object_id(vlan), m_switchId, 1, &vattr));

            sai_attribute_t attrs[5];
            sai_mac_t mac = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };

            attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
            attrs[0].value.s32 = SAI_ROUTER_INTERFACE_TYPE_VLAN;
            attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
            attrs[1].value.oid = m_defaultVr;
            attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;
            attrs[2].value.oid = vlan;
            attrs[3].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;
            memcpy(attrs[3].value.mac, mac, sizeof(sai_mac_t));
            attrs[4].id = SAI_ROUTER_INTERFACE_ATTR_NAT_ZONE_ID;
            attrs[4].value.u32 = zone;

            *rifOut = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_switchId);

            return m_sw->create(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(*rifOut), m_switchId, 5, attrs);
        }

        sai_status_t setZone(
                sai_object_id_t rif,
                uint32_t zone)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = SAI_ROUTER_INTERFACE_ATTR_NAT_ZONE_ID;
            attr.value.u32 = zone;

            return m_sw->set(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(rif), &attr);
        }

        std::string natEntryId(
                sai_nat_type_t type,
                const char *src,
                const char *dst,
                uint8_t proto = 0,
                uint16_t sport = 0,
                uint16_t dport = 0)
        {
            SWSS_LOG_ENTER();

            sai_nat_entry_t e;

            memset(&e, 0, sizeof(e));
            e.switch_id = m_switchId;
            e.vr_id = m_defaultVr;
            e.nat_type = type;

            if (src)
            {
                e.data.key.src_ip = ip(src);
                e.data.mask.src_ip = 0xffffffff;
            }

            if (dst)
            {
                e.data.key.dst_ip = ip(dst);
                e.data.mask.dst_ip = 0xffffffff;
            }

            if (proto)
            {
                e.data.key.proto = proto;
                e.data.mask.proto = 0xff;
            }

            if (sport)
            {
                e.data.key.l4_src_port = sport;
                e.data.mask.l4_src_port = 0xffff;
            }

            if (dport)
            {
                e.data.key.l4_dst_port = dport;
                e.data.mask.l4_dst_port = 0xffff;
            }

            return sai_serialize_nat_entry(e);
        }

        // the orchagent's SOURCE_NAT entry: SRC_IP, SRC_IP_MASK, counters on
        sai_status_t createSnat(
                const std::string& id,
                const char *translated,
                uint16_t port = 0)
        {
            SWSS_LOG_ENTER();

            std::vector<sai_attribute_t> attrs(4);

            attrs[0].id = SAI_NAT_ENTRY_ATTR_SRC_IP;
            attrs[0].value.ip4 = ip(translated);
            attrs[1].id = SAI_NAT_ENTRY_ATTR_SRC_IP_MASK;
            attrs[1].value.ip4 = 0xffffffff;
            attrs[2].id = SAI_NAT_ENTRY_ATTR_ENABLE_PACKET_COUNT;
            attrs[2].value.booldata = true;
            attrs[3].id = SAI_NAT_ENTRY_ATTR_ENABLE_BYTE_COUNT;
            attrs[3].value.booldata = true;

            if (port)
            {
                sai_attribute_t p;

                p.id = SAI_NAT_ENTRY_ATTR_L4_SRC_PORT;
                p.value.u16 = port;
                attrs.push_back(p);
            }

            return m_sw->create(SAI_OBJECT_TYPE_NAT_ENTRY, id, m_switchId, (uint32_t)attrs.size(), attrs.data());
        }

        sai_status_t createDnat(
                const std::string& id,
                const char *translated)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attrs[4];

            attrs[0].id = SAI_NAT_ENTRY_ATTR_DST_IP;
            attrs[0].value.ip4 = ip(translated);
            attrs[1].id = SAI_NAT_ENTRY_ATTR_DST_IP_MASK;
            attrs[1].value.ip4 = 0xffffffff;
            attrs[2].id = SAI_NAT_ENTRY_ATTR_ENABLE_PACKET_COUNT;
            attrs[2].value.booldata = true;
            attrs[3].id = SAI_NAT_ENTRY_ATTR_ENABLE_BYTE_COUNT;
            attrs[3].value.booldata = true;

            return m_sw->create(SAI_OBJECT_TYPE_NAT_ENTRY, id, m_switchId, 4, attrs);
        }

        // the three entries orchagent creates for a static NAT of the host
        void createStaticNatEntries()
        {
            SWSS_LOG_ENTER();

            ASSERT_EQ(SAI_STATUS_SUCCESS, createSnat(snatId(), "192.168.1.100"));
            ASSERT_EQ(SAI_STATUS_SUCCESS, createDnat(dnatId(), "10.0.1.3"));
            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create(SAI_OBJECT_TYPE_NAT_ENTRY, poolId(), m_switchId, 0, nullptr));
        }

        std::string snatId() { return natEntryId(SAI_NAT_TYPE_SOURCE_NAT, "10.0.1.3", nullptr); }
        std::string dnatId() { return natEntryId(SAI_NAT_TYPE_DESTINATION_NAT, nullptr, "192.168.1.100"); }
        std::string poolId() { return natEntryId(SAI_NAT_TYPE_DESTINATION_NAT_POOL, nullptr, "192.168.1.100"); }

        void addSession(
                const char *inside,
                uint16_t inside_port,
                const char *outside,
                const char *ext_host,
                uint16_t ext_port,
                uint32_t pkts,
                uint64_t bytes)
        {
            SWSS_LOG_ENTER();

            vpp_nat44_session_t s;

            memset(&s, 0, sizeof(s));
            s.inside_addr.s_addr = ip(inside);
            s.inside_port = inside_port;
            s.outside_addr.s_addr = ip(outside);
            s.outside_port = inside_port;
            s.ext_host_addr.s_addr = ip(ext_host);
            s.ext_host_port = ext_port;
            s.protocol = 6;
            s.is_static = true;
            s.total_pkts = pkts;
            s.total_bytes = bytes;

            vppMock().natSessions[s.inside_addr.s_addr].push_back(s);
        }

        uint64_t counter(
                const std::string& id,
                sai_attr_id_t which)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = which;
            attr.value.u64 = UINT64_MAX;

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_NAT_ENTRY, id, 1, &attr));

            return attr.value.u64;
        }
};

TEST_F(SwitchVppNatTest, StaticNatProgramsOneAddressOnlyMapping)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 1, &rif));
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    vppMock().calls.clear();

    createStaticNatEntries();

    auto maps = vppCallsTo("vpp_nat44_static_mapping_add_del");

    // SOURCE_NAT and DESTINATION_NAT are one mapping; the pool needs nothing
    ASSERT_EQ(1u, maps.size());
    EXPECT_EQ("10.0.1.3>192.168.1.100", maps[0].name);
    EXPECT_EQ(0u, maps[0].id);
    EXPECT_TRUE(maps[0].flag);
}

TEST_F(SwitchVppNatTest, ZoneOneVlanRifIsTheOutsideInterfaceOnceNatIsEnabled)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 1, &rif));

    EXPECT_TRUE(vppCallsTo("vpp_nat44_ed_output_interface_add_del").empty());
    EXPECT_TRUE(vppCallsTo("vpp_nat44_ed_enable_disable").empty());

    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    int en = vppCallIndex("vpp_nat44_ed_enable_disable", "");
    int fwd = vppCallIndex("vpp_nat44_forwarding_enable_disable", "");
    int out = vppCallIndex("vpp_nat44_ed_output_interface_add_del", "bvi200");

    ASSERT_GE(en, 0);
    ASSERT_GE(fwd, 0);
    ASSERT_GE(out, 0);
    EXPECT_TRUE(vppMock().calls[en].flag);
    EXPECT_TRUE(vppMock().calls[fwd].flag);
    EXPECT_TRUE(vppMock().calls[out].flag);
    // forwarding after enable: enabling the plugin resets it
    EXPECT_LT(en, fwd);
}

TEST_F(SwitchVppNatTest, ZoneZeroRouterInterfaceGetsNoNatFeature)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(100, 0, &rif));
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    EXPECT_TRUE(vppCallsTo("vpp_nat44_ed_output_interface_add_del").empty());
}

TEST_F(SwitchVppNatTest, ZoneSetAfterCreateMovesTheInterfaceInAndOut)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 0, &rif));
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, setZone(rif, 1));

    auto outs = vppCallsTo("vpp_nat44_ed_output_interface_add_del");

    ASSERT_EQ(1u, outs.size());
    EXPECT_EQ("bvi200", outs[0].name);
    EXPECT_TRUE(outs[0].flag);

    sai_attribute_t attr;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_NAT_ZONE_ID;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif, 1, &attr));
    EXPECT_EQ(1u, attr.value.u32);

    ASSERT_EQ(SAI_STATUS_SUCCESS, setZone(rif, 0));

    outs = vppCallsTo("vpp_nat44_ed_output_interface_add_del");

    ASSERT_EQ(2u, outs.size());
    EXPECT_FALSE(outs[1].flag);
}

TEST_F(SwitchVppNatTest, MappingStaysUntilBothHalvesAreRemoved)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 1, &rif));
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    createStaticNatEntries();

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NAT_ENTRY, snatId()));
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NAT_ENTRY, poolId()));

    EXPECT_TRUE(vppCallsTo("vpp_nat44_static_mapping_add_del").empty());

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NAT_ENTRY, dnatId()));

    auto maps = vppCallsTo("vpp_nat44_static_mapping_add_del");

    ASSERT_EQ(1u, maps.size());
    EXPECT_EQ("10.0.1.3>192.168.1.100", maps[0].name);
    EXPECT_FALSE(maps[0].flag);
}

TEST_F(SwitchVppNatTest, EntriesCreatedBeforeNatIsEnabledAreProgrammedWhenItIs)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 1, &rif));

    createStaticNatEntries();

    EXPECT_TRUE(vppCallsTo("vpp_nat44_static_mapping_add_del").empty());

    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    auto maps = vppCallsTo("vpp_nat44_static_mapping_add_del");

    ASSERT_EQ(1u, maps.size());
    EXPECT_TRUE(maps[0].flag);
    // the plugin is up before anything is put in it
    EXPECT_LT(vppCallIndex("vpp_nat44_ed_enable_disable", ""),
              vppCallIndex("vpp_nat44_static_mapping_add_del", "10.0.1.3>192.168.1.100"));
}

TEST_F(SwitchVppNatTest, DisablingNatRemovesEverythingAndTheNextEnableRestoresIt)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 1, &rif));
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    createStaticNatEntries();

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(false));

    auto maps = vppCallsTo("vpp_nat44_static_mapping_add_del");
    auto outs = vppCallsTo("vpp_nat44_ed_output_interface_add_del");
    auto en = vppCallsTo("vpp_nat44_ed_enable_disable");

    ASSERT_EQ(1u, maps.size());
    EXPECT_FALSE(maps[0].flag);
    ASSERT_EQ(1u, outs.size());
    EXPECT_FALSE(outs[0].flag);
    ASSERT_EQ(1u, en.size());
    EXPECT_FALSE(en[0].flag);

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    EXPECT_EQ(1u, vppCallsTo("vpp_nat44_static_mapping_add_del").size());
    EXPECT_EQ(1u, vppCallsTo("vpp_nat44_ed_output_interface_add_del").size());
}

TEST_F(SwitchVppNatTest, StaticNaptEntryProgramsAPortMapping)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    auto id = natEntryId(SAI_NAT_TYPE_SOURCE_NAT, "10.0.0.5", nullptr, 6, 2222);

    ASSERT_EQ(SAI_STATUS_SUCCESS, createSnat(id, "192.168.1.100", 22));

    auto maps = vppCallsTo("vpp_nat44_static_mapping_add_del");

    ASSERT_EQ(1u, maps.size());
    EXPECT_EQ("10.0.0.5:2222>192.168.1.100:22/6", maps[0].name);
}

TEST_F(SwitchVppNatTest, SettingTheTranslatedAddressMovesTheMapping)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    auto id = natEntryId(SAI_NAT_TYPE_SOURCE_NAT, "10.0.0.5", nullptr, 6, 2222);

    ASSERT_EQ(SAI_STATUS_SUCCESS, createSnat(id, "192.168.1.100", 22));

    vppMock().calls.clear();

    sai_attribute_t attr;

    attr.id = SAI_NAT_ENTRY_ATTR_SRC_IP;
    attr.value.ip4 = ip("192.168.1.101");

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set(SAI_OBJECT_TYPE_NAT_ENTRY, id, &attr));

    auto maps = vppCallsTo("vpp_nat44_static_mapping_add_del");

    // the stored L4_SRC_PORT still applies to the new address
    ASSERT_EQ(2u, maps.size());
    EXPECT_EQ("10.0.0.5:2222>192.168.1.100:22/6", maps[0].name);
    EXPECT_FALSE(maps[0].flag);
    EXPECT_EQ("10.0.0.5:2222>192.168.1.101:22/6", maps[1].name);
    EXPECT_TRUE(maps[1].flag);

    attr.id = SAI_NAT_ENTRY_ATTR_SRC_IP;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_NAT_ENTRY, id, 1, &attr));
    EXPECT_EQ(ip("192.168.1.101"), attr.value.ip4);
}

TEST_F(SwitchVppNatTest, DoubleNatIsStoredButNotProgrammed)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    auto id = natEntryId(SAI_NAT_TYPE_DOUBLE_NAT, "10.0.0.5", "10.0.0.6");

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_NAT_ENTRY_ATTR_SRC_IP;
    attrs[0].value.ip4 = ip("192.168.2.1");
    attrs[1].id = SAI_NAT_ENTRY_ATTR_DST_IP;
    attrs[1].value.ip4 = ip("192.168.2.2");

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create(SAI_OBJECT_TYPE_NAT_ENTRY, id, m_switchId, 2, attrs));

    EXPECT_TRUE(vppCallsTo("vpp_nat44_static_mapping_add_del").empty());
}

TEST_F(SwitchVppNatTest, CountersSumTheSessionsOfTheMappingAndStayMonotonic)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 1, &rif));
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    createStaticNatEntries();

    addSession("10.0.1.3", 40000, "192.168.1.100", "192.168.1.1", 443, 10, 1000);
    addSession("10.0.1.3", 40001, "192.168.1.100", "192.168.1.1", 22, 5, 500);
    // B's traffic that this mapping did not translate (a forwarding bypass session)
    addSession("10.0.1.3", 40002, "10.0.1.3", "10.9.9.9", 80, 100, 10000);

    EXPECT_EQ(15u, counter(snatId(), SAI_NAT_ENTRY_ATTR_PACKET_COUNT));
    EXPECT_EQ(1500u, counter(snatId(), SAI_NAT_ENTRY_ATTR_BYTE_COUNT));
    EXPECT_EQ(15u, counter(dnatId(), SAI_NAT_ENTRY_ATTR_PACKET_COUNT));

    // the :443 session expires and a new one starts
    vppMock().natSessions.clear();
    addSession("10.0.1.3", 40001, "192.168.1.100", "192.168.1.1", 22, 7, 700);
    addSession("10.0.1.3", 40003, "192.168.1.100", "192.168.1.1", 443, 1, 60);

    EXPECT_EQ(18u, counter(snatId(), SAI_NAT_ENTRY_ATTR_PACKET_COUNT));
    EXPECT_EQ(1760u, counter(snatId(), SAI_NAT_ENTRY_ATTR_BYTE_COUNT));

    // orchagent clears with a SET to 0
    sai_attribute_t attr;

    attr.id = SAI_NAT_ENTRY_ATTR_PACKET_COUNT;
    attr.value.u64 = 0;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set(SAI_OBJECT_TYPE_NAT_ENTRY, snatId(), &attr));

    EXPECT_EQ(0u, counter(snatId(), SAI_NAT_ENTRY_ATTR_PACKET_COUNT));

    vppMock().natSessions.clear();
    addSession("10.0.1.3", 40001, "192.168.1.100", "192.168.1.1", 22, 9, 900);
    addSession("10.0.1.3", 40003, "192.168.1.100", "192.168.1.1", 443, 1, 60);

    EXPECT_EQ(2u, counter(snatId(), SAI_NAT_ENTRY_ATTR_PACKET_COUNT));
}

TEST_F(SwitchVppNatTest, HitBitIsSetByTrafficAndClearedOnRead)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    createStaticNatEntries();

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_NAT_ENTRY_ATTR_HIT_BIT;
    attrs[1].id = SAI_NAT_ENTRY_ATTR_HIT_BIT_COR;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_NAT_ENTRY, snatId(), 2, attrs));
    EXPECT_FALSE(attrs[0].value.booldata);

    addSession("10.0.1.3", 40000, "192.168.1.100", "192.168.1.1", 443, 3, 300);

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_NAT_ENTRY, snatId(), 2, attrs));
    EXPECT_TRUE(attrs[0].value.booldata);

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_NAT_ENTRY, snatId(), 2, attrs));
    EXPECT_FALSE(attrs[0].value.booldata);
}

TEST_F(SwitchVppNatTest, OtherAttributesReadBackAsStored)
{
    createStaticNatEntries();

    sai_attribute_t attr;

    attr.id = SAI_NAT_ENTRY_ATTR_SRC_IP;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_NAT_ENTRY, snatId(), 1, &attr));
    EXPECT_EQ(ip("192.168.1.100"), attr.value.ip4);
}

TEST_F(SwitchVppNatTest, RemovingTheRouterInterfaceDropsItsOutsideFeature)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 1, &rif));
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(rif)));

    auto outs = vppCallsTo("vpp_nat44_ed_output_interface_add_del");

    ASSERT_EQ(1u, outs.size());
    EXPECT_EQ("bvi200", outs[0].name);
    EXPECT_FALSE(outs[0].flag);
}

TEST_F(SwitchVppNatTest, WithoutTheNatPluginNatIsReportedUnsupportedAndNothingIsProgrammed)
{
    vppMock().natSupported = false;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_AVAILABLE_SNAT_ENTRY;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), 1, &attr));
    EXPECT_EQ(0u, attr.value.u32);

    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 1, &rif));
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    createStaticNatEntries();

    EXPECT_TRUE(vppCallsTo("vpp_nat44_ed_enable_disable").empty());
    EXPECT_TRUE(vppCallsTo("vpp_nat44_static_mapping_add_del").empty());

    vppMock().natSupported = true;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), 1, &attr));
    EXPECT_NE(0u, attr.value.u32);
}
