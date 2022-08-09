#include "SingleReiniter.h"
#include "VidManager.h"
#include "CommandLineOptions.h"
#include "NotificationHandler.h"
#include "Workaround.h"
#include "RedisClient.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"

#include <unistd.h>
#include <inttypes.h>

using namespace syncd;
using namespace saimeta;

SingleReiniter::SingleReiniter(
        _In_ std::shared_ptr<RedisClient> client,
        _In_ std::shared_ptr<VirtualOidTranslator> translator,
        _In_ std::shared_ptr<sairedis::SaiInterface> sai,
        _In_ std::shared_ptr<NotificationHandler> handler,
        _In_ const ObjectIdMap& vidToRidMap,
        _In_ const ObjectIdMap& ridToVidMap,
        _In_ const std::vector<std::string>& asicKeys):
    m_vendorSai(sai),
    m_vidToRidMap(vidToRidMap),
    m_ridToVidMap(ridToVidMap),
    m_asicKeys(asicKeys),
    m_translator(translator),
    m_client(client),
    m_handler(handler)
{
    SWSS_LOG_ENTER();

    m_switch_rid = SAI_NULL_OBJECT_ID;
    m_switch_vid = SAI_NULL_OBJECT_ID;
}

SingleReiniter::~SingleReiniter()
{
    SWSS_LOG_ENTER();

    // empty
}

std::shared_ptr<SaiSwitch> SingleReiniter::hardReinit()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("hard reinit");

    prepareAsicState();

    processSwitches();
    processFdbs();
    processNeighbors();
    processOids();
    processRoutes(true);
    processRoutes(false);
    processInsegs();
    processNatEntries();

#ifdef ENABLE_PERF

    double total_create = 0;
    double total_set = 0;

    for (const auto &p: m_perf_create)
    {
        SWSS_LOG_NOTICE("create %s: %d: %f",
                sai_serialize_object_type(p.first).c_str(),
                std::get<0>(p.second),
                std::get<1>(p.second));

        total_create += std::get<1>(p.second);
    }

    for (const auto &p: m_perf_set)
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

    return m_sw;
}

void SingleReiniter::prepareAsicState()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("read asic state");

    for (auto& key: m_asicKeys)
    {
        sai_object_type_t objectType = getObjectTypeFromAsicKey(key);

        const std::string &strObjectId = getObjectIdFromAsicKey(key);

        auto info = sai_metadata_get_object_type_info(objectType);

        switch (objectType)
        {
            case SAI_OBJECT_TYPE_ROUTE_ENTRY:
                m_routes[strObjectId] = key;
                break;

            case SAI_OBJECT_TYPE_FDB_ENTRY:
                m_fdbs[strObjectId] = key;
                break;

            case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
                m_neighbors[strObjectId] = key;
                break;

            case SAI_OBJECT_TYPE_NAT_ENTRY:
                m_nats[strObjectId] = key;
                break;

            case SAI_OBJECT_TYPE_SWITCH:
                m_switches[strObjectId] = key;
                m_oids[strObjectId] = key;
                break;

            default:

                if (info->isnonobjectid)
                {
                    SWSS_LOG_THROW("passing non object id %s as generic object", info->objecttypename);
                }

                m_oids[strObjectId] = key;
                break;
        }

        m_attributesLists[key] = redisGetAttributesFromAsicKey(key);
    }
}

sai_object_type_t SingleReiniter::getObjectTypeFromAsicKey(
        _In_ const std::string &key)
{
    SWSS_LOG_ENTER();

    auto start = key.find_first_of(":") + 1;
    auto end = key.find(":", start);

    const std::string strObjectType = key.substr(start, end - start);

    sai_object_type_t objectType;
    sai_deserialize_object_type(strObjectType, objectType);

    if (!sai_metadata_is_object_type_valid(objectType))
    {
        SWSS_LOG_THROW("invalid object type: %s on asic key: %s",
                sai_serialize_object_type(objectType).c_str(),
                key.c_str());
    }

    return objectType;
}

std::string SingleReiniter::getObjectIdFromAsicKey(
        _In_ const std::string &key)
{
    SWSS_LOG_ENTER();

    auto start = key.find_first_of(":") + 1;
    auto end = key.find(":", start);

    return key.substr(end + 1);
}

