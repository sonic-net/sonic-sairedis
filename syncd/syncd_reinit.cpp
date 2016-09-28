#include <string>
#include <vector>
#include <unordered_map>

#include "syncd.h"

sai_uint32_t saiGetPortCount()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    sai_status_t status = sai_switch_api->get_switch_attribute(1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get port number: %d", status);

        exit_and_notify(EXIT_FAILURE);
    }

    SWSS_LOG_DEBUG("port count is %u", attr.value.u32);

    return attr.value.u32;
}

sai_object_id_t saiGetCpuId()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_CPU_PORT;

    sai_status_t status = sai_switch_api->get_switch_attribute(1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get cpu port: %d", status);

        exit_and_notify(EXIT_FAILURE);
    }

    SWSS_LOG_DEBUG("cpu port RID %llx", attr.value.oid);

    return attr.value.oid;
}

std::vector<sai_object_id_t> saiGetPortList()
{
    SWSS_LOG_ENTER();

    uint32_t portCount = saiGetPortCount();

    std::vector<sai_object_id_t> portList;

    portList.resize(portCount);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = portCount;
    attr.value.objlist.list = portList.data();

    sai_status_t status = sai_switch_api->get_switch_attribute(1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get port list: %d", status);

        exit_and_notify(EXIT_FAILURE);
    }

    return portList;
}

std::unordered_map<sai_uint32_t, sai_object_id_t> saiGetHardwareLaneMap()
{
    SWSS_LOG_ENTER();

    std::unordered_map<sai_uint32_t, sai_object_id_t> map;

    const std::vector<sai_object_id_t> portList = saiGetPortList();

    // NOTE: currently we don't support port breakout
    // this will need to be addressed in future
    const int lanesPerPort = 4;

    for (size_t i = 0; i < portList.size(); i++)
    {
        sai_uint32_t lanes[lanesPerPort];

        sai_attribute_t attr;

        attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
        attr.value.u32list.count = lanesPerPort;
        attr.value.u32list.list = lanes;

        sai_status_t status = sai_port_api->get_port_attribute(portList[i], 1, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to get hardware lane list pid: %lx: %d", portList[i], status);

            exit_and_notify(EXIT_FAILURE);
        }

        sai_int32_t laneCount = attr.value.u32list.count;

        for (int j = 0; j < laneCount; j++)
        {
            map[lanes[j]] = portList[i];
        }
    }

    return map;
}

sai_object_id_t saiGetDefaultTrapGroup()
{

    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP;

    sai_status_t status = sai_switch_api->get_switch_attribute(1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
         SWSS_LOG_ERROR("failed to get switch default trap group %d", status);

         exit_and_notify(EXIT_FAILURE);
    }

    return attr.value.oid;
}

sai_object_id_t saiGetDefaultVirtualRouter()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;

    sai_status_t status = sai_switch_api->get_switch_attribute(1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
         SWSS_LOG_ERROR("failed to get switch virtual router id %d", status);

         exit_and_notify(EXIT_FAILURE);
    }

    return attr.value.oid;
}

void redisClearLaneMap()
{
    SWSS_LOG_ENTER();

    g_redisClient->del(LANES);
}

std::unordered_map<sai_uint32_t, sai_object_id_t> redisGetLaneMap()
{
    SWSS_LOG_ENTER();

    auto hash = g_redisClient->hgetall(LANES);

    SWSS_LOG_DEBUG("previous lanes: %lu", hash.size());

    std::unordered_map<sai_uint32_t, sai_object_id_t> map;

    for (auto &kv: hash)
    {
        const std::string &str_key = kv.first;
        const std::string &str_value = kv.second;

        sai_uint32_t lane;
        sai_object_id_t portId;

        int index = 0;

        sai_deserialize_primitive(str_key, index, lane);

        index = 0;

        sai_deserialize_primitive(str_value, index, portId);

        map[lane] = portId;
    }

    return map;
}

