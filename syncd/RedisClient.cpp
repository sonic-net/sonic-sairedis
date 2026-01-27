#include "RedisClient.h"
#include "VidManager.h"

#include "sairediscommon.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"
#include "swss/redisapi.h"

using namespace syncd;

// vid and rid maps contains objects from all switches
#define VIDTORID                    "VIDTORID"
#define RIDTOVID                    "RIDTOVID"

// those here are per switch
#define LANES                       "LANES"
#define HIDDEN                      "HIDDEN"
#define COLDVIDS                    "COLDVIDS"

RedisClient::RedisClient(
        _In_ std::shared_ptr<swss::DBConnector> dbAsic):
    m_dbAsic(dbAsic)
{
    SWSS_LOG_ENTER();

    std::string fdbFlushLuaScript = swss::loadLuaScript("fdb_flush.lua"); // TODO script must be updated to version 2

    m_fdbFlushSha = swss::loadRedisScript(dbAsic.get(), fdbFlushLuaScript);
}

RedisClient::~RedisClient()
{
    SWSS_LOG_ENTER();

    // empty
}

bool RedisClient::isRedisEnabled() const
{
    SWSS_LOG_ENTER();

    return true;
}

std::string RedisClient::getRedisLanesKey(
        _In_ sai_object_id_t switchVid) const
{
    SWSS_LOG_ENTER();

    /*
     * Each switch will have it's own lanes: LANES:oid:0xYYYYYYYY.
     *
     * NOTE: To support multiple switches LANES needs to be made per switch.
     *
     * return std::string(LANES) + ":" + sai_serialize_object_id(m_switch_vid);
     *
     * Only switch with index 0 and global context 0 will have key "LANES" for
     * backward compatibility. We could convert that during runtime at first
     * time.
     */

    auto index = VidManager::getSwitchIndex(switchVid);

    auto context = VidManager::getGlobalContext(switchVid);

    if (index == 0 && context == 0)
    {
        return std::string(LANES);
    }

    return (LANES ":") + sai_serialize_object_id(switchVid);
}


void RedisClient::clearLaneMap(
        _In_ sai_object_id_t switchVid) const
{
    SWSS_LOG_ENTER();

    auto key = getRedisLanesKey(switchVid);

    m_dbAsic->del(key);
}

std::unordered_map<sai_uint32_t, sai_object_id_t> RedisClient::getLaneMap(
        _In_ sai_object_id_t switchVid) const
{
    SWSS_LOG_ENTER();

    auto key = getRedisLanesKey(switchVid);

    auto hash = m_dbAsic->hgetall(key);

    SWSS_LOG_DEBUG("previous lanes: %lu", hash.size());

    std::unordered_map<sai_uint32_t, sai_object_id_t> map;

    for (auto &kv: hash)
    {
        const std::string &str_key = kv.first;
        const std::string &str_value = kv.second;

        sai_uint32_t lane;
        sai_object_id_t portId;

        sai_deserialize_number(str_key, lane);

        sai_deserialize_object_id(str_value, portId);

        map[lane] = portId;
    }

    return map;
}

void RedisClient::saveLaneMap(
        _In_ sai_object_id_t switchVid,
        _In_ const std::unordered_map<sai_uint32_t, sai_object_id_t>& map) const
{
    SWSS_LOG_ENTER();

    clearLaneMap(switchVid);

    for (auto const &it: map)
    {
        sai_uint32_t lane = it.first;
        sai_object_id_t portId = it.second;

        std::string strLane = sai_serialize_number(lane);
        std::string strPortId = sai_serialize_object_id(portId);

        auto key = getRedisLanesKey(switchVid);

        m_dbAsic->hset(key, strLane, strPortId);
    }
}

std::unordered_map<sai_object_id_t, sai_object_id_t> RedisClient::getObjectMap(
        _In_ const std::string &key) const
{
    SWSS_LOG_ENTER();

    auto hash = m_dbAsic->hgetall(key);

    std::unordered_map<sai_object_id_t, sai_object_id_t> map;

    for (auto &kv: hash)
    {
        const std::string &str_key = kv.first;
        const std::string &str_value = kv.second;

        sai_object_id_t objectIdKey;
        sai_object_id_t objectIdValue;

        sai_deserialize_object_id(str_key, objectIdKey);

        sai_deserialize_object_id(str_value, objectIdValue);

        map[objectIdKey] = objectIdValue;
    }

    return map;
}

