#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include <swss/dbconnector.h>

#include "RedisClient.h"
#include "sairediscommon.h"
#include "meta/sai_serialize.h"

using namespace syncd;

// objects of switch index 7, so keys and map fields don't clash with other tests
#define TEST_SWITCH_INDEX 7ULL
#define TEST_SWITCH_VID ((TEST_SWITCH_INDEX << 56) | ((uint64_t)SAI_OBJECT_TYPE_SWITCH << 48))

// more objects than one command takes, so a batch spans commands
static const size_t COUNT = 300;

static sai_object_id_t portVid(
        _In_ uint64_t idx)
{
    SWSS_LOG_ENTER();

    return (TEST_SWITCH_INDEX << 56) | ((uint64_t)SAI_OBJECT_TYPE_PORT << 48) | idx;
}

static sai_object_id_t portRid(
        _In_ uint64_t idx)
{
    SWSS_LOG_ENTER();

    return 0x7700000000000000ULL | idx;
}

class RedisClientTest : public ::testing::Test
{
public:

    virtual void SetUp() override
    {
        m_dbAsic = std::make_shared<swss::DBConnector>("ASIC_DB", 0, true);
        m_redisClient = std::make_shared<RedisClient>(m_dbAsic);
    }

    virtual void TearDown() override
    {
        m_redisClient.reset();
        m_dbAsic.reset();
    }

protected:

    std::shared_ptr<swss::DBConnector> m_dbAsic;
    std::shared_ptr<RedisClient> m_redisClient;
};

TEST_F(RedisClientTest, createAsicObjectWritesAllAttributes)
{
    sai_object_meta_key_t metaKey;
    metaKey.objecttype = SAI_OBJECT_TYPE_PORT;
    metaKey.objectkey.key.object_id = portVid(1);

    auto key = sai_serialize_object_meta_key(metaKey);

    // a field given twice keeps its last value, as with an HSET per field
    m_redisClient->createAsicObject(metaKey, {
            {"SAI_PORT_ATTR_MTU", "1500"},
            {"SAI_PORT_ATTR_ADMIN_STATE", "true"},
            {"SAI_PORT_ATTR_MTU", "9100"}});

    auto attrs = m_redisClient->getAttributesFromAsicKey(ASIC_STATE_TABLE ":" + key);

    EXPECT_EQ(attrs, (std::unordered_map<std::string, std::string>{
                {"SAI_PORT_ATTR_MTU", "9100"},
                {"SAI_PORT_ATTR_ADMIN_STATE", "true"}}));

    m_redisClient->createTempAsicObject(metaKey, {});

    attrs = m_redisClient->getAttributesFromAsicKey(TEMP_PREFIX ASIC_STATE_TABLE ":" + key);

    EXPECT_EQ(attrs, (std::unordered_map<std::string, std::string>{{"NULL", "NULL"}}));

    m_redisClient->removeAsicObject(metaKey);
    m_redisClient->removeTempAsicObject(metaKey);

    EXPECT_FALSE(m_dbAsic->exists(ASIC_STATE_TABLE ":" + key));
    EXPECT_FALSE(m_dbAsic->exists(TEMP_PREFIX ASIC_STATE_TABLE ":" + key));
}

TEST_F(RedisClientTest, createAndRemoveAsicObjects)
{
    std::unordered_map<std::string, std::vector<swss::FieldValueTuple>> multiHash;
    std::vector<std::string> keys;

    for (size_t idx = 0; idx < COUNT; idx++)
    {
        auto key = "SAI_OBJECT_TYPE_PORT:" + sai_serialize_object_id(portVid(idx));

        keys.push_back(key);

        if (idx % 2)
        {
            multiHash[key] = {{"SAI_PORT_ATTR_MTU", std::to_string(idx)}};
        }
        else
        {
            multiHash[key] = {};
        }
    }

    m_redisClient->createAsicObjects(multiHash);

    for (size_t idx = 0; idx < COUNT; idx++)
    {
        auto attrs = m_redisClient->getAttributesFromAsicKey(ASIC_STATE_TABLE ":" + keys[idx]);

        if (idx % 2)
        {
            EXPECT_EQ(attrs["SAI_PORT_ATTR_MTU"], std::to_string(idx));
        }
        else
        {
            EXPECT_EQ(attrs["NULL"], "NULL");
        }
    }

    m_redisClient->removeAsicObjects(keys);

    for (const auto& key: keys)
    {
        EXPECT_FALSE(m_dbAsic->exists(ASIC_STATE_TABLE ":" + key));
    }
}