void redisSaveLaneMap(const std::unordered_map<sai_uint32_t, sai_object_id_t> &map)
{
    SWSS_LOG_ENTER();

    redisClearLaneMap();

    for (auto const &it: map)
    {
        sai_uint32_t lane = it.first;
        sai_object_id_t portId = it.second;

        std::string strLane;
        std::string strPortId;

        sai_serialize_primitive(lane, strLane);
        sai_serialize_primitive(portId, strPortId);

        // TODO use multi or hmset
        g_redisClient->hset(LANES, strLane, strPortId);
    }
}

std::vector<std::string> redisGetAsicStateKeys()
{
    SWSS_LOG_ENTER();

    return g_redisClient->keys("ASIC_STATE:*");
}

void redisClearVidToRidMap()
{
    SWSS_LOG_ENTER();

    g_redisClient->del(VIDTORID);
}

void redisClearRidToVidMap()
{
    SWSS_LOG_ENTER();

    g_redisClient->del(RIDTOVID);
}

std::unordered_map<sai_object_id_t, sai_object_id_t> redisGetObjectMap(std::string key)
{
    SWSS_LOG_ENTER();

    auto hash = g_redisClient->hgetall(key);

    std::unordered_map<sai_object_id_t, sai_object_id_t> map;

    for (auto &kv: hash)
    {
        const std::string &str_key = kv.first;
        const std::string &str_value = kv.second;

        sai_object_id_t objectIdKey;
        sai_object_id_t objectIdValue;

        int index = 0;

        sai_deserialize_primitive(str_key, index, objectIdKey);

        index = 0;

        sai_deserialize_primitive(str_value, index, objectIdValue);

        map[objectIdKey] = objectIdValue;
    }

    return map;
}

std::unordered_map<sai_object_id_t, sai_object_id_t> redisGetVidToRidMap()
{
    SWSS_LOG_ENTER();

    return redisGetObjectMap(VIDTORID);
}

std::unordered_map<sai_object_id_t, sai_object_id_t> redisGetRidToVidMap()
{
    SWSS_LOG_ENTER();

    return redisGetObjectMap(RIDTOVID);
}

void helperCheckLaneMap()
{
    SWSS_LOG_ENTER();

    auto redisLaneMap = redisGetLaneMap();

    auto laneMap = saiGetHardwareLaneMap();

    if (redisLaneMap.size() == 0)
    {
        SWSS_LOG_INFO("no lanes defined in redis, seems like it is first syncd start");

        // TODO put ports to db ?
        redisSaveLaneMap(laneMap);

        redisLaneMap = laneMap; // copy
    }

    if (laneMap.size() != redisLaneMap.size())
    {
        SWSS_LOG_ERROR("lanes map size differ: %lu vs %lu", laneMap.size(), redisLaneMap.size());

        exit_and_notify(EXIT_FAILURE);
    }

    for (auto kv: laneMap)
    {
        sai_uint32_t lane = kv.first;
        sai_object_id_t portId = kv.second;

        if (redisLaneMap.find(lane) == redisLaneMap.end())
        {
            SWSS_LOG_ERROR("lane %u not found in redis", lane);

            exit_and_notify(EXIT_FAILURE);
        }

        if (redisLaneMap[lane] != portId)
        {
            // if this happens, we need to remap VIDTORID and RIDTOVID
            SWSS_LOG_ERROR("FIXME: lane port id differs: %llx vs %llx, port ids must be remapped", portId, redisLaneMap[lane]);

            exit_and_notify(EXIT_FAILURE);
        }
    }
}

sai_object_id_t redisGetDefaultVirtualRouterId()
{
    SWSS_LOG_ENTER();

    auto redisVrId = g_redisClient->hget(HIDDEN, DEFAULT_VIRTUAL_ROUTER_ID);

    if (redisVrId == NULL)
        return SAI_NULL_OBJECT_ID;

    sai_object_id_t vr_id;

    int index = 0;

    sai_deserialize_primitive(*redisVrId, index, vr_id);

    return vr_id;
}

