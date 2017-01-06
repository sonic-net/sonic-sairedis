#include <iostream>
#include "syncd.h"
#include "sairedis.h"
#include "swss/table.h"
#include "swss/logger.h"
#include "swss/dbconnector.h"

enum ObjectStatus
{
    /**
     * @brief Object was not processed at all
     *
     * This enum must be declared first.
     */
    OBJECT_STATUS_NOT_PROCESSED = 0,

    /**
     * @brief Object was pre matched
     *
     * Object is currently in processing, but
     * because it may containg CREATE_ONLY attributes
     * which can be different in current and next view
     * this object maybe removed and recreated.
     */
    OBJECT_STATUS_PRE_MATCHED,

    /**
     * @brief Object was matched in previous view
     *
     * Previous VID was matched to temp VID since
     * it's the same. Objects exists in both previous
     * and next view and have the save VID/RID values.
     *
     * However attributes of that object could be different
     * and may be not matched yet. This object still needs
     * processing for attributes.
     *
     * Since only attributes can be updated then this object
     * may not be removed at all, it must be possible to update
     * only attribute values.
     */
    OBJECT_STATUS_MATCHED,

    /**
     * @brief Object was removed during child processing
     */
    OBJECT_STATUS_REMOVED,

    /**
     * @brief Object is in final stage
     *
     * This means object was matched/set/created
     * and proper actions were generated as diff data
     * to be executed on ASIC.
     */
    OBJECT_STATUS_FINAL,
};

class SaiAttr
{
    public:

        SaiAttr(
                _In_ const std::string &str_attr_id,
                _In_ const std::string &str_attr_value):
            m_str_attr_id(str_attr_id),
            m_str_attr_value(str_attr_value),
            m_meta(NULL)
        {
            SWSS_LOG_ENTER();

            sai_deserialize_attr_id(str_attr_id, &m_meta);

            m_attr.id = m_meta->attrid;

            sai_deserialize_attr_value(str_attr_value, *m_meta, m_attr, false);

            // TODO move this to metadata
            switch (m_meta->serializationtype)
            {
                case SAI_SERIALIZATION_TYPE_OBJECT_ID:
                case SAI_SERIALIZATION_TYPE_OBJECT_LIST:
                case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                    m_is_object_id_attr = true;
                    break;

                default:
                    m_is_object_id_attr = false;
                    break;
            }
        }

        ~SaiAttr()
        {
            SWSS_LOG_ENTER();

            sai_deserialize_free_attribute_value(m_meta->serializationtype, m_attr);
        }

        const sai_attribute_t* getSaiAttr() const
        {
            return &m_attr;
        }

        bool is_object_id_attr() const
        {
            return m_is_object_id_attr;
        }

        const std::string& getStrAttrId() const
        {
            return m_str_attr_id;
        }

        const std::string& getStrAttrValue() const
        {
            return m_str_attr_value;
        }

        const sai_attr_metadata_t* getAttrMetadata() const
        {
            return m_meta;
        }

    private:

        SaiAttr(const SaiAttr&);
        SaiAttr& operator=(const SaiAttr&);

        std::string m_str_attr_id;
        std::string m_str_attr_value;

        const sai_attr_metadata_t* m_meta;
        sai_attribute_t m_attr;
        bool m_is_object_id_attr;
};

class SaiObj
{
    public:

        SaiObj():
            object_status(OBJECT_STATUS_NOT_PROCESSED),
            oidObject(false)
        {
        }

        ObjectStatus object_status;

        std::string str_object_type;
        std::string str_object_id;

        sai_object_meta_key_t meta_key;

        bool oidObject;

        std::unordered_map<sai_attr_id_t, std::shared_ptr<SaiAttr>> attrs;

        std::shared_ptr<SaiAttr> getAttr(
                _In_ sai_attr_id_t id)
        {
            auto it = attrs.find(id);

            if (it == attrs.end())
            {
                SWSS_LOG_ERROR("object %s has no attribute %d", str_object_id.c_str(), id);

                for (const auto &ita: attrs)
                {
                    const auto &a = ita.second;

                    SWSS_LOG_ERROR("%s: %s", a->getStrAttrId().c_str(), a->getStrAttrValue().c_str());
                }

                throw std::runtime_error("object has no attribute");
            }

            return it->second;
        }