TEST_F(RedisClientTest, insertAndRemoveVidsAndRids)
{
    std::vector<sai_object_id_t> vids;
    std::vector<sai_object_id_t> rids;

    for (size_t idx = 0; idx < COUNT; idx++)
    {
        vids.push_back(portVid(idx));
        rids.push_back(portRid(idx));
    }

    m_redisClient->insertVidsAndRids(COUNT, vids.data(), rids.data());
    m_redisClient->insertVidAndRid(portVid(COUNT), portRid(COUNT));

    for (size_t idx = 0; idx <= COUNT; idx++)
    {
        EXPECT_EQ(m_redisClient->getVidForRid(portRid(idx)), portVid(idx));
        EXPECT_EQ(m_redisClient->getRidForVid(portVid(idx)), portRid(idx));
    }

    // all but the first one
    m_redisClient->removeVidsAndRids(COUNT - 1, vids.data() + 1, rids.data() + 1);

    EXPECT_EQ(m_redisClient->getVidForRid(portRid(0)), portVid(0));
    EXPECT_EQ(m_redisClient->getRidForVid(portVid(0)), portRid(0));

    for (size_t idx = 1; idx < COUNT; idx++)
    {
        EXPECT_EQ(m_redisClient->getVidForRid(portRid(idx)), SAI_NULL_OBJECT_ID);
        EXPECT_EQ(m_redisClient->getRidForVid(portVid(idx)), SAI_NULL_OBJECT_ID);
    }

    m_redisClient->removeVidAndRid(portVid(0), portRid(0));
    m_redisClient->removeVidAndRid(portVid(COUNT), portRid(COUNT));

    EXPECT_EQ(m_redisClient->getVidForRid(portRid(0)), SAI_NULL_OBJECT_ID);
    EXPECT_EQ(m_redisClient->getRidForVid(portVid(COUNT)), SAI_NULL_OBJECT_ID);
}

TEST_F(RedisClientTest, saveAndGetColdVids)
{
    std::set<sai_object_id_t> coldVids;

    for (size_t idx = 0; idx < COUNT; idx++)
    {
        coldVids.insert(portVid(idx));
    }

    m_redisClient->insertVidAndRid(portVid(0), portRid(0));

    m_redisClient->saveColdBootDiscoveredVids(TEST_SWITCH_VID, coldVids);

    EXPECT_EQ(m_redisClient->getColdVids(TEST_SWITCH_VID), coldVids);

    auto key = "COLDVIDS:" + sai_serialize_object_id(TEST_SWITCH_VID);

    EXPECT_EQ(*m_dbAsic->hget(key, sai_serialize_object_id(portVid(1))), "SAI_OBJECT_TYPE_PORT");

    m_dbAsic->del(key);

    m_redisClient->removeVidAndRid(portVid(0), portRid(0));
}

TEST_F(RedisClientTest, failedCommandKeepsConnectionInSync)
{
    std::unordered_map<std::string, std::vector<swss::FieldValueTuple>> multiHash;

    for (size_t idx = 0; idx < 3; idx++)
    {
        multiHash["SAI_OBJECT_TYPE_PORT:" + sai_serialize_object_id(portVid(idx))] = {{"SAI_PORT_ATTR_MTU", "9100"}};
    }

    // HSET on a string fails
    auto badKey = ASIC_STATE_TABLE ":SAI_OBJECT_TYPE_PORT:" + sai_serialize_object_id(portVid(1));

    m_dbAsic->set(badKey, "string");

    EXPECT_THROW(m_redisClient->createAsicObjects(multiHash), std::exception);

    // the other commands of the batch were executed, and the next commands
    // get their own replies

    for (size_t idx = 0; idx < 3; idx += 2)
    {
        auto key = ASIC_STATE_TABLE ":SAI_OBJECT_TYPE_PORT:" + sai_serialize_object_id(portVid(idx));

        EXPECT_EQ(*m_dbAsic->hget(key, "SAI_PORT_ATTR_MTU"), "9100");
    }

    EXPECT_EQ(*m_dbAsic->get(badKey), "string");

    m_redisClient->insertVidAndRid(portVid(5), portRid(5));

    EXPECT_EQ(m_redisClient->getVidForRid(portRid(5)), portVid(5));

    std::vector<std::string> keys;

    for (const auto& kvp: multiHash)
    {
        keys.push_back(kvp.first);
    }

    m_redisClient->removeAsicObjects(keys);
    m_redisClient->removeVidAndRid(portVid(5), portRid(5));

    EXPECT_FALSE(m_dbAsic->exists(badKey));
}
