#include <string>
#include <vector>
#include <unordered_map>

#include "syncd.h"

typedef std::unordered_map<std::string, std::string> StringHash;
typedef std::unordered_map<sai_object_id_t, sai_object_id_t> ObjectIdMap;

std::unordered_map<std::string, std::shared_ptr<SaiAttributeList>> g_attributesLists;

ObjectIdMap g_translated;

ObjectIdMap g_vidToRidMap;
ObjectIdMap g_ridToVidMap;

StringHash g_switches;
StringHash g_vlans;
StringHash g_neighbors;
StringHash g_oids;
StringHash g_fdbs;
StringHash g_routes;
StringHash g_traps;

void processAttributesForOids(sai_object_type_t objectType, std::shared_ptr<SaiAttributeList> list);
void processSwitch();
void processVlans();
void processNeighbors();
void processOids();
void processFdbs();
void processRoutes();
void processTraps();

sai_object_type_t getObjectTypeFromAsicKey(const std::string &key);

sai_object_type_t getObjectTypeFromVid(sai_object_id_t sai_object_id)
{
    SWSS_LOG_ENTER();

    sai_object_type_t objectType = (sai_object_type_t)(sai_object_id >> 48);

    if (objectType >= SAI_OBJECT_TYPE_MAX ||
            objectType == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_ERROR("invalid object type: %x on object id: %llx", objectType, sai_object_id);

        exit_and_notify(EXIT_FAILURE);
    }

    return objectType;
}

std::shared_ptr<SaiAttributeList> redisGetAttributesFromAsicKey(const std::string &key)
{
    SWSS_LOG_ENTER();

    sai_object_type_t objectType = getObjectTypeFromAsicKey(key);

    std::vector<swss::FieldValueTuple> values;

    auto hash = g_redisClient->hgetall(key);

    for (auto &kv: hash)
    {
        const std::string &key = kv.first;
        const std::string &value = kv.second;

        swss::FieldValueTuple fvt(key, value);

        values.push_back(fvt);
    }

    return std::shared_ptr<SaiAttributeList>(new SaiAttributeList(objectType, values, false));
}

sai_object_type_t getObjectTypeFromAsicKey(const std::string &key)
{
    SWSS_LOG_ENTER();

    const std::string strObjectType = key.substr(key.find_first_of(":") + 1, key.find_last_of(":"));

    int index = 0;

    sai_object_type_t objectType;
    sai_deserialize_primitive(strObjectType, index, objectType);

    if (objectType >= SAI_OBJECT_TYPE_MAX ||
        objectType == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_ERROR("invalid object type: %x on asic key: %s", objectType, key.c_str());

        exit_and_notify(EXIT_FAILURE);
    }

    return objectType;
}

std::string getObjectIdFromAsicKey(const std::string &key)
{
    SWSS_LOG_ENTER();

    return key.substr(key.find_last_of(":") + 1);
}

void redisSetVidAndRidMap(std::unordered_map<sai_object_id_t, sai_object_id_t> map)
{
    SWSS_LOG_ENTER();

    redisClearVidToRidMap();
    redisClearRidToVidMap();

    for (auto &kv: map)
    {
        std::string strVid;
        std::string strRid;

        sai_serialize_primitive(kv.first, strVid);
        sai_serialize_primitive(kv.second, strRid);

        g_redisClient->hset(VIDTORID, strVid, strRid);
        g_redisClient->hset(RIDTOVID, strRid, strVid);
    }
}

void checkAllIds()
{
    SWSS_LOG_ENTER();

    for (auto &kv: g_translated)
    {
        auto it = g_vidToRidMap.find(kv.first);

        if (it == g_vidToRidMap.end())
        {
            SWSS_LOG_ERROR("failed to find vid %llx in previous map", kv.first);

            exit_and_notify(EXIT_FAILURE);
        }

        g_vidToRidMap.erase(it);
    }

    if (g_vidToRidMap.size() != 0)
    {
        SWSS_LOG_ERROR("vid to rid map is not empty after translation");

        for (auto &kv: g_vidToRidMap)
        {
            sai_object_type_t objectType = getObjectTypeFromVid(kv.first);

            SWSS_LOG_ERROR("vid not translated: %llx, object type: %llx", kv.first, objectType);
        }

        exit_and_notify(EXIT_FAILURE);
    }

    redisSetVidAndRidMap(g_translated);
}

