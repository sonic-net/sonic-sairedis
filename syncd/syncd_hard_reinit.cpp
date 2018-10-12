#include "syncd.h"
#include "sairedis.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>

/*
 * To support multiple switches here we need to refactor this to a class
 * similar to SaiSwitch, and then have separate vid/rid map and ecah class will
 * be recreating only one switch and handling that
 */

typedef std::unordered_map<std::string, std::string> StringHash;
typedef std::unordered_map<sai_object_id_t, sai_object_id_t> ObjectIdMap;

static std::unordered_map<std::string, std::shared_ptr<SaiAttributeList>> g_attributesLists;

static ObjectIdMap g_translatedV2R;
static ObjectIdMap g_translatedR2V;
static ObjectIdMap g_vidToRidMap;
static ObjectIdMap g_ridToVidMap;

static StringHash g_oids;
static StringHash g_switches;
static StringHash g_fdbs;
static StringHash g_routes;
static StringHash g_neighbors;

#define ENABLE_PERF

#ifdef ENABLE_PERF
static std::map<sai_object_type_t, std::tuple<int,double>> g_perf_create;
static std::map<sai_object_type_t, std::tuple<int,double>> g_perf_set;
#endif


/*
 * Since we are only supporting 1 switch we can declare this as global, and we
 * can get away with this, but later on we need to convert this to a class like
 * SaiSwitch.
 */

static sai_object_id_t g_switch_rid = SAI_NULL_OBJECT_ID;
static sai_object_id_t g_switch_vid = SAI_NULL_OBJECT_ID;

static std::shared_ptr<SaiSwitch> g_sw;

#ifdef SAITHRIFT
/*
 * SAI switch global needed for RPC server
 */
extern sai_object_id_t gSwitchId;
#endif

void processAttributesForOids(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list);

sai_object_type_t getObjectTypeFromAsicKey(
        _In_ const std::string &key);

sai_object_type_t getObjectTypeFromVid(
        _In_ sai_object_id_t object_vid)
{
    SWSS_LOG_ENTER();

    sai_object_type_t objectType = redis_sai_object_type_query(object_vid);

    // TODO metadata is valid object type
    if (objectType >= SAI_OBJECT_TYPE_MAX ||
            objectType == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_THROW("invalid object type: %s on object id: %s",
                sai_serialize_object_type(objectType).c_str(),
                sai_serialize_object_id(object_vid).c_str());
    }

    return objectType;
}

std::shared_ptr<SaiAttributeList> redisGetAttributesFromAsicKey(
        _In_ const std::string &key)
{
    SWSS_LOG_ENTER();

    sai_object_type_t objectType = getObjectTypeFromAsicKey(key);

    std::vector<swss::FieldValueTuple> values;

    auto hash = g_redisClient->hgetall(key);

    for (auto &kv: hash)
    {
        const std::string &skey = kv.first;
        const std::string &svalue = kv.second;

        swss::FieldValueTuple fvt(skey, svalue);

        values.push_back(fvt);
    }

    return std::make_shared<SaiAttributeList>(objectType, values, false);
}

sai_object_type_t getObjectTypeFromAsicKey(
        _In_ const std::string &key)
{
    SWSS_LOG_ENTER();

    auto start = key.find_first_of(":") + 1;
    auto end = key.find(":", start);

    const std::string strObjectType = key.substr(start, end - start);

    sai_object_type_t objectType;
    sai_deserialize_object_type(strObjectType, objectType);

    // TODO metadata utils is valid object<F3>:t
    if (objectType >= SAI_OBJECT_TYPE_MAX ||
            objectType == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_THROW("invalid object type: %s on asic key: %s",
                sai_serialize_object_type(objectType).c_str(),
                key.c_str());
    }

    return objectType;
}

std::string getObjectIdFromAsicKey(
        _In_ const std::string &key)
{
    SWSS_LOG_ENTER();

    auto start = key.find_first_of(":") + 1;
    auto end = key.find(":", start);

    return key.substr(end + 1);
}

void redisClearVidToRidMap()
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: needs to be done per switch.
     */

    g_redisClient->del(VIDTORID);
}

void redisClearRidToVidMap()
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: needs to be done per switch.
     */

    g_redisClient->del(RIDTOVID);
}

