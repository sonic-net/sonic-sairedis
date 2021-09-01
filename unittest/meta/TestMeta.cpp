#include "Meta.h"
#include "MetaTestSaiInterface.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saimeta;

TEST(Meta, initialize)
{
    Meta m(std::make_shared<DummySaiInterface>());

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.initialize(0,0));
}

TEST(Meta, uninitialize)
{
    Meta m(std::make_shared<DummySaiInterface>());

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.uninitialize());
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
}
