#include "SaiSwitch.h"
#include "VendorSai.h"
#include "SaiDiscovery.h"
#include "VidManager.h"
#include "GlobalSwitchId.h"
#include "RedisClient.h"
#include "DisabledRedisClient.h"

#include "meta/sai_serialize.h"
#include "swss/logger.h"

using namespace syncd;

#define MAX_OBJLIST_LEN 128

/*
 * NOTE: If real ID will change during hard restarts, then we need to remap all
 * VID/RID, but we can only do that if we will save entire tree with all
 * dependencies.
 */

SaiSwitch::SaiSwitch(
        _In_ sai_object_id_t switch_vid,
        _In_ sai_object_id_t switch_rid,
        _In_ std::shared_ptr<BaseRedisClient> client,
        _In_ std::shared_ptr<VirtualOidTranslator> translator,
        _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai,
        _In_ bool warmBoot):
    SaiSwitchInterface(switch_vid, switch_rid),
    m_vendorSai(vendorSai),
    m_warmBoot(warmBoot),
    m_translator(translator),
    m_client(client)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("constructor");

    GlobalSwitchId::setSwitchId(m_switch_rid);

    m_hardware_info = saiGetHardwareInfo();

    /*
     * Discover put objects to redis needs to be called before checking lane
     * map and ports, since it will deduce whether put discovered objects to
     * redis to not interfere with possible user created objects previously.
     *
     * TODO: When user will use sairedis we need to send discovered view
     * with all objects dependencies to sairedis so metadata db could
     * be populated, and all references could be increased.
     */

    helperDiscover();

    if (warmBoot)
    {
        checkWarmBootDiscoveredRids();
    }

    helperSaveDiscoveredObjectsToRedis();

    helperInternalOids();

    if (getSwitchType() == SAI_SWITCH_TYPE_NPU || getSwitchType() == SAI_SWITCH_TYPE_VOQ)
    {
        helperCheckLaneMap();
    }

    helperLoadColdVids();

    if (getSwitchType() == SAI_SWITCH_TYPE_NPU)
    {
        saiGetMacAddress(m_default_mac_address);
        saiGetVxlanDefaultRouterMacAddress(m_vxlan_default_router_mac_address);
    }
}

/*
 * NOTE: all those methods could be implemented inside SaiSwitch class so then
 * we could skip using switch_id in params and even they could be public then.
 */

sai_uint32_t SaiSwitch::saiGetPortCount() const
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    sai_status_t status = m_vendorSai->get(SAI_OBJECT_TYPE_SWITCH, m_switch_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to get port number: %s",
                sai_serialize_status(status).c_str());
    }

    SWSS_LOG_DEBUG("port count is %u", attr.value.u32);

    return attr.value.u32;
}

void SaiSwitch::saiGetMacAddress(
        _Out_ sai_mac_t &mac) const
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;

    sai_status_t status = m_vendorSai->get(SAI_OBJECT_TYPE_SWITCH, m_switch_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to get mac address: %s",
                sai_serialize_status(status).c_str());
    }

    SWSS_LOG_DEBUG("mac address is: %s",
            sai_serialize_mac(attr.value.mac).c_str());

    memcpy(mac, attr.value.mac, sizeof(sai_mac_t));
}

void SaiSwitch::saiGetVxlanDefaultRouterMacAddress(
        _Out_ sai_mac_t &mac) const
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_VXLAN_DEFAULT_ROUTER_MAC;

    sai_status_t status = m_vendorSai->get(SAI_OBJECT_TYPE_SWITCH, m_switch_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        // not all devices may support this
        SWSS_LOG_WARN("failed to obtain SAI_SWITCH_ATTR_VXLAN_DEFAULT_ROUTER_MAC, setting to 00:00:00:00:00:00");

        memset(attr.value.mac, 0, sizeof(sai_mac_t));
    }

    SWSS_LOG_DEBUG("mac address is: %s",
            sai_serialize_mac(attr.value.mac).c_str());

    memcpy(mac, attr.value.mac, sizeof(sai_mac_t));
}

void SaiSwitch::getDefaultMacAddress(
        _Out_ sai_mac_t& mac) const
{
    SWSS_LOG_ENTER();

    memcpy(mac, m_default_mac_address, sizeof(sai_mac_t));
}

void SaiSwitch::getVxlanDefaultRouterMacAddress(
        _Out_ sai_mac_t& mac) const
{
    SWSS_LOG_ENTER();

    memcpy(mac, m_vxlan_default_router_mac_address, sizeof(sai_mac_t));
}