void redisSetVidAndRidMap(
        _In_ const std::unordered_map<sai_object_id_t, sai_object_id_t> &map)
{
    SWSS_LOG_ENTER();

    /*
     * TODO clear can be done after recreating all switches unless vid/rid map
     * will be per switch. An all this must be ATOMIC.
     *
     * This needs to be addressed when we want to support multiple switches.
     */

    redisClearVidToRidMap();
    redisClearRidToVidMap();

    for (auto &kv: map)
    {
        std::string strVid = sai_serialize_object_id(kv.first);
        std::string strRid = sai_serialize_object_id(kv.second);

        g_redisClient->hset(VIDTORID, strVid, strRid);
        g_redisClient->hset(RIDTOVID, strRid, strVid);
    }
}

void checkAllIds()
{
    SWSS_LOG_ENTER();

    for (auto &kv: g_translatedV2R)
    {
        auto it = g_vidToRidMap.find(kv.first);

        if (it == g_vidToRidMap.end())
        {
            SWSS_LOG_THROW("failed to find vid %s in previous map",
                    sai_serialize_object_id(kv.first).c_str());
        }

        g_vidToRidMap.erase(it);
    }

    size_t size = g_vidToRidMap.size();

    if (size != 0)
    {
        for (auto &kv: g_vidToRidMap)
        {
            sai_object_type_t objectType = getObjectTypeFromVid(kv.first);

            SWSS_LOG_ERROR("vid not translated: %s, object type: %s",
                    sai_serialize_object_id(kv.first).c_str(),
                    sai_serialize_object_type(objectType).c_str());
        }

        SWSS_LOG_THROW("vid to rid map is not empty (%zu) after translation", size);
    }

    redisSetVidAndRidMap(g_translatedV2R);

    /*
     * Now we must check whether we need to remove some objects like VLAN
     * members etc.
     *
     * TODO: Should this be done at start, before other oprations?
     * We are able to determine which objets are missing from rid map
     * as long as id's between restart don't change.
     */

    if (g_sw == nullptr)
    {
        /*
         * No switch was created.
         */

        return;
    }

    /*
     * We can have situation here, that some objects were removed but they
     * still exists in default map, like vlan members, and this will introduce
     * inconsistency redis vs defaults. This will be ok since switch will not
     * hold all id's, it only will hold defaults. But this swill mean, that we
     * need to remove those VLAN members from ASIC.
     *
     * We could just call discover again, but thats too long, we just needto
     * remove removed objects since we need Existing objects for ApplyView.
     *
     * Order matters here, we can't remove bridge before removing all bridge
     * ports etc. We would need to use dependency tree to do it in order.
     * But since we will only remove default objects we can try
     *
     * XXX this is workaround. FIXME
     */

    std::vector<sai_object_type_t> removeOrder = {
        SAI_OBJECT_TYPE_VLAN_MEMBER,
        SAI_OBJECT_TYPE_STP_PORT,
        SAI_OBJECT_TYPE_BRIDGE_PORT,
        SAI_OBJECT_TYPE_NULL };

    for (sai_object_type_t ot: removeOrder)
    {
        for (sai_object_id_t rid: g_sw->getExistingObjects())
        {
            if (g_translatedR2V.find(rid) != g_translatedR2V.end())
            {
                continue;
            }

            if (ot == sai_object_type_query(rid) ||
                    ot == SAI_OBJECT_TYPE_NULL)
            {
                g_sw->removeExistingObject(rid);
            }
        }
    }
}

std::unordered_map<sai_object_id_t, sai_object_id_t> redisGetVidToRidMap()
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: To support multiple switches VIDTORID must be per switch.
     */

    return SaiSwitch::redisGetObjectMap(VIDTORID);
}

std::unordered_map<sai_object_id_t, sai_object_id_t> redisGetRidToVidMap()
{
    SWSS_LOG_ENTER();

    /*
     * NOTE: To support multiple switches RIDTOVID must be per switch.
     */

    return SaiSwitch::redisGetObjectMap(RIDTOVID);
}

