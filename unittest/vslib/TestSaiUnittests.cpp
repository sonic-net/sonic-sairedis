#include "Sai.h"

//#include "meta/sai_serialize.h"
#include "saivs.h"

#include "swss/notificationproducer.h"

#include <gtest/gtest.h>

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

    return 0;
}

sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

TEST(SaiUnittests, ctr)
{
    Sai sai;

    sai.initialize(0, &test_services);

    std::vector<swss::FieldValueTuple> values;

    sai_attribute_t attr;

    sai_object_id_t switch_id;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_EQ(sai.create(SAI_OBJECT_TYPE_SWITCH, &switch_id, SAI_NULL_OBJECT_ID, 1, &attr), SAI_STATUS_SUCCESS);

    attr.id = SAI_SWITCH_ATTR_PORT_MAX_MTU;
    attr.value.u32 = 41;

    EXPECT_NE(sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr), SAI_STATUS_SUCCESS);

    //sai.call_handleUnittestChannelOp("op", "key", values);

    swss::DBConnector db("ASIC_DB", 0, true);
    swss::NotificationProducer vsntf(&db, SAI_VS_UNITTEST_CHANNEL);

    std::vector<swss::FieldValueTuple> entry;

    // needs to be done only once
    vsntf.send(SAI_VS_UNITTEST_ENABLE_UNITTESTS, "true", entry);

    entry.emplace_back("SAI_SWITCH_ATTR_PORT_MAX_MTU", "42");

    std::string data = "SAI_OBJECT_TYPE_SWITCH:oid:0x2100000000";

    vsntf.send(SAI_VS_UNITTEST_SET_RO_OP, data, entry);

    usleep(100*1000);

    // just scramble value to make sure that GET will succeed
    attr.value.u32 = 1;

    EXPECT_EQ(sai.get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr), SAI_STATUS_SUCCESS);

    EXPECT_EQ(attr.value.u32, 42);
}

TEST(SaiUnittests, handleUnittestChannelOp)
{
    Sai sai;

    std::vector<swss::FieldValueTuple> values;

    swss::DBConnector db("ASIC_DB", 0, true);
    swss::NotificationProducer vsntf(&db, SAI_VS_UNITTEST_CHANNEL);

    std::vector<swss::FieldValueTuple> entry;

    // api not initialized
    vsntf.send("unknown op", "true", entry);

    sai.initialize(0, &test_services);

    vsntf.send("unknown op", "true", entry);

    usleep(100*1000);
}

TEST(SaiUnittests, channelOpSetReadOnlyAttribute)
{
    Sai sai;

    sai.initialize(0, &test_services);

    std::vector<swss::FieldValueTuple> values;

    swss::DBConnector db("ASIC_DB", 0, true);
    swss::NotificationProducer vsntf(&db, SAI_VS_UNITTEST_CHANNEL);

    std::vector<swss::FieldValueTuple> entry;

    // empty fields
    vsntf.send(SAI_VS_UNITTEST_SET_RO_OP, "invalid", entry);

    entry.emplace_back("SAI_SWITCH_ATTR_PORT_MAX_MTU", "42");

    vsntf.send(SAI_VS_UNITTEST_SET_RO_OP, "SAI_OBJECT_TYPE_NULL:oid:0x2100000000", entry);

    vsntf.send(SAI_VS_UNITTEST_SET_RO_OP, "SAI_OBJECT_TYPE_PORT:oid:0x2100000000", entry);

    vsntf.send(SAI_VS_UNITTEST_SET_RO_OP, "SAI_OBJECT_TYPE_ROUTE_ENTRY:oid:0x2100000000", entry);

    vsntf.send(SAI_VS_UNITTEST_SET_RO_OP, "SAI_OBJECT_TYPE_SWITCH:oid:0x0", entry);

    entry.clear();
    entry.emplace_back("SAI_SWITCH_ATTR_PORT_MAX_MTU_unvalid", "42");
    vsntf.send(SAI_VS_UNITTEST_SET_RO_OP, "SAI_OBJECT_TYPE_SWITCH:oid:0x2100000000", entry);

    entry.clear();
    entry.emplace_back("SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES", "42");
    vsntf.send(SAI_VS_UNITTEST_SET_RO_OP, "SAI_OBJECT_TYPE_SWITCH:oid:0x2100000000", entry);

    entry.clear();
    entry.emplace_back("SAI_SWITCH_ATTR_PORT_MAX_MTU", "42");

    vsntf.send(SAI_VS_UNITTEST_SET_RO_OP, "SAI_OBJECT_TYPE_SWITCH:oid:0x2100000000", entry);

    usleep(100*1000);
}

TEST(SaiUnittests, channelOpSetStats)
{
    Sai sai;

    sai.initialize(0, &test_services);

    std::vector<swss::FieldValueTuple> values;

    swss::DBConnector db("ASIC_DB", 0, true);
    swss::NotificationProducer vsntf(&db, SAI_VS_UNITTEST_CHANNEL);

    std::vector<swss::FieldValueTuple> entry;

    // null oid
    vsntf.send(SAI_VS_UNITTEST_SET_STATS_OP, "oid:0x0", entry);

    // invalid oid
    vsntf.send(SAI_VS_UNITTEST_SET_STATS_OP, "oid:0x1111111111", entry);

    // valid oid
    vsntf.send(SAI_VS_UNITTEST_SET_STATS_OP, "oid:0x2100000000", entry);

    entry.clear();
    entry.emplace_back("SAI_SWITCH_STAT_IN_CONFIGURED_DROP_REASONS_0_DROPPED_PKTS", "foo");
    vsntf.send(SAI_VS_UNITTEST_SET_STATS_OP, "oid:0x2100000000", entry);

    entry.clear();
    entry.emplace_back("bar", "54");
    vsntf.send(SAI_VS_UNITTEST_SET_STATS_OP, "oid:0x2100000000", entry);

    usleep(100*1000);
}