sai_object_id_t redisGetDefaultTrapGroupId()
{
    SWSS_LOG_ENTER();

    auto redisVrId = g_redisClient->hget(HIDDEN, DEFAULT_TRAP_GROUP_ID);

    if (redisVrId == NULL)
        return SAI_NULL_OBJECT_ID;

    sai_object_id_t vr_id;

    int index = 0;

    sai_deserialize_primitive(*redisVrId, index, vr_id);

    return vr_id;
}

sai_object_id_t redisGetCpuId()
{
    SWSS_LOG_ENTER();

    auto redisCpuId = g_redisClient->hget(HIDDEN, CPU_PORT_ID);

    if (redisCpuId == NULL)
        return SAI_NULL_OBJECT_ID;

    sai_object_id_t cpuId;

    int index = 0;

    sai_deserialize_primitive(*redisCpuId, index, cpuId);

    return cpuId;
}

void redisSetDefaultVirtualRouterId(sai_object_id_t vr_id)
{
    SWSS_LOG_ENTER();

    std::string strVrId;

    sai_serialize_primitive(vr_id, strVrId);

    g_redisClient->hset(HIDDEN, DEFAULT_VIRTUAL_ROUTER_ID, strVrId);
}

void redisSetDefaultTrapGroup(sai_object_id_t vr_id)
{
    SWSS_LOG_ENTER();

    std::string strVrId;

    sai_serialize_primitive(vr_id, strVrId);

    g_redisClient->hset(HIDDEN, DEFAULT_TRAP_GROUP_ID, strVrId);
}

void redisCreateRidAndVidMapping(sai_object_id_t rid, sai_object_id_t vid)
{
    SWSS_LOG_ENTER();

    std::string strRid, strVid;

    sai_serialize_primitive(rid, strRid);
    sai_serialize_primitive(vid, strVid);

    g_redisClient->hset(VIDTORID, strVid, strRid);
    g_redisClient->hset(RIDTOVID, strRid, strVid);

    SWSS_LOG_DEBUG("set VID %llx and RID %llx", vid, rid);
}

void redisSetDummyAsicStateForRealObjectId(sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    sai_object_type_t objectType = sai_object_type_query(rid);

    if (objectType == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_ERROR("sai_object_type_query returned NULL type for RID %llx", rid);

        exit_and_notify(EXIT_FAILURE);
    }

    std::string strObjectType;

    sai_serialize_primitive(objectType, strObjectType);

    sai_object_id_t vid = redis_create_virtual_object_id(objectType);

    std::string strVid;

    sai_serialize_primitive(vid, strVid);

    std::string strKey = "ASIC_STATE:" + strObjectType + ":" + strVid;

    g_redisClient->hset(strKey, "NULL", "NULL");

    redisCreateRidAndVidMapping(rid, vid);
}

void helperCheckDefaultVirtualRouterId()
{
    SWSS_LOG_ENTER();

    sai_object_id_t vrId = saiGetDefaultVirtualRouter();

    sai_object_id_t redisVrId = redisGetDefaultVirtualRouterId();

    if (redisVrId == SAI_NULL_OBJECT_ID)
    {
        redisSetDummyAsicStateForRealObjectId(vrId);

        SWSS_LOG_INFO("redis default virtual router id is not defined yet");

        redisSetDefaultVirtualRouterId(vrId);

        redisVrId = vrId;
    }

    if (vrId != redisVrId)
    {
        // if this happens, we need to remap VIDTORID and RIDTOVID
        SWSS_LOG_ERROR("FIXME: default virtual router id differs: %llx vs %llx, ids must be remapped", vrId, redisVrId);

        exit_and_notify(EXIT_FAILURE);
    }
}