sai_switch_type_t SaiSwitch::getSwitchType() const
{
    SWSS_LOG_ENTER();
    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_TYPE;

    sai_status_t status = m_vendorSai->get(SAI_OBJECT_TYPE_SWITCH, m_switch_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_WARN("failed to get switch type with status: %s, assume default SAI_SWITCH_TYPE_NPU",
                sai_serialize_status(status).c_str());

        // Set to SAI_SWITCH_TYPE_NPU and move on
        attr.value.s32 = SAI_SWITCH_TYPE_NPU;
    }

    SWSS_LOG_INFO("switch type: '%s'", (attr.value.s32 == SAI_SWITCH_TYPE_NPU ? "SAI_SWITCH_TYPE_NPU" : "SAI_SWITCH_TYPE_PHY"));

    return (sai_switch_type_t) attr.value.s32;
}

#define MAX_HARDWARE_INFO_LENGTH 0x1000

std::string SaiSwitch::saiGetHardwareInfo() const
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    char info[MAX_HARDWARE_INFO_LENGTH];

    memset(info, 0, MAX_HARDWARE_INFO_LENGTH);

    attr.id = SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO;

    attr.value.s8list.count = MAX_HARDWARE_INFO_LENGTH;
    attr.value.s8list.list = (int8_t*)info;

    sai_status_t status = m_vendorSai->get(SAI_OBJECT_TYPE_SWITCH, m_switch_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        /*
         * TODO: We should have THROW here, but currently getting hardware info
         * is not supported, so we just return empty string like it's not set.
         * Later on basing on this entry we will distinguish whether previous
         * switch and next switch are the same.
         */
        SWSS_LOG_WARN("failed to get switch hardware info: %s",
                sai_serialize_status(status).c_str());
    }

    SWSS_LOG_DEBUG("hardware info: '%s'", info);

    return std::string(info);
}

std::vector<sai_object_id_t> SaiSwitch::saiGetPortList() const
{
    SWSS_LOG_ENTER();

    uint32_t portCount = saiGetPortCount();

    std::vector<sai_object_id_t> portList;

    portList.resize(portCount);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = portCount;
    attr.value.objlist.list = portList.data();

    /*
     * NOTE: We assume port list is always returned in the same order.
     */

    sai_status_t status = m_vendorSai->get(SAI_OBJECT_TYPE_SWITCH, m_switch_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to get port list: %s",
                sai_serialize_status(status).c_str());
    }

    portList.resize(attr.value.objlist.count);

    SWSS_LOG_DEBUG("number of ports: %zu", portList.size());

    return portList;
}

std::unordered_map<sai_uint32_t, sai_object_id_t> SaiSwitch::saiGetHardwareLaneMap() const
{
    SWSS_LOG_ENTER();

    std::unordered_map<sai_uint32_t, sai_object_id_t> map;

    const std::vector<sai_object_id_t> portList = saiGetPortList();

    /*
     * NOTE: Currently we don't support port breakout this will need to be
     * addressed in future.
     */

    for (const auto &port_rid: portList)
    {
        sai_uint32_t lanes[MAX_LANES_PER_PORT];

        memset(lanes, 0, sizeof(lanes));

        sai_attribute_t attr;

        attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
        attr.value.u32list.count = MAX_LANES_PER_PORT;
        attr.value.u32list.list = lanes;

        sai_status_t status = m_vendorSai->get(SAI_OBJECT_TYPE_PORT, port_rid, 1, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_THROW("failed to get hardware lane list port RID %s: %s",
                    sai_serialize_object_id(port_rid).c_str(),
                    sai_serialize_status(status).c_str());
        }

        sai_int32_t laneCount = attr.value.u32list.count;

        for (int j = 0; j < laneCount; j++)
        {
            map[lanes[j]] = port_rid;
        }
    }

    return map;
}

std::unordered_map<sai_object_id_t, sai_object_id_t> SaiSwitch::getVidToRidMap() const
{
    SWSS_LOG_ENTER();

    return m_client->getVidToRidMap(m_switch_vid);
}

std::unordered_map<sai_object_id_t, sai_object_id_t> SaiSwitch::getRidToVidMap() const
{
    SWSS_LOG_ENTER();

    return m_client->getRidToVidMap(m_switch_vid);
}

