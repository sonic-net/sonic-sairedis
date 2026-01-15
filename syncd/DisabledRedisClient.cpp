#include "DisabledRedisClient.h"

#include "swss/logger.h"

using namespace syncd;

bool DisabledRedisClient::isRedisEnabled() const
{
    SWSS_LOG_ENTER();

    return false;
}

void DisabledRedisClient::clearLaneMap(
        _In_ sai_object_id_t switchVid) const
{
    SWSS_LOG_ENTER();
}

std::unordered_map<sai_uint32_t, sai_object_id_t> DisabledRedisClient::getLaneMap(
        _In_ sai_object_id_t switchVid) const
{
    SWSS_LOG_ENTER();

    return {};
}

void DisabledRedisClient::saveLaneMap(
        _In_ sai_object_id_t switchVid,
        _In_ const std::unordered_map<sai_uint32_t, sai_object_id_t>& map) const
{
    SWSS_LOG_ENTER();
}

std::unordered_map<sai_object_id_t, sai_object_id_t> DisabledRedisClient::getVidToRidMap(
        _In_ sai_object_id_t switchVid) const
{
    SWSS_LOG_ENTER();

    return {};
}

std::unordered_map<sai_object_id_t, sai_object_id_t> DisabledRedisClient::getRidToVidMap(
        _In_ sai_object_id_t switchVid) const
{
    SWSS_LOG_ENTER();

    return {};
}

std::unordered_map<sai_object_id_t, sai_object_id_t> DisabledRedisClient::getVidToRidMap() const
{
    SWSS_LOG_ENTER();

    return {};
}

std::unordered_map<sai_object_id_t, sai_object_id_t> DisabledRedisClient::getRidToVidMap() const
{
    SWSS_LOG_ENTER();

    return {};
}

void DisabledRedisClient::setDummyAsicStateObject(
        _In_ sai_object_id_t objectVid)
{
    SWSS_LOG_ENTER();
}

void DisabledRedisClient::setDummyAsicStateObjects(
        _In_ size_t count,
        _In_ const sai_object_id_t* objectVids)
{
    SWSS_LOG_ENTER();
}

void DisabledRedisClient::saveColdBootDiscoveredVids(
        _In_ sai_object_id_t switchVid,
        _In_ const std::set<sai_object_id_t>& coldVids)
{
    SWSS_LOG_ENTER();
}

std::shared_ptr<std::string> DisabledRedisClient::getSwitchHiddenAttribute(
        _In_ sai_object_id_t switchVid,
        _In_ const std::string& attrIdName)
{
    SWSS_LOG_ENTER();

    return nullptr;
}

void DisabledRedisClient::saveSwitchHiddenAttribute(
        _In_ sai_object_id_t switchVid,
        _In_ const std::string& attrIdName,
        _In_ sai_object_id_t objectRid)
{
    SWSS_LOG_ENTER();
}

std::set<sai_object_id_t> DisabledRedisClient::getColdVids(
        _In_ sai_object_id_t switchVid)
{
    SWSS_LOG_ENTER();

    return {};
}

void DisabledRedisClient::setPortLanes(
        _In_ sai_object_id_t switchVid,
        _In_ sai_object_id_t portRid,
        _In_ const std::vector<uint32_t>& lanes)
{
    SWSS_LOG_ENTER();
}

size_t DisabledRedisClient::getAsicObjectsSize(
        _In_ sai_object_id_t switchVid) const
{
    SWSS_LOG_ENTER();

    return 0;
}

int DisabledRedisClient::removePortFromLanesMap(
        _In_ sai_object_id_t switchVid,
        _In_ sai_object_id_t portRid) const
{
    SWSS_LOG_ENTER();

    return 0;
}

void DisabledRedisClient::removeAsicObject(
        _In_ sai_object_id_t objectVid) const
{
    SWSS_LOG_ENTER();
}

void DisabledRedisClient::removeAsicObject(
        _In_ const sai_object_meta_key_t& metaKey)
{
    SWSS_LOG_ENTER();
}

void DisabledRedisClient::removeTempAsicObject(
        _In_ const sai_object_meta_key_t& metaKey)
{
    SWSS_LOG_ENTER();
}

