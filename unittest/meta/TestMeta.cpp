#include "Meta.h"
#include "MockMeta.h"
#include "MetaTestSaiInterface.h"

#include <arpa/inet.h>
#include <net/ethernet.h>

#include <gtest/gtest.h>

#include <boost/algorithm/string/join.hpp>

#include <memory>

#define VLAN_ID 2

using namespace saimeta;

class MetaTest : public ::testing::Test
{
    protected:
        sai_object_id_t m_switch_id, m_vlan_id, m_vr_id, m_port_id, m_rif_id;
        std::vector<sai_attribute_t> m_attrs;
        sai_attribute_t m_attr;
        Meta *m_meta;

        void SetupMeta()
        {
            SWSS_LOG_ENTER();
            m_attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
            m_attr.value.booldata = true;

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_meta->create(SAI_OBJECT_TYPE_SWITCH, &m_switch_id, SAI_NULL_OBJECT_ID, 1, &m_attr));

            m_attr.id = SAI_VLAN_ATTR_VLAN_ID;
            m_attr.value.u16 = VLAN_ID;

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_meta->create(SAI_OBJECT_TYPE_VLAN, &m_vlan_id, m_switch_id, 1, &m_attr));

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_meta->create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &m_vr_id, m_switch_id, 0, &m_attr));
        }
        void CreatePort()
        {
            SWSS_LOG_ENTER();
            std::vector<sai_attribute_t> attrs;
            sai_attribute_t attr;

            uint32_t list[1] = { 2 };

            attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
            attr.value.u32list.count = 1;
            attr.value.u32list.list = list;
            attrs.push_back(attr);

            attr.id = SAI_PORT_ATTR_SPEED;
            attr.value.u32 = 10000;
            attrs.push_back(attr);

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_meta->create(SAI_OBJECT_TYPE_PORT, &m_port_id, m_switch_id, (uint32_t)attrs.size(), attrs.data()));
        }
        void CreateRif()
        {
            SWSS_LOG_ENTER();
            std::vector<sai_attribute_t> attrs;
            sai_attribute_t attr;

            attr.id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
            attr.value.oid = m_vr_id;
            attrs.push_back(attr);

            attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
            attr.value.s32 = SAI_ROUTER_INTERFACE_TYPE_PORT;
            attrs.push_back(attr);

            attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
            attr.value.oid = m_port_id;
            attrs.push_back(attr);

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_meta->create(SAI_OBJECT_TYPE_ROUTER_INTERFACE, &m_rif_id, m_switch_id, (uint32_t)attrs.size(), attrs.data()));
        }
        sai_fdb_entry_t GenFdbEntry()
        {
            SWSS_LOG_ENTER();
            m_attrs.clear();

            sai_fdb_entry_t fdb_entry;
            memset(&fdb_entry, 0, sizeof(fdb_entry));
            fdb_entry.switch_id = m_switch_id;
            fdb_entry.bv_id = m_vlan_id;

            m_attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
            m_attr.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
            m_attrs.push_back(m_attr);
            return fdb_entry;
        }
        sai_mcast_fdb_entry_t GenMcastFdbEntry()
        {
            SWSS_LOG_ENTER();
            m_attrs.clear();
            sai_object_id_t l2mc_group_id = 0;
            m_meta->create(SAI_OBJECT_TYPE_L2MC_GROUP, &l2mc_group_id, m_switch_id, 0, 0);

            sai_mcast_fdb_entry_t mcast_fdb_entry;
            memset(&mcast_fdb_entry, 0, sizeof(mcast_fdb_entry));
            mcast_fdb_entry.bv_id = m_vlan_id;
            mcast_fdb_entry.switch_id = m_switch_id;

            m_attr.id = SAI_MCAST_FDB_ENTRY_ATTR_GROUP_ID;
            m_attr.value.oid = l2mc_group_id;
            m_attrs.push_back(m_attr);

            m_attr.id = SAI_MCAST_FDB_ENTRY_ATTR_PACKET_ACTION;
            m_attr.value.s32 = SAI_PACKET_ACTION_FORWARD;
            m_attrs.push_back(m_attr);
            return mcast_fdb_entry;
        }
        sai_neighbor_entry_t GenNeighEntry()
        {
            SWSS_LOG_ENTER();
            CreatePort();
            CreateRif();

            sai_neighbor_entry_t neigh_entry;
            memset(&neigh_entry, 0, sizeof(neigh_entry));
            neigh_entry.switch_id = m_switch_id;
            neigh_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
            inet_pton(AF_INET, std::string("1.1.1.1").c_str(), &neigh_entry.ip_address.addr.ip4);
            neigh_entry.rif_id = m_rif_id;

            m_attrs.clear();
            sai_mac_t mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
            m_attr.id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
            memcpy(m_attr.value.mac, mac, 6);
            m_attrs.push_back(m_attr);

            m_attr.id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
            m_attr.value.s32 = SAI_PACKET_ACTION_FORWARD;
            m_attrs.push_back(m_attr);

            return neigh_entry;
        }
        sai_route_entry_t GenRouteEntry()
        {
            SWSS_LOG_ENTER();
            m_attrs.clear();
            sai_route_entry_t route_entry;
            memset(&route_entry, 0, sizeof(route_entry));

            route_entry.switch_id = m_switch_id;

            route_entry.vr_id = m_vr_id;

            route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
            inet_pton(AF_INET, std::string("255.255.255.255").c_str(), &route_entry.destination.mask.ip4);
            inet_pton(AF_INET, std::string("1.1.1.1").c_str(), &route_entry.destination.addr.ip4);
            return route_entry;
        }
        sai_l2mc_entry_t GenL2MCEntry()
        {
            SWSS_LOG_ENTER();
            m_attrs.clear();
            sai_l2mc_entry_t l2mc_entry;
            memset(&l2mc_entry, 0, sizeof(l2mc_entry));

            l2mc_entry.bv_id = m_vlan_id;
            l2mc_entry.switch_id = m_switch_id;

            m_attr.id = SAI_L2MC_ENTRY_ATTR_PACKET_ACTION;
            m_attr.value.s32 = SAI_PACKET_ACTION_FORWARD;
            m_attrs.push_back(m_attr);

            return l2mc_entry;

        }
        sai_ipmc_entry_t GenIPMCEntry()
        {
            SWSS_LOG_ENTER();
            m_attrs.clear();
            sai_object_id_t rpfGroupId = 0;

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_meta->create(SAI_OBJECT_TYPE_RPF_GROUP, &rpfGroupId, m_switch_id, 0, m_attrs.data()));

            sai_ipmc_entry_t ipmc_entry;
            memset(&ipmc_entry, 0, sizeof(ipmc_entry));

            ipmc_entry.switch_id = m_switch_id;
            ipmc_entry.vr_id = m_vr_id;

            m_attr.id = SAI_IPMC_ENTRY_ATTR_PACKET_ACTION;
            m_attr.value.s32 = SAI_PACKET_ACTION_FORWARD;
            m_attrs.push_back(m_attr);

            m_attr.id = SAI_IPMC_ENTRY_ATTR_RPF_GROUP_ID;
            m_attr.value.oid = rpfGroupId;
            m_attrs.push_back(m_attr);

            return ipmc_entry;
        }
        sai_nat_entry_t GenNATEntry()
        {
            SWSS_LOG_ENTER();
            m_attrs.clear();

            sai_nat_entry_t nat_entry;

            memset(&nat_entry, 0, sizeof(nat_entry));

            nat_entry.switch_id = m_switch_id;
            nat_entry.vr_id = m_vr_id;

            m_attr.id = SAI_NAT_ENTRY_ATTR_NAT_TYPE;
            m_attr.value.s32 = SAI_NAT_TYPE_NONE;
            m_attrs.push_back(m_attr);

            return nat_entry;
        }
        sai_my_sid_entry_t GenMySIDEntry()
        {
            SWSS_LOG_ENTER();
            m_attrs.clear();

            sai_my_sid_entry_t my_sid_entry;

            memset(&my_sid_entry, 0, sizeof(my_sid_entry));

            my_sid_entry.switch_id = m_switch_id;
            my_sid_entry.vr_id = m_vr_id;

            m_attr.id = SAI_MY_SID_ENTRY_ATTR_ENDPOINT_BEHAVIOR;
            m_attr.value.s32 = SAI_MY_SID_ENTRY_ENDPOINT_BEHAVIOR_E;
            m_attrs.push_back(m_attr);

            return my_sid_entry;
        }
        void SetUp() override
        {
            m_meta = new Meta(std::make_shared<MetaTestSaiInterface>());
            SetupMeta();
        }
        void TearDown() override
        {
            m_attrs.clear();
        }
};