std::unordered_map<sai_object_id_t, sai_object_id_t> RedisClient::getVidToRidMap(
        _In_ sai_object_id_t switchVid) const
{
    SWSS_LOG_ENTER();

    auto map = getObjectMap(VIDTORID);

    std::unordered_map<sai_object_id_t, sai_object_id_t> filtered;

    for (auto& v2r: map)
    {
        auto switchId = VidManager::switchIdQuery(v2r.first);

        if (switchId == switchVid)
        {
            filtered[v2r.first] = v2r.second;
        }
    }

    return filtered;
}

std::unordered_map<sai_object_id_t, sai_object_id_t> RedisClient::getRidToVidMap(
        _In_ sai_object_id_t switchVid) const
{
    SWSS_LOG_ENTER();

    auto map = getObjectMap(RIDTOVID);

    std::unordered_map<sai_object_id_t, sai_object_id_t> filtered;

    for (auto& r2v: map)
    {
        auto switchId = VidManager::switchIdQuery(r2v.second);

        if (switchId == switchVid)
        {
            filtered[r2v.first] = r2v.second;
        }
    }

    return filtered;
}

std::unordered_map<sai_object_id_t, sai_object_id_t> RedisClient::getVidToRidMap() const
{
    SWSS_LOG_ENTER();

    return getObjectMap(VIDTORID);
}

std::unordered_map<sai_object_id_t, sai_object_id_t> RedisClient::getRidToVidMap() const
{
    SWSS_LOG_ENTER();

    return getObjectMap(RIDTOVID);
}

void RedisClient::setDummyAsicStateObject(
        _In_ sai_object_id_t objectVid)
{
    SWSS_LOG_ENTER();

    sai_object_type_t objectType = VidManager::objectTypeQuery(objectVid);

    std::string strObjectType = sai_serialize_object_type(objectType);

    std::string strVid = sai_serialize_object_id(objectVid);

    std::string strKey = ASIC_STATE_TABLE + (":" + strObjectType + ":" + strVid);

    m_dbAsic->hset(strKey, "NULL", "NULL");
}

void RedisClient::setDummyAsicStateObjects(
        _In_ size_t count,
        _In_ const sai_object_id_t* objectVids)
{
    SWSS_LOG_ENTER();

    swss::RedisPipeline pipe(m_dbAsic.get(), count);

    for (size_t idx = 0; idx < count; idx++)
    {
        sai_object_type_t objectType = VidManager::objectTypeQuery(objectVids[idx]);

        std::string strObjectType = sai_serialize_object_type(objectType);

        std::string strVid = sai_serialize_object_id(objectVids[idx]);

        std::string strKey = ASIC_STATE_TABLE + (":" + strObjectType + ":" + strVid);

        swss::RedisCommand hset;
        hset.format("HSET %s %s %s", strKey.c_str(), "NULL", "NULL");
        pipe.push(hset, REDIS_REPLY_INTEGER);
    }

    pipe.flush();
}

std::string RedisClient::getRedisColdVidsKey(
        _In_ sai_object_id_t switchVid) const
{
    SWSS_LOG_ENTER();

    /*
     * Each switch will have it's own cold vids: COLDVIDS:oid:0xYYYYYYYY.
     *
     * NOTE: To support multiple switches COLDVIDS needs to be made per switch.
     *
     * return std::string(COLDVIDS) + ":" + sai_serialize_object_id(m_switch_vid);
     *
     * Only switch with index 0 and global context 0 will have key "COLDVIDS" for
     * backward compatibility. We could convert that during runtime at first
     * time.
     */

    auto index = VidManager::getSwitchIndex(switchVid);

    auto context = VidManager::getGlobalContext(switchVid);

    if (index == 0 && context == 0)
    {
        return std::string(COLDVIDS);
    }

    return (COLDVIDS ":") + sai_serialize_object_id(switchVid);
}

