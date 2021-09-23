#include "Meta.h"
#include "MockMeta.h"
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

TEST(Meta, queryAattributeEnumValuesCapability)
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

    EXPECT_EQ(SAI_STATUS_SUCCESS, m.queryAattributeEnumValuesCapability(switchId, SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_SWITCHING_MODE, &list));

    // set count without list;

    list.list = nullptr;

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.queryAattributeEnumValuesCapability(switchId, SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_SWITCHING_MODE, &list));

    // non enum attribute

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.queryAattributeEnumValuesCapability(switchId, SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_BCAST_CPU_FLOOD_ENABLE, &list));

    // invalid attribute

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, m.queryAattributeEnumValuesCapability(switchId, SAI_OBJECT_TYPE_SWITCH, 10000, &list));
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