void saiRemoveDefaultVlanMembers()
{
    SWSS_LOG_ENTER();

    // after asic init, all ports are vlan 1 members
    // we will remove them to not complicate reinit
    // if user want to add vlan 1, then it needs to be
    // done explicitly.

    const auto& portList = saiGetPortList();

    std::vector<sai_object_id_t> vlanMemberList;

    vlanMemberList.resize(portList.size());

    sai_attribute_t attr;

    attr.id = SAI_VLAN_ATTR_MEMBER_LIST;

    attr.value.objlist.count = vlanMemberList.size();
    attr.value.objlist.list = vlanMemberList.data();

    sai_status_t status = sai_vlan_api->get_vlan_attribute(DEFAULT_VLAN_NUMBER, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to obtain vlan %d members", DEFAULT_VLAN_NUMBER);

        exit_and_notify(EXIT_FAILURE);
    }

    vlanMemberList.resize(attr.value.objlist.count);

    for (auto &vm: vlanMemberList)
    {
        status = sai_vlan_api->remove_vlan_member(vm);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to remove vlan member 0x%llx from vland %d", vm, DEFAULT_VLAN_NUMBER);

            exit_and_notify(EXIT_FAILURE);
        }
    }
}

void hardReinit()
{
    SWSS_LOG_ENTER();

    saiRemoveDefaultVlanMembers();

    // repopulate asic view from redis db after hard asic initialize

    g_vidToRidMap = redisGetVidToRidMap();
    g_ridToVidMap = redisGetRidToVidMap();

    std::vector<std::string> asicStateKeys = redisGetAsicStateKeys();

    for (auto &key: asicStateKeys)
    {
        sai_object_type_t objectType = getObjectTypeFromAsicKey(key);
        const std::string &strObjectId = getObjectIdFromAsicKey(key);

        switch (objectType)
        {
            case SAI_OBJECT_TYPE_ROUTE:
                g_routes[strObjectId] = key;
                break;

            case SAI_OBJECT_TYPE_VLAN:
                g_vlans[strObjectId] = key;
                break;

            case SAI_OBJECT_TYPE_FDB:
                g_fdbs[strObjectId] = key;
                break;

            case SAI_OBJECT_TYPE_NEIGHBOR:
                g_neighbors[strObjectId] = key;
                break;

            case SAI_OBJECT_TYPE_TRAP:
                g_traps[strObjectId] = key;
                break;

            case SAI_OBJECT_TYPE_SWITCH:
                g_switches[strObjectId] = key;
                break;

            default:
                g_oids[strObjectId] = key;
                break;
        }

        g_attributesLists[key] = redisGetAttributesFromAsicKey(key);
    }

    processSwitch();
    processVlans();
    processFdbs();
    processNeighbors();
    processOids();
    processRoutes();
    processTraps();

    checkAllIds();
}