void processSwitches()
{
    SWSS_LOG_ENTER();

    /*
     * If there are any switches, we need to create them first to perform any
     * other operations.
     *
     * NOTE: This method needs to be revisited if we want to support multiple
     * switches.
     */

    if (switches.size() != 0)
    {
        SWSS_LOG_THROW("performing hard reinit, but there are %zu switches defined, bug!", switches.size());
    }

    if (g_switches.size() > 1)
    {
        SWSS_LOG_THROW("multiple switches %zu in hard reinit are not supported yet, FIXME", g_switches.size());
    }

    /*
     * Sanity check in metadata make sure that there are no mandatory on create
     * and create only attributes that are obejct id attributes, sinec we would
     * need create those objects first but we need switch first. So here we
     * selecting only MANDATORY_ON_CREATE and CREATE_ONLY attributes to create
     * switch.
     */

    for (const auto& s: g_switches)
    {
        std::string strSwitchVid = s.first;
        std::string asicKey = s.second;

        sai_object_id_t switch_vid;

        sai_deserialize_object_id(strSwitchVid, switch_vid);

        if (switch_vid == SAI_NULL_OBJECT_ID)
        {
            SWSS_LOG_THROW("switch id can't be NULL");
        }

        auto oit = g_oids.find(strSwitchVid);

        if (oit == g_oids.end())
        {
            SWSS_LOG_THROW("failed to find VID %s in OIDs map", strSwitchVid.c_str());
        }

        std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        /*
         * If any of those attributes are pointers, fix them, so they will
         * point to callbacks in syncd memory.
         */

        check_notifications_pointers(attrCount, attrList);

        /*
         * Now we need to select only attributes MANDATORY_ON_CREATE and
         * CREATE_ONLY and which will not contain object ids.
         *
         * No need to call processAttributesForOids since we know that there
         * are no OID attributes.
         */

        uint32_t attr_count = 0;            // attr count needed for create
        uint32_t attr_count_left = 0;       // attr count after create

        std::vector<sai_attribute_t> attrs;         // attrs for create
        std::vector<sai_attribute_t> attrs_left;    // attrs for set

        for (uint32_t idx = 0; idx < attrCount; ++idx)
        {
            auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attrList[idx].id);

            if (SAI_HAS_FLAG_MANDATORY_ON_CREATE(meta->flags) || SAI_HAS_FLAG_CREATE_ONLY(meta->flags))
            {
                /*
                 * If attribute is mandatory on create or create only, we need
                 * to select it for switch create method, since it's required
                 * on create or it will not be possible to change it after
                 * create.
                 *
                 * Currently switch don't have any conditional attributes but
                 * we could take this into account. Even if any of those
                 * conditional attributes will present, it will be not be oid
                 * attribute.
                 */

                attrs.push_back(attrList[idx]); // struct copy, we will keep the same pointers

                attr_count++;
            }
            else
            {
                /*
                 * Those attributes can be OID attributes, so we need to
                 * process them after creating switch.
                 */

                attrs_left.push_back(attrList[idx]); // struct copy, we will keep the same pointers
                attr_count_left++;
            }
        }

        sai_attribute_t *attr_list = attrs.data();

        sai_object_id_t switch_rid;

        SWSS_LOG_NOTICE("creating switch VID: %s",
                sai_serialize_object_id(switch_vid).c_str());

        sai_status_t status = sai_metadata_sai_switch_api->create_switch(&switch_rid, attr_count, attr_list);

#ifdef SAITHRIFT
        gSwitchId = switch_rid;
        SWSS_LOG_NOTICE("Initialize gSwitchId with ID = 0x%lx", gSwitchId);
#endif

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_THROW("failed to create switch RID: %s",
                    sai_serialize_status(status).c_str());
        }

        SWSS_LOG_NOTICE("created switch RID: %s",
                sai_serialize_object_id(switch_rid).c_str());
        /*
         * Save this switch ids as translated.
         */

        g_translatedV2R[switch_vid] = switch_rid;
        g_translatedR2V[switch_rid] = switch_vid;

        auto sw = switches[switch_vid] = std::make_shared<SaiSwitch>(switch_vid, switch_rid);

        /*
         * Since we have only one switch we can get away with this.
         */

        g_switch_rid = switch_rid;
        g_switch_vid = switch_vid;

        g_sw = sw;

        startDiagShell();

        /*
         * We processed switch. We have switch vid/rid so we can process all
         * other attributes of switches that are not mandatory on create and are
         * not crate only.
         *
         * Since those left attributes may contain VIDs we need to process
         * attributes for oids.
         */

        processAttributesForOids(SAI_OBJECT_TYPE_SWITCH, attr_count_left, attrs_left.data());

        for (uint32_t idx = 0; idx < attr_count_left; ++idx)
        {
            sai_attribute_t *attr = &attrs_left[idx];

            status = sai_metadata_sai_switch_api->set_switch_attribute(switch_rid, attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                if (is_set_attribute_workaround(SAI_OBJECT_TYPE_SWITCH, attr->id, status))
                {
                    continue;
                }

                SWSS_LOG_THROW("failed to set attribute %s on switch VID %s: %s",
                        sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr->id)->attridname,
                        sai_serialize_object_id(switch_rid).c_str(),
                        sai_serialize_status(status).c_str());
            }
        }
    }
}

