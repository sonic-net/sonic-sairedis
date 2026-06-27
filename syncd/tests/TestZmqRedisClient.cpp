#include <gtest/gtest.h>
#include <memory>
#include <map>

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

    // Like keyExistsAfterFlush, but returns the object's fields so tests can
    // assert which attributes survived a merge write. Resetting the client
    // joins the AsyncDBUpdater thread, so all pending writes are committed
    // before we read.
    std::map<std::string, std::string> fieldsAfterFlush(const std::string& key)
    {
        SWSS_LOG_ENTER();
        m_redisClient.reset();
        swss::Table table(m_dbAsic.get(), "ASIC_STATE");
        std::vector<swss::FieldValueTuple> values;
        table.get(key, values);
        return std::map<std::string, std::string>(values.begin(), values.end());
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

TEST_F(ZmqRedisClientTest, removeAsicObjectByObjectVidDeletesFromRedis)
{
    // VIRTUAL_ROUTER VID: object type 0x03 in bits 55..48, index 0x8.
    // VidManager::objectTypeQuery decodes it back to SAI_OBJECT_TYPE_VIRTUAL_ROUTER.
    sai_object_id_t objectVid = 0x0003000000000008;

    sai_object_meta_key_t metaKey;
    metaKey.objecttype = SAI_OBJECT_TYPE_VIRTUAL_ROUTER;
    metaKey.objectkey.key.object_id = objectVid;

    // The removeAsicObject(objectVid) overload builds the key as
    // sai_serialize_object_type(objectTypeQuery(vid)) + ":" + sai_serialize_object_id(vid).
    // For an object-id meta key this matches sai_serialize_object_meta_key, so
    // creating via the meta key and removing via the VID target the same Redis key.
    std::string key = sai_serialize_object_meta_key(metaKey);

    std::vector<swss::FieldValueTuple> attrs;
    m_redisClient->createAsicObject(metaKey, attrs);
    EXPECT_NO_THROW(m_redisClient->removeAsicObject(objectVid));

    EXPECT_FALSE(keyExistsAfterFlush(key));
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

// Regression test for the HSET (vs SET) write op: setAsicObject updates a
// single field and must not erase the object's other attributes. With the
// old SET_COMMAND (del + set) the PACKET_ACTION field below would be wiped.
TEST_F(ZmqRedisClientTest, setAsicObjectPreservesOtherFields)
{
    auto metaKey = makeRouteMetaKey(0x3000000000009, 0x0a000009);

    // Create the object with two attributes.
    std::vector<swss::FieldValueTuple> attrs;
    attrs.emplace_back("SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID", "oid:0x5000000000040");
    attrs.emplace_back("SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION", "SAI_PACKET_ACTION_FORWARD");
    m_redisClient->createAsicObject(metaKey, attrs);

    // Update only NEXT_HOP_ID; PACKET_ACTION must survive the merge.
    m_redisClient->setAsicObject(metaKey, "SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID", "oid:0x5000000000041");

    auto fields = fieldsAfterFlush(sai_serialize_object_meta_key(metaKey));
    EXPECT_EQ(fields["SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID"], "oid:0x5000000000041");
    EXPECT_EQ(fields["SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION"], "SAI_PACKET_ACTION_FORWARD");
}

// createAsicObject also uses the HSET merge op: a second create on the same
// key adds a field without erasing the field written by the first create.
TEST_F(ZmqRedisClientTest, createAsicObjectMergesFields)
{
    auto metaKey = makeRouteMetaKey(0x300000000000a, 0x0a00000a);

    // First create writes NEXT_HOP_ID.
    m_redisClient->createAsicObject(metaKey,
            {{"SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID", "oid:0x5000000000050"}});

    // Second create on the same key writes PACKET_ACTION; the merge keeps both.
    m_redisClient->createAsicObject(metaKey,
            {{"SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION", "SAI_PACKET_ACTION_FORWARD"}});

    auto fields = fieldsAfterFlush(sai_serialize_object_meta_key(metaKey));
    EXPECT_EQ(fields["SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID"], "oid:0x5000000000050");
    EXPECT_EQ(fields["SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION"], "SAI_PACKET_ACTION_FORWARD");
}