void RedisClient::saveColdBootDiscoveredVids(
        _In_ sai_object_id_t switchVid,
        _In_ const std::set<sai_object_id_t>& coldVids)
{
    SWSS_LOG_ENTER();

    auto key = getRedisColdVidsKey(switchVid);

    for (auto vid: coldVids)
    {
        sai_object_type_t objectType = VidManager::objectTypeQuery(vid);

        std::string strObjectType = sai_serialize_object_type(objectType);

        std::string strVid = sai_serialize_object_id(vid);

        m_dbAsic->hset(key, strVid, strObjectType);
    }
}

std::string RedisClient::getRedisHiddenKey(
        _In_ sai_object_id_t switchVid) const
{
    SWSS_LOG_ENTER();

    /*
     * Each switch will have it's own hidden: HIDDEN:oid:0xYYYYYYYY.
     *
     * NOTE: To support multiple switches HIDDEN needs to be made per switch.
     *
     * return std::string(HIDDEN) + ":" + sai_serialize_object_id(m_switch_vid);
     *
     * Only switch with index 0 and global context 0 will have key "HIDDEN" for
     * backward compatibility. We could convert that during runtime at first
     * time.
     */

    auto index = VidManager::getSwitchIndex(switchVid);

    auto context = VidManager::getGlobalContext(switchVid);

    if (index == 0 && context == 0)
    {
        return std::string(HIDDEN);
    }

    return (HIDDEN ":") + sai_serialize_object_id(switchVid);
}

std::shared_ptr<std::string> RedisClient::getSwitchHiddenAttribute(
        _In_ sai_object_id_t switchVid,
        _In_ const std::string& attrIdName)
{
    SWSS_LOG_ENTER();

    auto key = getRedisHiddenKey(switchVid);

    return m_dbAsic->hget(key, attrIdName);
}

void RedisClient::saveSwitchHiddenAttribute(
        _In_ sai_object_id_t switchVid,
        _In_ const std::string& attrIdName,
        _In_ sai_object_id_t objectRid)
{
    SWSS_LOG_ENTER();

    auto key = getRedisHiddenKey(switchVid);

    std::string strRid = sai_serialize_object_id(objectRid);

    m_dbAsic->hset(key, attrIdName, strRid);
}

std::set<sai_object_id_t> RedisClient::getColdVids(
        _In_ sai_object_id_t switchVid)
{
    SWSS_LOG_ENTER();

    auto key = getRedisColdVidsKey(switchVid);

    auto hash = m_dbAsic->hgetall(key);

    /*
     * NOTE: some objects may not exists after 2nd restart, like VLAN_MEMBER or
     * BRIDGE_PORT, since user could decide to remove them on previous boot.
     */

    std::set<sai_object_id_t> coldVids;

    for (auto kvp: hash)
    {
        auto strVid = kvp.first;

        sai_object_id_t vid;
        sai_deserialize_object_id(strVid, vid);

        /*
         * Just make sure that vid in COLDVIDS is present in current vid2rid map
         */

        auto rid = m_dbAsic->hget(VIDTORID, strVid);

        if (rid == nullptr)
        {
            SWSS_LOG_INFO("no RID for VID %s, probably object was removed previously", strVid.c_str());
        }

        coldVids.insert(vid);
    }

    return coldVids;
}

void RedisClient::setPortLanes(
        _In_ sai_object_id_t switchVid,
        _In_ sai_object_id_t portRid,
        _In_ const std::vector<uint32_t>& lanes)
{
    SWSS_LOG_ENTER();

    auto key = getRedisLanesKey(switchVid);

    for (uint32_t lane: lanes)
    {
        std::string strLane = sai_serialize_number(lane);
        std::string strPortRid = sai_serialize_object_id(portRid);

        m_dbAsic->hset(key, strLane, strPortRid);
    }
}

size_t RedisClient::getAsicObjectsSize(
        _In_ sai_object_id_t switchVid) const
{
    SWSS_LOG_ENTER();

    // NOTE: this goes over all objects, and if we have N switches then it will
    // go N times on every switch and it can be slow, we need to find better
    // way to do this

    auto keys = m_dbAsic->keys(ASIC_STATE_TABLE ":*");

    size_t count = 0;

    for (auto& key: keys)
    {
        auto mk = key.substr(key.find_first_of(":") + 1);

        sai_object_meta_key_t metaKey;

        sai_deserialize_object_meta_key(mk, metaKey);

        // we need to check only objects that's belong to requested switch

        auto swid = VidManager::switchIdQuery(metaKey.objectkey.key.object_id);

        if (swid == switchVid)
        {
            count++;
        }
    }

    return count;
}

