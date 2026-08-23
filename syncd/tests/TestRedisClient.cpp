#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include <swss/logger.h>
#include <swss/dbconnector.h>
#include <swss/table.h>

#include "RedisClient.h"
#include "sairediscommon.h"
#include "meta/sai_serialize.h"

using namespace syncd;

class RedisClientTest : public ::testing::Test
{
public:
    RedisClientTest() = default;
    virtual ~RedisClientTest() = default;

public:
    virtual void SetUp() override
    {
        SWSS_LOG_ENTER();

        m_dbAsic = std::make_shared<swss::DBConnector>("ASIC_DB", 0, true);

        // start from a known state, since ASIC_DB is shared between tests

        m_dbAsic->flushdb();

        m_redisClient = std::make_shared<RedisClient>(m_dbAsic);
    }

    virtual void TearDown() override
    {
        SWSS_LOG_ENTER();

        m_redisClient.reset();

        m_dbAsic->flushdb();

        m_dbAsic.reset();
    }

protected:

    static sai_object_meta_key_t makeRouteMetaKey(
            uint32_t ip4Addr)
    {
        SWSS_LOG_ENTER();

        sai_object_meta_key_t metaKey;

        metaKey.objecttype = SAI_OBJECT_TYPE_ROUTE_ENTRY;
        metaKey.objectkey.key.route_entry.switch_id = 0x21000000000000;
        metaKey.objectkey.key.route_entry.vr_id = 0x300000000000a;
        metaKey.objectkey.key.route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        metaKey.objectkey.key.route_entry.destination.addr.ip4 = ip4Addr;
        metaKey.objectkey.key.route_entry.destination.mask.ip4 = 0xffffffff;

        return metaKey;
    }

    size_t countKeys(
            const std::string& pattern) const
    {
        SWSS_LOG_ENTER();

        return m_dbAsic->keys(pattern).size();
    }

    std::shared_ptr<swss::DBConnector> m_dbAsic;
    std::shared_ptr<RedisClient> m_redisClient;
};

// removeAsicStateTable() removes every ASIC_STATE key. It writes more objects
// than a single DEL chunk holds, so the chunked bulk removal is exercised.
TEST_F(RedisClientTest, removeAsicStateTableRemovesAllObjects)
{
    const size_t count = 300;

    for (size_t idx = 0; idx < count; idx++)
    {
        m_redisClient->createAsicObject(makeRouteMetaKey(static_cast<uint32_t>(idx)),
                {{"SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION", "SAI_PACKET_ACTION_FORWARD"}});
    }

    EXPECT_EQ(m_redisClient->getAsicStateKeys().size(), count);

    m_redisClient->removeAsicStateTable();

    EXPECT_EQ(m_redisClient->getAsicStateKeys().size(), 0u);
}

// Removing an already empty table must be a no-op and must not throw.
TEST_F(RedisClientTest, removeAsicStateTableOnEmptyTable)
{
    EXPECT_EQ(m_redisClient->getAsicStateKeys().size(), 0u);

    EXPECT_NO_THROW(m_redisClient->removeAsicStateTable());

    EXPECT_EQ(m_redisClient->getAsicStateKeys().size(), 0u);
}

// removeTempAsicStateTable() must only remove TEMP keys and must leave the
// current view untouched.
TEST_F(RedisClientTest, removeTempAsicStateTableLeavesAsicStateIntact)
{
    const size_t count = 300;

    for (size_t idx = 0; idx < count; idx++)
    {
        auto metaKey = makeRouteMetaKey(static_cast<uint32_t>(idx));

        m_redisClient->createAsicObject(metaKey,
                {{"SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION", "SAI_PACKET_ACTION_FORWARD"}});

        m_redisClient->createTempAsicObject(metaKey,
                {{"SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION", "SAI_PACKET_ACTION_FORWARD"}});
    }

    EXPECT_EQ(countKeys(TEMP_PREFIX ASIC_STATE_TABLE ":*"), count);

    m_redisClient->removeTempAsicStateTable();

    EXPECT_EQ(countKeys(TEMP_PREFIX ASIC_STATE_TABLE ":*"), 0u);
    EXPECT_EQ(m_redisClient->getAsicStateKeys().size(), count);

    EXPECT_NO_THROW(m_redisClient->removeTempAsicStateTable());
}

// createAsicObjects() is the bulk equivalent of createAsicObject(): it must
// produce the same keys, and an object with no attributes must still be
// written, as a NULL:NULL entry.
TEST_F(RedisClientTest, createAsicObjectsWritesSameKeysAsCreateAsicObject)
{
    auto withAttrs = makeRouteMetaKey(0x0a000001);
    auto withoutAttrs = makeRouteMetaKey(0x0a000002);

    std::unordered_map<std::string, std::vector<swss::FieldValueTuple>> multiHash;

    multiHash[sai_serialize_object_meta_key(withAttrs)] =
        {{"SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID", "oid:0x5000000000041"}};

    multiHash[sai_serialize_object_meta_key(withoutAttrs)] = {};

    m_redisClient->createAsicObjects(multiHash);

    EXPECT_EQ(m_redisClient->getAsicStateKeys().size(), 2u);

    auto attrs = m_redisClient->getAttributesFromAsicKey(
            (ASIC_STATE_TABLE ":") + sai_serialize_object_meta_key(withAttrs));

    EXPECT_EQ(attrs["SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID"], "oid:0x5000000000041");

    auto empty = m_redisClient->getAttributesFromAsicKey(
            (ASIC_STATE_TABLE ":") + sai_serialize_object_meta_key(withoutAttrs));

    EXPECT_EQ(empty["NULL"], "NULL");
}

// setVidAndRidMap() must write both directions of the map, and must replace
// any previously stored map.
TEST_F(RedisClientTest, setVidAndRidMapWritesBothDirections)
{
    const sai_object_id_t vid1 = 0x1000000000001;
    const sai_object_id_t vid2 = 0x1000000000002;
    const sai_object_id_t rid1 = 0x2000000000001;
    const sai_object_id_t rid2 = 0x2000000000002;

    std::unordered_map<sai_object_id_t, sai_object_id_t> map;

    map[vid1] = rid1;
    map[vid2] = rid2;

    m_redisClient->setVidAndRidMap(map);

    auto vid2rid = m_redisClient->getVidToRidMap();
    auto rid2vid = m_redisClient->getRidToVidMap();

    EXPECT_EQ(vid2rid.size(), 2u);
    EXPECT_EQ(rid2vid.size(), 2u);

    EXPECT_EQ(vid2rid[vid1], rid1);
    EXPECT_EQ(rid2vid[rid2], vid2);

    // an empty map must clear what was stored before

    m_redisClient->setVidAndRidMap({});

    EXPECT_EQ(m_redisClient->getVidToRidMap().size(), 0u);
    EXPECT_EQ(m_redisClient->getRidToVidMap().size(), 0u);
}
