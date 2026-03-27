#include "ZmqRedisClient.h"
#include "VidManager.h"
#include "sairediscommon.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

using namespace syncd;

ZmqRedisClient::ZmqRedisClient(
        _In_ std::shared_ptr<swss::DBConnector> dbAsic)
    : RedisClient(dbAsic),
      m_asyncDbUpdater(std::make_shared<swss::AsyncDBUpdater>(dbAsic.get(), ASIC_STATE_TABLE))
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("ZmqRedisClient created with async ASIC_DB writes");
}

void ZmqRedisClient::removeAsicObject(
        _In_ sai_object_id_t objectVid) const
{
    SWSS_LOG_ENTER();

    sai_object_type_t ot = VidManager::objectTypeQuery(objectVid);

    auto strVid = sai_serialize_object_id(objectVid);

    std::string key = sai_serialize_object_type(ot) + ":" + strVid;

    std::vector<swss::FieldValueTuple> values;
    auto kco = std::make_shared<swss::KeyOpFieldsValuesTuple>(key, DEL_COMMAND, values);
    m_asyncDbUpdater->update(kco);
}

void ZmqRedisClient::removeAsicObject(
        _In_ const sai_object_meta_key_t& metaKey)
{
    SWSS_LOG_ENTER();

    std::string key = sai_serialize_object_meta_key(metaKey);
    std::vector<swss::FieldValueTuple> values;
    auto kco = std::make_shared<swss::KeyOpFieldsValuesTuple>(key, DEL_COMMAND, values);
    m_asyncDbUpdater->update(kco);
}

void ZmqRedisClient::removeAsicObjects(
        _In_ const std::vector<std::string>& keys)
{
    SWSS_LOG_ENTER();

    for (const auto& key: keys)
    {
        std::vector<swss::FieldValueTuple> values;
        auto kco = std::make_shared<swss::KeyOpFieldsValuesTuple>(key, DEL_COMMAND, values);
        m_asyncDbUpdater->update(kco);
    }
}

void ZmqRedisClient::createAsicObject(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ const std::vector<swss::FieldValueTuple>& attrs)
{
    SWSS_LOG_ENTER();

    std::string key = sai_serialize_object_meta_key(metaKey);

    std::vector<swss::FieldValueTuple> values = attrs;

    if (values.size() == 0)
    {
        values.emplace_back("NULL", "NULL");
    }

    auto kco = std::make_shared<swss::KeyOpFieldsValuesTuple>(key, SET_COMMAND, values);
    m_asyncDbUpdater->update(kco);
}

void ZmqRedisClient::createAsicObjects(
        _In_ const std::unordered_map<std::string, std::vector<swss::FieldValueTuple>>& multiHash)
{
    SWSS_LOG_ENTER();

    for (const auto& kvp: multiHash)
    {
        std::vector<swss::FieldValueTuple> values = kvp.second;

        if (values.size() == 0)
        {
            values.emplace_back("NULL", "NULL");
        }

        auto kco = std::make_shared<swss::KeyOpFieldsValuesTuple>(kvp.first, SET_COMMAND, values);
        m_asyncDbUpdater->update(kco);
    }
}

void ZmqRedisClient::setAsicObject(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ const std::string& attr,
        _In_ const std::string& value)
{
    SWSS_LOG_ENTER();

    std::string key = sai_serialize_object_meta_key(metaKey);
    std::vector<swss::FieldValueTuple> values;
    values.emplace_back(attr, value);
    auto kco = std::make_shared<swss::KeyOpFieldsValuesTuple>(key, SET_COMMAND, values);
    m_asyncDbUpdater->update(kco);
}