sai_object_id_t processSingleVid(sai_object_id_t vid)
{
    SWSS_LOG_ENTER();

    if (vid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_DEBUG("processed VID 0 to RID 0");

        return SAI_NULL_OBJECT_ID;
    }

    auto it = g_translated.find(vid);

    if (it != g_translated.end())
    {
        // this object was already processed,
        // just return real object id

        SWSS_LOG_DEBUG("processed VID %llx to RID %llx", vid, it->second);

        return it->second;
    }

    sai_object_id_t rid;
    sai_object_type_t objectType = getObjectTypeFromVid(vid);

    std::string strVid;
    sai_serialize_primitive(vid, strVid);

    bool createObject = true;

    if (objectType == SAI_OBJECT_TYPE_VIRTUAL_ROUTER)
    {
        auto it = g_vidToRidMap.find(vid);

        if (it == g_vidToRidMap.end())
        {
            SWSS_LOG_ERROR("failed to find VID %llx in VIDTORID map", vid);

            exit_and_notify(EXIT_FAILURE);
        }

        sai_object_id_t virtualRouterRid = it->second;

        sai_object_id_t defaultVirtualRouterId = redisGetDefaultVirtualRouterId();

        if (virtualRouterRid == defaultVirtualRouterId)
        {
            // this is default virtual router id
            // we don't need to create it, just set attributes

            rid = defaultVirtualRouterId;

            createObject = false;

            SWSS_LOG_INFO("default virtual router will not be created, processed VID %llx to RID %llx", vid, rid);
        }

    }
    else if (objectType == SAI_OBJECT_TYPE_TRAP_GROUP)
    {
        auto it = g_vidToRidMap.find(vid);

        if (it == g_vidToRidMap.end())
        {
            SWSS_LOG_ERROR("failed to find VID %llx in VIDTORID map", vid);

            exit_and_notify(EXIT_FAILURE);
        }

        sai_object_id_t trapGroupRid = it->second;

        sai_object_id_t defaultTrapGroupId = redisGetDefaultTrapGroupId();

        if (trapGroupRid == defaultTrapGroupId)
        {
            // this is default trap group id
            // we don't need to create it, just set attributes

            rid = defaultTrapGroupId;

            createObject = false;

            SWSS_LOG_INFO("default trap group will not be created, processed VID %llx to RID %llx", vid, rid);
        }

    }
    else if (objectType == SAI_OBJECT_TYPE_PORT)
    {
        // currently we assume that port id's don't change,
        // so we can just treat previous rid as current rid

        auto it = g_vidToRidMap.find(vid);

        if (it == g_vidToRidMap.end())
        {
            SWSS_LOG_ERROR("failed to find VID %llx in VIDTORID map", vid);

            exit_and_notify(EXIT_FAILURE);
        }

        rid = it->second;

        createObject = false;

        SWSS_LOG_INFO("port will not be created, processed VID %llx to RID %llx", vid, rid);
    }

    auto oit = g_oids.find(strVid);

    if (oit == g_oids.end())
    {
        SWSS_LOG_ERROR("failed to find VID %s in OIDs map", strVid.c_str());

        exit_and_notify(EXIT_FAILURE);
    }

    std::string asicKey = oit->second;;

    std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

    processAttributesForOids(objectType, list); // recursion

    sai_attribute_t *attrList = list->get_attr_list();

    uint32_t attrCount = list->get_attr_count();

    if (createObject)
    {
        create_fn create = common_create[objectType];

        if (create == NULL)
        {
            SWSS_LOG_ERROR("create function is not defined for object type %llx", objectType);

            exit_and_notify(EXIT_FAILURE);
        }

        sai_status_t status = create(&rid, attrCount, attrList);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to create object %llx: %d", objectType, status);

            exit_and_notify(EXIT_FAILURE);
        }

        SWSS_LOG_DEBUG("created object of type %x, processed VID %llx to RID %llx", objectType, vid, rid);
    }
    else
    {
        SWSS_LOG_DEBUG("setting attributes on object of type %x, processed VID %llx to RID %llx", objectType, vid, rid);

        set_attribute_fn set = common_set_attribute[objectType];

        for (uint32_t idx = 0; idx < attrCount; idx++)
        {
            sai_attribute_t *attr = &attrList[idx];

            sai_status_t status = set(rid, attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_ERROR("failed to set attribute for object type %llx attr id %llx: %d", objectType, attr->id, status);

                exit_and_notify(EXIT_FAILURE);
            }
        }
    }

    g_translated[vid] = rid;

    return rid;
}

sai_attr_serialization_type_t getSerializationType(sai_object_type_t objectType, sai_attr_id_t attrId)
{
    SWSS_LOG_ENTER();

    sai_attr_serialization_type_t serializationType;
    sai_status_t status = sai_get_serialization_type(objectType, attrId, serializationType);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("unable to find serialization type on object type %x and attr id %x", objectType, attrId);

        exit_and_notify(EXIT_FAILURE);
    }

    return serializationType;
}