TEST_F(MetaTest, create_duplicate_fdb_entry)
{
    sai_fdb_entry_t fdb_entry = GenFdbEntry();
    sai_status_t status = m_meta->create(&fdb_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);
    status = m_meta->create(&fdb_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_ITEM_ALREADY_EXISTS, status);
}

TEST_F(MetaTest, remove_nonexistent_fdb_entry)
{
    sai_fdb_entry_t fdb_entry = GenFdbEntry();
    sai_status_t status = m_meta->remove(&fdb_entry);
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, status);
}

TEST_F(MetaTest, create_duplicate_mcast_fdb_entry)
{
    sai_mcast_fdb_entry_t mcast_fdb_entry = GenMcastFdbEntry();
    sai_status_t status = m_meta->create(&mcast_fdb_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);
    status = m_meta->create(&mcast_fdb_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_ITEM_ALREADY_EXISTS, status);
}

TEST_F(MetaTest, create_mcast_fdb_entry_with_nonexistent_vlan)
{
    sai_status_t status = m_meta->remove(SAI_OBJECT_TYPE_VLAN, m_vlan_id);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    sai_mcast_fdb_entry_t mcast_fdb_entry = GenMcastFdbEntry();
    status = m_meta->create(&mcast_fdb_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, status);
}

TEST_F(MetaTest, remove_nonexistent_mcast_fdb_entry)
{
    sai_mcast_fdb_entry_t mcast_fdb_entry = GenMcastFdbEntry();
    sai_status_t status = m_meta->remove(&mcast_fdb_entry);
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, status);
}

TEST_F(MetaTest, create_duplicate_neigh_entry)
{
    sai_neighbor_entry_t neigh_entry = GenNeighEntry();
    sai_status_t status = m_meta->create(&neigh_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);
    status = m_meta->create(&neigh_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_ITEM_ALREADY_EXISTS, status);
}

TEST_F(MetaTest, create_neigh_entry_with_nonexistent_rif)
{
    // GenNeighEntry() is responsible for creating the RIF, so we must call it before trying to remove the RIF
    sai_neighbor_entry_t neigh_entry = GenNeighEntry();
    sai_status_t status = m_meta->remove(SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_rif_id);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    status = m_meta->create(&neigh_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, status);
}

TEST_F(MetaTest, remove_nonexistent_neigh_entry)
{
    sai_neighbor_entry_t neigh_entry = GenNeighEntry();
    sai_status_t status = m_meta->remove(&neigh_entry);
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, status);
}

TEST_F(MetaTest, create_duplicate_route_entry)
{
    sai_route_entry_t route_entry = GenRouteEntry();
    sai_status_t status = m_meta->create(&route_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);
    status = m_meta->create(&route_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_ITEM_ALREADY_EXISTS, status);
}

TEST_F(MetaTest, remove_nonexistent_route_entry)
{
    sai_route_entry_t route_entry = GenRouteEntry();
    sai_status_t status = m_meta->remove(&route_entry);
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, status);
}

TEST_F(MetaTest, create_duplicate_l2mc_entry)
{
    sai_l2mc_entry_t l2mc_entry = GenL2MCEntry();
    sai_status_t status = m_meta->create(&l2mc_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);
    status = m_meta->create(&l2mc_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_ITEM_ALREADY_EXISTS, status);
}

TEST_F(MetaTest, create_l2mc_entry_with_nonexistent_vlan)
{
    sai_status_t status = m_meta->remove(SAI_OBJECT_TYPE_VLAN, m_vlan_id);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    sai_l2mc_entry_t l2mc_entry = GenL2MCEntry();
    status = m_meta->create(&l2mc_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, status);
}

TEST_F(MetaTest, remove_nonexistent_l2mc_entry)
{
    sai_l2mc_entry_t l2mc_entry = GenL2MCEntry();
    sai_status_t status = m_meta->remove(&l2mc_entry);
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, status);
}

TEST_F(MetaTest, create_duplicate_ipmc_entry)
{
    sai_ipmc_entry_t ipmc_entry = GenIPMCEntry();
    sai_status_t status = m_meta->create(&ipmc_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);
    status = m_meta->create(&ipmc_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_ITEM_ALREADY_EXISTS, status);
}

TEST_F(MetaTest, create_ipmc_entry_with_nonexistent_vr)
{
    sai_status_t status = m_meta->remove(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, m_vr_id);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    sai_ipmc_entry_t ipmc_entry = GenIPMCEntry();
    status = m_meta->create(&ipmc_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, status);
}