void helperCheckDefaultTrapGroup()
{
    SWSS_LOG_ENTER();

    sai_object_id_t tgId = saiGetDefaultTrapGroup();

    sai_object_id_t redisTgId = redisGetDefaultTrapGroupId();

    if (redisTgId == SAI_NULL_OBJECT_ID)
    {
        redisSetDummyAsicStateForRealObjectId(tgId);

        SWSS_LOG_INFO("redis default trap group is not defined yet");

        redisSetDefaultTrapGroup(tgId);

        redisTgId = tgId;
    }

    if (tgId != redisTgId)
    {
        // if this happens, we need to remap VIDTORID and RIDTOVID
        SWSS_LOG_ERROR("FIXME: default trap group id differs: %llx vs %llx, ids must be remapped", tgId, redisTgId);

        exit_and_notify(EXIT_FAILURE);
    }
}

void redisSetCpuId(sai_object_id_t cpuId)
{
    SWSS_LOG_ENTER();

    std::string strCpuId;

    sai_serialize_primitive(cpuId, strCpuId);

    g_redisClient->hset(HIDDEN, CPU_PORT_ID, strCpuId);
}

void helperCheckCpuId()
{
    SWSS_LOG_ENTER();

    sai_object_id_t cpuId = saiGetCpuId();

    sai_object_id_t redisCpuId = redisGetCpuId();

    if (redisCpuId == SAI_NULL_OBJECT_ID)
    {
        redisSetDummyAsicStateForRealObjectId(cpuId);

        SWSS_LOG_INFO("redis cpu id is not defined yet");

        redisSetCpuId(cpuId);

        redisCpuId = cpuId;
    }

    if (cpuId != redisCpuId)
    {
        // if this happens, we need to remap VIDTORID and RIDTOVID
        SWSS_LOG_ERROR("FIXME: cpu id differs: %llx vs %llx, ids must be remapped", cpuId, redisCpuId);

        exit_and_notify(EXIT_FAILURE);
    }
}

void helperCheckPortIds()
{
    SWSS_LOG_ENTER();

    auto laneMap = saiGetHardwareLaneMap();

    for (auto kv: laneMap)
    {
        sai_object_id_t portId = kv.second;

        // translate will create entry if missing
        // we assume here that port numbers didn't changed
        // during restarts
        sai_object_id_t vid = translate_rid_to_vid(portId);

        sai_object_type_t objectType = sai_object_type_query(portId);

        if (objectType == SAI_OBJECT_TYPE_NULL)
        {
            SWSS_LOG_ERROR("sai_object_type_query returned NULL type for RID %llx", portId);

            exit_and_notify(EXIT_FAILURE);
        }

        std::string strObjectType;

        sai_serialize_primitive(objectType, strObjectType);

        std::string strVid;

        sai_serialize_primitive(vid, strVid);

        std::string strKey = "ASIC_STATE:" + strObjectType + ":" + strVid;

        g_redisClient->hset(strKey, "NULL", "NULL");
    }
}

void helperCheckVlanId()
{
    SWSS_LOG_ENTER();

    // TODO create vlan entry only when its hard restart

    sai_object_type_t objectType = SAI_OBJECT_TYPE_VLAN;

    std::string strObjectType;

    sai_serialize_primitive(objectType, strObjectType);

    std::string strVlanId;

    sai_vlan_id_t vlanId = 1;

    sai_serialize_primitive(vlanId, strVlanId);

    std::string strKey = "ASIC_STATE:" + strObjectType + ":" + strVlanId;

    g_redisClient->hset(strKey, "NULL", "NULL");
}

void onSyncdStart(bool warmStart)
{
    // it may happen that after initialize we will receive
    // some port notifications with port'ids that are not in
    // redis db yet, so after checking VIDTORID map there will
    // be entries and translate_vid_to_rid will generate new
    // id's for ports, this may cause race condition so we need
    // to use a lock here to prevent that

    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    helperCheckLaneMap();

    helperCheckCpuId();

    helperCheckDefaultVirtualRouterId();

    helperCheckDefaultTrapGroup();

    helperCheckVlanId();

    helperCheckPortIds();

    if (warmStart)
    {
        SWSS_LOG_NOTICE("skipping hard reinit since WARM start was performed");
        return;
    }

    SWSS_LOG_NOTICE("performing hard reinit since COLD start was performed");

    hardReinit();
}