int RedisClient::removePortFromLanesMap(
        _In_ sai_object_id_t switchVid,
        _In_ sai_object_id_t portRid) const
{
    SWSS_LOG_ENTER();

    // key - lane number, value - port RID
    auto map = getLaneMap(switchVid);

    int removed = 0;

    auto key = getRedisLanesKey(switchVid);

    for (auto& kv: map)
    {
        if (kv.second == portRid)
        {
            std::string strLane = sai_serialize_number(kv.first);

            m_dbAsic->hdel(key, strLane);

            removed++;
        }
    }

    return removed;
}

void RedisClient::removeAsicObject(
        _In_ sai_object_id_t objectVid) const
{
    SWSS_LOG_ENTER();

    sai_object_type_t ot = VidManager::objectTypeQuery(objectVid);

    auto strVid = sai_serialize_object_id(objectVid);

    std::string key = (ASIC_STATE_TABLE ":") + sai_serialize_object_type(ot) + ":" + strVid;

    SWSS_LOG_INFO("removing ASIC DB key: %s", key.c_str());

    m_dbAsic->del(key);
}

void RedisClient::removeAsicObject(
        _In_ const sai_object_meta_key_t& metaKey)
{
    SWSS_LOG_ENTER();

    std::string key = (ASIC_STATE_TABLE ":") + sai_serialize_object_meta_key(metaKey);

    m_dbAsic->del(key);
}

void RedisClient::removeTempAsicObject(
        _In_ const sai_object_meta_key_t& metaKey)
{
    SWSS_LOG_ENTER();

    std::string key = (TEMP_PREFIX ASIC_STATE_TABLE ":") + sai_serialize_object_meta_key(metaKey);

    m_dbAsic->del(key);
}

void RedisClient::removeAsicObjects(
        _In_ const std::vector<std::string>& keys)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> prefixKeys;

    // we need to rewrite keys to add table prefix
    for (const auto& key: keys)
    {
         prefixKeys.push_back((ASIC_STATE_TABLE ":") + key);
    }

    m_dbAsic->del(prefixKeys);
}

void RedisClient::removeTempAsicObjects(
        _In_ const std::vector<std::string>& keys)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> prefixKeys;

    // we need to rewrite keys to add table prefix
    for (const auto& key: keys)
    {
         prefixKeys.push_back((TEMP_PREFIX ASIC_STATE_TABLE ":") + key);
    }

    m_dbAsic->del(prefixKeys);
}

void RedisClient::setAsicObject(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ const std::string& attr,
        _In_ const std::string& value)
{
    SWSS_LOG_ENTER();

    std::string key = (ASIC_STATE_TABLE ":") + sai_serialize_object_meta_key(metaKey);

    m_dbAsic->hset(key, attr, value);
}

void RedisClient::setTempAsicObject(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ const std::string& attr,
        _In_ const std::string& value)
{
    SWSS_LOG_ENTER();

    std::string key = (TEMP_PREFIX ASIC_STATE_TABLE ":") + sai_serialize_object_meta_key(metaKey);

    m_dbAsic->hset(key, attr, value);
}

void RedisClient::createAsicObject(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ const std::vector<swss::FieldValueTuple>& attrs)
{
    SWSS_LOG_ENTER();

    std::string key = (ASIC_STATE_TABLE ":") + sai_serialize_object_meta_key(metaKey);

    if (attrs.size() == 0)
    {
        m_dbAsic->hset(key, "NULL", "NULL");
        return;
    }

    for (const auto& e: attrs)
    {
        m_dbAsic->hset(key, fvField(e), fvValue(e));
    }
}

void RedisClient::createTempAsicObject(
        _In_ const sai_object_meta_key_t& metaKey,
        _In_ const std::vector<swss::FieldValueTuple>& attrs)
{
    SWSS_LOG_ENTER();

    std::string key = (TEMP_PREFIX ASIC_STATE_TABLE ":") + sai_serialize_object_meta_key(metaKey);

    if (attrs.size() == 0)
    {
        m_dbAsic->hset(key, "NULL", "NULL");
        return;
    }

    for (const auto& e: attrs)
    {
        m_dbAsic->hset(key, fvField(e), fvValue(e));
    }
}

