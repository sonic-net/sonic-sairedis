#include "TestLegacy.h"

#include <arpa/inet.h>

#include <gtest/gtest.h>

#include <memory>

using namespace TestLegacy;

// STATIC HELPERS

//static sai_object_id_t create_vlan(
//        _In_ sai_object_id_t switch_id)
//{
//    SWSS_LOG_ENTER();
//
//    sai_object_id_t vlan;
//
//    sai_attribute_t attrs[9] = { };
//
//    attrs[0].id = SAI_VLAN_ATTR_VLAN_ID;
//    attrs[0].value.u16 = 1;
//
//    auto status = g_meta->create(SAI_OBJECT_TYPE_VLAN, &vlan, switch_id, 1, attrs);
//    EXPECT_EQ(SAI_STATUS_SUCCESS, status);
//
//    return vlan;
//}

// ACTUAL TESTS

TEST(LegacyVlan, vlan_create)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t    status;

    sai_attribute_t vlan1_att;
    vlan1_att.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan1_att.value.u16 = 1;

    sai_object_id_t vlan1_id;

    sai_object_id_t switch_id = create_switch();

    status = g_meta->create(SAI_OBJECT_TYPE_VLAN, &vlan1_id, switch_id, 1, &vlan1_att);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    sai_attribute_t vlan;
    vlan.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan.value.u16 = 2;

    sai_object_id_t vlan_id;

    SWSS_LOG_NOTICE("create tests");

//    SWSS_LOG_NOTICE("existing vlan");
//    status = g_meta->create(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &vlan);
//    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    vlan.value.u16 = MAXIMUM_VLAN_NUMBER + 1;

//    SWSS_LOG_NOTICE("vlan outside range");
//    status = g_meta->create(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &vlan);
//    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    vlan.value.u16 = 2;

    SWSS_LOG_NOTICE("correct");
    status = g_meta->create(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &vlan);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("existing");
    status = g_meta->create(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &vlan);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(LegacyVlan, vlan_remove)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t status;

    sai_attribute_t vlan1_att;
    vlan1_att.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan1_att.value.u16 = 1;

    sai_object_id_t vlan1_id;
    sai_object_id_t switch_id = create_switch();

    SWSS_LOG_NOTICE("create");

    status = g_meta->create(SAI_OBJECT_TYPE_VLAN, &vlan1_id, switch_id, 1, &vlan1_att);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    sai_attribute_t vlan;
    vlan.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan.value.u16 = 2;

    sai_object_id_t vlan_id;

    SWSS_LOG_NOTICE("correct");
    status = g_meta->create(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &vlan);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove tests");

    SWSS_LOG_NOTICE("invalid vlan");
    status = g_meta->remove(SAI_OBJECT_TYPE_VLAN, SAI_NULL_OBJECT_ID);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