void trapGroupWorkaround(
        _In_ sai_object_id_t vid,
        _Inout_ sai_object_id_t& rid,
        _In_ bool& createObject,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t* attrList)
{
    SWSS_LOG_ENTER();

    if (createObject)
    {
        /*
         * There is a bug on brcm that create trap group with queue attribute
         * will fail, but it can be set after create without this attribute, so
         * we will here create tap group and set it later.
         *
         * This needs to be fixed by brcm.
         */

        createObject = false; // force to "SET" left attributes
    }
    else
    {
        // default trap group or existing trap group
        return;
    }

    sai_object_type_t objectType = SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP;

    SWSS_LOG_INFO("creating trap group and setting attributes 1 by 1 as workaround");

    const sai_attribute_t* queue_attr = sai_metadata_get_attr_by_id(SAI_HOSTIF_TRAP_GROUP_ATTR_QUEUE, attrCount, attrList);

    if (queue_attr == NULL)
    {
        SWSS_LOG_THROW("missing QUEUE attribute on TRAP_GROUP creation even if it's not MANDATORY");
    }

    sai_status_t status = sai_metadata_sai_hostif_api->create_hostif_trap_group(&rid, g_switch_rid, 1, queue_attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to create TRAP_GROUP %s",
                sai_serialize_status(status).c_str());
    }

    SWSS_LOG_DEBUG("created TRAP_GROUP (%s), processed VID %s to RID %s",
            sai_serialize_object_type(objectType).c_str(),
            sai_serialize_object_id(vid).c_str(),
            sai_serialize_object_id(rid).c_str());
}

void listFailedAttributes(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t* attrList)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < attrCount; idx++)
    {
        const sai_attribute_t *attr = &attrList[idx];

        auto meta = sai_metadata_get_attr_metadata(objectType, attr->id);

        if (meta == NULL)
        {
            SWSS_LOG_ERROR("failed to get atribute metadata %s %d",
                    sai_serialize_object_type(objectType).c_str(),
                    attr->id);

            continue;
        }

        SWSS_LOG_ERROR("%s = %s", meta->attridname, sai_serialize_attr_value(*meta, *attr).c_str());
    }
}