void SingleReiniter::processSwitches()
{
    SWSS_LOG_ENTER();

    /*
     * If there are any switches, we need to create them first to perform any
     * other operations.
     *
     * NOTE: This method needs to be revisited if we want to support multiple
     * switches.
     */

    if (m_switches.size() > 1)
    {
        SWSS_LOG_THROW("multiple switches %zu in single hard reinit are not allowed", m_switches.size());
    }

    /*
     * Sanity check in metadata make sure that there are no mandatory on create
     * and create only attributes that are object id attributes, since we would
     * need create those objects first but we need switch first. So here we
     * selecting only MANDATORY_ON_CREATE and CREATE_ONLY attributes to create
     * switch.
     */

    for (const auto& s: m_switches)
    {
        std::string strSwitchVid = s.first;
        std::string asicKey = s.second;

        sai_deserialize_object_id(strSwitchVid, m_switch_vid);

        if (m_switch_vid == SAI_NULL_OBJECT_ID)
        {
            SWSS_LOG_THROW("switch id can't be NULL");
        }

        auto oit = m_oids.find(strSwitchVid);

        if (oit == m_oids.end())
        {
            SWSS_LOG_THROW("failed to find VID %s in OIDs map", strSwitchVid.c_str());
        }

        std::shared_ptr<SaiAttributeList> list = m_attributesLists[asicKey];

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        /*
         * If any of those attributes are pointers, fix them, so they will
         * point to callbacks in syncd memory.
         */

        m_handler->updateNotificationsPointers(attrCount, attrList); // TODO need per switch template static

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

        SWSS_LOG_NOTICE("creating switch VID: %s",
                sai_serialize_object_id(m_switch_vid).c_str());

        sai_status_t status;

        {
            SWSS_LOG_TIMER("Cold boot: create switch");
            status = m_vendorSai->create(SAI_OBJECT_TYPE_SWITCH, &m_switch_rid, 0, attr_count, attr_list);
        }

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_THROW("failed to create switch RID: %s",
                    sai_serialize_status(status).c_str());
        }

        SWSS_LOG_NOTICE("created switch RID: %s",
                sai_serialize_object_id(m_switch_rid).c_str());
        /*
         * Save this switch ids as translated.
         */

        m_translatedV2R[m_switch_vid] = m_switch_rid;
        m_translatedR2V[m_switch_rid] = m_switch_vid;

        /*
         * SaiSwitch class object must be created before before any other
         * object, so when doing discover we will get full default ASIC view.
         */

        m_sw = std::make_shared<SaiSwitch>(m_switch_vid, m_switch_rid, m_client, m_translator, m_vendorSai);

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

            status = m_vendorSai->set(SAI_OBJECT_TYPE_SWITCH, m_switch_rid, attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                if (Workaround::isSetAttributeWorkaround(SAI_OBJECT_TYPE_SWITCH, attr->id, status))
                {
                    continue;
                }

                SWSS_LOG_THROW("failed to set attribute %s on switch VID %s: %s",
                        sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr->id)->attridname,
                        sai_serialize_object_id(m_switch_rid).c_str(),
                        sai_serialize_status(status).c_str());
            }
        }
    }
}

void SingleReiniter::processFdbs()
{
    SWSS_LOG_ENTER();

    for (auto &kv: m_fdbs)
    {
        const std::string &strFdbEntry = kv.first;
        const std::string &asicKey = kv.second;

        sai_object_meta_key_t meta_key;

        meta_key.objecttype = SAI_OBJECT_TYPE_FDB_ENTRY;

        sai_deserialize_fdb_entry(strFdbEntry, meta_key.objectkey.key.fdb_entry);

        processStructNonObjectIds(meta_key);

        std::shared_ptr<SaiAttributeList> list = m_attributesLists[asicKey];

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        processAttributesForOids(SAI_OBJECT_TYPE_FDB_ENTRY, attrCount, attrList);

        sai_status_t status = m_vendorSai->create(&meta_key.objectkey.key.fdb_entry, attrCount, attrList);

        if (status != SAI_STATUS_SUCCESS)
        {
            listFailedAttributes(SAI_OBJECT_TYPE_FDB_ENTRY, attrCount, attrList);

            SWSS_LOG_THROW("failed to create_fdb_entry %s: %s",
                    strFdbEntry.c_str(),
                    sai_serialize_status(status).c_str());
        }
    }
}