void SaiSwitch::helperCheckLaneMap()
{
    SWSS_LOG_ENTER();

    auto redisLaneMap = m_client->getLaneMap(m_switch_vid);

    auto laneMap = saiGetHardwareLaneMap();

    if (redisLaneMap.size() == 0)
    {
        SWSS_LOG_INFO("no lanes defined in redis, seems like it is first syncd start");

        m_client->saveLaneMap(m_switch_vid, laneMap);

        redisLaneMap = laneMap; // copy
    }

    if (laneMap.size() != redisLaneMap.size())
    {
        SWSS_LOG_THROW("lanes map size differ: %lu vs %lu", laneMap.size(), redisLaneMap.size());
    }

    for (auto kv: laneMap)
    {
        sai_uint32_t lane = kv.first;
        sai_object_id_t portId = kv.second;

        if (redisLaneMap.find(lane) == redisLaneMap.end())
        {
            SWSS_LOG_THROW("lane %u not found in redis", lane);
        }

        if (redisLaneMap[lane] != portId)
        {
            // if this happens, we need to remap VIDTORID and RIDTOVID
            SWSS_LOG_THROW("FIXME: lane port id differs: %s vs %s, port ids must be remapped",
                    sai_serialize_object_id(portId).c_str(),
                    sai_serialize_object_id(redisLaneMap[lane]).c_str());
        }
    }
}

void SaiSwitch::redisSetDummyAsicStateForRealObjectId(
        _In_ sai_object_id_t rid) const
{
    SWSS_LOG_ENTER();

    sai_object_id_t vid = m_translator->translateRidToVid(rid, m_switch_vid);

    m_client->setDummyAsicStateObject(vid);
}

void SaiSwitch::redisSetDummyAsicStateForRealObjectIds(
        _In_ size_t count,
        _In_ const sai_object_id_t* rids) const
{
    SWSS_LOG_ENTER();

    std::vector<sai_object_id_t> vids(count);

    m_translator->translateRidsToVids(m_switch_vid, count, rids, vids.data());

    m_client->setDummyAsicStateObjects(count, vids.data());
}

std::string SaiSwitch::getHardwareInfo() const
{
    SWSS_LOG_ENTER();

    /*
     * Since this attribute is CREATE_ONLY this value will not change in entire
     * life of switch.
     */

    return m_hardware_info;
}

bool SaiSwitch::isDiscoveredRid(
        _In_ sai_object_id_t rid) const
{
    SWSS_LOG_ENTER();

    return m_discovered_rids.find(rid) != m_discovered_rids.end();
}

std::set<sai_object_id_t> SaiSwitch::getDiscoveredRids() const
{
    SWSS_LOG_ENTER();

    return m_discovered_rids;
}

void SaiSwitch::removeExistingObjectReference(
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    auto it = m_discovered_rids.find(rid);

    if (it == m_discovered_rids.end())
    {
        SWSS_LOG_THROW("unable to find existing RID %s",
                sai_serialize_object_id(rid).c_str());
    }

    SWSS_LOG_INFO("removing ref RID %s",
            sai_serialize_object_id(rid).c_str());

    m_discovered_rids.erase(it);
}

void SaiSwitch::removeExistingObject(
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    auto it = m_discovered_rids.find(rid);

    if (it == m_discovered_rids.end())
    {
        SWSS_LOG_THROW("unable to find existing RID %s",
                sai_serialize_object_id(rid).c_str());
    }

    sai_object_type_t ot = m_vendorSai->objectTypeQuery(rid);

    if (ot == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_THROW("m_vendorSai->objectTypeQuery returned NULL on RID %s",
                sai_serialize_object_id(rid).c_str());
    }

    auto info = sai_metadata_get_object_type_info(ot);

    sai_object_meta_key_t meta_key = { .objecttype = ot, .objectkey = {.key = { .object_id = rid } } };

    SWSS_LOG_INFO("removing %s", sai_serialize_object_meta_key(meta_key).c_str());

    sai_status_t status = m_vendorSai->remove(meta_key.objecttype, meta_key.objectkey.key.object_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        m_discovered_rids.erase(it);
    }
    else
    {
        SWSS_LOG_ERROR("failed to remove %s RID %s: %s",
                info->objecttypename,
                sai_serialize_object_id(rid).c_str(),
                sai_serialize_status(status).c_str());
    }
}

/**
 * @brief Helper function to get attribute oid from switch.
 *
 * Helper will try to obtain oid value for given attribute id.  On success, it
 * will try to obtain this value from redis database.  When value is not in
 * redis yet, it will store it, but when value was already there, it will
 * compare redis value to current oid and when they are different, it will
 * throw exception requesting for fix. When oid values are equal, function
 * returns current value.
 *
 * @param attr_id Attribute id to obtain oid from it.
 *
 * @return Valid object id (rid) if present, SAI_NULL_OBJECT_ID on failure.
 */