sai_object_id_t processSingleVid(
        _In_ sai_object_id_t vid)
{
    SWSS_LOG_ENTER();

    if (vid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_DEBUG("processed VID 0 to RID 0");

        return SAI_NULL_OBJECT_ID;
    }

    auto it = g_translatedV2R.find(vid);

    if (it != g_translatedV2R.end())
    {
        /*
         * This object was already processed, just return real object id.
         */

        SWSS_LOG_DEBUG("processed VID %s to RID %s",
                sai_serialize_object_id(vid).c_str(),
                sai_serialize_object_id(it->second).c_str());

        return it->second;
    }

    sai_object_type_t objectType = getObjectTypeFromVid(vid);

    std::string strVid = sai_serialize_object_id(vid);

    auto oit = g_oids.find(strVid);

    if (oit == g_oids.end())
    {
        SWSS_LOG_THROW("failed to find VID %s in OIDs map", strVid.c_str());
    }

    std::string asicKey = oit->second;;

    std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

    sai_attribute_t *attrList = list->get_attr_list();

    uint32_t attrCount = list->get_attr_count();

    processAttributesForOids(objectType, attrCount, attrList);

    bool createObject = true;

    /*
     * Now let's determine whether this object need to be created.  Default
     * obejcts like default virtual router, queues or cpu can't be created.
     * When object exists on the switch (even VLAN member) it will not be
     * created, but matched. We just need to watch for RO/CO attributes.
     *
     * NOTE: this also should be per switch.
     */

    auto v2rMapIt = g_vidToRidMap.find(vid);

    if (v2rMapIt == g_vidToRidMap.end())
    {
        SWSS_LOG_THROW("failed to find VID %s in VIDTORID map",
                sai_serialize_object_id(vid).c_str());
    }

    sai_object_id_t rid;

    if (g_sw->isDefaultCreatedRid(v2rMapIt->second))
    {
        rid = v2rMapIt->second;

        createObject = false;

        SWSS_LOG_DEBUG("object %s will not be created, processed VID %s to RID %s",
                sai_serialize_object_type(objectType).c_str(),
                sai_serialize_object_id(vid).c_str(),
                sai_serialize_object_id(rid).c_str());
    }

    if (objectType == SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP)
    {
        /*
         * We need special case for trap group, look inside for details.
         */

        trapGroupWorkaround(vid, rid, createObject, attrCount, attrList);
    }

    auto info = sai_metadata_get_object_type_info(objectType);

    if (createObject)
    {
        sai_object_meta_key_t meta_key;

        meta_key.objecttype = objectType;

        /*
         * Since we have only one switch, we can get away using g_switch_rid here.
         */

#ifdef ENABLE_PERF
        auto start = std::chrono::high_resolution_clock::now();
#endif

        sai_status_t status = info->create(&meta_key, g_switch_rid, attrCount, attrList);

#ifdef ENABLE_PERF
        auto end = std::chrono::high_resolution_clock::now();

        typedef std::chrono::duration<double, std::ratio<1>> second_t;

        double duration = std::chrono::duration_cast<second_t>(end - start).count();

        std::get<0>(g_perf_create[objectType])++;
        std::get<1>(g_perf_create[objectType]) += duration;
#endif

        if (status != SAI_STATUS_SUCCESS)
        {
            listFailedAttributes(objectType, attrCount, attrList);

            SWSS_LOG_THROW("failed to create object %s: %s",
                    sai_serialize_object_type(objectType).c_str(),
                    sai_serialize_status(status).c_str());
        }

        rid = meta_key.objectkey.key.object_id;

        SWSS_LOG_DEBUG("created object of type %s, processed VID %s to RID %s",
                sai_serialize_object_type(objectType).c_str(),
                sai_serialize_object_id(vid).c_str(),
                sai_serialize_object_id(rid).c_str());
    }
    else
    {
        SWSS_LOG_DEBUG("setting attributes on object of type %x, processed VID 0x%lx to RID 0x%lx", objectType, vid, rid);

        for (uint32_t idx = 0; idx < attrCount; idx++)
        {
            sai_attribute_t *attr = &attrList[idx];

            sai_object_meta_key_t meta_key;

            meta_key.objecttype = objectType;
            meta_key.objectkey.key.object_id = rid;

            auto meta = sai_metadata_get_attr_metadata(objectType, attr->id);

            if (meta == NULL)
            {
                SWSS_LOG_THROW("failed to get atribute metadata %s: %d",
                        sai_serialize_object_type(objectType).c_str(),
                        attr->id);
            }

            // XXX workaround
            if (meta->objecttype == SAI_OBJECT_TYPE_SWITCH &&
                    attr->id == SAI_SWITCH_ATTR_SRC_MAC_ADDRESS)
            {
                SWSS_LOG_WARN("skipping to set MAC addres since not supported on mlnx 2700");
                continue;
            }

            if (SAI_HAS_FLAG_CREATE_ONLY(meta->flags))
            {
                /*
                 * If we will be performing this on default existing created
                 * object then it may happen taht during snoop in previous
                 * iteration we put some attribute that is create only, then
                 * this set will fail and we need to skip this set.
                 *
                 * NOTE: We could do get here to see if it actually matches.
                 */

                if (g_sw->isDefaultCreatedRid(rid))
                {
                    continue;
                }

                SWSS_LOG_WARN("skipping create only attr %s: %s",
                        meta->attridname,
                        sai_serialize_attr_value(*meta, *attr).c_str());

                continue;
            }

#ifdef ENABLE_PERF
            auto start = std::chrono::high_resolution_clock::now();
#endif

            sai_status_t status = info->set(&meta_key, attr);

#ifdef ENABLE_PERF
            auto end = std::chrono::high_resolution_clock::now();

            typedef std::chrono::duration<double, std::ratio<1>> second_t;

            double duration = std::chrono::duration_cast<second_t>(end - start).count();

            std::get<0>(g_perf_set[objectType])++;
            std::get<1>(g_perf_set[objectType]) += duration;
#endif

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_THROW(
                        "failed to set %s value %s: %s",
                        meta->attridname,
                        sai_serialize_attr_value(*meta, *attr).c_str(),
                        sai_serialize_status(status).c_str());
            }
        }
    }

    g_translatedV2R[vid] = rid;
    g_translatedR2V[rid] = vid;

    return rid;
}