TEST_F(MetaTest, remove_nonexistent_ipmc_entry)
{
    sai_ipmc_entry_t ipmc_entry = GenIPMCEntry();
    sai_status_t status = m_meta->remove(&ipmc_entry);
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, status);
}

TEST_F(MetaTest, create_duplicate_nat_entry)
{
    sai_nat_entry_t nat_entry = GenNATEntry();
    sai_status_t status = m_meta->create(&nat_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);
    status = m_meta->create(&nat_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_ITEM_ALREADY_EXISTS, status);
}

TEST_F(MetaTest, create_nat_entry_with_nonexistent_vr)
{
    sai_status_t status = m_meta->remove(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, m_vr_id);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    sai_nat_entry_t nat_entry = GenNATEntry();
    status = m_meta->create(&nat_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, status);
}

TEST_F(MetaTest, remove_nonexistent_nat_entry)
{
    sai_nat_entry_t nat_entry = GenNATEntry();
    sai_status_t status = m_meta->remove(&nat_entry);
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, status);
}

TEST_F(MetaTest, create_duplicate_my_sid_entry)
{
    sai_my_sid_entry_t my_sid_entry = GenMySIDEntry();
    sai_status_t status = m_meta->create(&my_sid_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);
    status = m_meta->create(&my_sid_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_ITEM_ALREADY_EXISTS, status);
}

TEST_F(MetaTest, create_my_sid_entry_with_nonexistent_vr)
{
    sai_status_t status = m_meta->remove(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, m_vr_id);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    sai_my_sid_entry_t my_sid_entry = GenMySIDEntry();
    status = m_meta->create(&my_sid_entry, (uint32_t)m_attrs.size(), m_attrs.data());
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, status);
}

TEST_F(MetaTest, remove_nonexistent_my_sid_entry)
{
    sai_my_sid_entry_t my_sid_entry = GenMySIDEntry();
    sai_status_t status = m_meta->remove(&my_sid_entry);
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, status);
}

TEST_F(MetaTest, remove_nonexistent_generic_object)
{
    sai_attribute_t attr;
    sai_object_id_t vlan_id;
    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 99;

    sai_status_t status = m_meta->create(SAI_OBJECT_TYPE_VLAN, &vlan_id, m_switch_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    status = m_meta->remove(SAI_OBJECT_TYPE_VLAN, vlan_id);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    status = m_meta->remove(SAI_OBJECT_TYPE_VLAN, vlan_id);
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, status);
}

TEST(Meta, initialize)
{
    Meta m(std::make_shared<DummySaiInterface>());

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.apiInitialize(0, 0));
}

TEST(Meta, uninitialize)
{
    Meta m(std::make_shared<DummySaiInterface>());

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.apiUninitialize());
}

TEST(Meta, quad_mcast_fdb_entry)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_object_id_t vlanId = 0;

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 2;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VLAN, &vlanId, switchId, 1, &attr));

    sai_object_id_t l2mcGroupId = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_L2MC_GROUP, &l2mcGroupId, switchId, 0, 0));

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_MCAST_FDB_ENTRY_ATTR_GROUP_ID;
    attrs[0].value.oid = l2mcGroupId;

    attrs[1].id = SAI_MCAST_FDB_ENTRY_ATTR_PACKET_ACTION;
    attrs[1].value.s32 = SAI_PACKET_ACTION_FORWARD;

    sai_mcast_fdb_entry_t e;

    memset(&e, 0, sizeof(e));

    e.bv_id = vlanId;
    e.switch_id = switchId;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(&e, 2, attrs));

    attr.id = SAI_MCAST_FDB_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = SAI_PACKET_ACTION_DROP;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.set(&e, &attr));

    attr.id = SAI_MCAST_FDB_ENTRY_ATTR_GROUP_ID;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.get(&e, 1, &attr));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.remove(&e));
}

TEST(Meta, quad_l2mc_entry)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_object_id_t vlanId = 0;

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 2;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VLAN, &vlanId, switchId, 1, &attr));

    sai_object_id_t l2mcGroupId = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_L2MC_GROUP, &l2mcGroupId, switchId, 0, 0));


    attr.id = SAI_L2MC_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = SAI_PACKET_ACTION_FORWARD;

    sai_l2mc_entry_t e;

    memset(&e, 0, sizeof(e));

    e.bv_id = vlanId;
    e.switch_id = switchId;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(&e, 1, &attr));

    attr.id = SAI_L2MC_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = SAI_PACKET_ACTION_DROP;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.set(&e, &attr));

    attr.id = SAI_L2MC_ENTRY_ATTR_PACKET_ACTION;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.get(&e, 1, &attr));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.remove(&e));
}

TEST(Meta, quad_inseg_entry)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_object_id_t vlanId = 0;

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 2;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VLAN, &vlanId, switchId, 1, &attr));

    attr.id = SAI_L2MC_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = SAI_PACKET_ACTION_FORWARD;

    sai_inseg_entry_t e;

    memset(&e, 0, sizeof(e));

    e.switch_id = switchId;
    e.label = 1;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(&e, 1, &attr));

    attr.id = SAI_INSEG_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = SAI_PACKET_ACTION_DROP;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.set(&e, &attr));

    attr.id = SAI_INSEG_ENTRY_ATTR_PACKET_ACTION;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.get(&e, 1, &attr));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.remove(&e));
}

TEST(Meta, quad_nat_entry)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_object_id_t vrId = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &vrId, switchId, 0, &attr));

    attr.id = SAI_NAT_ENTRY_ATTR_NAT_TYPE;
    attr.value.s32 = SAI_NAT_TYPE_NONE;

    sai_nat_entry_t e;

    memset(&e, 0, sizeof(e));

    e.switch_id = switchId;
    e.vr_id = vrId;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(&e, 1, &attr));

    attr.id = SAI_NAT_ENTRY_ATTR_NAT_TYPE;
    attr.value.s32 = SAI_NAT_TYPE_NONE;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.set(&e, &attr));

    attr.id = SAI_NAT_ENTRY_ATTR_NAT_TYPE;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.get(&e, 1, &attr));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.remove(&e));
}