sai_object_id_t SaiSwitch::helperGetSwitchAttrOid(
        _In_ sai_attr_id_t attr_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    /*
     * Get original value from the ASIC.
     */

    auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr_id);

    if (meta == NULL)
    {
        SWSS_LOG_THROW("can't get switch attribute %d metadata", attr_id);
    }

    if (meta->attrvaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID)
    {
        SWSS_LOG_THROW("atribute %s is not OID attribute", meta->attridname);
    }

    attr.id = attr_id;

    sai_status_t status = m_vendorSai->get(SAI_OBJECT_TYPE_SWITCH, m_switch_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_WARN("failed to get %s: %s",
                meta->attridname,
                sai_serialize_status(status).c_str());

        return SAI_NULL_OBJECT_ID;
    }

    SWSS_LOG_INFO("%s RID %s",
            meta->attridname,
            sai_serialize_object_id(attr.value.oid).c_str());

    sai_object_id_t rid = attr.value.oid;

    sai_object_id_t redis_rid = SAI_NULL_OBJECT_ID;

    if (rid == SAI_NULL_OBJECT_ID)
    {
        return rid;
    }

    /*
     * Get value value of the same attribute from redis.
     */

    auto ptr_redis_rid_str = m_client->getSwitchHiddenAttribute(m_switch_vid, meta->attridname);

    if (ptr_redis_rid_str == NULL)
    {
        /*
         * Redis value of this attribute is not present yet, save it!
         */

        redisSaveInternalOids(rid);

        SWSS_LOG_INFO("redis %s id is not defined yet in redis", meta->attridname);

        m_client->saveSwitchHiddenAttribute(m_switch_vid, meta->attridname, rid);

        m_default_rid_map[attr_id] = rid;

        return rid;
    }

    sai_deserialize_object_id(*ptr_redis_rid_str, redis_rid);

    if (rid != redis_rid)
    {
        /*
         * In that case we need to remap VIDTORID and RIDTOVID. This is
         * required since if previous value will be used in redis maps, and it
         * will be invalid when that value will be used to call SAI api.
         */

        SWSS_LOG_THROW("FIXME: %s RID differs: %s (asic) vs %s (redis), ids must be remapped v2r/r2v",
                meta->attridname,
                sai_serialize_object_id(rid).c_str(),
                sai_serialize_object_id(redis_rid).c_str());
    }

    m_default_rid_map[attr_id] = rid;

    return rid;
}

bool SaiSwitch::isColdBootDiscoveredRid(
        _In_ sai_object_id_t rid) const
{
    SWSS_LOG_ENTER();

    auto coldBootDiscoveredVids = getColdBootDiscoveredVids();

    /*
     * If object was discovered in cold boot, it must have valid RID assigned,
     * except objects that were removed like VLAN_MEMBER.
     */

    sai_object_id_t vid = m_translator->translateRidToVid(rid, m_switch_vid);

    return coldBootDiscoveredVids.find(vid) != coldBootDiscoveredVids.end();
}

bool SaiSwitch::isSwitchObjectDefaultRid(
        _In_ sai_object_id_t rid) const
{
    SWSS_LOG_ENTER();

    for (const auto &p: m_default_rid_map)
    {
        if (p.second == rid)
        {
            return true;
        }
    }

    return false;
}

bool SaiSwitch::isNonRemovableRid(
        _In_ sai_object_id_t rid) const
{
    SWSS_LOG_ENTER();

    if (rid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_THROW("NULL rid passed");
    }

    /*
     * Check for SAI_SWITCH_ATTR_DEFAULT_* oids like cpu, default virtual
     * router.  Those objects can't be removed if user ask for it.
     */

    /* Here we are checking for isSwitchObjectDefaultRid first then ColdBootDiscoveredRid
     * as it is possible we can discover switch Internal OID as part of warm-boot also especially
     * when we are doing SAI upgrade as part of warm-boot.*/

    if (isSwitchObjectDefaultRid(rid))
    {
        return true;
    }

    if (!isColdBootDiscoveredRid(rid))
    {
        /*
         * This object was not discovered on cold boot so it can be removed.
         */

        return false;
    }

    sai_object_type_t ot = m_vendorSai->objectTypeQuery(rid);

    /*
     * List of objects after init (mlnx 2700):
     *
     * PORT: 33                     // prevent
     * VIRTUAL_ROUTER: 1            // default
     * STP: 1                       // default
     * HOSTIF_TRAP_GROUP: 1         // default
     * QUEUE: 528                   // prevent
     * SCHEDULER_GROUP: 512         // prevent
     * INGRESS_PRIORITY_GROUP: 256  // prevent
     * HASH: 2                      // prevent
     * SWITCH: 1                    // prevent
     * VLAN: 1                      // default
     * VLAN_MEMBER: 32              // can be removed
     * STP_PORT: 32                 // can be removed (cpu don't belong to stp)
     * BRIDGE: 1                    // default
     * BRIDGE_PORT: 33              // can be removed but cpu bridge port can't
     */

    switch (ot)
    {
        case SAI_OBJECT_TYPE_VLAN_MEMBER:
        case SAI_OBJECT_TYPE_STP_PORT:
        case SAI_OBJECT_TYPE_BRIDGE_PORT:

            /*
             * Those objects were discovered during cold boot, but they can
             * still be removed since switch allows that.
             */

            return false;

        case SAI_OBJECT_TYPE_PORT:
        case SAI_OBJECT_TYPE_QUEUE:
        case SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP:
        case SAI_OBJECT_TYPE_SCHEDULER_GROUP:
        case SAI_OBJECT_TYPE_HASH:

            /*
             * TODO: Some vendors support removing of those objects then we
             * need to came up with different approach. Probably SaiSwitch
             * will need to decide whether it's possible to remove object.
             */

            return true;

        default:
            break;
    }

    SWSS_LOG_WARN("can't determine wheter object %s RID %s can be removed, FIXME",
            sai_serialize_object_type(ot).c_str(),
            sai_serialize_object_id(rid).c_str());

    return true;
}