void RedisClient::createAsicObjects(
        _In_ const std::unordered_map<std::string, std::vector<swss::FieldValueTuple>>& multiHash)
{
    SWSS_LOG_ENTER();

    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> hash;

    // we need to rewrite hash to add table prefix
    for (const auto& kvp: multiHash)
    {
        hash[(ASIC_STATE_TABLE ":") + kvp.first] = kvp.second;

        if (kvp.second.size() == 0)
        {
            hash[(ASIC_STATE_TABLE ":") + kvp.first].emplace_back(std::make_pair<std::string, std::string>("NULL", "NULL"));
        }
    }

    m_dbAsic->hmset(hash);
}

void RedisClient::createTempAsicObjects(
        _In_ const std::unordered_map<std::string, std::vector<swss::FieldValueTuple>>& multiHash)
{
    SWSS_LOG_ENTER();

    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> hash;

    // we need to rewrite hash to add table prefix
    for (const auto& kvp: multiHash)
    {
        hash[(TEMP_PREFIX ASIC_STATE_TABLE ":") + kvp.first] = kvp.second;

        if (kvp.second.size() == 0)
        {
            hash[(TEMP_PREFIX ASIC_STATE_TABLE ":") + kvp.first].emplace_back(std::make_pair<std::string, std::string>("NULL", "NULL"));
        }
    }

    m_dbAsic->hmset(hash);
}

void RedisClient::setVidAndRidMap(
        _In_ const std::unordered_map<sai_object_id_t, sai_object_id_t>& map)
{
    SWSS_LOG_ENTER();

    m_dbAsic->del(VIDTORID);
    m_dbAsic->del(RIDTOVID);

    for (auto &kv: map)
    {
        std::string strVid = sai_serialize_object_id(kv.first);
        std::string strRid = sai_serialize_object_id(kv.second);

        m_dbAsic->hset(VIDTORID, strVid, strRid);
        m_dbAsic->hset(RIDTOVID, strRid, strVid);
    }
}

std::vector<std::string> RedisClient::getAsicStateKeys() const
{
    SWSS_LOG_ENTER();

    return m_dbAsic->keys(ASIC_STATE_TABLE ":*");
}

std::vector<std::string> RedisClient::getAsicStateSwitchesKeys() const
{
    SWSS_LOG_ENTER();

    return m_dbAsic->keys(ASIC_STATE_TABLE ":SAI_OBJECT_TYPE_SWITCH:*");
}

void RedisClient::removeColdVid(
        _In_ sai_object_id_t vid)
{
    SWSS_LOG_ENTER();

    auto strVid = sai_serialize_object_id(vid);

    auto key = getRedisColdVidsKey(vid);

    m_dbAsic->hdel(key, strVid);
}

std::unordered_map<std::string, std::string> RedisClient::getAttributesFromAsicKey(
        _In_ const std::string& key) const
{
    SWSS_LOG_ENTER();

    std::unordered_map<std::string, std::string> map;
    m_dbAsic->hgetall(key, std::inserter(map, map.end()));
    return map;
}

bool RedisClient::hasNoHiddenKeysDefined() const
{
    SWSS_LOG_ENTER();

    auto keys = m_dbAsic->keys(HIDDEN "*");

    return keys.size() == 0;
}

void RedisClient::removeVidAndRid(
        _In_ sai_object_id_t vid,
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    auto strVid = sai_serialize_object_id(vid);
    auto strRid = sai_serialize_object_id(rid);

    m_dbAsic->hdel(VIDTORID, strVid);
    m_dbAsic->hdel(RIDTOVID, strRid);
}

void RedisClient::insertVidAndRid(
        _In_ sai_object_id_t vid,
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    auto strVid = sai_serialize_object_id(vid);
    auto strRid = sai_serialize_object_id(rid);

    m_dbAsic->hset(VIDTORID, strVid, strRid);
    m_dbAsic->hset(RIDTOVID, strRid, strVid);
}