void processAttributesForOids(
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("processing list for object type %s",
            sai_serialize_object_type(objectType).c_str());

    for (uint32_t idx = 0; idx < attr_count; idx++)
    {
        sai_attribute_t &attr = attr_list[idx];

        auto meta = sai_metadata_get_attr_metadata(objectType, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
                    sai_serialize_object_type(objectType).c_str(),
                    attr.id);
        }

        uint32_t count = 0;
        sai_object_id_t *objectIdList;

        switch (meta->attrvaluetype)
        {
            case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
                count = 1;
                objectIdList = &attr.value.oid;
                break;

            case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
                count = attr.value.objlist.count;
                objectIdList = attr.value.objlist.list;
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                if (attr.value.aclfield.enable)
                {
                    count = 1;
                    objectIdList = &attr.value.aclfield.data.oid;
                }
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                if (attr.value.aclfield.enable)
                {
                    count = attr.value.aclfield.data.objlist.count;
                    objectIdList = attr.value.aclfield.data.objlist.list;
                }
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                if (attr.value.aclaction.enable)
                {
                    count = 1;
                    objectIdList = &attr.value.aclaction.parameter.oid;
                }
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                if (attr.value.aclaction.enable)
                {
                    count = attr.value.aclaction.parameter.objlist.count;
                    objectIdList = attr.value.aclaction.parameter.objlist.list;
                }
                break;

            default:

                // TODO later isoidattribute
                if (meta->allowedobjecttypeslength > 0)
                {
                    SWSS_LOG_THROW("attribute %s is oid attribute, but not processed, FIXME", meta->attridname);
                }

                /*
                 * This is not oid attribute, we can skip processing.
                 */

                continue;
        }

        /*
         * Attribute contains object id's, they need to be translated some of
         * them could be already translated.
         */

        for (uint32_t j = 0; j < count; j++)
        {
            sai_object_id_t vid = objectIdList[j];

            sai_object_id_t rid = processSingleVid(vid);

            objectIdList[j] = rid;
        }
    }
}

void processOids()
{
    SWSS_LOG_ENTER();

    for (const auto &kv: g_oids)
    {
        const std::string &strObjectId = kv.first;

        sai_object_id_t vid;
        sai_deserialize_object_id(strObjectId, vid);

        processSingleVid(vid);
    }
}

void processStructNonObjectIds(
        _In_ sai_object_meta_key_t &meta_key)
{
    SWSS_LOG_ENTER();

    auto info = sai_metadata_get_object_type_info(meta_key.objecttype);

    /*
     * Call processSingleVid method for each oid in non object id (struct
     * entry) in generic way.
     */

    if (info->isnonobjectid)
    {
        for (size_t j = 0; j < info->structmemberscount; ++j)
        {
            const sai_struct_member_info_t *m = info->structmembers[j];

            if (m->membervaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID)
            {
                continue;
            }

            sai_object_id_t vid = m->getoid(&meta_key);

            sai_object_id_t rid = processSingleVid(vid);

            m->setoid(&meta_key, rid);

            SWSS_LOG_DEBUG("processed vid 0x%lx to rid 0x%lx in %s:%s", vid, rid,
                    info->objecttypename,
                    m->membername);
        }
    }
}