void SaiSwitch::helperDiscover()
{
    SWSS_LOG_ENTER();

    SaiDiscovery sd(m_vendorSai);

    m_discovered_rids = sd.discover(m_switch_rid);

    m_defaultOidMap = sd.getDefaultOidMap();
}

void SaiSwitch::helperLoadColdVids()
{
    SWSS_LOG_ENTER();

    m_coldBootDiscoveredVids = m_client->getColdVids(m_switch_vid);

    SWSS_LOG_NOTICE("read %zu COLD VIDS", m_coldBootDiscoveredVids.size());
}

std::set<sai_object_id_t> SaiSwitch::getColdBootDiscoveredVids() const
{
    SWSS_LOG_ENTER();

    if (m_coldBootDiscoveredVids.size() != 0)
    {
        return m_coldBootDiscoveredVids;
    }

    /*
     * Normally we should throw here, but we want to keep backward
     * compatibility and don't break anything.
     */

    SWSS_LOG_WARN("cold boot discovered VIDs set is empty, using discovered set");

    std::set<sai_object_id_t> discoveredVids;

    for (sai_object_id_t rid: m_discovered_rids)
    {
        sai_object_id_t vid = m_translator->translateRidToVid(rid, m_switch_vid);

        discoveredVids.insert(vid);
    }

    return discoveredVids;
}

std::set<sai_object_id_t> SaiSwitch::getWarmBootDiscoveredVids() const
{
    SWSS_LOG_ENTER();

    return m_warmBootDiscoveredVids;
}

void SaiSwitch::redisSaveInternalOids(
        _In_ sai_object_id_t rid) const
{
    SWSS_LOG_ENTER();

    std::set<sai_object_id_t> coldVids;

    sai_object_id_t vid = m_translator->translateRidToVid(rid, m_switch_vid);

    coldVids.insert(vid);

    /* Save Switch Internal OID put in current view asic state and also
     * in ColdVid Table discovered as cold or warm boot.
     * Please note it is possible to discover new Switch internal OID in warm-boot also
     * if SAI gets upgraded as part of warm-boot so we are adding to ColdVid also
     * so that comparison logic do not remove this OID in warm-boot case. One example
     * is SAI_SWITCH_ATTR_DEFAULT_STP_INST_ID which is discovered in warm-boot
     * when upgrading to new SAI Version*/

    m_client->setDummyAsicStateObject(vid);

    m_client->saveColdBootDiscoveredVids(m_switch_vid, coldVids);

    SWSS_LOG_NOTICE("put switch internal discovered rid %s to Asic View and COLDVIDS",
            sai_serialize_object_id(rid).c_str());

}

void SaiSwitch::redisSaveColdBootDiscoveredVids() const
{
    SWSS_LOG_ENTER();

    std::set<sai_object_id_t> coldVids;

    for (sai_object_id_t rid: m_discovered_rids)
    {
        sai_object_id_t vid = m_translator->translateRidToVid(rid, m_switch_vid);

        coldVids.insert(vid);
    }

    m_client->saveColdBootDiscoveredVids(m_switch_vid, coldVids);
}

