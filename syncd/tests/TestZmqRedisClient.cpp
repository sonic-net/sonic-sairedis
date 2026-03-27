#include <gtest/gtest.h>
#include <memory>

#include <swss/logger.h>

#include <swss/dbconnector.h>
#include <swss/table.h>

#include "ZmqRedisClient.h"
#include "meta/sai_serialize.h"

using namespace syncd;

class ZmqRedisClientTest : public ::testing::Test
{
public:
    ZmqRedisClientTest() = default;
    virtual ~ZmqRedisClientTest() = default;

public:
    virtual void SetUp() override
    {
        m_dbAsic = std::make_shared<swss::DBConnector>("ASIC_DB", 0, true);
        m_redisClient = std::make_shared<ZmqRedisClient>(m_dbAsic);
    }

    virtual void TearDown() override
    {
        m_redisClient.reset();
        m_dbAsic.reset();
    }

protected:
    // Destroy the client to flush the AsyncDBUpdater queue, then verify
    // a key exists in ASIC_STATE_TABLE. Destroying the client joins the
    // AsyncDBUpdater thread, guaranteeing all pending writes have been
    // committed to Redis before we read
    bool keyExistsAfterFlush(const std::string& key)
    {
        SWSS_LOG_ENTER();
        m_redisClient.reset();
        swss::Table table(m_dbAsic.get(), "ASIC_STATE");
        std::vector<swss::FieldValueTuple> values;
        return table.get(key, values);
    }

    static sai_object_meta_key_t makeRouteMetaKey(sai_object_id_t vrId, uint32_t ip4Addr)
    {
        SWSS_LOG_ENTER();
        sai_object_meta_key_t metaKey;
        metaKey.objecttype = SAI_OBJECT_TYPE_ROUTE_ENTRY;
        metaKey.objectkey.key.route_entry.switch_id = 0x21000000000000;
        metaKey.objectkey.key.route_entry.vr_id = vrId;
        metaKey.objectkey.key.route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        metaKey.objectkey.key.route_entry.destination.addr.ip4 = ip4Addr;
        metaKey.objectkey.key.route_entry.destination.mask.ip4 = 0xffffffff;
        return metaKey;
    }

    std::shared_ptr<swss::DBConnector> m_dbAsic;
    std::shared_ptr<ZmqRedisClient> m_redisClient;
};

TEST_F(ZmqRedisClientTest, isRedisEnabledReturnsTrue)
{
    EXPECT_TRUE(m_redisClient->isRedisEnabled());
}

TEST_F(ZmqRedisClientTest, createAsicObjectWritesToRedis)
{
    auto metaKey = makeRouteMetaKey(0x3000000000002, 0x0a000001);

    std::vector<swss::FieldValueTuple> attrs;
    attrs.emplace_back("SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID", "oid:0x5000000000010");

    EXPECT_NO_THROW(m_redisClient->createAsicObject(metaKey, attrs));

    EXPECT_TRUE(keyExistsAfterFlush(sai_serialize_object_meta_key(metaKey)));
}

TEST_F(ZmqRedisClientTest, createAsicObjectWithNoAttrsWritesNullEntry)
{
    auto metaKey = makeRouteMetaKey(0x3000000000003, 0x0a000002);

    std::vector<swss::FieldValueTuple> attrs;
    EXPECT_NO_THROW(m_redisClient->createAsicObject(metaKey, attrs));

    EXPECT_TRUE(keyExistsAfterFlush(sai_serialize_object_meta_key(metaKey)));
}

TEST_F(ZmqRedisClientTest, setAsicObjectWritesToRedis)
{
    auto metaKey = makeRouteMetaKey(0x3000000000004, 0x0a000003);

    std::vector<swss::FieldValueTuple> attrs;
    m_redisClient->createAsicObject(metaKey, attrs);
    EXPECT_NO_THROW(m_redisClient->setAsicObject(metaKey, "SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID", "oid:0x5000000000020"));

    EXPECT_TRUE(keyExistsAfterFlush(sai_serialize_object_meta_key(metaKey)));
}

TEST_F(ZmqRedisClientTest, removeAsicObjectByMetaKeyDeletesFromRedis)
{
    auto metaKey = makeRouteMetaKey(0x3000000000005, 0x0a000004);

    std::vector<swss::FieldValueTuple> attrs;
    m_redisClient->createAsicObject(metaKey, attrs);
    EXPECT_NO_THROW(m_redisClient->removeAsicObject(metaKey));

    EXPECT_FALSE(keyExistsAfterFlush(sai_serialize_object_meta_key(metaKey)));
}

TEST_F(ZmqRedisClientTest, removeAsicObjectsDeletesAllKeysFromRedis)
{
    auto metaKey1 = makeRouteMetaKey(0x3000000000006, 0x0a000005);
    auto metaKey2 = makeRouteMetaKey(0x3000000000006, 0x0a000006);

    std::string key1 = sai_serialize_object_meta_key(metaKey1);
    std::string key2 = sai_serialize_object_meta_key(metaKey2);

    std::vector<swss::FieldValueTuple> attrs;
    m_redisClient->createAsicObject(metaKey1, attrs);
    m_redisClient->createAsicObject(metaKey2, attrs);
    EXPECT_NO_THROW(m_redisClient->removeAsicObjects({key1, key2}));

    // flush and check key1; client is reset inside keyExistsAfterFlush
    EXPECT_FALSE(keyExistsAfterFlush(key1));

    // client already reset, read key2 directly
    swss::Table table(m_dbAsic.get(), "ASIC_STATE");
    std::vector<swss::FieldValueTuple> values;
    EXPECT_FALSE(table.get(key2, values));
}

TEST_F(ZmqRedisClientTest, createAsicObjectsWritesAllEntriesToRedis)
{
    auto metaKey1 = makeRouteMetaKey(0x3000000000007, 0x0a000007);
    auto metaKey2 = makeRouteMetaKey(0x3000000000007, 0x0a000008);

    std::string key1 = sai_serialize_object_meta_key(metaKey1);
    std::string key2 = sai_serialize_object_meta_key(metaKey2);

    std::unordered_map<std::string, std::vector<swss::FieldValueTuple>> multiHash;
    multiHash[key1] = {{"SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID", "oid:0x5000000000030"}};
    multiHash[key2] = {{"SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID", "oid:0x5000000000031"}};

    EXPECT_NO_THROW(m_redisClient->createAsicObjects(multiHash));

    // flush and check key1; client is reset inside keyExistsAfterFlush
    EXPECT_TRUE(keyExistsAfterFlush(key1));

    // client already reset, read key2 directly
    swss::Table table(m_dbAsic.get(), "ASIC_STATE");
    std::vector<swss::FieldValueTuple> values;
    EXPECT_TRUE(table.get(key2, values));
}
