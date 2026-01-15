#include "VirtualOidTranslator.h"
#include "RedisClient.h"
#include "VendorSai.h"
#include "lib/RedisVidIndexGenerator.h"
#include "lib/sairediscommon.h"
#include "ServiceMethodTable.h"
#include "vslib/Sai.h"

#include <gtest/gtest.h>

using namespace syncd;
using namespace std::placeholders;

static std::map<std::string, std::string> profileMap;

static std::map<std::string, std::string>::iterator profileIter;

static const char* profileGetValue(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return NULL;
    }

    auto it = profileMap.find(variable);

    if (it == profileMap.end())
    {
        SWSS_LOG_NOTICE("%s: NULL", variable);
        return NULL;
    }

    SWSS_LOG_NOTICE("%s: %s", variable, it->second.c_str());

    return it->second.c_str();
}

static int profileGetNextValue(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
    {
        SWSS_LOG_INFO("resetting profile map iterator");

        profileIter = profileMap.begin();
        return 0;
    }

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return -1;
    }

    if (profileIter == profileMap.end())
    {
        SWSS_LOG_INFO("iterator reached end");
        return -1;
    }

    *variable = profileIter->first.c_str();
    *value = profileIter->second.c_str();

    SWSS_LOG_INFO("key: %s:%s", *variable, *value);

    profileIter++;

    return 0;
}

TEST(VirtualOidTranslator, tryTranslateVidToRid)
{
    profileMap["SAI_VS_SWITCH_TYPE"] = "SAI_VS_SWITCH_TYPE_BCM56850";

    auto dbAsic = std::make_shared<swss::DBConnector>("ASIC_DB", 0);
    auto client = std::make_shared<RedisClient>(dbAsic);
    auto sai = std::make_shared<saivs::Sai>();

    ServiceMethodTable smt;

    smt.profileGetValue = std::bind(&profileGetValue, _1, _2);
    smt.profileGetNextValue = std::bind(&profileGetNextValue, _1, _2, _3);

    sai_service_method_table_t test_services = smt.getServiceMethodTable();

    sai_status_t status = sai->apiInitialize(0, &test_services);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    auto switchConfigContainer = std::make_shared<sairedis::SwitchConfigContainer>();
    auto redisVidIndexGenerator = std::make_shared<sairedis::RedisVidIndexGenerator>(dbAsic, REDIS_KEY_VIDCOUNTER);

    auto virtualObjectIdManager =
        std::make_shared<sairedis::VirtualObjectIdManager>(
                0,
                switchConfigContainer,
                redisVidIndexGenerator);

    VirtualOidTranslator vot(client, virtualObjectIdManager, sai);

    sai_object_id_t rid;

    EXPECT_TRUE(vot.tryTranslateVidToRid(0, rid));

    EXPECT_EQ(rid, 0);

    EXPECT_FALSE(vot.tryTranslateVidToRid(0x21, rid));

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    sai_object_id_t swid;

    status = sai->create(SAI_OBJECT_TYPE_SWITCH, &swid, SAI_NULL_OBJECT_ID, 1, &attr);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    vot.insertRidAndVid(0x2100000000,0x21000000000000);

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    vot.translateRidToVid(0x2100000000,0x21000000000000);

    EXPECT_TRUE(vot.tryTranslateVidToRid(0x21000000000000, rid));

    vot.clearLocalCache();

    EXPECT_TRUE(vot.tryTranslateVidToRid(0x21000000000000, rid));

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    // meta key

    sai_object_meta_key_t mk;

    mk.objecttype = SAI_OBJECT_TYPE_PORT;
    mk.objectkey.key.object_id = 0;

    EXPECT_TRUE(vot.tryTranslateVidToRid(mk));

    mk.objecttype = SAI_OBJECT_TYPE_FDB_ENTRY;
    mk.objectkey.key.fdb_entry.switch_id = 0x21000000000000;
    mk.objectkey.key.fdb_entry.bv_id = 0;

    EXPECT_TRUE(vot.tryTranslateVidToRid(mk));

    mk.objectkey.key.fdb_entry.bv_id = 0x21;

    EXPECT_FALSE(vot.tryTranslateVidToRid(mk));

    sai->apiUninitialize();
}