TEST(Meta, quad_impc_entry)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_object_id_t rpfGroupId = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_RPF_GROUP, &rpfGroupId, switchId, 0, &attr));

    sai_object_id_t vrId = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &vrId, switchId, 0, &attr));

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_IPMC_ENTRY_ATTR_PACKET_ACTION;
    attrs[0].value.s32 = SAI_PACKET_ACTION_FORWARD;

    attrs[1].id = SAI_IPMC_ENTRY_ATTR_RPF_GROUP_ID;
    attrs[1].value.oid = rpfGroupId;

    sai_ipmc_entry_t e;

    memset(&e, 0, sizeof(e));

    e.switch_id = switchId;
    e.vr_id = vrId;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(&e, 2, attrs));

    attr.id = SAI_IPMC_ENTRY_ATTR_PACKET_ACTION;
    attr.value.s32 = SAI_PACKET_ACTION_DROP;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.set(&e, &attr));

    attr.id = SAI_IPMC_ENTRY_ATTR_RPF_GROUP_ID;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.get(&e, 1, &attr));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.remove(&e));
}

TEST(Meta, flushFdbEntries)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.flushFdbEntries(switchId, 0, 0));

    EXPECT_NE(SAI_STATUS_SUCCESS, m.flushFdbEntries(switchId, 10000, 0));

    EXPECT_NE(SAI_STATUS_SUCCESS, m.flushFdbEntries(switchId, 1, 0));

    EXPECT_NE(SAI_STATUS_SUCCESS, m.flushFdbEntries(0, 0, 0));

    attr.id = 10000;

    EXPECT_NE(SAI_STATUS_SUCCESS, m.flushFdbEntries(switchId, 1, &attr));

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_FDB_FLUSH_ATTR_ENTRY_TYPE;
    attrs[0].value.s32 = SAI_FDB_FLUSH_ENTRY_TYPE_ALL;

    attrs[1].id = SAI_FDB_FLUSH_ATTR_ENTRY_TYPE;
    attrs[1].value.s32 = SAI_FDB_FLUSH_ENTRY_TYPE_ALL;

    EXPECT_NE(SAI_STATUS_SUCCESS, m.flushFdbEntries(switchId, 2, attrs));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.flushFdbEntries(switchId, 1, attrs));

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.flushFdbEntries(0x21000000000001, 1, attrs));
    EXPECT_EQ(SAI_STATUS_SUCCESS, m.flushFdbEntries(0x21000000000000, 1, attrs));

    // SAI_FDB_FLUSH_ENTRY_TYPE attribute out of range

    attrs[0].value.s32 = 1000;

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.flushFdbEntries(switchId, 1, attrs));

    // test flush with vlan

    sai_object_id_t vlanId = 0;

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 2;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VLAN, &vlanId, switchId, 1, &attr));

    attrs[0].id = SAI_FDB_FLUSH_ATTR_ENTRY_TYPE;
    attrs[0].value.s32 = SAI_FDB_FLUSH_ENTRY_TYPE_ALL;

    attrs[1].id = SAI_FDB_FLUSH_ATTR_BV_ID;
    attrs[1].value.oid = vlanId;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.flushFdbEntries(switchId, 2, attrs));

    // test dynamic flush

    attrs[0].value.s32 = SAI_FDB_FLUSH_ENTRY_TYPE_DYNAMIC;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.flushFdbEntries(switchId, 2, attrs));

    // test static flush

    attrs[0].value.s32 = SAI_FDB_FLUSH_ENTRY_TYPE_STATIC;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.flushFdbEntries(switchId, 2, attrs));
}

TEST(Meta, meta_warm_boot_notify)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    m.meta_warm_boot_notify();
}

TEST(Meta, meta_init_db)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    m.meta_init_db();
}

TEST(Meta, dump)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    m.dump();
}

TEST(Meta, objectTypeGetAvailability)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    EXPECT_NE(SAI_STATUS_SUCCESS, m.objectTypeGetAvailability(0, SAI_OBJECT_TYPE_NULL, 0, nullptr, nullptr));

    EXPECT_NE(SAI_STATUS_SUCCESS, m.objectTypeGetAvailability(switchId, SAI_OBJECT_TYPE_NULL, 0, nullptr, nullptr));

    attr.id = SAI_DEBUG_COUNTER_ATTR_TYPE;
    attr.value.s32 = SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS;

    uint64_t count;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.objectTypeGetAvailability(switchId, SAI_OBJECT_TYPE_DEBUG_COUNTER, 1, &attr, &count));

    // invalid attribute

    attr.id = 100000;

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.objectTypeGetAvailability(switchId, SAI_OBJECT_TYPE_DEBUG_COUNTER, 1, &attr, &count));

    // defined multiple times

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_DEBUG_COUNTER_ATTR_TYPE;
    attrs[0].value.s32 = SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS;

    attrs[1].id = SAI_DEBUG_COUNTER_ATTR_TYPE;
    attrs[1].value.s32 = SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS;

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.objectTypeGetAvailability(switchId, SAI_OBJECT_TYPE_DEBUG_COUNTER, 2, attrs, &count));

    // enum not on allowed list

    attr.id = SAI_DEBUG_COUNTER_ATTR_TYPE;
    attr.value.s32 = 10000;

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.objectTypeGetAvailability(switchId, SAI_OBJECT_TYPE_DEBUG_COUNTER, 1, &attr, &count));

    // not resource type

    attr.id = SAI_DEBUG_COUNTER_ATTR_BIND_METHOD;
    attr.value.s32 = 0;

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.objectTypeGetAvailability(switchId, SAI_OBJECT_TYPE_DEBUG_COUNTER, 1, &attr, &count));
}

TEST(Meta, queryAttributeCapability)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_attr_capability_t cap;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.queryAttributeCapability(switchId, SAI_OBJECT_TYPE_ACL_ENTRY, SAI_ACL_ENTRY_ATTR_TABLE_ID, &cap));

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.queryAttributeCapability(switchId, SAI_OBJECT_TYPE_ACL_ENTRY, 100000, &cap));
}

TEST(Meta, queryAttributeEnumValuesCapability)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_s32_list_t list;

    int32_t vals[2];

    list.count = 2;
    list.list = vals;

    vals[0] = 0;
    vals[1] = 100000;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.queryAttributeEnumValuesCapability(switchId, SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_SWITCHING_MODE, &list));

    // set count without list;

    list.list = nullptr;

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.queryAttributeEnumValuesCapability(switchId, SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_SWITCHING_MODE, &list));

    // non enum attribute

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.queryAttributeEnumValuesCapability(switchId, SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_BCAST_CPU_FLOOD_ENABLE, &list));

    // invalid attribute

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.queryAttributeEnumValuesCapability(switchId, SAI_OBJECT_TYPE_SWITCH, 10000, &list));
}