void processFdbs()
{
    SWSS_LOG_ENTER();

    for (auto &kv: g_fdbs)
    {
        const std::string &strFdbEntry = kv.first;
        const std::string &asicKey = kv.second;

        sai_object_meta_key_t meta_key;

        meta_key.objecttype = SAI_OBJECT_TYPE_FDB_ENTRY;

        sai_deserialize_fdb_entry(strFdbEntry, meta_key.objectkey.key.fdb_entry);

        processStructNonObjectIds(meta_key);

        std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        processAttributesForOids(SAI_OBJECT_TYPE_FDB_ENTRY, attrCount, attrList);

        sai_status_t status = sai_metadata_sai_fdb_api->
            create_fdb_entry(&meta_key.objectkey.key.fdb_entry, attrCount, attrList);

        if (status != SAI_STATUS_SUCCESS)
        {
            listFailedAttributes(SAI_OBJECT_TYPE_FDB_ENTRY, attrCount, attrList);

            SWSS_LOG_THROW("failed to create_fdb_entry %s: %s",
                    strFdbEntry.c_str(),
                    sai_serialize_status(status).c_str());
        }
    }
}

void processNeighbors()
{
    SWSS_LOG_ENTER();

    for (auto &kv: g_neighbors)
    {
        const std::string &strNeighborEntry = kv.first;
        const std::string &asicKey = kv.second;

        sai_object_meta_key_t meta_key;

        meta_key.objecttype = SAI_OBJECT_TYPE_NEIGHBOR_ENTRY;

        sai_deserialize_neighbor_entry(strNeighborEntry, meta_key.objectkey.key.neighbor_entry);

        processStructNonObjectIds(meta_key);

        std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        processAttributesForOids(SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, attrCount, attrList);

        sai_status_t status = sai_metadata_sai_neighbor_api->
            create_neighbor_entry(&meta_key.objectkey.key.neighbor_entry, attrCount, attrList);

        if (status != SAI_STATUS_SUCCESS)
        {
            listFailedAttributes(SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, attrCount, attrList);

            SWSS_LOG_THROW("failed to create_neighbor_entry %s: %s",
                    strNeighborEntry.c_str(),
                    sai_serialize_status(status).c_str());
        }
    }
}

void processRoutes(bool defaultOnly)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("apply routes");

    for (auto &kv: g_routes)
    {
        const std::string &strRouteEntry = kv.first;
        const std::string &asicKey = kv.second;

        bool isDefault = strRouteEntry.find("/0") != std::string::npos;

        if (defaultOnly ^ isDefault)
        {
            /*
             * Since there is a requirement in brcm that default route needs to
             * be put first in the asic, then we execute default routes first
             * and then other routes.
             */

            continue;
        }

        sai_object_meta_key_t meta_key;

        meta_key.objecttype = SAI_OBJECT_TYPE_ROUTE_ENTRY;

        sai_deserialize_route_entry(strRouteEntry, meta_key.objectkey.key.route_entry);

        processStructNonObjectIds(meta_key);

        std::shared_ptr<SaiAttributeList> list = g_attributesLists[asicKey];

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        processAttributesForOids(SAI_OBJECT_TYPE_ROUTE_ENTRY, attrCount, attrList);

        sai_status_t status = sai_metadata_sai_route_api->
            create_route_entry(&meta_key.objectkey.key.route_entry, attrCount, attrList);

        if (status != SAI_STATUS_SUCCESS)
        {
            listFailedAttributes(SAI_OBJECT_TYPE_ROUTE_ENTRY, attrCount, attrList);

            SWSS_LOG_ERROR("translated route: %s",
                    sai_serialize_route_entry(meta_key.objectkey.key.route_entry).c_str());

            SWSS_LOG_THROW(
                    "failed to create ROUTE %s: %s",
                    strRouteEntry.c_str(),
                    sai_serialize_status(status).c_str());
        }
    }
}

std::vector<std::string> redisGetAsicStateKeys()
{
    SWSS_LOG_ENTER();

    return g_redisClient->keys(ASIC_STATE_TABLE + std::string(":*"));
}