        void addAttr(
                _In_ std::shared_ptr<SaiAttr> &a)
        {
            attrs[a->getSaiAttr()->id] = a;
        }

        bool hasAttr(
                _In_ sai_attr_id_t id)
        {
            return attrs.find(id) != attrs.end();
        }

        sai_object_id_t getVid() const
        {
            if (oidObject)
            {
                return meta_key.key.object_id;
            }

            SWSS_LOG_ERROR("object %s it not object id type", str_object_id.c_str());

            throw std::runtime_error("object is not object id type");
        }

        // TODO we need rid/vid mapping for object ids

        // TODO we need dependency tree
        //
        // TODO we need reference count (or list) on each attribute

    private:

        SaiObj(const SaiAttr&);
        SaiObj& operator=(const SaiAttr&);
};

typedef std::unordered_map<sai_object_id_t, sai_object_id_t> ObjectIdMap;
typedef std::unordered_map<std::string, std::shared_ptr<SaiObj>> StrObjectIdToSaiObjectHash;
typedef std::unordered_map<sai_object_id_t, std::shared_ptr<SaiObj>> ObjectIdToSaiObjectHash;

class AsicView
{
    public:

        AsicView()
        {
            // empty intentionally
        }

        void fromDump(
                _In_ const swss::TableDump &dump)
        {
            SWSS_LOG_ENTER();

            for (const auto &key: dump)
            {
                auto start = key.first.find_first_of(":");

                if (start == std::string::npos)
                {
                    SWSS_LOG_ERROR("failed to find colon in %s", key.first.c_str());

                    throw std::runtime_error("failed to find colon inside key");
                }

                std::shared_ptr<SaiObj> o = std::make_shared<SaiObj>();

                o->str_object_type  = key.first.substr(0, start);
                o->str_object_id    = key.first.substr(start + 1);

                sai_deserialize_object_type(o->str_object_type, o->meta_key.object_type);

                switch (o->meta_key.object_type)
                {
                    case SAI_OBJECT_TYPE_SWITCH:
                        soSwitches[o->str_object_id] = o;
                        break;

                    case SAI_OBJECT_TYPE_FDB:
                        sai_deserialize_fdb_entry(o->str_object_id, o->meta_key.key.fdb_entry);
                        soFdbs[o->str_object_id] = o;
                        break;

                    case SAI_OBJECT_TYPE_NEIGHBOR:
                        sai_deserialize_neighbor_entry(o->str_object_id, o->meta_key.key.neighbor_entry);
                        soNeighbors[o->str_object_id] = o;
                        break;

                    case SAI_OBJECT_TYPE_ROUTE:
                        sai_deserialize_route_entry(o->str_object_id, o->meta_key.key.route_entry);
                        soRoutes[o->str_object_id] = o;
                        break;

                    case SAI_OBJECT_TYPE_VLAN:
                        sai_deserialize_vlan_id(o->str_object_id, o->meta_key.key.vlan_id);
                        soVlans[o->str_object_id] = o;
                        break;

                    case SAI_OBJECT_TYPE_TRAP:
                        sai_deserialize_hostif_trap_id(o->str_object_id, o->meta_key.key.trap_id);
                        soTraps[o->str_object_id] = o;
                        break;

                    default:
                        sai_deserialize_object_id(o->str_object_id, o->meta_key.key.object_id);
                        soOids[o->str_object_id] = o;
                        oOids[o->meta_key.key.object_id] = o;
                        otOids[o->meta_key.object_type][o->meta_key.key.object_id] = o;

                        // TODO move to metadata
                        o->oidObject = true;
                        break;
                }

                soAll[o->str_object_id] = o;
                sotAll[o->meta_key.object_type][o->str_object_id] = o;

                populateAttributes(o, key.second);
            }
        }