void RedisClient::insertVidsAndRids(
        _In_ size_t count,
        _In_ const sai_object_id_t* vids,
        _In_ const sai_object_id_t* rids)
{
    SWSS_LOG_ENTER();

    swss::RedisPipeline pipe(m_dbAsic.get(), count * 2);

    for (size_t idx = 0; idx < count; idx++)
    {
        auto strVid = sai_serialize_object_id(vids[idx]);
        auto strRid = sai_serialize_object_id(rids[idx]);

        {
            swss::RedisCommand hset;
            hset.format("HSET %s %s %s", VIDTORID, strVid.c_str(), strRid.c_str());
            pipe.push(hset, REDIS_REPLY_INTEGER);
        }

        {
            swss::RedisCommand hset;
            hset.format("HSET %s %s %s", RIDTOVID, strRid.c_str(), strVid.c_str());
            pipe.push(hset, REDIS_REPLY_INTEGER);
        }
    }

    pipe.flush();
}

sai_object_id_t RedisClient::getVidForRid(
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    auto strRid = sai_serialize_object_id(rid);

    auto pvid = m_dbAsic->hget(RIDTOVID, strRid);

    if (pvid == nullptr)
    {
        // rid2vid map should never contain null, so we can return NULL which
        // will mean that mapping don't exists

        return SAI_NULL_OBJECT_ID;
    }

    sai_object_id_t vid;

    sai_deserialize_object_id(*pvid, vid);

    return vid;
}

void RedisClient::getVidsForRids(
        _In_ size_t count,
        _In_ const sai_object_id_t* rids,
        _Out_ sai_object_id_t* vids)
{
    SWSS_LOG_ENTER();

    swss::RedisCommand hmget;

    std::vector<std::string> cmds;
    cmds.reserve(count + 2);

    cmds.push_back("HMGET");
    cmds.push_back(RIDTOVID);

    for (size_t idx = 0; idx < count; idx++)
    {
        cmds.push_back(sai_serialize_object_id(rids[idx]));
    }

    hmget.format(cmds);

    swss::RedisReply r(m_dbAsic.get(), hmget);

    auto ctx = r.getContext();

    if (ctx->type != REDIS_REPLY_ARRAY)
    {
        SWSS_LOG_THROW("expected array reply, got %d", ctx->type);
    }

    if (ctx->elements != count)
    {
        SWSS_LOG_THROW("expected %zu elements, got %zu", count, ctx->elements);
    }

    for (size_t idx = 0; idx < ctx->elements; idx++)
    {
        auto element = ctx->element[idx];

        if (element->type == REDIS_REPLY_STRING)
        {
            sai_deserialize_object_id(element->str, vids[idx]);
        }
        else if (element->type == REDIS_REPLY_NIL)
        {
            vids[idx] = SAI_NULL_OBJECT_ID;
        }
        else
        {
            SWSS_LOG_THROW("expected string or nil reply, got %d", element->type);
        }
    }
}

sai_object_id_t RedisClient::getRidForVid(
        _In_ sai_object_id_t vid)
{
    SWSS_LOG_ENTER();

    auto strVid = sai_serialize_object_id(vid);

    auto prid = m_dbAsic->hget(VIDTORID, strVid);

    if (prid == nullptr)
    {
        // rid2vid map should never contain null, so we can return NULL which
        // will mean that mapping don't exists

        return SAI_NULL_OBJECT_ID;
    }

    sai_object_id_t rid;

    sai_deserialize_object_id(*prid, rid);

    return rid;
}

void RedisClient::removeAsicStateTable()
{
    SWSS_LOG_ENTER();

    const auto &asicStateKeys = m_dbAsic->keys(ASIC_STATE_TABLE ":*");

    for (const auto &key: asicStateKeys)
    {
        m_dbAsic->del(key);
    }
}

void RedisClient::removeTempAsicStateTable()
{
    SWSS_LOG_ENTER();

    const auto &tempAsicStateKeys = m_dbAsic->keys(TEMP_PREFIX ASIC_STATE_TABLE ":*");

    for (const auto &key: tempAsicStateKeys)
    {
        m_dbAsic->del(key);
    }
}