void readAsicState()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("read asic state");

    /*
     * Repopulate asic view from redis db after hard asic initialize.
     */

    /*
     * To support multiple switchies this needs to be refactores since we need
     * a class with hard reinit, per switch
     */

    g_vidToRidMap = redisGetVidToRidMap();
    g_ridToVidMap = redisGetRidToVidMap();

    std::vector<std::string> asicStateKeys = redisGetAsicStateKeys();

    for (const auto &key: asicStateKeys)
    {
        /*
         * TODO if key will be meta_key anyway we could use deserialize here.
         */

        sai_object_type_t objectType = getObjectTypeFromAsicKey(key);
        const std::string &strObjectId = getObjectIdFromAsicKey(key);

        auto info = sai_metadata_get_object_type_info(objectType);

        switch (objectType)
        {
            case SAI_OBJECT_TYPE_ROUTE_ENTRY:
                g_routes[strObjectId] = key;
                break;

            case SAI_OBJECT_TYPE_FDB_ENTRY:
                g_fdbs[strObjectId] = key;
                break;

            case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
                g_neighbors[strObjectId] = key;
                break;

            case SAI_OBJECT_TYPE_SWITCH:
                g_switches[strObjectId] = key;
                g_oids[strObjectId] = key;
                break;

            default:

                if (info->isnonobjectid)
                {
                    SWSS_LOG_THROW("passing non object id %s as generic object", info->objecttypename);
                }

                g_oids[strObjectId] = key;
                break;
        }

        g_attributesLists[key] = redisGetAttributesFromAsicKey(key);
    }


}

void hardReinit()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("hard reinit");

    readAsicState();

    processSwitches();

    {
        SWSS_LOG_TIMER("processing objects after switch create");

        processFdbs();
        processNeighbors();
        processOids();
        processRoutes(true);
        processRoutes(false);
    }

#ifdef ENABLE_PERF

    double total_create = 0;
    double total_set = 0;

    for (const auto &p: g_perf_create)
    {
        SWSS_LOG_NOTICE("create %s: %d: %f",
                sai_serialize_object_type(p.first).c_str(),
                std::get<0>(p.second),
                std::get<1>(p.second));

        total_create += std::get<1>(p.second);
    }

    for (const auto &p: g_perf_set)
    {
        SWSS_LOG_NOTICE("set %s: %d: %f",
                sai_serialize_object_type(p.first).c_str(),
                std::get<0>(p.second),
                std::get<1>(p.second));

        total_set += std::get<1>(p.second);
    }

    SWSS_LOG_NOTICE("create %lf, set: %lf", total_create, total_set);
#endif

    checkAllIds();
}

void performWarmRestart()
{
    SWSS_LOG_ENTER();

    /*
     * There should be no case when we are doing warm restart and there is no
     * switch defined, we will throw at sucha case.
     *
     * This case could be possible when no switches were created and only api
     * was initialized, but we will skip this scenario and address is when we
     * will have need for it.
     */

    auto entries = g_redisClient->keys(ASIC_STATE_TABLE + std::string(":SAI_OBJECT_TYPE_SWITCH:*"));

    if (entries.size() == 0)
    {
        SWSS_LOG_THROW("on warm restart there is no switches defined in DB, not supported yet, FIXME");
    }

    if (entries.size() != 1)
    {
        SWSS_LOG_THROW("multiple switches defined in warm start: %zu, not supported yet, FIXME", entries.size());
    }

    /*
     * Here wa have only one switch defined, let's extract his vid and rid.
     */

    /*
     * Entry should be in format ASIC_STATE:SAI_OBJECT_TYPE_SWITCH:oid:0xYYYY
     *
     * Let's extract oid value
     */

    std::string key = entries.at(0);

    auto start = key.find_first_of(":") + 1;
    auto end = key.find(":", start);

    std::string strSwitchVid = key.substr(end + 1);

    sai_object_id_t switch_vid;

    sai_deserialize_object_id(strSwitchVid, switch_vid);

    sai_object_id_t orig_rid = translate_vid_to_rid(switch_vid);

    sai_object_id_t switch_rid;
    sai_attribute_t switch_attr;
    switch_attr.id             = SAI_SWITCH_ATTR_INIT_SWITCH;
    switch_attr.value.booldata = true;
    sai_status_t status = sai_metadata_sai_switch_api->create_switch(&switch_rid, 1, &switch_attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("failed to create switch RID: %s",
                       sai_serialize_status(status).c_str());
    }
    if (orig_rid != switch_rid)
    {
        SWSS_LOG_THROW("Unexpected RID 0x%lx (expected 0x%lx)",
                       switch_rid, orig_rid);
    }

    g_translatedV2R[switch_vid] = switch_rid;
    g_translatedR2V[switch_rid] = switch_vid;

    /*
     * Perform all get operations on existing switch.
     */

    auto sw = switches[switch_vid] = std::make_shared<SaiSwitch>(switch_vid, switch_rid);

    g_switch_rid = switch_rid;
    g_switch_vid = switch_vid;

    g_sw = sw;

    startDiagShell();
}