        StrObjectIdToSaiObjectHash soSwitches;
        StrObjectIdToSaiObjectHash soFdbs;
        StrObjectIdToSaiObjectHash soNeighbors;
        StrObjectIdToSaiObjectHash soRoutes;
        StrObjectIdToSaiObjectHash soVlans;
        StrObjectIdToSaiObjectHash soTraps;
        StrObjectIdToSaiObjectHash soOids;
        StrObjectIdToSaiObjectHash soAll;

        std::map<sai_object_type_t, StrObjectIdToSaiObjectHash> sotAll;
        std::map<sai_object_type_t, ObjectIdToSaiObjectHash> otOids;

        ObjectIdToSaiObjectHash oOids;

        // on temp view this needs to be used for actual NEW rids created
        // and then reused with rid mapping to create new rid/vid map
        ObjectIdMap ridToVid;
        ObjectIdMap vidToRid;

        // if we would make this map for all obejct on strings
        // then we don't need to have this per object type
        std::unordered_map<std::string,std::string> neighborIdMap;

        sai_object_id_t cpuPortRid;
        sai_object_id_t defaultVirtualRouterRid;
        sai_object_id_t defaultTrapGroupRid;
        sai_object_id_t defaultStpInstanceRid;

        // TODO we need 2 functions, one with processed types and one with all for search
        std::vector<std::shared_ptr<SaiObj>> getObjectsByObjectType(
                _In_ sai_object_type_t object_type)
        {
            SWSS_LOG_ENTER();

            // order on list is random
            std::vector<std::shared_ptr<SaiObj>> list;

            // we need to use find, since object type may not exist
            auto it = sotAll.find(object_type);

            if (it != sotAll.end())
            {
                for (const auto &p: it->second)
                {
                    list.push_back(p.second);
                }
            }

            return list;
        }

        ~AsicView(){}

    private:

        void populateAttributes(
                _In_ std::shared_ptr<SaiObj> &o,
                _In_ const swss::TableMap &map)
        {
            SWSS_LOG_ENTER();

            for (const auto &field: map)
            {
                std::shared_ptr<SaiAttr> a = std::make_shared<SaiAttr>(field.first, field.second);

                o->addAttr(a);
            }
        }

        AsicView(const SaiAttr&);
        AsicView& operator=(const SaiAttr&);
};

void redisGetAsicView(
        _In_ std::string tableName,
        _In_ AsicView &view)
{
    SWSS_LOG_ENTER();

    swss::DBConnector db(ASIC_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);

    SWSS_LOG_NOTICE("tableName: %s", tableName.c_str());

    swss::Table table(&db, tableName);

    swss::TableDump dump;

    table.dump(dump);

    view.fromDump(dump);

    SWSS_LOG_NOTICE("objects count: %zu", view.soAll.size());
}