void processAttributesForOids(sai_object_type_t objectType, std::shared_ptr<SaiAttributeList> list)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("processing list for object type %llx", objectType);

    sai_attribute_t *attrList = list->get_attr_list();

    uint32_t attrCount = list->get_attr_count();

    for (uint32_t idx = 0; idx < attrCount; idx++)
    {
        sai_attribute_t &attr = attrList[idx];

        sai_attr_serialization_type_t serializationType = getSerializationType(objectType, attr.id);

        uint32_t count = 0;
        sai_object_id_t *objectIdList;

        switch (serializationType)
        {
            case SAI_SERIALIZATION_TYPE_OBJECT_ID:
                count = 1;
                objectIdList = &attr.value.oid;
                break;

            case SAI_SERIALIZATION_TYPE_OBJECT_LIST:
                count = attr.value.objlist.count;
                objectIdList = attr.value.objlist.list;
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                count = 1;
                objectIdList = &attr.value.aclfield.data.oid;
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                count = attr.value.aclfield.data.objlist.count;
                objectIdList = attr.value.aclfield.data.objlist.list;
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                count = 1;
                objectIdList = &attr.value.aclaction.parameter.oid;
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                count = attr.value.aclaction.parameter.objlist.count;
                objectIdList = attr.value.aclaction.parameter.objlist.list;
                break;

            case SAI_SERIALIZATION_TYPE_PORT_BREAKOUT:
                count = attr.value.portbreakout.port_list.count;
                objectIdList = attr.value.portbreakout.port_list.list;
                break;

            default:
                continue;
        }

        // attribute contains object id's, they need to be translated
        // some of them could be already translated

        for (uint32_t idx = 0; idx < count; idx++)
        {
            sai_object_id_t vid = objectIdList[idx];

            sai_object_id_t rid = processSingleVid(vid);

            objectIdList[idx] = rid;
        }
    }
}

sai_object_id_t getObjectIdFromString(const std::string &strObjectId)
{
    SWSS_LOG_ENTER();

    int index = 0;
    sai_object_id_t objectId;
    sai_deserialize_primitive(strObjectId, index, objectId);

    return objectId;
}

void processOids()
{
    for (auto &kv: g_oids)
    {
        const std::string &strObjectId = kv.first;

        sai_object_id_t vidObjectId = getObjectIdFromString(strObjectId);

        processSingleVid(vidObjectId);
    }
}

void processSwitch()
{
    SWSS_LOG_ENTER();

    for (auto &kv: g_switches)
    {
        const std::string &asicKey = kv.second;

        std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

        processAttributesForOids(SAI_OBJECT_TYPE_SWITCH, list);

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        for (uint32_t idx = 0; idx < attrCount; idx++)
        {
            sai_attribute_t *attr = &attrList[idx];

            sai_status_t status = sai_switch_api->set_switch_attribute(attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_ERROR("failed to set_switch_attribute %llx: %d", attr->id, status);

                exit_and_notify(EXIT_FAILURE);
            }
        }
    }
}

sai_vlan_id_t getVlanIdFromString(const std::string &strVlanId)
{
    SWSS_LOG_ENTER();

    int index = 0;
    sai_vlan_id_t vlanId;
    sai_deserialize_primitive(strVlanId, index, vlanId);

    return vlanId;
}

void processVlans()
{
    SWSS_LOG_ENTER();

    for (auto &kv: g_vlans)
    {
        const std::string &strVlanId = kv.first;
        const std::string &asicKey = kv.second;

        sai_vlan_id_t vlanId = getVlanIdFromString(strVlanId);

        if (vlanId != 1)
        {
            // don't create vlan 1, we assume it exists
            sai_status_t status = sai_vlan_api->create_vlan(vlanId);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_ERROR("failed to create_vlan %d: %d", vlanId, status);

                exit_and_notify(EXIT_FAILURE);
            }
        }

        std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

        processAttributesForOids(SAI_OBJECT_TYPE_VLAN, list);

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t count = list->get_attr_count();

        for (uint32_t idx = 0; idx < count; idx++)
        {
            sai_attribute_t *attr = &attrList[idx];

            sai_status_t status = sai_vlan_api->set_vlan_attribute(vlanId, attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_ERROR("failed to set_vlan_attribute %llx: %d", attr->id, status);

                exit_and_notify(EXIT_FAILURE);
            }
        }
    }
}