TEST(Meta, queryStatsCapability)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_stat_capability_list_t queue_stats_capability;
    sai_stat_capability_t stat_initializer;
    stat_initializer.stat_enum = 0;
    stat_initializer.stat_modes = 0;
    std::vector<sai_stat_capability_t> qstat_cap_list(20, stat_initializer);
    queue_stats_capability.count = 15;
    queue_stats_capability.list = qstat_cap_list.data();


    EXPECT_EQ(SAI_STATUS_SUCCESS, m.queryStatsCapability(switchId, SAI_OBJECT_TYPE_QUEUE, &queue_stats_capability));

    queue_stats_capability.list = nullptr;

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.queryStatsCapability(switchId, SAI_OBJECT_TYPE_QUEUE, &queue_stats_capability));

}

TEST(Meta, queryStatsStCapability)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_stat_st_capability_list_t queue_stats_capability;
    sai_stat_st_capability_t stat_initializer;
    stat_initializer.capability.stat_enum = 0;
    stat_initializer.capability.stat_modes = 0;
    stat_initializer.minimal_polling_interval = 0;
    std::vector<sai_stat_st_capability_t> qstat_cap_list(20, stat_initializer);
    queue_stats_capability.count = 15;
    queue_stats_capability.list = qstat_cap_list.data();

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.queryStatsStCapability(switchId, SAI_OBJECT_TYPE_QUEUE, &queue_stats_capability));

    queue_stats_capability.list = nullptr;

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.queryStatsStCapability(switchId, SAI_OBJECT_TYPE_QUEUE, &queue_stats_capability));
}

TEST(Meta, meta_validate_stats)
{
    MockMeta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_object_id_t vlanId = 0;

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 2;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VLAN, &vlanId, switchId, 1, &attr));

    sai_stat_id_t counter_ids[2];

    counter_ids[0] = SAI_VLAN_STAT_IN_OCTETS;
    counter_ids[1] = SAI_VLAN_STAT_IN_PACKETS;

    uint64_t counters[2];

    m.meta_unittests_enable(true);

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.call_meta_validate_stats(SAI_OBJECT_TYPE_VLAN, vlanId, 2, counter_ids, counters, SAI_STATS_MODE_READ));

    // invalid mode

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.call_meta_validate_stats(SAI_OBJECT_TYPE_VLAN, vlanId, 2, counter_ids, counters, (sai_stats_mode_t)7));
#pragma GCC diagnostic pop

    // invalid counter

    counter_ids[0] = 10000;

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.call_meta_validate_stats(SAI_OBJECT_TYPE_VLAN, vlanId, 2, counter_ids, counters, SAI_STATS_MODE_READ));

    // object with no stats

    sai_object_id_t vrId = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &vrId, switchId, 0, &attr));

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.call_meta_validate_stats(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, vrId, 2, counter_ids, counters, SAI_STATS_MODE_READ));
}

TEST(Meta, quad_generic_programmable_entry)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    std::string table_name = "test_table";
    std::string json_value = "test_json";

    sai_attribute_t attrs[2];
    attrs[0].id = SAI_GENERIC_PROGRAMMABLE_ATTR_OBJECT_NAME;
    attrs[0].value.s8list.count = (uint32_t)table_name.size();
    attrs[0].value.s8list.list = (int8_t *)const_cast<char *>(table_name.c_str());

    attrs[1].id = SAI_GENERIC_PROGRAMMABLE_ATTR_ENTRY;
    attrs[1].value.s8list.count = (uint32_t)json_value.size();
    attrs[1].value.s8list.list = (int8_t *)const_cast<char *>(json_value.c_str());

    sai_object_id_t objId = 0;
    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_GENERIC_PROGRAMMABLE, &objId, switchId, 2, attrs));

    attr.id = SAI_GENERIC_PROGRAMMABLE_ATTR_ENTRY;
    attr.value.s8list.count = (uint32_t)json_value.size();
    attr.value.s8list.list = (int8_t *)const_cast<char *>(json_value.c_str());
    EXPECT_EQ(SAI_STATUS_SUCCESS, m.set(SAI_OBJECT_TYPE_GENERIC_PROGRAMMABLE, objId, &attr));

    attr.id = SAI_GENERIC_PROGRAMMABLE_ATTR_ENTRY;
    EXPECT_EQ(SAI_STATUS_SUCCESS, m.get(SAI_OBJECT_TYPE_GENERIC_PROGRAMMABLE, objId, 1, &attr));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.remove(SAI_OBJECT_TYPE_GENERIC_PROGRAMMABLE, objId));
}


TEST(Meta, quad_my_sid_entry)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_object_id_t vlanId = 0;

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 2;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VLAN, &vlanId, switchId, 1, &attr));

    sai_object_id_t vrId = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &vrId, switchId, 0, &attr));

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_MY_SID_ENTRY_ATTR_ENDPOINT_BEHAVIOR;
    attrs[0].value.s32 = SAI_MY_SID_ENTRY_ENDPOINT_BEHAVIOR_E;

    sai_my_sid_entry_t e;

    memset(&e, 0, sizeof(e));

    e.switch_id = switchId;
    e.vr_id = vrId;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(&e, 1, attrs));

    attr.id = SAI_MY_SID_ENTRY_ATTR_ENDPOINT_BEHAVIOR;
    attr.value.s32 = SAI_MY_SID_ENTRY_ENDPOINT_BEHAVIOR_X;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.set(&e, &attr));

    attr.id = SAI_MCAST_FDB_ENTRY_ATTR_GROUP_ID;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.get(&e, 1, &attr));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.remove(&e));
}

TEST(Meta, quad_bulk_route_entry)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_object_id_t vlanId = 0;

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 2;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VLAN, &vlanId, switchId, 1, &attr));

    sai_object_id_t vrId = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &vrId, switchId, 0, &attr));

    // create

    sai_route_entry_t e[2];

    memset(e, 0, sizeof(e));

    e[0].switch_id = switchId;
    e[1].switch_id = switchId;

    e[0].vr_id = vrId;
    e[1].vr_id = vrId;

    e[0].destination.addr.ip4 = 1;
    e[1].destination.addr.ip4 = 2;

    uint32_t attr_count[2];

    attr_count[0] = 0;
    attr_count[1] = 0;

    sai_attribute_t list1[2];
    sai_attribute_t list2[2];

    std::vector<const sai_attribute_t*> alist;

    alist.push_back(list1);
    alist.push_back(list2);

    const sai_attribute_t **attr_list = alist.data();

    sai_status_t statuses[2];

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.bulkCreate(2, e, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));

    // set

    sai_attribute_t setlist[2];

    setlist[0].id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    setlist[0].value.s32 = SAI_PACKET_ACTION_DROP;

    setlist[1].id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    setlist[1].value.s32 = SAI_PACKET_ACTION_DROP;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.bulkSet(2, e, setlist, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));

    // remove

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.bulkRemove(2, e, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));
}