sai_status_t checkObjectsStatus(
        _In_ const AsicView &view)
{
    SWSS_LOG_ENTER();

    int count = 0;

    for (const auto &p: view.soAll)
    {
        // TODO at last stage we need to look for FINAL status
        if (p.second->object_status == OBJECT_STATUS_NOT_PROCESSED)
        {
            const auto &o = *p.second;

            SWSS_LOG_ERROR("object was not processed: %s %s, status: %d",
                    o.str_object_type.c_str(),
                    o.str_object_id.c_str(),
                    o.object_status);

            count++;
        }
    }

    if (count > 0)
    {
        SWSS_LOG_ERROR("%d objects were not processed", count);

        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

ObjectIdToSaiObjectHash vectorToHash(
        _In_ const std::vector<std::shared_ptr<SaiObj>> &vec)
{
    SWSS_LOG_ENTER();

    ObjectIdToSaiObjectHash hash;

    for (const auto &i: vec)
    {
        hash[i->getVid()] = i;
    }

    return hash;
}

void matchOids(
        _In_ AsicView &curv,
        _In_ AsicView &tmpv)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("matching oids");

    // for all oid objects in temp view, match all vid's in the current view
    // some objects will have the same VID in both views (eg. ports)

    for (const auto &i: tmpv.oOids)
    {
        auto it = curv.oOids.find(i.first);

        if (it == curv.oOids.end())
        {
            continue;
        }

        // save vid and rid in temp view

        sai_object_id_t vid = it->second->getVid();
        sai_object_id_t rid = curv.vidToRid.at(vid);

        tmpv.ridToVid[rid] = vid;
        tmpv.vidToRid[vid] = rid;

        // set both objects status as matched

        i.second->object_status = OBJECT_STATUS_MATCHED;
        it->second->object_status = OBJECT_STATUS_MATCHED;

        SWSS_LOG_INFO("matched %s RID 0x%lx VID 0x%lx", i.second->str_object_type.c_str(), rid, vid);
    }

    // TODO we should check if all PORT's and SWITCH are matched
    // since this is our starting point

    SWSS_LOG_NOTICE("matched oids");
}

sai_status_t applyViewTransition(
        _In_ AsicView &current,
        _In_ AsicView &temp)
{
    SWSS_LOG_ENTER();

    // match all oids that are the same in previous and current view

    matchOids(current, temp);

    // TODO start processing matched objects (from ports)

    // TODO we will need in temp view option str_matched_id
    // string if empty than it mean it's the same
    // and then we will need second pass on all objects to see
    // if we have the same attributes

    return SAI_STATUS_SUCCESS;
}

sai_status_t internalSyncdApplyView()
{
    sai_status_t status;

    SWSS_LOG_ENTER();

    AsicView current;
    AsicView temp;

    ObjectIdMap vidToRidMap = redisGetVidToRidMap();
    ObjectIdMap ridToVidMap = redisGetRidToVidMap();

    // populate

    current.ridToVid = ridToVidMap;
    current.vidToRid = vidToRidMap;

    // those calls could be calls to SAI, but when this will be
    // separate lib then we would like to limit sai to minimum
    // or reimplement getting those

    current.cpuPortRid              = redisGetCpuId();
    current.defaultVirtualRouterRid = redisGetDefaultVirtualRouterId();
    current.defaultTrapGroupRid     = redisGetDefaultTrapGroupId();
    current.defaultStpInstanceRid   = redisGetDefaultStpInstanceId();

    // TODO support thoses, since those are default do we need them,
    // they will be listed on dump anyway, but we need to know which is which
    // if new objects will be created
    //
    // g_defaultQueuesRids
    // g_defaultPriorityGroupsRids
    // g_defaultSchedulerGroupsRids

    redisGetAsicView(ASIC_STATE_TABLE, current);
    redisGetAsicView(TEMP_PREFIX ASIC_STATE_TABLE, temp);

    SWSS_LOG_ERROR("apply view transition is not implemented yet");

    status = applyViewTransition(current, temp);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("apply view transition failed: %s", sai_serialize_status(status).c_str());

        return status;
    }

    status = checkObjectsStatus(current);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("object status failed on current view: %s", sai_serialize_status(status).c_str());

        return status;
    }

    status = checkObjectsStatus(temp);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("object status failed on temp view: %s", sai_serialize_status(status).c_str());

        return status;
    }

    // since we matched all rid/vid, number should be the same in previous and next view

    if ((current.ridToVid.size() != temp.ridToVid.size()) ||
            (current.vidToRid.size() != temp.vidToRid.size()))

    {
        SWSS_LOG_ERROR("wrong number of vir/rid items in map, forgot to translate");

        return SAI_STATUS_FAILURE;
    }

    // TODO execute needed api's and updare ridtovid/vidtorid map

    return status;
}

// we need this as C extern
sai_status_t syncdApplyView()
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_INFO);

    sai_status_t status = SAI_STATUS_FAILURE;

    try
    {
        SWSS_LOG_TIMER("apply");

        status = internalSyncdApplyView();
    }
    catch (const std::runtime_error& e)
    {
        SWSS_LOG_ERROR("Exceptiomn: %s", e.what());
    }

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    return status;
}


// TODO we need to be careful since not all "SET" attrubutes can be set,
// some objects could be destroyed