void DisabledRedisClient::removeAsicObjects(
        _In_ const std::vector<std::string>& keys)
{
    SWSS_LOG_ENTER();
}

void DisabledRedisClient::removeTempAsicObjects(
        _In_ const std::vector<std::string>& keys)
{
    SWSS_LOG_ENTER();
}

void DisabledRedisClient::createAsicObject(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ const std::vector<swss::FieldValueTuple>& attrs)
{
    SWSS_LOG_ENTER();
}

void DisabledRedisClient::createTempAsicObject(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ const std::vector<swss::FieldValueTuple>& attrs)
{
    SWSS_LOG_ENTER();
}

void DisabledRedisClient::createAsicObjects(
        _In_ const std::unordered_map<std::string, std::vector<swss::FieldValueTuple>>& multiHash)
{
    SWSS_LOG_ENTER();
}

void DisabledRedisClient::createTempAsicObjects(
        _In_ const std::unordered_map<std::string, std::vector<swss::FieldValueTuple>>& multiHash)
{
    SWSS_LOG_ENTER();
}

void DisabledRedisClient::setVidAndRidMap(
        _In_ const std::unordered_map<sai_object_id_t, sai_object_id_t>& map)
{
    SWSS_LOG_ENTER();
}

std::vector<std::string> DisabledRedisClient::getAsicStateKeys() const
{
    SWSS_LOG_ENTER();

    return {};
}

std::vector<std::string> DisabledRedisClient::getAsicStateSwitchesKeys() const
{
    SWSS_LOG_ENTER();

    return {};
}

void DisabledRedisClient::removeColdVid(
        _In_ sai_object_id_t vid)
{
    SWSS_LOG_ENTER();
}

std::unordered_map<std::string, std::string> DisabledRedisClient::getAttributesFromAsicKey(
        _In_ const std::string& key) const
{
    SWSS_LOG_ENTER();

    return {};
}

bool DisabledRedisClient::hasNoHiddenKeysDefined() const
{
    SWSS_LOG_ENTER();

    return true;
}

void DisabledRedisClient::removeVidAndRid(
        _In_ sai_object_id_t vid,
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();
}

void DisabledRedisClient::insertVidAndRid(
        _In_ sai_object_id_t vid,
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();
}

void DisabledRedisClient::insertVidsAndRids(
        _In_ size_t count,
        _In_ const sai_object_id_t* vids,
        _In_ const sai_object_id_t* rids)
{
    SWSS_LOG_ENTER();
}

sai_object_id_t DisabledRedisClient::getVidForRid(
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    return SAI_NULL_OBJECT_ID;
}

sai_object_id_t DisabledRedisClient::getRidForVid(
        _In_ sai_object_id_t vid)
{
    SWSS_LOG_ENTER();

    return SAI_NULL_OBJECT_ID;
}

void DisabledRedisClient::getVidsForRids(
        _In_ size_t count,
        _In_ const sai_object_id_t* rids,
        _Out_ sai_object_id_t* vids)
{
    SWSS_LOG_ENTER();

    for (size_t i = 0; i < count; i++)
    {
        vids[i] = SAI_NULL_OBJECT_ID;
    }
}

void DisabledRedisClient::removeAsicStateTable()
{
    SWSS_LOG_ENTER();
}

void DisabledRedisClient::removeTempAsicStateTable()
{
    SWSS_LOG_ENTER();
}

std::map<sai_object_id_t, swss::TableDump> DisabledRedisClient::getAsicView()
{
    SWSS_LOG_ENTER();

    return {};
}

std::map<sai_object_id_t, swss::TableDump> DisabledRedisClient::getTempAsicView()
{
    SWSS_LOG_ENTER();

    return {};
}

void DisabledRedisClient::setAsicObject(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ const std::string& attr,
        _In_ const std::string& value)
{
    SWSS_LOG_ENTER();
}

void DisabledRedisClient::setTempAsicObject(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ const std::string& attr,
        _In_ const std::string& value)
{
    SWSS_LOG_ENTER();
}

void DisabledRedisClient::processFlushEvent(
        _In_ sai_object_id_t switchVid,
        _In_ sai_object_id_t portVid,
        _In_ sai_object_id_t bvId,
        _In_ sai_fdb_flush_entry_type_t type)
{
    SWSS_LOG_ENTER();
}