//    SWSS_LOG_NOTICE("default vlan");
//    status = g_meta->remove(SAI_OBJECT_TYPE_VLAN, vlan1_id);
//    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("success");
    status = g_meta->remove(SAI_OBJECT_TYPE_VLAN, vlan_id);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("non existing");
    status = g_meta->remove(SAI_OBJECT_TYPE_VLAN, vlan_id);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(LegacyVlan, vlan_set)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_status_t status;

    sai_attribute_t attr;

    memset(&attr, 0, sizeof(attr));

    sai_attribute_t vlan1_att;
    vlan1_att.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan1_att.value.u16 = 1;

    sai_object_id_t vlan1_id;
    sai_object_id_t switch_id = create_switch();

    sai_object_id_t stp = create_stp(switch_id);

    SWSS_LOG_NOTICE("create");

    status = g_meta->create(SAI_OBJECT_TYPE_VLAN, &vlan1_id, switch_id, 1, &vlan1_att);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    sai_attribute_t vlan;
    vlan.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan.value.u16 = 2;

    sai_object_id_t vlan_id;

    SWSS_LOG_NOTICE("correct");
    status = g_meta->create(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &vlan);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("set tests");

    SWSS_LOG_NOTICE("invalid vlan");
    status = g_meta->set(SAI_OBJECT_TYPE_VLAN, SAI_NULL_OBJECT_ID, &vlan);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("set is null");
    status = g_meta->set(SAI_OBJECT_TYPE_VLAN, vlan_id, &vlan);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("attr is null");
    status = g_meta->set(SAI_OBJECT_TYPE_VLAN, vlan_id, NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    attr.id = -1;

    SWSS_LOG_NOTICE("invalid attribute");
    status = g_meta->set(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    attr.id = SAI_VLAN_ATTR_MEMBER_LIST;

    SWSS_LOG_NOTICE("read only");
    status = g_meta->set(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("max learned addresses");
    attr.id = SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES;
    attr.value.u32 = 1;
    status = g_meta->set(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("null stp instance");
    attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
    attr.value.oid = SAI_NULL_OBJECT_ID;
    status = g_meta->set(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("wrong type on stp instance");
    attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
    attr.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_HASH,switch_id);
    status = g_meta->set(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("wrong type on stp instance");
    attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
    attr.value.oid = create_dummy_object_id(SAI_OBJECT_TYPE_STP,switch_id);
    status = g_meta->set(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("good stp oid");
    attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
    attr.value.oid = stp;
    status = g_meta->set(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("learn disable");
    attr.id = SAI_VLAN_ATTR_LEARN_DISABLE;
    attr.value.booldata = false;
    status = g_meta->set(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("metadat");
    attr.id = SAI_VLAN_ATTR_META_DATA;
    attr.value.u32 = 1;
    status = g_meta->set(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(LegacyVlan, vlan_get)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_object_id_t switch_id = create_switch();
    sai_status_t status;

    sai_attribute_t attr;

    memset(&attr, 0, sizeof(attr));

    sai_attribute_t vlan1_att;
    vlan1_att.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan1_att.value.u16 = 1;

    sai_object_id_t vlan1_id;

    sai_object_id_t stp = create_stp(switch_id);

    SWSS_LOG_NOTICE("create");

    status = g_meta->create(SAI_OBJECT_TYPE_VLAN, &vlan1_id, switch_id, 1, &vlan1_att);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    sai_attribute_t vlan;
    vlan.id = SAI_VLAN_ATTR_VLAN_ID;
    vlan.value.u16 = 2;

    sai_object_id_t vlan_id;

    SWSS_LOG_NOTICE("correct");
    status = g_meta->create(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &vlan);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("get tests");

    attr.id = SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES;

    SWSS_LOG_NOTICE("invalid vlan");
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, 0, 1, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

//    SWSS_LOG_NOTICE("invalid vlan");
//    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, 3, 1, &attr);
//    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("attr is null");
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, NULL);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("zero attributes");
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 0, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    attr.id = -1;

    SWSS_LOG_NOTICE("invalid attribute");
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    attr.id = SAI_VLAN_ATTR_MEMBER_LIST;
    attr.value.objlist.count = 1;
    attr.value.objlist.list = NULL;

    SWSS_LOG_NOTICE("read only null list");
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    sai_object_id_t list[5] = { };

    list[0] = SAI_NULL_OBJECT_ID;
    list[1] = create_dummy_object_id(SAI_OBJECT_TYPE_HASH,switch_id);
    list[2] = create_dummy_object_id(SAI_OBJECT_TYPE_VLAN_MEMBER,switch_id);
    list[3] = stp;

    attr.value.objlist.count = 0;
    attr.value.objlist.list = list;

    SWSS_LOG_NOTICE("readonly 0 count and not null");
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
//    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    attr.value.objlist.count = 5;

    SWSS_LOG_NOTICE("readonly count and not null");
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    attr.value.objlist.count = 0;
    attr.value.objlist.list = NULL;

    SWSS_LOG_NOTICE("readonly count 0 and null");
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("max learned addresses");
    attr.id = SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES;
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("stp instance");
    attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("learn disable");
    attr.id = SAI_VLAN_ATTR_LEARN_DISABLE;
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("metadata");
    attr.id = SAI_VLAN_ATTR_META_DATA;
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}

TEST(LegacyVlan, vlan_flow)
{
    SWSS_LOG_ENTER();

    clear_local();

    sai_attribute_t attr;

    sai_status_t    status;

    sai_object_id_t vlan_id;

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 2;
    sai_object_id_t switch_id = create_switch();

    sai_object_id_t stp = create_stp(switch_id);

    SWSS_LOG_NOTICE("create");

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = 2;

    SWSS_LOG_NOTICE("correct");
    status = g_meta->create(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("existing");
    status = g_meta->create(SAI_OBJECT_TYPE_VLAN, &vlan_id, switch_id, 1, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("set");

    SWSS_LOG_NOTICE("max learned addresses");
    attr.id = SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES;
    attr.value.u32 = 1;
    status = g_meta->set(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("good stp oid");
    attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
    attr.value.oid = stp;
    status = g_meta->set(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("learn disable");
    attr.id = SAI_VLAN_ATTR_LEARN_DISABLE;
    attr.value.booldata = false;
    status = g_meta->set(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("metadata");
    attr.id = SAI_VLAN_ATTR_META_DATA;
    attr.value.u32 = 1;
    status = g_meta->set(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("get");

    sai_object_id_t list[5] = { };

    list[0] = SAI_NULL_OBJECT_ID;
    list[1] = create_dummy_object_id(SAI_OBJECT_TYPE_HASH,switch_id);
    list[2] = create_dummy_object_id(SAI_OBJECT_TYPE_VLAN_MEMBER,switch_id);
    list[3] = stp;

    attr.value.objlist.count = 0;
    attr.value.objlist.list = list;

    SWSS_LOG_NOTICE("readonly 0 count and not null");
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    attr.value.objlist.count = 5;

    SWSS_LOG_NOTICE("readonly count and not null");
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    attr.value.objlist.count = 0;
    attr.value.objlist.list = NULL;

    SWSS_LOG_NOTICE("readonly count 0 and null");
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("max learned addresses");
    attr.id = SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES;
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("stp instance");
    attr.id = SAI_VLAN_ATTR_STP_INSTANCE;
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("learn disable");
    attr.id = SAI_VLAN_ATTR_LEARN_DISABLE;
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("metadata");
    attr.id = SAI_VLAN_ATTR_META_DATA;
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("remove");

    SWSS_LOG_NOTICE("success");
    status = g_meta->remove(SAI_OBJECT_TYPE_VLAN, vlan_id);
    EXPECT_EQ(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("non existing");
    status = g_meta->remove(SAI_OBJECT_TYPE_VLAN, vlan_id);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    SWSS_LOG_NOTICE("learn disable");
    attr.id = SAI_VLAN_ATTR_LEARN_DISABLE;
    status = g_meta->get(SAI_OBJECT_TYPE_VLAN, vlan_id, 1, &attr);
    EXPECT_NE(SAI_STATUS_SUCCESS, status);

    remove_switch(switch_id);
}