sai_object_id_t create_port(
        _In_ Meta &m,
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();
    sai_object_id_t port;

    static uint32_t id = 1;
    id++;
    sai_attribute_t attrs[9] = { };

    uint32_t list[1] = { id };

    attrs[0].id = SAI_PORT_ATTR_HW_LANE_LIST;
    attrs[0].value.u32list.count = 1;
    attrs[0].value.u32list.list = list;

    attrs[1].id = SAI_PORT_ATTR_SPEED;
    attrs[1].value.u32 = 10000;

    auto status = m.create(SAI_OBJECT_TYPE_PORT, &port, switch_id, 2, attrs);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    return port;
}

sai_object_id_t create_rif(
        _In_ Meta &m,
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_id_t port_id,
        _In_ sai_object_id_t vr_id)
{
    SWSS_LOG_ENTER();
    sai_object_id_t rif;

    sai_attribute_t attrs[9] = { };

    attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
    attrs[0].value.oid = vr_id;

    attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    attrs[1].value.s32 = SAI_ROUTER_INTERFACE_TYPE_PORT;

    attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
    attrs[2].value.oid = port_id;

    auto status = m.create(SAI_OBJECT_TYPE_ROUTER_INTERFACE, &rif, switch_id, 3, attrs);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    return rif;
}

TEST(Meta, quad_bulk_neighbor_entry)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_object_id_t vlanId = 0;

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 2;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VLAN, &vlanId, switchId, 1, &attr));

    sai_object_id_t vrId = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &vrId, switchId, 0, &attr));

    sai_object_id_t portId = create_port(m, switchId);
    sai_object_id_t rifId = create_rif(m, switchId, portId, vrId);

    // create

    sai_neighbor_entry_t e[2];

    memset(e, 0, sizeof(e));

    e[0].switch_id = switchId;
    e[1].switch_id = switchId;

    e[0].ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    e[0].ip_address.addr.ip4 = htonl(0x0a00000e);
    e[1].ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    e[1].ip_address.addr.ip4 = htonl(0x0a00000f);

    e[0].rif_id = rifId;
    e[1].rif_id = rifId;

    uint32_t attr_count[2];

    attr_count[0] = 2;
    attr_count[1] = 2;

    sai_attribute_t list1[2];
    sai_attribute_t list2[2];
    sai_mac_t mac1 = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    sai_mac_t mac2 = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    list1[0].id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
    memcpy(list1[0].value.mac, mac1, 6);
    list1[1].id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    list1[1].value.s32 = SAI_PACKET_ACTION_FORWARD;

    list2[0].id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
    memcpy(list2[0].value.mac, mac2, 6);
    list2[1].id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    list2[1].value.s32 = SAI_PACKET_ACTION_FORWARD;

    std::vector<const sai_attribute_t*> alist;

    alist.push_back(list1);
    alist.push_back(list2);

    const sai_attribute_t **attr_list = alist.data();

    sai_status_t statuses[2];

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.bulkCreate(2, e, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));

    // set
    sai_attribute_t setlist[2];

    setlist[0].id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    setlist[0].value.s32 = SAI_PACKET_ACTION_FORWARD;

    setlist[1].id = SAI_NEIGHBOR_ENTRY_ATTR_PACKET_ACTION;
    setlist[1].value.s32 = SAI_PACKET_ACTION_FORWARD;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.bulkSet(2, e, setlist, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));

    // remove
    EXPECT_EQ(SAI_STATUS_SUCCESS, m.bulkRemove(2, e, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));

}

TEST(Meta, quad_bulk_nat_entry)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_object_id_t vlanId = 0;

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 2;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VLAN, &vlanId, switchId, 1, &attr));

    sai_object_id_t vrId = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &vrId, switchId, 0, &attr));

    // create

    sai_nat_entry_t e[2];

    memset(e, 0, sizeof(e));

    e[0].switch_id = switchId;
    e[1].switch_id = switchId;

    e[0].vr_id = vrId;
    e[1].vr_id = vrId;

    e[0].nat_type = SAI_NAT_TYPE_SOURCE_NAT;
    e[1].nat_type = SAI_NAT_TYPE_DESTINATION_NAT;

    uint32_t attr_count[2];

    attr_count[0] = 0;
    attr_count[1] = 0;

    sai_attribute_t list1[2];
    sai_attribute_t list2[2];

    std::vector<const sai_attribute_t*> alist;

    alist.push_back(list1);
    alist.push_back(list2);

    const sai_attribute_t **attr_list = alist.data();

    sai_status_t statuses[2];

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.bulkCreate(2, e, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));

    // set

    sai_attribute_t setlist[2];

    setlist[0].id = SAI_NAT_ENTRY_ATTR_NAT_TYPE;
    setlist[0].value.s32 = SAI_NAT_TYPE_NONE;

    setlist[1].id = SAI_NAT_ENTRY_ATTR_NAT_TYPE;
    setlist[1].value.s32 = SAI_NAT_TYPE_NONE;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.bulkSet(2, e, setlist, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));

    // remove

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.bulkRemove(2, e, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));
}

TEST(Meta, getStats)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_stat_id_t counter_ids[2];

    counter_ids[0] = SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_0_DROPPED_PKTS;
    counter_ids[1] = SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_1_DROPPED_PKTS;

    uint64_t counters[2];

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.getStats(SAI_OBJECT_TYPE_SWITCH, switchId, 2, counter_ids, counters));
}

TEST(Meta, getStatsExt)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_stat_id_t counter_ids[2];

    counter_ids[0] = SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_0_DROPPED_PKTS;
    counter_ids[1] = SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_1_DROPPED_PKTS;

    uint64_t counters[2];

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.getStatsExt(SAI_OBJECT_TYPE_SWITCH, switchId, 2, counter_ids, SAI_STATS_MODE_READ, counters));
}

TEST(Meta, clearStats)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_stat_id_t counter_ids[2];

    counter_ids[0] = SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_0_DROPPED_PKTS;
    counter_ids[1] = SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_1_DROPPED_PKTS;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.clearStats(SAI_OBJECT_TYPE_SWITCH, switchId, 2, counter_ids));
}