void SaiSwitch::helperSaveDiscoveredObjectsToRedis()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("save discovered objects to redis");

    /*
     * There is a problem:
     *
     * After switch creation, on the switch objects are created internally like
     * VLAN members, queues, SGs etc.  Some of those objects are removable.
     * User can decide that he don't want VLAN members and he will remove them.
     * Those objects will be removed from ASIC view in redis as well.
     *
     * Now after hard reinit, syncd will pick up what is in the db and it will
     * try to recreate ASIC state.  First it will create switch, and this
     * switch will create those VLAN members again inside ASIC and it will try
     * to put them back to the DB, since we need to keep track of all default
     * objects.
     *
     * We need a way to decide whether we need to put those objects to DB or
     * not. Since we are performing syncd hard reinit and recreating switch
     * that there was something in the DB already. And basing on that we can
     * deduce that we don't need to put again all our discovered objects to the
     * DB, since some of those objects could be removed at the beginning.
     *
     * Hard reinit is performed before taking any action from the redis queue.
     * But when user will decide to create switch, table consumer will put that
     * switch to the DB right away before calling SaiSwitch constructor.  But
     * that switch will be the only object in the ASIC table, so out simple
     * deduction here could be checking if there is more than object in the db.
     * If there are at least 2, then we don't need to save discovered oids.
     *
     * We will use at least number 32, since that should be at least number of
     * ports that previously discovered. We could also query for PORTs.  We can
     * also deduce this using HIDDEN key objects, if they are defined then that
     * would mean we already put objects to db at the first place.
     *
     * PS. This is not the best way to solve this problem, but works.
     *
     * TODO: Some of those objects could be removed, like vlan members etc, we
     * could actually put those objects back, but only those objects which we
     * would consider non removable, and this is hard to determine now. A
     * method getNonRemovableObjects would be nice, then we could put those
     * objects to the view every time, and not put only discovered objects that
     * reflect removable objects like vlan member.
     *
     * This must be per switch.
     */

    const int ObjectsTreshold = 32;

    size_t keys = m_client->getAsicObjectsSize(m_switch_vid);

    bool manyObjectsPresent = keys > ObjectsTreshold;

    SWSS_LOG_NOTICE("objects in ASIC state table present: %zu", keys);

    if (manyObjectsPresent)
    {
        SWSS_LOG_NOTICE("will NOT put discovered objects into db");

        // TODO or just check here and remove directly form ASIC_VIEW ?
        return;
    }

    SWSS_LOG_NOTICE("putting ALL discovered objects to redis");

    for (sai_object_id_t rid: m_discovered_rids)
    {
        /*
         * We also could thing of optimizing this since it's one call to redis
         * per rid, and probably this should be ATOMIC.
         *
         * NOTE: We are also storing read only object's here, like default
         * virtual router, CPU, default trap group, etc.
         */

        redisSetDummyAsicStateForRealObjectId(rid);
    }

    /*
     * If we are here, this is probably COLD boot, since any previous boot
     * would put lots of objects into redis DB (ports, queues, scheduler_groups
     * etc), and since this is cold boot, we can put those discovered objects
     * to cold boot objects map to redis DB. This will become handy when doing
     * warm boot and figuring out which object is default created and which is
     * user created, since after warm boot user could previously assign buffer
     * profile on ingress priority group and this buffer profile will be
     * discovered by sai discovery logic.
     *
     * Question is here whether we should put VID here or RID. And after cold
     * boot when hard reinit logic happens, we need to remap them, also note
     * that some object could be removed like VLAN members and they will not
     * have existing corresponding OID.
     */

    redisSaveColdBootDiscoveredVids();
}

void SaiSwitch::helperInternalOids()
{
    SWSS_LOG_ENTER();

    auto info = sai_metadata_get_object_type_info(SAI_OBJECT_TYPE_SWITCH);

    for (int idx = 0; info->attrmetadata[idx] != NULL; ++idx)
    {
        const sai_attr_metadata_t *md = info->attrmetadata[idx];

        if (md->attrvaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_ID &&
                md->defaultvaluetype == SAI_DEFAULT_VALUE_TYPE_SWITCH_INTERNAL)
        {
            helperGetSwitchAttrOid(md->attrid);
        }
    }
}

sai_object_id_t SaiSwitch::getDefaultValueForOidAttr(
        _In_ sai_object_id_t rid,
        _In_ sai_attr_id_t attr_id)
{
    SWSS_LOG_ENTER();

    auto it = m_defaultOidMap.find(rid);

    if (it == m_defaultOidMap.end())
    {
        return SAI_NULL_OBJECT_ID;
    }

    auto ita = it->second.find(attr_id);

    if (ita == it->second.end())
    {
        return SAI_NULL_OBJECT_ID;
    }

    return ita->second;
}