void SingleReiniter::processNeighbors()
{
    SWSS_LOG_ENTER();

    for (auto &kv: m_neighbors)
    {
        const std::string &strNeighborEntry = kv.first;
        const std::string &asicKey = kv.second;

        sai_object_meta_key_t meta_key;

        meta_key.objecttype = SAI_OBJECT_TYPE_NEIGHBOR_ENTRY;

        sai_deserialize_neighbor_entry(strNeighborEntry, meta_key.objectkey.key.neighbor_entry);

        processStructNonObjectIds(meta_key);

        std::shared_ptr<SaiAttributeList> list = m_attributesLists[asicKey];

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        processAttributesForOids(SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, attrCount, attrList);

        sai_status_t status = m_vendorSai->create(&meta_key.objectkey.key.neighbor_entry, attrCount, attrList);

        if (status != SAI_STATUS_SUCCESS)
        {
            listFailedAttributes(SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, attrCount, attrList);

            SWSS_LOG_THROW("failed to create_neighbor_entry %s: %s",
                    strNeighborEntry.c_str(),
                    sai_serialize_status(status).c_str());
        }
    }
}

void SingleReiniter::processRoutes(
        _In_ bool defaultOnly)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("apply routes");

    for (auto &kv: m_routes)
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

        std::shared_ptr<SaiAttributeList> list = m_attributesLists[asicKey];

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        processAttributesForOids(SAI_OBJECT_TYPE_ROUTE_ENTRY, attrCount, attrList);

        sai_status_t status = m_vendorSai->create(&meta_key.objectkey.key.route_entry, attrCount, attrList);

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

void SingleReiniter::processInsegs()
{
    SWSS_LOG_ENTER();

    for (auto &kv: m_insegs)
    {
        const std::string &strInsegEntry = kv.first;
        const std::string &asicKey = kv.second;

        sai_object_meta_key_t meta_key;

        meta_key.objecttype = SAI_OBJECT_TYPE_INSEG_ENTRY;

        sai_deserialize_inseg_entry(strInsegEntry, meta_key.objectkey.key.inseg_entry);

        processStructNonObjectIds(meta_key);

        std::shared_ptr<SaiAttributeList> list = m_attributesLists[asicKey];

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        processAttributesForOids(SAI_OBJECT_TYPE_INSEG_ENTRY, attrCount, attrList);

        sai_status_t status = sai_metadata_sai_mpls_api->
            create_inseg_entry(&meta_key.objectkey.key.inseg_entry, attrCount, attrList);

        if (status != SAI_STATUS_SUCCESS)
        {
            listFailedAttributes(SAI_OBJECT_TYPE_INSEG_ENTRY, attrCount, attrList);

            SWSS_LOG_THROW("failed to create_inseg_entry %s: %s",
                    strInsegEntry.c_str(),
                    sai_serialize_status(status).c_str());
        }
    }
}

void SingleReiniter::processNatEntries()
{
    SWSS_LOG_ENTER();

    for (auto &kv: m_nats)
    {
        const std::string &strNatEntry = kv.first;
        const std::string &asicKey = kv.second;

        sai_object_meta_key_t meta_key;

        meta_key.objecttype = SAI_OBJECT_TYPE_NAT_ENTRY;

        sai_deserialize_nat_entry(strNatEntry, meta_key.objectkey.key.nat_entry);

        processStructNonObjectIds(meta_key);

        std::shared_ptr<SaiAttributeList> list = m_attributesLists[asicKey];

        sai_attribute_t *attrList = list->get_attr_list();

        uint32_t attrCount = list->get_attr_count();

        processAttributesForOids(SAI_OBJECT_TYPE_NAT_ENTRY, attrCount, attrList);

        sai_status_t status = m_vendorSai->create(&meta_key.objectkey.key.nat_entry, attrCount, attrList);

        if (status != SAI_STATUS_SUCCESS)
        {
            listFailedAttributes(SAI_OBJECT_TYPE_NAT_ENTRY, attrCount, attrList);

            SWSS_LOG_THROW("failed to create_nat_entry %s: %s",
                    strNatEntry.c_str(),
                    sai_serialize_status(status).c_str());
        }
    }
}

void SingleReiniter::trapGroupWorkaround(
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

    sai_status_t status = m_vendorSai->create(SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP, &rid, m_switch_rid, 1, queue_attr);

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

void SingleReiniter::listFailedAttributes(
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
            SWSS_LOG_ERROR("failed to get attribute metadata %s %d",
                    sai_serialize_object_type(objectType).c_str(),
                    attr->id);

            continue;
        }

        SWSS_LOG_ERROR("%s = %s", meta->attridname, sai_serialize_attr_value(*meta, *attr).c_str());
    }
}