sai_fdb_entry_t getFdbEntryFromString(const std::string &strFdbEntry)
{
    SWSS_LOG_ENTER();

    int index = 0;
    sai_fdb_entry_t fdbEntry;
    sai_deserialize_primitive(strFdbEntry, index, fdbEntry);

    return fdbEntry;
}

void processFdbs()
{
    SWSS_LOG_ENTER();

    for (auto &kv: g_fdbs)
    {
        const std::string &strFdbEntry = kv.first;
        const std::string &asicKey = kv.second;

        sai_fdb_entry_t fdbEntry = getFdbEntryFromString(strFdbEntry);

        std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

        processAttributesForOids(SAI_OBJECT_TYPE_FDB, list);

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        sai_status_t status = sai_fdb_api->create_fdb_entry(&fdbEntry, attrCount, attrList);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to create_fdb_entry: %d", status);

            exit_and_notify(EXIT_FAILURE);
        }
    }
}

sai_neighbor_entry_t getNeighborEntryFromString(const std::string &strNeighborEntry)
{
    SWSS_LOG_ENTER();

    int index = 0;
    sai_neighbor_entry_t neighborEntry;
    sai_deserialize_neighbor_entry(strNeighborEntry, index, neighborEntry);

    return neighborEntry;
}

void processNeighbors()
{
    SWSS_LOG_ENTER();

    for (auto &kv: g_neighbors)
    {
        const std::string &strNeighborEntry = kv.first;
        const std::string &asicKey = kv.second;

        sai_neighbor_entry_t neighborEntry = getNeighborEntryFromString(strNeighborEntry);

        neighborEntry.rif_id = processSingleVid(neighborEntry.rif_id);

        std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

        processAttributesForOids(SAI_OBJECT_TYPE_NEIGHBOR, list);

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        sai_status_t status = sai_neighbor_api->create_neighbor_entry(&neighborEntry, attrCount, attrList);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to create_neighbor_entry: %d", status);

            exit_and_notify(EXIT_FAILURE);
        }
    }
}

sai_unicast_route_entry_t getRouteEntryFromString(const std::string &strRouteEntry)
{
    SWSS_LOG_ENTER();

    int index = 0;
    sai_unicast_route_entry_t routeEntry;
    sai_deserialize_route_entry(strRouteEntry, index, routeEntry);

    return routeEntry;
}

void processRoutes()
{
    SWSS_LOG_ENTER();

    for (auto &kv: g_routes)
    {
        const std::string &strRouteEntry = kv.first;
        const std::string &asicKey = kv.second;

        sai_unicast_route_entry_t routeEntry = getRouteEntryFromString(strRouteEntry);

        routeEntry.vr_id = processSingleVid(routeEntry.vr_id);

        std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

        processAttributesForOids(SAI_OBJECT_TYPE_ROUTE, list);

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        sai_status_t status = sai_route_api->create_route(&routeEntry, attrCount, attrList);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to create_route_entry: %d", status);

            exit_and_notify(EXIT_FAILURE);
        }
    }
}

sai_hostif_trap_id_t getTrapIdFromString(const std::string &strObjectId)
{
    SWSS_LOG_ENTER();

    int index = 0;
    sai_object_id_t dummyId;
    sai_deserialize_primitive(strObjectId, index, dummyId);

    // trap id is encoded as sai_object_id_t
    sai_hostif_trap_id_t trapId = (sai_hostif_trap_id_t)dummyId;

    return trapId;
}

void processTraps()
{
    SWSS_LOG_ENTER();

    for (auto &kv: g_traps)
    {
        const std::string &strTrapId = kv.first;
        const std::string &asicKey = kv.second;

        sai_hostif_trap_id_t trapId = getTrapIdFromString(strTrapId);

        std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

        processAttributesForOids(SAI_OBJECT_TYPE_TRAP, list);

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        for (uint32_t idx = 0; idx < attrCount; idx++)
        {
            sai_attribute_t *attr = &attrList[idx];

            sai_status_t status = sai_hostif_api->set_trap_attribute(trapId, attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_ERROR("failed to set_trap_attribute %d: %d", trapId, status);

                exit_and_notify(EXIT_FAILURE);
            }
        }
    }
}
