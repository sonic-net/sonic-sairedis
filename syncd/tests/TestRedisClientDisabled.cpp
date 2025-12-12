#include <gtest/gtest.h>
#include <memory>

#include <swss/dbconnector.h>

#include "RedisClient.h"

using namespace syncd;

class RedisClientDisabledTest : public ::testing::Test
{
public:
    RedisClientDisabledTest() = default;
    virtual ~RedisClientDisabledTest() = default;

public:
    virtual void SetUp() override
    {
        m_dbAsic = std::make_shared<swss::DBConnector>("ASIC_DB", 0, true);
        m_redisClient = std::make_shared<RedisClient>(m_dbAsic, true, false);
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

TEST_F(RedisClientDisabledTest, getLaneMapReturnsEmpty)
{
    sai_object_id_t switchVid = 0x21000000000000;
    auto result = m_redisClient->getLaneMap(switchVid);
    EXPECT_TRUE(result.empty());
}

TEST_F(RedisClientDisabledTest, getVidToRidMapWithSwitchVidReturnsEmpty)
{
    sai_object_id_t switchVid = 0x21000000000000;
    auto result = m_redisClient->getVidToRidMap(switchVid);
    EXPECT_TRUE(result.empty());
}

TEST_F(RedisClientDisabledTest, getRidToVidMapWithSwitchVidReturnsEmpty)
{
    sai_object_id_t switchVid = 0x21000000000000;
    auto result = m_redisClient->getRidToVidMap(switchVid);
    EXPECT_TRUE(result.empty());
}

TEST_F(RedisClientDisabledTest, getVidToRidMapReturnsEmpty)
{
    auto result = m_redisClient->getVidToRidMap();
    EXPECT_TRUE(result.empty());
}

TEST_F(RedisClientDisabledTest, getRidToVidMapReturnsEmpty)
{
    auto result = m_redisClient->getRidToVidMap();
    EXPECT_TRUE(result.empty());
}

TEST_F(RedisClientDisabledTest, getColdVidsReturnsEmpty)
{
    sai_object_id_t switchVid = 0x21000000000000;
    auto result = m_redisClient->getColdVids(switchVid);
    EXPECT_TRUE(result.empty());
}

TEST_F(RedisClientDisabledTest, getAsicObjectsSizeReturnsZero)
{
    sai_object_id_t switchVid = 0x21000000000000;
    auto result = m_redisClient->getAsicObjectsSize(switchVid);
    EXPECT_EQ(result, 0);
}

TEST_F(RedisClientDisabledTest, removePortFromLanesMapReturnsZero)
{
    sai_object_id_t switchVid = 0x21000000000000;
    sai_object_id_t portRid = 0x10000000000001;
    auto result = m_redisClient->removePortFromLanesMap(switchVid, portRid);
    EXPECT_EQ(result, 0);
}

TEST_F(RedisClientDisabledTest, getAsicStateKeysReturnsEmpty)
{
    auto result = m_redisClient->getAsicStateKeys();
    EXPECT_TRUE(result.empty());
}

TEST_F(RedisClientDisabledTest, getAsicStateSwitchesKeysReturnsEmpty)
{
    auto result = m_redisClient->getAsicStateSwitchesKeys();
    EXPECT_TRUE(result.empty());
}

TEST_F(RedisClientDisabledTest, getAttributesFromAsicKeyReturnsEmpty)
{
    auto result = m_redisClient->getAttributesFromAsicKey("somekey");
    EXPECT_TRUE(result.empty());
}

TEST_F(RedisClientDisabledTest, getVidForRidReturnsNullObjectId)
{
    sai_object_id_t rid = 0x10000000000001;
    auto result = m_redisClient->getVidForRid(rid);
    EXPECT_EQ(result, SAI_NULL_OBJECT_ID);
}

TEST_F(RedisClientDisabledTest, getRidForVidReturnsNullObjectId)
{
    sai_object_id_t vid = 0x21000000000001;
    auto result = m_redisClient->getRidForVid(vid);
    EXPECT_EQ(result, SAI_NULL_OBJECT_ID);
}

TEST_F(RedisClientDisabledTest, getAsicViewReturnsEmpty)
{
    auto result = m_redisClient->getAsicView();
    EXPECT_TRUE(result.empty());
}

TEST_F(RedisClientDisabledTest, getTempAsicViewReturnsEmpty)
{
    auto result = m_redisClient->getTempAsicView();
    EXPECT_TRUE(result.empty());
}

TEST_F(RedisClientDisabledTest, getSwitchHiddenAttributeReturnsNullptr)
{
    sai_object_id_t switchVid = 0x21000000000000;
    auto result = m_redisClient->getSwitchHiddenAttribute(switchVid, "someAttr");
    EXPECT_EQ(result, nullptr);
}

TEST_F(RedisClientDisabledTest, voidMethodsDoNotThrow)
{
    sai_object_id_t switchVid = 0x21000000000000;
    sai_object_id_t vid = 0x21000000000001;
    sai_object_id_t rid = 0x10000000000001;

    EXPECT_NO_THROW(m_redisClient->clearLaneMap(switchVid));

    std::unordered_map<sai_uint32_t, sai_object_id_t> laneMap;
    EXPECT_NO_THROW(m_redisClient->saveLaneMap(switchVid, laneMap));

    EXPECT_NO_THROW(m_redisClient->setDummyAsicStateObject(vid));

    sai_object_id_t objectVids[1] = {vid};
    EXPECT_NO_THROW(m_redisClient->setDummyAsicStateObjects(1, objectVids));

    std::set<sai_object_id_t> coldVids;
    EXPECT_NO_THROW(m_redisClient->saveColdBootDiscoveredVids(switchVid, coldVids));

    EXPECT_NO_THROW(m_redisClient->saveSwitchHiddenAttribute(switchVid, "attr", rid));

    std::vector<uint32_t> lanes = {1, 2};
    EXPECT_NO_THROW(m_redisClient->setPortLanes(switchVid, rid, lanes));

    EXPECT_NO_THROW(m_redisClient->removeAsicObject(vid));

    sai_object_meta_key_t metaKey;
    metaKey.objecttype = SAI_OBJECT_TYPE_SWITCH;
    metaKey.objectkey.key.object_id = vid;
    EXPECT_NO_THROW(m_redisClient->removeAsicObject(metaKey));

    EXPECT_NO_THROW(m_redisClient->removeTempAsicObject(metaKey));

    std::vector<swss::FieldValueTuple> attrs;
    EXPECT_NO_THROW(m_redisClient->createAsicObject(metaKey, attrs));

    EXPECT_NO_THROW(m_redisClient->createTempAsicObject(metaKey, attrs));

    std::unordered_map<std::string, std::vector<swss::FieldValueTuple>> multiHash;
    EXPECT_NO_THROW(m_redisClient->createAsicObjects(multiHash));

    EXPECT_NO_THROW(m_redisClient->createTempAsicObjects(multiHash));

    std::unordered_map<sai_object_id_t, sai_object_id_t> vidRidMap;
    EXPECT_NO_THROW(m_redisClient->setVidAndRidMap(vidRidMap));

    EXPECT_NO_THROW(m_redisClient->removeColdVid(vid));

    EXPECT_NO_THROW(m_redisClient->removeVidAndRid(vid, rid));

    EXPECT_NO_THROW(m_redisClient->insertVidAndRid(vid, rid));

    sai_object_id_t vids[1] = {vid};
    sai_object_id_t rids[1] = {rid};
    EXPECT_NO_THROW(m_redisClient->insertVidsAndRids(1, vids, rids));

    sai_object_id_t outVids[1];
    EXPECT_NO_THROW(m_redisClient->getVidsForRids(1, rids, outVids));

    EXPECT_NO_THROW(m_redisClient->removeAsicStateTable());

    EXPECT_NO_THROW(m_redisClient->removeTempAsicStateTable());

    EXPECT_NO_THROW(m_redisClient->setAsicObject(metaKey, "attr", "value"));

    EXPECT_NO_THROW(m_redisClient->setTempAsicObject(metaKey, "attr", "value"));

    std::vector<std::string> keys;
    EXPECT_NO_THROW(m_redisClient->removeTempAsicObjects(keys));

    EXPECT_NO_THROW(m_redisClient->processFlushEvent(switchVid, vid, vid, SAI_FDB_FLUSH_ENTRY_TYPE_DYNAMIC));
}