sai_object_id_t SingleReiniter::processSingleVid(
        _In_ sai_object_id_t vid)
{
    SWSS_LOG_ENTER();

    if (vid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_DEBUG("processed VID 0 to RID 0");

        return SAI_NULL_OBJECT_ID;
    }

    auto it = m_translatedV2R.find(vid);

    if (it != m_translatedV2R.end())
    {
        /*
         * This object was already processed, just return real object id.
         */

        SWSS_LOG_DEBUG("processed VID %s to RID %s",
                sai_serialize_object_id(vid).c_str(),
                sai_serialize_object_id(it->second).c_str());

        return it->second;
    }

    sai_object_type_t objectType = VidManager::objectTypeQuery(vid);

    std::string strVid = sai_serialize_object_id(vid);

    auto oit = m_oids.find(strVid);

    if (oit == m_oids.end())
    {
        SWSS_LOG_THROW("failed to find VID %s in OIDs map", strVid.c_str());
    }

    std::string asicKey = oit->second;;

    std::shared_ptr<SaiAttributeList> list = m_attributesLists[asicKey];

    sai_attribute_t *attrList = list->get_attr_list();

    uint32_t attrCount = list->get_attr_count();

    processAttributesForOids(objectType, attrCount, attrList);

    bool createObject = true;

    /*
     * Now let's determine whether this object need to be created.  Default
     * objects like default virtual router, queues or cpu can't be created.
     * When object exists on the switch (even VLAN member) it will not be
     * created, but matched. We just need to watch for RO/CO attributes.
     *
     * NOTE: this also should be per switch.
     */

    auto v2rMapIt = m_vidToRidMap.find(vid);

    if (v2rMapIt == m_vidToRidMap.end())
    {
        SWSS_LOG_THROW("failed to find VID %s in VIDTORID map",
                sai_serialize_object_id(vid).c_str());
    }

    sai_object_id_t rid;

    if (m_sw->isDiscoveredRid(v2rMapIt->second))
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

    if (createObject)
    {
        sai_object_meta_key_t meta_key;

        meta_key.objecttype = objectType;

        /*
         * Since we have only one switch, we can get away using m_switch_rid here.
         */

#ifdef ENABLE_PERF
        auto start = std::chrono::high_resolution_clock::now();
#endif

        sai_status_t status = m_vendorSai->create(meta_key.objecttype, &meta_key.objectkey.key.object_id, m_switch_rid, attrCount, attrList);

#ifdef ENABLE_PERF
        auto end = std::chrono::high_resolution_clock::now();

        typedef std::chrono::duration<double, std::ratio<1>> second_t;

        double duration = std::chrono::duration_cast<second_t>(end - start).count();

        std::get<0>(m_perf_create[objectType])++;
        std::get<1>(m_perf_create[objectType]) += duration;
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
        SWSS_LOG_DEBUG("setting attributes on object of type %x, processed VID 0x%" PRIx64 " to RID 0x%" PRIx64 " ", objectType, vid, rid);

        for (uint32_t idx = 0; idx < attrCount; idx++)
        {
            sai_attribute_t *attr = &attrList[idx];

            sai_object_meta_key_t meta_key;

            meta_key.objecttype = objectType;
            meta_key.objectkey.key.object_id = rid;

            auto meta = sai_metadata_get_attr_metadata(objectType, attr->id);

            if (meta == NULL)
            {
                SWSS_LOG_THROW("failed to get attribute metadata %s: %d",
                        sai_serialize_object_type(objectType).c_str(),
                        attr->id);
            }

            // XXX workaround
            if (meta->objecttype == SAI_OBJECT_TYPE_SWITCH &&
                    attr->id == SAI_SWITCH_ATTR_SRC_MAC_ADDRESS)
            {
                SWSS_LOG_WARN("skipping to set MAC address since not supported on Mellanox platforms");
                continue;
            }

            if (SAI_HAS_FLAG_CREATE_ONLY(meta->flags))
            {
                /*
                 * If we will be performing this on default existing created
                 * object then it may happen that during snoop in previous
                 * iteration we put some attribute that is create only, then
                 * this set will fail and we need to skip this set.
                 *
                 * NOTE: We could do get here to see if it actually matches.
                 */

                if (m_sw->isDiscoveredRid(rid))
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

            sai_status_t status = m_vendorSai->set(meta_key.objecttype, meta_key.objectkey.key.object_id, attr);

#ifdef ENABLE_PERF
            auto end = std::chrono::high_resolution_clock::now();

            typedef std::chrono::duration<double, std::ratio<1>> second_t;

            double duration = std::chrono::duration_cast<second_t>(end - start).count();

            std::get<0>(m_perf_set[objectType])++;
            std::get<1>(m_perf_set[objectType]) += duration;
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

    m_translatedV2R[vid] = rid;
    m_translatedR2V[rid] = vid;

    return rid;
}

void SingleReiniter::processAttributesForOids(
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

void SingleReiniter::processOids()
{
    SWSS_LOG_ENTER();

    for (const auto &kv: m_oids)
    {
        const std::string &strObjectId = kv.first;

        sai_object_id_t vid;
        sai_deserialize_object_id(strObjectId, vid);

        processSingleVid(vid);
    }
}

void SingleReiniter::processStructNonObjectIds(
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

            SWSS_LOG_DEBUG("processed vid 0x%" PRIx64 " to rid 0x%" PRIx64 " in %s:%s", vid, rid,
                    info->objecttypename,
                    m->membername);
        }
    }
}

void SingleReiniter::checkAllIds()
{
    SWSS_LOG_ENTER();

    for (auto &kv: m_translatedV2R)
    {
        auto it = m_vidToRidMap.find(kv.first);

        if (it == m_vidToRidMap.end())
        {
            SWSS_LOG_THROW("failed to find vid %s in previous map",
                    sai_serialize_object_id(kv.first).c_str());
        }

        m_vidToRidMap.erase(it);
    }

    size_t size = m_vidToRidMap.size();

    if (size != 0)
    {
        for (auto &kv: m_vidToRidMap)
        {
            sai_object_type_t objectType = VidManager::objectTypeQuery(kv.first);

            SWSS_LOG_ERROR("vid not translated: %s, object type: %s",
                    sai_serialize_object_id(kv.first).c_str(),
                    sai_serialize_object_type(objectType).c_str());
        }

        SWSS_LOG_THROW("vid to rid map is not empty (%zu) after translation", size);
    }
}

SingleReiniter::ObjectIdMap SingleReiniter::getTranslatedVid2Rid() const
{
    SWSS_LOG_ENTER();

    return m_translatedV2R;
}

void SingleReiniter::postRemoveActions()
{
    SWSS_LOG_ENTER();

    /*
     * Now we must check whether we need to remove some objects like VLAN
     * members etc.
     *
     * TODO: Should this be done at start, before other operations?
     * We are able to determine which objects are missing from rid map
     * as long as id's between restart don't change.
     */

    if (m_sw == nullptr)
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
     * We could just call discover again, but that's too long, we just need to
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
        for (sai_object_id_t rid: m_sw->getDiscoveredRids())
        {
            if (m_translatedR2V.find(rid) != m_translatedR2V.end())
            {
                continue;
            }

            if (ot == m_vendorSai->objectTypeQuery(rid) ||
                    ot == SAI_OBJECT_TYPE_NULL)
            {
                m_sw->removeExistingObject(rid);

                /*
                 * If removing existing object, also make sure we remove it
                 * from COLDVIDS map since this object will no longer exists.
                 */

                if (m_ridToVidMap.find(rid) == m_ridToVidMap.end())
                    continue;

                auto vid = m_ridToVidMap.at(rid);

                SWSS_LOG_INFO("removing existing cold VID: %s",
                        sai_serialize_object_id(vid).c_str());

                m_client->removeColdVid(vid);
            }
        }
    }
}

std::shared_ptr<SaiAttributeList> SingleReiniter::redisGetAttributesFromAsicKey(
        _In_ const std::string &key)
{
    SWSS_LOG_ENTER();

    sai_object_type_t objectType = getObjectTypeFromAsicKey(key);

    std::vector<swss::FieldValueTuple> values;

    auto hash = m_client->getAttributesFromAsicKey(key);

    for (auto &kv: hash)
    {
        const std::string &skey = kv.first;
        const std::string &svalue = kv.second;

        swss::FieldValueTuple fvt(skey, svalue);

        values.push_back(fvt);
    }

    return std::make_shared<SaiAttributeList>(objectType, values, false);
}

std::shared_ptr<SaiSwitch> SingleReiniter::getSwitch() const
{
    SWSS_LOG_ENTER();

    return m_sw;
}