TEST(Meta, quad_bulk_my_sid_entry)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_object_id_t vlanId = 0;

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 2;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VLAN, &vlanId, switchId, 1, &attr));

    sai_object_id_t vrId = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &vrId, switchId, 0, &attr));

    // create

    sai_my_sid_entry_t e[2];

    memset(e, 0, sizeof(e));

    e[0].switch_id = switchId;
    e[1].switch_id = switchId;

    e[0].vr_id = vrId;
    e[1].vr_id = vrId;

    e[0].locator_block_len = 1;
    e[1].locator_block_len = 2;

    uint32_t attr_count[2];

    attr_count[0] = 1;
    attr_count[1] = 1;

    sai_attribute_t list1[2];
    sai_attribute_t list2[2];

    list1[0].id = SAI_MY_SID_ENTRY_ATTR_ENDPOINT_BEHAVIOR;
    list1[0].value.s32 = SAI_MY_SID_ENTRY_ENDPOINT_BEHAVIOR_E;

    list2[0].id = SAI_MY_SID_ENTRY_ATTR_ENDPOINT_BEHAVIOR;
    list2[0].value.s32 = SAI_MY_SID_ENTRY_ENDPOINT_BEHAVIOR_E;

    std::vector<const sai_attribute_t*> alist;

    alist.push_back(list1);
    alist.push_back(list2);

    const sai_attribute_t **attr_list = alist.data();

    sai_status_t statuses[2];

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.bulkCreate(2, e, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));

    // set

    sai_attribute_t setlist[2];

    setlist[0].id = SAI_MY_SID_ENTRY_ATTR_ENDPOINT_BEHAVIOR;
    setlist[0].value.s32 = SAI_MY_SID_ENTRY_ENDPOINT_BEHAVIOR_X;

    setlist[1].id = SAI_MY_SID_ENTRY_ATTR_ENDPOINT_BEHAVIOR;
    setlist[1].value.s32 = SAI_MY_SID_ENTRY_ENDPOINT_BEHAVIOR_X;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.bulkSet(2, e, setlist, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));

    // remove

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.bulkRemove(2, e, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));
}

TEST(Meta, quad_bulk_inseg_entry)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_object_id_t vlanId = 0;

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 2;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VLAN, &vlanId, switchId, 1, &attr));

    sai_object_id_t vrId = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &vrId, switchId, 0, &attr));

    // create

    sai_inseg_entry_t e[2];

    memset(e, 0, sizeof(e));

    e[0].switch_id = switchId;
    e[1].switch_id = switchId;

    e[0].label = 1;
    e[1].label = 2;

    uint32_t attr_count[2];

    attr_count[0] = 1;
    attr_count[1] = 1;

    sai_attribute_t list1[2];
    sai_attribute_t list2[2];

    list1[0].id = SAI_L2MC_ENTRY_ATTR_PACKET_ACTION;
    list1[0].value.s32 = SAI_PACKET_ACTION_FORWARD;

    list2[0].id = SAI_L2MC_ENTRY_ATTR_PACKET_ACTION;
    list2[0].value.s32 = SAI_PACKET_ACTION_FORWARD;

    std::vector<const sai_attribute_t*> alist;

    alist.push_back(list1);
    alist.push_back(list2);

    const sai_attribute_t **attr_list = alist.data();

    sai_status_t statuses[2];

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.bulkCreate(2, e, attr_count, attr_list, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));

    // set

    sai_attribute_t setlist[2];

    setlist[0].id = SAI_L2MC_ENTRY_ATTR_PACKET_ACTION;
    setlist[0].value.s32 = SAI_PACKET_ACTION_DROP;

    setlist[1].id = SAI_L2MC_ENTRY_ATTR_PACKET_ACTION;
    setlist[1].value.s32 = SAI_PACKET_ACTION_DROP;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.bulkSet(2, e, setlist, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));

    // remove

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.bulkRemove(2, e, SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR, statuses));
}

TEST(Meta, logSet)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.logSet(SAI_API_SWITCH, SAI_LOG_LEVEL_NOTICE));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.logSet((sai_api_t)1000, SAI_LOG_LEVEL_NOTICE));

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.logSet(SAI_API_SWITCH, (sai_log_level_t)1000));
#pragma GCC diagnostic pop

}

TEST(Meta, meta_sai_on_switch_shutdown_request)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    m.meta_sai_on_switch_shutdown_request(0);
    m.meta_sai_on_switch_shutdown_request(switchId);
    m.meta_sai_on_switch_shutdown_request(0x21000000000001);
}

TEST(Meta, meta_sai_on_switch_state_change)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    m.meta_sai_on_switch_state_change(0, SAI_SWITCH_OPER_STATUS_UP);
    m.meta_sai_on_switch_state_change(switchId, SAI_SWITCH_OPER_STATUS_UP);
    m.meta_sai_on_switch_state_change(0x21000000000001, SAI_SWITCH_OPER_STATUS_UP);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    m.meta_sai_on_switch_state_change(0, (sai_switch_oper_status_t)1000);
    m.meta_sai_on_switch_state_change(switchId, (sai_switch_oper_status_t)1000);
    m.meta_sai_on_switch_state_change(0x21000000000001, (sai_switch_oper_status_t)1000);
#pragma GCC diagnostic pop
}

TEST(Meta, populate)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    swss::TableDump dump;

    dump["SAI_OBJECT_TYPE_ACL_TABLE:oid:0x7000000000626"]["SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE"] = "true";
    dump["SAI_OBJECT_TYPE_ROUTE_ENTRY:{\"dest\":\"10.0.0.0/32\",\"switch_id\":\"oid:0x21000000000000\",\"vr\":\"oid:0x3000000000022\"}"]
        ["SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID"] = "oid:0x60000000005cf";
    dump["SAI_OBJECT_TYPE_VLAN:oid:0x2600000000002f"]["SAI_VLAN_ATTR_VLAN_ID"] = "2";
    dump["SAI_OBJECT_TYPE_SWITCH:oid:0x21000000000000"]["SAI_SWITCH_ATTR_PORT_LIST"] =
           "2:oid:0x1000000000002,oid:0x1000000000003";
    dump["SAI_OBJECT_TYPE_ACL_ENTRY:oid:0x8000000000635"]["SAI_ACL_ENTRY_ATTR_FIELD_SRC_PORT"] = "oid:0x1000000000003";
    dump["SAI_OBJECT_TYPE_ACL_ENTRY:oid:0x8000000000635"]["SAI_ACL_ENTRY_ATTR_FIELD_OUT_PORTS"] = "2:oid:0x1000000000002,oid:0x1000000000003";
    dump["SAI_OBJECT_TYPE_ACL_ENTRY:oid:0x8000000000635"]["SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT"] = "oid:0x1000000000003";
    dump["SAI_OBJECT_TYPE_ACL_ENTRY:oid:0x8000000000635"]["SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT_LIST"] = "2:oid:0x1000000000002,oid:0x1000000000003";

    m.populate(dump);
}