std::map<sai_object_id_t, swss::TableDump> RedisClient::getAsicView()
{
    SWSS_LOG_ENTER();

    return getAsicView(ASIC_STATE_TABLE);
}

std::map<sai_object_id_t, swss::TableDump> RedisClient::getTempAsicView()
{
    SWSS_LOG_ENTER();

    return getAsicView(TEMP_PREFIX ASIC_STATE_TABLE);
}

std::map<sai_object_id_t, swss::TableDump> RedisClient::getAsicView(
        _In_ const std::string &tableName)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("get asic view from %s", tableName.c_str());

    swss::Table table(m_dbAsic.get(), tableName);

    swss::TableDump dump;

    table.dump(dump);

    std::map<sai_object_id_t, swss::TableDump> map;

    for (auto& key: dump)
    {
        sai_object_meta_key_t mk;
        sai_deserialize_object_meta_key(key.first, mk);

        auto switchVID = VidManager::switchIdQuery(mk.objectkey.key.object_id);

        map[switchVID][key.first] = key.second;
    }

    SWSS_LOG_NOTICE("%s switch count: %zu:", tableName.c_str(), map.size());

    for (auto& kvp: map)
    {
        SWSS_LOG_NOTICE("%s: objects count: %zu",
                sai_serialize_object_id(kvp.first).c_str(),
                kvp.second.size());
    }

    return map;
}


void RedisClient::processFlushEvent(
        _In_ sai_object_id_t switchVid,
        _In_ sai_object_id_t portVid,
        _In_ sai_object_id_t bvId,
        _In_ sai_fdb_flush_entry_type_t type)
{
    SWSS_LOG_ENTER();

    // TODO this must be per switch if we will have multiple switches, needs to be filtered by switch ID also

    /*
       [{ "fdb_entry":"{ \"bridge_id\":\"oid:0x23000000000000\", \"mac\":\"00:00:00:00:00:00\", \"switch_id\":\"oid:0x21000000000000\"}", "fdb_event":"SAI_FDB_EVENT_FLUSHED", "list":[
       {"id":"SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID","value":"oid:0x3a0000000009cf"},
       {"id":"SAI_FDB_ENTRY_ATTR_TYPE","value":"SAI_FDB_ENTRY_TYPE_DYNAMIC"},
       {"id":"SAI_FDB_ENTRY_ATTR_PACKET_ACTION","value":"SAI_PACKET_ACTION_FORWARD"} ] }]
    */

    SWSS_LOG_NOTICE("received a flush port fdb event, portVid = %s, bvId = %s",
            sai_serialize_object_id(portVid).c_str(),
            sai_serialize_object_id(bvId).c_str());

    // TODO script must be updated to version 2

    std::string pattern = (bvId == SAI_NULL_OBJECT_ID)
        ? (ASIC_STATE_TABLE ":SAI_OBJECT_TYPE_FDB_ENTRY:*")
        : (ASIC_STATE_TABLE ":SAI_OBJECT_TYPE_FDB_ENTRY:*" + sai_serialize_object_id(bvId) + "*");

    std::string portStr = (portVid == SAI_NULL_OBJECT_ID) ? "" : sai_serialize_object_id(portVid);

    SWSS_LOG_NOTICE("pattern %s, portStr %s", pattern.c_str(), portStr.c_str());

    std::vector<int> vals; // 0 - flush dynamic, 1 - flush static

    switch (type)
    {
        case SAI_FDB_FLUSH_ENTRY_TYPE_DYNAMIC:
            vals.push_back(0);
            break;

        case SAI_FDB_FLUSH_ENTRY_TYPE_STATIC:
            vals.push_back(1);
            break;

        case SAI_FDB_FLUSH_ENTRY_TYPE_ALL:
            vals.push_back(0);
            vals.push_back(1);
            break;

        default:
            SWSS_LOG_THROW("unknown fdb flush entry type: %d", type);
    }

    for (int flush_static: vals)
    {
        swss::RedisCommand command;

        command.format(
                "EVALSHA %s 3 %s %s %s",
                m_fdbFlushSha.c_str(),
                pattern.c_str(),
                portStr.c_str(),
                std::to_string(flush_static).c_str());

        swss::RedisReply r(m_dbAsic.get(), command);
    }
}