std::vector<uint32_t> SaiSwitch::saiGetPortLanes(
        _In_ sai_object_id_t port_rid)
{
    SWSS_LOG_ENTER();

    std::vector<uint32_t> lanes;

    lanes.resize(MAX_LANES_PER_PORT);

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
    attr.value.u32list.count = MAX_LANES_PER_PORT;
    attr.value.u32list.list = lanes.data();

    sai_status_t status = m_vendorSai->get(SAI_OBJECT_TYPE_PORT, port_rid, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to get hardware lane list port RID %s: %s",
                sai_serialize_object_id(port_rid).c_str(),
                sai_serialize_status(status).c_str());
    }

    if (attr.value.u32list.count == 0)
    {
        SWSS_LOG_THROW("switch returned lane count ZERO for port RID %s",
                sai_serialize_object_id(port_rid).c_str());
    }

    lanes.resize(attr.value.u32list.count);

    return lanes;
}

void SaiSwitch::redisUpdatePortLaneMap(
        _In_ sai_object_id_t port_rid)
{
    SWSS_LOG_ENTER();

    auto lanes = saiGetPortLanes(port_rid);

    m_client->setPortLanes(m_switch_vid, port_rid, lanes);

    SWSS_LOG_NOTICE("added %zu lanes to redis lane map for port RID %s",
            lanes.size(),
            sai_serialize_object_id(port_rid).c_str());
}

void SaiSwitch::onPostPortsCreate(
        _In_ size_t count,
        _In_ const sai_object_id_t* port_rids)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("discovering objects after creating ports");

    SaiDiscovery sd(m_vendorSai, SaiDiscovery::Flags::SkipDefaultEmptyAttributes);

    auto discovered = sd.discover(count, port_rids);

    auto defaultOidMap = sd.getDefaultOidMap();

    // we need to merge default oid maps

    for (auto& kvp: defaultOidMap)
    {
        for (auto& it: kvp.second)
        {
            m_defaultOidMap[kvp.first][it.first] = it.second;
        }
    }

    SWSS_LOG_NOTICE("discovered %zu new objects (including port) after creating %zu ports",
            discovered.size(), count);

    m_discovered_rids.insert(discovered.begin(), discovered.end());

    std::vector<sai_object_id_t> rids{discovered.begin(), discovered.end()};

    /*
     * We also could think of optimizing this since it's one command
     * per rid in a redis pipeline, and probably this should be ATOMIC.
     *
     * NOTE: We are also storing read only object's here, like default
     * virtual router, CPU, default trap group, etc.
     */
    redisSetDummyAsicStateForRealObjectIds(rids.size(), rids.data());

    for (size_t idx = 0; idx < count; idx++)
    {
        redisUpdatePortLaneMap(port_rids[idx]);
    }
}

bool SaiSwitch::isWarmBoot() const
{
    SWSS_LOG_ENTER();

    return m_warmBoot;
}

void SaiSwitch::collectPortRelatedObjects(
        _In_ sai_object_id_t portRid)
{
    SWSS_LOG_ENTER();

    std::set<sai_object_id_t> related;

    sai_attr_id_t attrs[] = {
        SAI_PORT_ATTR_QOS_QUEUE_LIST,
        SAI_PORT_ATTR_QOS_SCHEDULER_GROUP_LIST,
        SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST
    };

    for (size_t i = 0; i < sizeof(attrs)/sizeof(sai_attr_id_t); i++)
    {
        std::vector<sai_object_id_t> objlist;

        objlist.resize(MAX_OBJLIST_LEN);

        sai_attribute_t attr;

        attr.id = attrs[i];

        attr.value.objlist.count = MAX_OBJLIST_LEN;
        attr.value.objlist.list = objlist.data();

        // since we have those objects already discovered we don't need to
        // query SAI to get related objects, but lets do that since user could
        // add/remove some of related objects (this should also be tracked in
        // SaiSwitch class internally

        auto status = m_vendorSai->get(SAI_OBJECT_TYPE_PORT, portRid, 1, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_THROW("failed to obtain related obejcts for port rid %s: %s, attr id: %d",
                    sai_serialize_object_id(portRid).c_str(),
                    sai_serialize_status(status).c_str(),
                    attr.id);
        }

        objlist.resize(attr.value.objlist.count);

        related.insert(objlist.begin(), objlist.end());
    }

    // treat port serdes as related object

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_PORT_SERDES_ID;

    auto status = m_vendorSai->get(SAI_OBJECT_TYPE_PORT, portRid, 1, &attr);

    if (status == SAI_STATUS_SUCCESS && attr.value.oid != SAI_NULL_OBJECT_ID)
    {
        // some platforms not support PORT SERDES, so get it only on success

        related.insert(attr.value.oid);
    }

    SWSS_LOG_NOTICE("obtained %zu port %s related RIDs",
            related.size(),
            sai_serialize_object_id(portRid).c_str());

    m_portRelatedObjects[portRid] = related;
}