TEST(Meta, bulkGetClearStats)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, m.bulkGetStats(SAI_NULL_OBJECT_ID,
                                                         SAI_OBJECT_TYPE_PORT,
                                                         0,
                                                         nullptr,
                                                         0,
                                                         nullptr,
                                                         SAI_STATS_MODE_BULK_READ,
                                                         nullptr,
                                                         nullptr));
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, m.bulkClearStats(SAI_NULL_OBJECT_ID,
                                                           SAI_OBJECT_TYPE_PORT,
                                                           0,
                                                           nullptr,
                                                           0,
                                                           nullptr,
                                                           SAI_STATS_MODE_BULK_CLEAR,
                                                           nullptr));
}

TEST(Meta, quad_ars)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_object_id_t ars;

    std::vector<sai_attribute_t> attrs;

    attr.id = SAI_ARS_ATTR_MODE;
    attr.value.u32 = SAI_ARS_MODE_FLOWLET_QUALITY;
    attrs.push_back(attr);

    attr.id = SAI_ARS_ATTR_IDLE_TIME;
    attr.value.u32 = 100;
    attrs.push_back(attr);

    attr.id = SAI_ARS_ATTR_MAX_FLOWS;
    attr.value.u32 = 500;
    attrs.push_back(attr);

    attr.id = SAI_ARS_ATTR_MON_ENABLE;
    attr.value.booldata = true;
    attrs.push_back(attr);

    attr.id = SAI_ARS_ATTR_SAMPLEPACKET_ENABLE;
    attr.value.oid = SAI_NULL_OBJECT_ID;
    attrs.push_back(attr);

    attr.id = SAI_ARS_ATTR_MAX_ALT_MEMEBERS_PER_GROUP;
    attr.value.u32 = 15;
    attrs.push_back(attr);

    attr.id = SAI_ARS_ATTR_MAX_PRIMARY_MEMEBERS_PER_GROUP;
    attr.value.u32 = 14;
    attrs.push_back(attr);

    attr.id = SAI_ARS_ATTR_PRIMARY_PATH_QUALITY_THRESHOLD;
    attr.value.u32 = 14;
    attrs.push_back(attr);

    attr.id = SAI_ARS_ATTR_ALTERNATE_PATH_COST;
    attr.value.u32 = 1;
    attrs.push_back(attr);

    attr.id = SAI_ARS_ATTR_ALTERNATE_PATH_BIAS;
    attr.value.u32 = 1;
    attrs.push_back(attr);

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_ARS, &ars, switchId, (uint32_t)attrs.size(), attrs.data()));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.get(SAI_OBJECT_TYPE_ARS, ars, (uint32_t)attrs.size(), attrs.data()));

    attr.id = SAI_ARS_ATTR_ALTERNATE_PATH_BIAS;
    attr.value.u32 = 1;
    EXPECT_EQ(SAI_STATUS_SUCCESS, m.set(SAI_OBJECT_TYPE_ARS, ars, &attr));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.remove(SAI_OBJECT_TYPE_ARS, ars));
}

TEST(Meta, quad_ars_profile)
{
    Meta m(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t switchId = 0;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_SWITCH, &switchId, SAI_NULL_OBJECT_ID, 1, &attr));

    sai_object_id_t ars_profile;

    std::vector<sai_attribute_t> attrs;

    attr.id = SAI_ARS_PROFILE_ATTR_ALGO;
    attr.value.u32 = SAI_ARS_PROFILE_ALGO_EWMA;
    attrs.push_back(attr);

    attr.id = SAI_ARS_PROFILE_ATTR_SAMPLING_INTERVAL;
    attr.value.u32 = 15;
    attrs.push_back(attr);

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.create(SAI_OBJECT_TYPE_ARS_PROFILE, &ars_profile, switchId, (uint32_t)attrs.size(), attrs.data()));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.get(SAI_OBJECT_TYPE_ARS_PROFILE, ars_profile, (uint32_t)attrs.size(), attrs.data()));

    attr.id = SAI_ARS_PROFILE_ATTR_ARS_RANDOM_SEED;
    attr.value.u32 = 1;
    EXPECT_EQ(SAI_STATUS_SUCCESS, m.set(SAI_OBJECT_TYPE_ARS_PROFILE, ars_profile, &attr));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.remove(SAI_OBJECT_TYPE_ARS_PROFILE, ars_profile));
}

TEST(Meta, bulkGet)
{
    Meta sai(std::make_shared<MetaTestSaiInterface>());

    sai_object_id_t oids[1] = {0};
    uint32_t attrcount[1] = {0};
    sai_attribute_t* attrs[1] = {0};
    sai_status_t statuses[1] = {0};

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER,
            sai.bulkGet(
                SAI_OBJECT_TYPE_PORT,
                1,
                oids,
                attrcount,
                attrs,
                SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR,
                statuses));
}

TEST(Meta, remove_flow_entry)
{
    Meta sai(std::make_shared<MetaTestSaiInterface>());

    sai_flow_entry_t* e = nullptr;

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, sai.remove(e));
}

TEST(Meta, remove_meter_bucket_entry)
{
    Meta sai(std::make_shared<MetaTestSaiInterface>());

    sai_meter_bucket_entry_t* e = nullptr;

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, sai.remove(e));
}

TEST(Meta, remove_prefix_compression_entry)
{
    Meta sai(std::make_shared<MetaTestSaiInterface>());

    sai_prefix_compression_entry_t* e = nullptr;

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, sai.remove(e));
}

TEST(Meta, isPortObjectIdValid)
{
    EXPECT_EQ(Meta::isPortObjectIdValid(SAI_OBJECT_TYPE_PORT), true);
    EXPECT_EQ(Meta::isPortObjectIdValid(SAI_OBJECT_TYPE_BRIDGE_PORT), true);
    EXPECT_EQ(Meta::isPortObjectIdValid(SAI_OBJECT_TYPE_LAG), true);

    EXPECT_EQ(Meta::isPortObjectIdValid(SAI_OBJECT_TYPE_TUNNEL),false);
    EXPECT_EQ(Meta::isPortObjectIdValid(SAI_OBJECT_TYPE_NULL), false);
    EXPECT_EQ(Meta::isPortObjectIdValid(SAI_OBJECT_TYPE_VLAN), false);
}

TEST(Meta, getValidPortObjectTypes)
{
    auto v = Meta::getValidPortObjectTypes();

    EXPECT_EQ(v.size(), 3);

    auto s = boost::algorithm::join(v, ",");

    EXPECT_EQ(s, "PORT,LAG,BRIDGE_PORT");
}
