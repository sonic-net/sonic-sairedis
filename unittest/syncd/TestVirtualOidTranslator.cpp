#include "VirtualOidTranslator.h"
#include "DisabledRedisClient.h"
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

TEST(VirtualOidTranslator, tryTranslateRidToVidDisabledRedisClient)
{
    /*
     * With DisabledRedisClient (ZMQ / no ASIC_DB RID map), tryTranslateRidToVid
     * must use the in-memory m_rid2vid map filled by insertRidAndVid - not
     * getVidForRid (which always returns SAI_NULL_OBJECT_ID).
     *
     * ASIC_DB is still used for RedisVidIndexGenerator / VirtualObjectIdManager;
     * only the translator's BaseRedisClient is DisabledRedisClient, so RID->VID
     * is not read from Redis for this path.
     */
    profileMap["SAI_VS_SWITCH_TYPE"] = "SAI_VS_SWITCH_TYPE_BCM56850";

    auto dbAsic = std::make_shared<swss::DBConnector>("ASIC_DB", 0);
    auto client = std::make_shared<DisabledRedisClient>();
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

    const sai_object_id_t rid = 0x21000000aa;
    const sai_object_id_t vid = 0x21000000000000aaULL;

    sai_object_id_t out = rid;

    EXPECT_FALSE(vot.tryTranslateRidToVid(rid, out));
    EXPECT_EQ(out, SAI_NULL_OBJECT_ID);

    vot.insertRidAndVid(rid, vid);

    EXPECT_EQ(client->getVidForRid(rid), SAI_NULL_OBJECT_ID);

    out = rid;
    EXPECT_TRUE(vot.tryTranslateRidToVid(rid, out));
    EXPECT_EQ(out, vid);

    sai_object_id_t nullOut = 0xdeadbeefULL;
    EXPECT_TRUE(vot.tryTranslateRidToVid(SAI_NULL_OBJECT_ID, nullOut));
    EXPECT_EQ(nullOut, SAI_NULL_OBJECT_ID);

    sai->apiUninitialize();
}

TEST(VirtualOidTranslator, translateRidsToVidsDisabledRedisClient)
{
    /*
     * DisabledRedisClient::getVidsForRids returns all SAI_NULL_OBJECT_ID.
     * translateRidsToVids must still resolve RIDs that were registered with
     * insertRidsAndVids (in-memory map), without allocating replacement VIDs.
     *
     * ASIC_DB is for RedisVidIndexGenerator only; translator uses DisabledRedisClient.
     */
    profileMap["SAI_VS_SWITCH_TYPE"] = "SAI_VS_SWITCH_TYPE_BCM56850";

    auto dbAsic = std::make_shared<swss::DBConnector>("ASIC_DB", 0);
    auto client = std::make_shared<DisabledRedisClient>();
    auto sai = std::make_shared<saivs::Sai>();

    ServiceMethodTable smt;
    smt.profileGetValue = std::bind(&profileGetValue, _1, _2);
    smt.profileGetNextValue = std::bind(&profileGetNextValue, _1, _2, _3);
    sai_service_method_table_t test_services = smt.getServiceMethodTable();

    sai_status_t status = sai->apiInitialize(0, &test_services);
    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    auto switchConfigContainer = std::make_shared<sairedis::SwitchConfigContainer>();
    auto redisVidIndexGenerator = std::make_shared<sairedis::RedisVidIndexGenerator>(dbAsic, REDIS_KEY_VIDCOUNTER);
    auto virtualObjectIdManager = std::make_shared<sairedis::VirtualObjectIdManager>(
            0,
            switchConfigContainer,
            redisVidIndexGenerator);

    VirtualOidTranslator vot(client, virtualObjectIdManager, sai);

    const sai_object_id_t switchVid = 0x21000000000000ULL;
    const sai_object_id_t rid0 = 0x1100000022ULL;
    const sai_object_id_t rid1 = 0x2200000022ULL;
    const sai_object_id_t vid0 = 0x2100000000000001ULL;
    const sai_object_id_t vid1 = 0x2100000000000002ULL;

    std::vector<sai_object_id_t> rids = {rid0, rid1};
    std::vector<sai_object_id_t> insertVids = {vid0, vid1};

    vot.insertRidsAndVids(rids.size(), rids.data(), insertVids.data());

    std::vector<sai_object_id_t> probe(2);
    client->getVidsForRids(rids.size(), rids.data(), probe.data());
    EXPECT_EQ(probe[0], SAI_NULL_OBJECT_ID);
    EXPECT_EQ(probe[1], SAI_NULL_OBJECT_ID);

    std::vector<sai_object_id_t> outVids = {SAI_NULL_OBJECT_ID, SAI_NULL_OBJECT_ID};
    vot.translateRidsToVids(switchVid, rids.size(), rids.data(), outVids.data());

    EXPECT_EQ(outVids[0], vid0);
    EXPECT_EQ(outVids[1], vid1);

    sai_object_id_t singleRid = rid0;
    sai_object_id_t singleVid = SAI_NULL_OBJECT_ID;
    vot.translateRidsToVids(switchVid, 1, &singleRid, &singleVid);
    EXPECT_EQ(singleVid, vid0);

    sai->apiUninitialize();
}