void SaiSwitch::postPortRemove(
        _In_ sai_object_id_t portRid)
{
    SWSS_LOG_ENTER();

    if (m_portRelatedObjects.find(portRid) == m_portRelatedObjects.end())
    {
        SWSS_LOG_THROW("no port related objects populated for port RID %s",
                sai_serialize_object_id(portRid).c_str());
    }

    /*
     * Port was successfully removed from vendor SAI,
     * we need to remove queues, ipgs and sg from:
     *
     * - redis ASIC DB
     * - discovered existing objects in saiswitch class
     * - local vid2rid map
     * - redis RIDTOVID map
     *
     * - also remove LANES mapping
     */

    for (auto rid: m_portRelatedObjects.at(portRid))
    {
        // remove from existing objects

        if (isDiscoveredRid(rid))
        {
            removeExistingObjectReference(rid);
        }

        // remove from RID2VID and VID2RID map in redis

        auto vid = m_client->getVidForRid(rid);

        if (vid == SAI_NULL_OBJECT_ID)
        {
            if (!m_client->isRedisEnabled())
            {
                SWSS_LOG_DEBUG("Redis disabled, skipping rid %s removal from RIDTOVID",
                        sai_serialize_object_id(rid).c_str());
                continue;
            }
            SWSS_LOG_THROW("expected rid %s to be present in RIDTOVID",
                    sai_serialize_object_id(rid).c_str());
        }

        // TODO should this remove rid,vid and object be as db op?

        m_translator->eraseRidAndVid(rid, vid);

        // remove from ASIC DB

        m_client->removeAsicObject(vid);
    }

    int removed = m_client->removePortFromLanesMap(m_switch_vid, portRid);

    SWSS_LOG_NOTICE("removed %u lanes from redis lane map for port RID %s",
            removed,
            sai_serialize_object_id(portRid).c_str());

    if (removed == 0 && m_client->isRedisEnabled())
    {
        SWSS_LOG_THROW("NO LANES found in redis lane map for given port RID %s",
                sai_serialize_object_id(portRid).c_str());
    }

    SWSS_LOG_NOTICE("post port remove actions succeeded");
}

void SaiSwitch::checkWarmBootDiscoveredRids()
{
    SWSS_LOG_ENTER();

    if (!m_warmBoot)
    {
        return;
    }

    SWSS_LOG_NOTICE("check warm boot RIDs");

    /**
     * It may happen, that after warm boot some new oids were discovered that
     * were not present on warm shutdown, this may happen during vendor SAI
     * update and for example introducing some new default objects on switch or
     * queues on cpu. In this case, translator will create new VID/RID pair on
     * database and local memory.
     *
     * After switch was created, rid discovery method was called, and all
     * discovered RIDs should be present in current RID2VID map in redis
     * database. If any RID is missing, then ether:
     * - there is bug in vendor code and after warm boot some RID values changed
     * - or we have a bug and forgot to put rid/vid pair to redis
     * - or we new objects was actually introduced with new firmware like for
     *   example PORT_SERDES
     *
     * Assumption here is that during warm boot ASIC state will not change.
     */

    auto rid2vid = getRidToVidMap();

    for (sai_object_id_t rid: getDiscoveredRids())
    {
        sai_object_id_t vid = m_translator->translateRidToVid(rid, m_switch_vid);

        m_warmBootDiscoveredVids.insert(vid);

        if (rid2vid.find(rid) == rid2vid.end())
        {
            auto ot = m_vendorSai->objectTypeQuery(rid);

            SWSS_LOG_NOTICE("spotted new RID %s missing from current RID2VID (new VID %s) (%s) on WARM BOOT",
                    sai_serialize_object_id(rid).c_str(),
                    sai_serialize_object_id(vid).c_str(),
                    sai_serialize_object_type(ot).c_str());

            m_warmBootNewDiscoveredVids.insert(vid);

            // this means that some new objects were discovered but they are
            // not present in current ASIC_VIEW, and we need to create dummy
            // entries for them

            redisSetDummyAsicStateForRealObjectId(rid);
        }
    }

    if (m_warmBootNewDiscoveredVids.size())
    {
        SWSS_LOG_NOTICE("discovered %zu new RIDs on WARM BOOT, new firmware? or bug", m_warmBootNewDiscoveredVids.size());
    }

    SWSS_LOG_NOTICE("all discovered RIDs are present in current RID2VID map for switch VID %s",
            sai_serialize_object_id(m_switch_vid).c_str());
}
