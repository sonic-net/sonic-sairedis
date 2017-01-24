#include <iostream>
#include <algorithm>

#include "syncd.h"
#include "sairedis.h"
#include "swss/table.h"
#include "swss/logger.h"
#include "swss/dbconnector.h"

typedef enum _sai_object_status_t
{
    /**
     * @brief Object was not processed at all
     *
     * This enum must be declared first.
     */
    SAI_OBJECT_STATUS_NOT_PROCESSED = 0,

    /**
     * @brief Object was pre matched
     *
     * Object is currently in processing, but
     * because it may containg CREATE_ONLY attributes
     * which can be different in current and next view
     * this object maybe removed and recreated.
     */
    SAI_OBJECT_STATUS_PRE_MATCHED,

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
    SAI_OBJECT_STATUS_MATCHED,

    /**
     * @brief Object was removed during child processing
     */
    SAI_OBJECT_STATUS_REMOVED,

    /**
     * @brief Object is in final stage
     *
     * This means object was matched/set/created
     * and proper actions were generated as diff data
     * to be executed on ASIC.
     */
    SAI_OBJECT_STATUS_FINAL,

} sai_object_status_t;

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

            m_is_object_id_attr = m_meta->allowedobjecttypes.size() > 0;
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

        bool isObjectIdAttr() const
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
            object_status(SAI_OBJECT_STATUS_NOT_PROCESSED),
            oidObject(false)
        {
        }

        // TODO move to m_ prefix and private member
        sai_object_status_t object_status;

        std::string str_object_type;
        std::string str_object_id;

        sai_object_meta_key_t meta_key;

        bool oidObject;

        std::unordered_map<sai_attr_id_t, std::shared_ptr<SaiAttr>> attrs;

        const std::unordered_map<sai_attr_id_t, std::shared_ptr<SaiAttr>>& getAllAttributes() const
        {
            return attrs;
        }

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

        void setObjectStatus(
                _In_ sai_object_status_t os)
        {
            object_status = os;
        }

        sai_object_status_t getObjectStatus() const
        {
            return object_status;
        }

        sai_object_type_t getObjectType() const
        {
            return meta_key.object_type;
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

        SaiObj(const SaiObj&);
        SaiObj& operator=(const SaiObj&);
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
        std::unordered_map<std::string,std::string> routeIdMap;
        std::unordered_map<std::string,std::string> fdbIdMap;
        std::unordered_map<std::string,std::string> switchIdMap;

        sai_object_id_t cpuPortRid;
        sai_object_id_t defaultVirtualRouterRid;
        sai_object_id_t defaultTrapGroupRid;
        sai_object_id_t defaultStpInstanceRid;

        std::vector<std::shared_ptr<SaiObj>> getObjectsByObjectType(
                _In_ sai_object_type_t object_type) const
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

        // TODO instead of iterating via all list all the time
        // lets have this as set, and then when we set object
        // status we just remove it from set, bu there is problem
        // since status in set on object not on view

        std::vector<std::shared_ptr<SaiObj>> getNotProcessedObjectsByObjectType(
                _In_ sai_object_type_t object_type) const
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
                    if (p.second->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
                    {
                        list.push_back(p.second);
                    }
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
        if (p.second->object_status != SAI_OBJECT_STATUS_FINAL)
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
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView)
{
    SWSS_LOG_ENTER();

    /*
     * For all OID objects in temporary view we match all VID's in the current
     * view some objects will have the same VID's in both views (eg. ports), in
     * that case their RID's also are the same, so in that case we mark both
     * objects in both views as "MATCHED" which will speed up search comparison
     * logic for those type of objects.
     */

    for (const auto &temporaryIt: temporaryView.oOids)
    {
        sai_object_id_t temporaryVid = temporaryIt.first;

        const auto &currentIt = currentView.oOids.find(temporaryVid);

        if (currentIt == currentView.oOids.end())
        {
            continue;
        }

        sai_object_id_t vid = temporaryVid;
        sai_object_id_t rid = currentView.vidToRid.at(vid);

        // save VID and RID in temporary view

        temporaryView.ridToVid[rid] = vid;
        temporaryView.vidToRid[vid] = rid;

        // set both objects status as matched

        temporaryIt.second->setObjectStatus(SAI_OBJECT_STATUS_MATCHED);
        currentIt->second->setObjectStatus(SAI_OBJECT_STATUS_MATCHED);

        SWSS_LOG_INFO("matched %s RID 0x%lx VID 0x%lx", currentIt->second->str_object_type.c_str(), rid, vid);
    }

    SWSS_LOG_NOTICE("matched oids");
}

void checkMatchedPorts(
        _In_ const AsicView &temporaryView)
{
    SWSS_LOG_ENTER();

    /*
     * We should check if all PORT's and SWITCH are matched since this is our
     * starting point for comparison logic.
     */

    auto ports = temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_PORT);

    for (const auto &p: ports)
    {
        if (p->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED)
        {
            continue;
        }

        /*
         * If that happens then:
         *
         * - we have a bug in our matching port logic, or
         * - we need to remap ports VID/RID after syncd restart, or
         * - after changing switch profile we have different number of ports
         *
         * In any of those cased this is FATAL and needs to be addressed,
         * currently we don't expect port OIDs to change, there is check on
         * syncd start for that, and our goal is that each time we will load
         * the same profile for switch so different number of ports should be
         * rulled out.
         */

        SWSS_LOG_ERROR("port %s object status is not MATCHED (%d)", p->str_object_id.c_str(), p->getObjectStatus());

        throw std::runtime_error("not all PORT objects status are MATCHED");
    }

    SWSS_LOG_NOTICE("all ports are matched");
}

/**
 * @brief Check if both list contains the same objects
 *
 * Function returns TRUE only when all list contain exact the same objects
 * compared by RID values and with exact same order.
 *
 * TODO Currently order on the list matters, but we need to update this logic
 * so order will not matter, just values of object will need to be considered.
 * We need to have extra index of processed objects and and process only not
 * processed yet.  We should also cover NULL case and duplicated objects.
 * Normally we should not have duplicated object id's on the list, and we can
 * easy check that using hash.
 *
 * In case of really long list, easier way to solve this can be getting all the
 * RIDs from current view (they must exist), getting all the matched RIDs from
 * temporary list (if one of them don't exists then lists are not equal) sort
 * both list nlog(n) and then compare sequentially.
 */
bool hasEqualObjectList(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ uint32_t current_count,
        _In_ const sai_object_id_t *current_list,
        _In_ uint32_t temporary_count,
        _In_ const sai_object_id_t *temporary_list)
{
    SWSS_LOG_ENTER();

    if (current_count != temporary_count)
    {
        /*
         * Length of lists are not equal, so lists are different.
         */

        return false;
    }

    for (uint32_t idx = 0; idx < current_count; ++idx)
    {
        sai_object_id_t currentVid = current_list[idx];
        sai_object_id_t temporaryVid = temporary_list[idx];

        if (currentVid == SAI_NULL_OBJECT_ID &&
                temporaryVid == SAI_NULL_OBJECT_ID)
        {
            /*
             * Both current and temporary are the same so we
             * continue for next item on list.
             */

            continue;
        }

        if (currentVid != SAI_NULL_OBJECT_ID &&
                temporaryVid != SAI_NULL_OBJECT_ID)
        {
            /*
             * Check for object type of both objects, they must
             * match.  But maybe this is not necessary, since true
             * is returned only when RIDs match, so it's more like
             * sanity check.
             */

            sai_object_type_t temporaryObjectType = getObjectTypeFromVid(temporaryVid);
            sai_object_type_t currentObjectType = getObjectTypeFromVid(currentVid);

            if (temporaryObjectType == SAI_OBJECT_TYPE_NULL ||
                    currentObjectType == SAI_OBJECT_TYPE_NULL)
            {
                /*
                 * This case should never happen, we always should
                 * be able to extract valid object type from any
                 * VID, if this happens then we have a bug.
                 */

                SWSS_LOG_ERROR("temporary object type is %d and current object type is %d, FATAL",
                        temporaryObjectType,
                        currentObjectType);

                throw std::runtime_error("temporary or current object returned NULL object type, FATAL");
            }

            if (temporaryObjectType != currentObjectType)
            {
                /*
                 * Compared object types are different, so they can't be equal,
                 * so return false here. No need for checking other objects on
                 * list.
                 */

                return false;
            }

            auto temporaryIt = temporaryView.vidToRid.find(temporaryVid);

            if (temporaryIt == temporaryView.vidToRid.end())
            {
                /*
                 * Temporary RID don't exist yet for this object, so it mean's
                 * this object will be created in the future after all
                 * comparison logic finishes.
                 *
                 * Here we know that this temporary object is not processed yet
                 * but during recursive processing we know that this OID value
                 * was already processed, and two things could happened:
                 *
                 * - we matched existing current object for this VID and actual
                 *   RID was assigned, or
                 *
                 * - during matching we didn't matched current object so RID is
                 *   not assigned, and this object will be created later on
                 *   which will assign new RID
                 *
                 * Since we are here where RID don't exist this is the second
                 * case, also we know that current object VID exists so his RID
                 * also exists, so those RID's can't be equal, we need return
                 * false here.
                 *
                 * Fore more strong verification we can introduce and extra
                 * flag in SaiObj indicating that object was processed and it
                 * needs to be created.
                 */

                SWSS_LOG_INFO("temporary RID don't exists (VID 0x%lx), attributes are not equal", temporaryVid);

                return false;
            }

            /*
             * Current VID exists, so current RID also must exists but let's
             * put sanity check here just in case if we mess something up, this
             * should never happen.
             */

            auto currentIt = currentView.vidToRid.find(currentVid);

            if (currentIt == currentView.vidToRid.end())
            {
                SWSS_LOG_ERROR("current VID 0x%lx exists but current RID is missing, FATAL", currentVid);

                throw std::runtime_error("current VID exists, but current RID is missing, FATAL");
            }

            sai_object_id_t temporaryRid = temporaryIt->second;
            sai_object_id_t currentRid = currentIt->second;

            /*
             * If RID's are equal, then object attribute values are equal as well.
             */

            if (temporaryRid == currentRid)
            {
                continue;
            }

            /*
             * If RIDs are different, then list are not equal.
             */

            return false;
        }

        /*
         * If we are here that means one of attributes value OIDs
         * is NULL and other is not, so they are not equal we need
         * to return false.
         */

        return false;
    }

    /*
     * We processed all objects on both lists, and they all are equal so both
     * list are equal even if they are empty. We need to return true in this
     * case.
     */

    return true;
}

/**
 * @brief Check if current and temporary object has
 * the same attribute and attribute has the same value on both.
 *
 * This also includes object ID attributes, thats why we need
 * current and temporary view to compare RID values.
 *
 * NOTE: both objects must be the same object type, otherwise
 * this compare make no sense.
 *
 * NOTE: this function does not check if attributes are
 * different, whether we can update existing one to new one,
 * for that we will need different method
 */
bool hasEqualAttribute(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> &current,
        _In_ const std::shared_ptr<SaiObj> &temporary,
        _In_ sai_attr_id_t id)
{
    SWSS_LOG_ENTER();

    /*
     * Currently we only check if both attributes exists on both objects.
     *
     * One of them maybe missing, if it has default value and the values still
     * maybe the same so in that case we should/could also return true.
     */

    if (current->hasAttr(id) && temporary->hasAttr(id))
    {
        const auto &currentAttr = current->getAttr(id);
        const auto &temporaryAttr = temporary->getAttr(id);

        if (currentAttr->getStrAttrValue() == temporaryAttr->getStrAttrValue())
        {
            /*
             * Serialized value of the attributes are equal so attributes
             * must be equal, this is even true for object ID attributes
             * since this will be only true if VID in both attributes are
             * the same, and if VID's are the same then RID's are also
             * the same, so no need to actual RID's compare.
             */

            /*
             * Woth noticing here acout acl entry field/action since they have
             * enable flag, and if it's disabled then parameter value don't
             * matter. But this is fine here since serialized value don't
             * contain parameter value if it's disabled so we don't need to
             * take extra care about this case here.
             */

            return true;
        }

        if (currentAttr->isObjectIdAttr() == false)
        {
            /*
             * This means attribute is primitive and don't contain
             * any object ids, so we can return false right away
             * instead of getting into switch below
             */

            return false;
        }

        /*
         * In this place we know that attribute values are different,
         * but if attribute serialize type is object id, their RID's
         * maybe equal, and that means actual attributes values
         * are equal as well, so we should return true in that case.
         */

        const sai_attr_metadata_t *meta = temporaryAttr->getAttrMetadata();

        const sai_attribute_t *currentSaiAttr = currentAttr->getSaiAttr();
        const sai_attribute_t *temporarySaiAttr = temporaryAttr->getSaiAttr();

        switch (meta->serializationtype)
        {
            case SAI_SERIALIZATION_TYPE_OBJECT_ID:

                return hasEqualObjectList(
                        currentView,
                        temporaryView,
                        1,
                        &currentSaiAttr->value.oid,
                        1,
                        &temporarySaiAttr->value.oid);


            case SAI_SERIALIZATION_TYPE_OBJECT_LIST:

                return hasEqualObjectList(
                        currentView,
                        temporaryView,
                        currentSaiAttr->value.objlist.count,
                        currentSaiAttr->value.objlist.list,
                        temporarySaiAttr->value.objlist.count,
                        temporarySaiAttr->value.objlist.list);

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:
            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:
            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:

                /*
                 * TODO: for those types we need to check enable flag first,
                 * since when flag is disabled other parameters can be garbage.
                 */

                SWSS_LOG_ERROR("serialization type not supported yet: %d, FIXME", meta->serializationtype);

                throw std::runtime_error("serialization type not sypported yet, FIXME");

            default:

                /*
                 * Attribute value is primitive type here, we already compared
                 * them by string, this means they are different so we need to
                 * return false here, or there is other object id attribute
                 * that is not supported yet, then we need to add it for
                 * compare support.
                 */

                SWSS_LOG_ERROR("serialization type is not supported %d, FIXME", meta->serializationtype);

                throw std::runtime_error("serialization type in not supported, FIXME");
        }
    }

    /*
     * Currently we don't support case where only one attribute is present, but
     * we should consider that if other attribute has default value.
     */

    return false;
}

typedef struct _sai_object_compare_info_t
{
    size_t equal_attributes;

    std::shared_ptr<SaiObj> obj;

} sai_object_compare_info_t;

bool compareByEqualAttributes(
        _In_ const sai_object_compare_info_t &a,
        _In_ const sai_object_compare_info_t &b)
{
    /*
     * Note that this will sort in descending order
     */

    return a.equal_attributes > b.equal_attributes;
}

std::shared_ptr<SaiObj> selectRandomCandidate(
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ERROR();

    size_t candidateCount = candidateObjects.size();

    SWSS_LOG_INFO("selecting random candidate from pool size %zu", candidateCount);

    size_t index = std::rand() % candidateCount;

    return candidateObjects.at(index).obj;
}

bool isOidOnAttrValue(
        _In_ const std::shared_ptr<SaiAttr> &attr,
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    const sai_attr_metadata_t *meta = attr->getAttrMetadata();

    const sai_attribute_t* attribute = attr->getSaiAttr();

    switch (meta->serializationtype)
    {
        case SAI_SERIALIZATION_TYPE_OBJECT_ID:

            return attribute->value.oid == oid;

        case SAI_SERIALIZATION_TYPE_OBJECT_LIST:


        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:
        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:
        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:

            SWSS_LOG_ERROR("this serialization type %d is not supported yet, FIXME", meta->serializationtype);

            throw std::runtime_error("this serialization type is not supported yet");

        default:

            return false;
    }
}

std::vector<std::shared_ptr<SaiObj>> findBestCandidates(
        _In_ const AsicView &view,
        _In_ const std::shared_ptr<SaiObj> rootObj,
        _In_ sai_object_type_t childObjectType,
        _In_ const size_t child_attr_metadata_count,
        _In_ const sai_attr_metadata_t *child_attr_metadata)
{
    SWSS_LOG_ENTER();

    /*
     * Purpose of this function is to search in given view a candidate objects,
     * which condition is any of the oid attributes in childObjectType accepts
     * root object type, and is set at is value, it can be oid directly or list
     * of objects.
     */

    /*
     * Find all attributes on CHILD object metadata which can take
     * ROOT object as attribute parameter (it can be on list). Right now we
     * know those attribute names but this way is more generic and we can
     * extract this logic later on, also make sure those attributes are not
     * READ_ONLY and are present on CHILD objects.
     */

    std::vector<std::shared_ptr<SaiObj>> candidates;

    sai_object_type_t rootObjectType = rootObj->getObjectType();

    std::set<sai_attr_id_t> childObjectTypeAttrIdsWithRootObjectType;

    for (size_t idx = 0; idx < child_attr_metadata_count; ++idx)
    {
        const sai_attr_metadata_t& meta = child_attr_metadata[idx];

        if (HAS_FLAG_READ_ONLY(meta.flags) || meta.allowedobjecttypes.size() == 0)
        {
            /*
             * We skip attributes with READ_ONLY flag or are non object id
             * attributes.
             */

            continue;
        }

        for (const auto &ot: meta.allowedobjecttypes)
        {
            if (ot == rootObjectType)
            {
                SWSS_LOG_INFO("attr %s is taking %s", meta.attridname, sai_serialize_object_type(rootObjectType).c_str());

                childObjectTypeAttrIdsWithRootObjectType.insert(meta.attrid);
                break;
            }
        }
    }

    if (childObjectTypeAttrIdsWithRootObjectType.size() == 0)
    {
        SWSS_LOG_WARN("there is no attribute in %s that takes %s",
                sai_serialize_object_type(childObjectType).c_str(),
                sai_serialize_object_type(rootObjectType).c_str());

        return candidates;
    }

    SWSS_LOG_INFO("found %zu attribute(s) in %s that take %s as attr value",
                childObjectTypeAttrIdsWithRootObjectType.size(),
                sai_serialize_object_type(childObjectType).c_str(),
                sai_serialize_object_type(rootObjectType).c_str());

    /*
     * Now we have list of attributes that take ROOT as object id lets
     * check now in view unprocessed (or all?) objects for VID of that
     * ROOT inside any available CHILD objects.
     */

    const auto &childObjects = view.getObjectsByObjectType(childObjectType);

    /*
     * Now for each CHILD, iterate through attribute for searching for ROOT VID
     * and save those CHILD (since there may be many of them).
     */

    sai_object_id_t rootVid = rootObj->getVid();

    for (const auto &childObject: childObjects)
    {
        for (const auto &attrId: childObjectTypeAttrIdsWithRootObjectType)
        {
            if (!childObject->hasAttr(attrId))
            {
                continue;
            }

            /*
             * Current CHILD has this attribute, now me must check
             * if root object VID is on that attr value.
             */

            const auto &attr = childObject->getAttr(attrId);

            if (isOidOnAttrValue(attr, rootVid))
            {
                /*
                 * Success this CHILD object attribute value contains this ROOT object
                 */

                candidates.push_back(childObject);
                break;
            }
        }
    }

    return candidates;
}

typedef struct _sai_heuristic_item_t
{
    std::shared_ptr<SaiObj> childObj;

    std::vector<std::shared_ptr<SaiObj>> childsOfChildMatched;

    ~_sai_heuristic_item_t() noexcept (true) {};

} sai_heuristic_item_t;

typedef struct _sai_heuristic_result_t
{
    std::shared_ptr<SaiObj> mainObj;

    std::vector<sai_heuristic_item_t> childCandidates;

    ~_sai_heuristic_result_t() noexcept (true){};

} sai_heuristic_result_t;

std::shared_ptr<SaiObj> findCurrentBestMatchUsingHeuristicForBufferPool(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("find");

    /*
     * This method will use heuristic method for findinng best candidate since
     * all of them have the same number of equal attributes.
     *
     * So our approach here is this, we know manually graph dependency:
     * BUFFER_POOL -> BUFFER_PROFILE -> { QUEUE, INGRESS_PRIORITY_GROUP, PORT
     * }, there maybe several attributes involved so we need to remember them
     * which one are in use on which level.
     *
     * Since BUFFER_POOL has no other dependencies (its a root) w know here
     * that BUFFER_PROFILE was not processed yet, but PORT has been matched and
     * QUEUE or INGRESS_PRIORITY_GROUP could been matched or not, that depens
     * on configuration. So out task here is to find the same path in both
     * graphs which can be tricky and ugly since we have 2 hops here and we
     * need to do that for each candidate object.
     */

    /*
     * TODO: in futre we would need a reverse graph mapping to automate this
     * process, like right now BUFFER_PROFILE knows that it can use BUFFER_POOL
     * but BUFFER_POOL don't know which object can be using it. We need to add
     * this to metadata in new SAI.
     */

    sai_heuristic_result_t heuristicResult = { temporaryObj, { } };

    const auto &candidateBufferProfiles = findBestCandidates(
            temporaryView,
            temporaryObj,
            SAI_OBJECT_TYPE_BUFFER_PROFILE,
            sai_buffer_profile_attr_metadata_count,
            sai_buffer_profile_attr_metadata);

    if (candidateBufferProfiles.size() == 0)
    {
        SWSS_LOG_WARN("candidate buffer profile size is zero, selecting random");

        return selectRandomCandidate(candidateObjects);
    }

    SWSS_LOG_INFO("found %zu buffer profiles candidates", candidateBufferProfiles.size());

    /*
     * Now here we can have multiple buffer profiles, in future when we will
     * have multiple objects like this it may be not efficient to use this
     * logic because much faster will be to choose random and accept
     * reconfigure changes, in this case is not that big deal since for buffer
     * profile is always possible to SET.
     */

    for (const auto &bufferProfile: candidateBufferProfiles)
    {
        const auto &candidateQueues = findBestCandidates(
                temporaryView,
                bufferProfile,
                SAI_OBJECT_TYPE_QUEUE,
                sai_queue_attr_metadata_count,
                sai_queue_attr_metadata);

        const auto &candidateIngressPriorityGroups = findBestCandidates(
                temporaryView,
                bufferProfile,
                SAI_OBJECT_TYPE_PRIORITY_GROUP,
                sai_priority_group_attr_metadata_count,
                sai_priority_group_attr_metadata);

        const auto &candidatePorts = findBestCandidates(
                temporaryView,
                bufferProfile,
                SAI_OBJECT_TYPE_PORT,
                sai_port_attr_metadata_count,
                sai_port_attr_metadata);

        std::vector<std::shared_ptr<SaiObj>> allMatchedCandidates;

        /*
         * Combine all candidates and filter them by MATCHED status.
         */

        // TODO we should select matched and with existing RID or maybe we can
        // just compare VID in temp and current

        for (const auto &candidate: candidateQueues)
        {
            if (candidate->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED)
            {
                allMatchedCandidates.push_back(candidate);
            }
        }

        for (const auto &candidate: candidateIngressPriorityGroups)
        {
            if (candidate->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED)
            {
                allMatchedCandidates.push_back(candidate);
            }
        }

        for (const auto &candidate: candidatePorts)
        {
            if (candidate->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED)
            {
                allMatchedCandidates.push_back(candidate);
            }
        }

        SWSS_LOG_INFO("for current buffer profile %s we have %zu candidates matched",
                bufferProfile->str_object_id.c_str(),
                allMatchedCandidates.size());

        sai_heuristic_item_t item = { bufferProfile, allMatchedCandidates };

        heuristicResult.childCandidates.push_back(item);
    }

    // TODO we should have 1 heuristic result for temporary objects
    // and them muliple for candiddate objects and a way to compare them

    // TODO those are buffer pools we are chekking agains't since we found two of them
    for (const auto &candidateObj: candidateObjects)
    {

        // TODO for them we also need to do the same path in graph and for each of them get all matched
        // candidates and then we can start comparing how many vids we matched per each one
        // and select the one with the highest one

        SWSS_LOG_INFO("%s", candidateObj.obj->str_object_id.c_str());

        throw std::runtime_error("test");

    }


    SWSS_LOG_WARN("heuristic failed to find right candidate, we select random object");

    return selectRandomCandidate(candidateObjects);
}

std::shared_ptr<SaiObj> findCurrentBestMatchForGenericObjectUsingHeuristic(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * This method will select heuristic method for best current object
     * matching for given temporary object based on object type.  If heuristic
     * don't exists yet, then current object will be choosen at random.
     */

    sai_object_type_t object_type = temporaryObj->getObjectType();

    /*
     * TODO later on we can have a list of heuristic function pointers since
     * all signatures will be the same.
     *
     * Also if heuristic function can't decide it should return random object
     * as well, it could return nullptr and we could deal with it here as well.
     */

    switch (object_type)
    {
        case SAI_OBJECT_TYPE_BUFFER_POOL:

            return findCurrentBestMatchUsingHeuristicForBufferPool(
                    currentView,
                    temporaryView,
                    temporaryObj,
                    candidateObjects);

        default:

            SWSS_LOG_WARN("%s is not supported for heuristic, will select object at random",
                    sai_serialize_object_type(object_type).c_str());

            break;
    }

    return selectRandomCandidate(candidateObjects);
}

std::shared_ptr<SaiObj> findCurrentBestMatchForGenericObject(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * This method will try to find current best match object for a given
     * temporary object. This method should be used only on object id objects,
     * since non object id structures also contains object id which in this
     * case are not take into account. Besides for objects like FDB, ROUTE or
     * NEIGHBOR we can do quick hash lookup instead of looping for all objects
     * where there can be a lot of them.
     *
     * Special case here we can add later on is VLAN, since we can have a lot
     * of VLANS so instead looking via all of them we just need to make sure
     * that we will create reverse map via VLAN_ID KEY and then we can make
     * hash lookup to see if such vlan is present.
     */

    /*
     * Since our system design is to restart orch agend withourt restarting
     * syncd and recreating objects and reassign new VIDs created inside orch
     * agent, in our most cases values of objects will not change.  This will
     * cause to make our comparison logic here faily simple:
     *
     * Find all objects that have the same equal attributes on current object
     * and choose the one with the most attributes that match current and
     * temporary object.
     *
     * This seems simple, but there are a lot of cases that needs to be taken
     * into account:
     *
     * - what if we have several objects with the same number of equal
     *   attributes then we can choose at random or implement some heuristic
     *   logic to try figure out which of those objects will be the best, even
     *   then if we choose wrong object, then there can be a lot of removes and
     *   recreating objects on the ASIC
     *
     * - what if in temporary object CREATE_ONLY attributes don't match but
     *   many of CREATE_AND_SET are the same, in that case we can choose object
     *   with most matching attributes, but object will still needs to be
     *   destroyed becasuse we need to set new CREATE_ONLY attributes
     *
     * - there are also cases with default values of attributes where attribute
     *   is present only in one object but on other one it have defaule value
     *   and this default value is the same as the one in attribute
     *
     * - another case is for objects that needs to be removed since there are
     *   no corresponding objects in remporary view, but they can't be removed,
     *   objects like PORT or default QUEUEs or INGRESS_PRIORITY_GROUPs, then
     *   we need to bring their current set values to default ones, which also
     *   can be challenging since we need to know prevous default value and it
     *   could be assigned by switch internally, like default MAC addres or
     *   default TRAP group etc
     *
     * - there is also interesting case with KEYs attributes which when
     *   doing remove/create they needs to be removed first since
     *   we can't have 2 identical cases
     *
     * There are alot of aspects to consider here, in here we will cake only
     * couple of them in consideration, and other will be taken care inside
     * processObjectForViewTransition method which will handle all other cases
     * not mentioned here.
     */

    /*
     * First check if object is oid object, if yes, chek if it status is
     * matched.
     *
     * TODO, maybe this don't need to be here, it can be one level up since
     * we need special cases for non object ids.
     */

    if (temporaryObj->oidObject)
    {
        /*
         * This approach will not work for neighbor but it could work for
         * routes in SAI 0.9.4 with default virtual router. But make no sense
         * to invest time since in SAI 1.0 it will not work for routes as well.
         */

        if (temporaryObj->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED)
        {
            /*
             * Object status is matched so current and temporary VID are the
             * same so we can just take object directly.
             */

            SWSS_LOG_INFO("found best match for %s %s since object status is MATCHED",
                    temporaryObj->str_object_type.c_str(),
                    temporaryObj->str_object_id.c_str());

            return currentView.oOids.at(temporaryObj->getVid());
        }
    }

    /*
     * Get not processed objects of temporary object type, and all
     * attributes that are set on that object.
     */

    sai_object_type_t object_type = temporaryObj->getObjectType();

    const auto notProcessedObjects = currentView.getNotProcessedObjectsByObjectType(object_type);

    const auto attrs = temporaryObj->getAllAttributes();

    /*
     * Complexity here is O((n^2)*m) since we iterate via all not processed
     * objects, then we iterate through all present attributes.  N is squared
     * since for given object type we iterate via entire list for each object,
     * this can be optimized a little bit inside AsicView class.
     */

    SWSS_LOG_INFO("not processed objects for %s: %zu, attrs: %zu",
            temporaryObj->str_object_type.c_str(),
            notProcessedObjects.size(),
            attrs.size());

    std::vector<sai_object_compare_info_t> candidateObjects;

    for (const auto &currentObj: notProcessedObjects)
    {
        sai_object_compare_info_t soci = { 0, currentObj };

        bool has_different_create_only_attr = false;

        for (const auto &attr: attrs)
        {
            sai_attr_id_t attrId = attr.first;

            /*
             * Function hasEqualAttribute check if attribute exists on both objects.
             */

            // TODO add to logs which attributes and then change do debug
            if (hasEqualAttribute(currentView, temporaryView, currentObj, temporaryObj, attrId))
            {
                soci.equal_attributes++;

                SWSS_LOG_INFO("ob equal %s %s, %s: %s", temporaryObj->str_object_id.c_str(), currentObj->str_object_id.c_str(),
                        attr.second->getStrAttrId().c_str(),
                        attr.second->getStrAttrValue().c_str());
            }
            else
            {
                SWSS_LOG_INFO("ob not equal %s %s, %s: %s", temporaryObj->str_object_id.c_str(), currentObj->str_object_id.c_str(),
                        attr.second->getStrAttrId().c_str(),
                        attr.second->getStrAttrValue().c_str());

                /*
                 * Function hasEqualAttribute returns true only when both
                 * attributes are existing and both are equal, so here it
                 * returned false, so it may mean 2 things:
                 *
                 * - attribute don't exists in current view, or
                 * - attributes are different
                 *
                 * If we check if attribute also exists in current view and has
                 * CREATE_ONLY flag then attributes are different and we
                 * disqualify this object since new temporary object needs to
                 * pass new differetn attribute with CREATE_ONLY flag.
                 *
                 * Case when attribute don't exists is much more complicated
                 * since it maybe conditional and have default value, we will
                 * do that check when we select best match.
                 */

                /*
                 * Get attribute metadata to see if contains CREATE_ONLY flag.
                 */

                const sai_attr_metadata_t* meta = attr.second->getAttrMetadata();

                if (HAS_FLAG_CREATE_ONLY(meta->flags) && currentObj->hasAttr(attrId))
                {
                    has_different_create_only_attr = true;

                    SWSS_LOG_INFO("obj has not equal create only attributes %s", temporaryObj->str_object_id.c_str());

                    /*
                     * In this case there is no need to compare other
                     * attributes since we won't be able to update them anyway.
                     */

                    break;
                }
            }
        }

        if (has_different_create_only_attr)
        {
            /*
             * Those objects differs with attribute which is marked as
             * CREATE_ONLY so we will not be able to update current if
             * necessary using SET operations.
             */

            continue;
        }

        candidateObjects.push_back(soci);
    }

    SWSS_LOG_INFO("number candidate objects for %s is %zu", temporaryObj->str_object_id.c_str(), candidateObjects.size());

    if (candidateObjects.size() == 0)
    {
        /*
         * We didn't found any object.
         */

        return nullptr;
    }

    if (candidateObjects.size() == 1)
    {
        /*
         * We found only one object so it must be it.
         */

        return candidateObjects.begin()->obj;
    }

    /*
     * Sort candidate objects by equal attributes in descending order, we know
     * here that we have at least 2 candidates.
     */

    std::sort(candidateObjects.begin(), candidateObjects.end(), compareByEqualAttributes);

    if (candidateObjects[0].equal_attributes > candidateObjects[1].equal_attributes)
    {
        /*
         * We have only 1 object with the greatest number of equal attributes
         * lets choose that object as our best match.
         */

        return candidateObjects.begin()->obj;
    }

    /*
     * In here there are at least 2 current objects that have the same
     * number of equal attributes. In here we can do two things
     *
     * - select object at random, or
     * - use heuristic/smart lookup for inside graph
     *
     * Smart lookup would be for example searching wheter current object is
     * pointing to the same PORT as temporary object (since ports are matched
     * at the beginning). For different types of objects we need different type
     * of logic and we can start adding that when needed and when missing we
     * will just choose at random possiblu causing some remove/recreate but
     * this logic is not perfect at this point.
     */

    /*
     * Lets also remove candidates with less equal attributes
     */

    size_t previousCandidates = candidateObjects.size();

    size_t equalAttributes = candidateObjects[0].equal_attributes;

    auto endIt = std::remove_if(candidateObjects.begin(), candidateObjects.end(),
            [equalAttributes](const sai_object_compare_info_t& candidate)
            { return candidate.equal_attributes != equalAttributes; });

    candidateObjects.erase(endIt, candidateObjects.end());

    SWSS_LOG_INFO("multiple candidates found (%zu of %zu) for %s, will use heuristic",
            candidateObjects.size(),
            previousCandidates,
            temporaryObj->str_object_id.c_str());

    return findCurrentBestMatchForGenericObjectUsingHeuristic(
            currentView,
            temporaryView,
            temporaryObj,
            candidateObjects);
}

std::shared_ptr<SaiObj> findCurrentBestMatchForHostif(/*{{{*/
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    auto hostifs = currentView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_HOST_INTERFACE);

    for (const auto &c: hostifs)
    {
        if (hasEqualAttribute(currentView, temporaryView, c, t, SAI_HOSTIF_ATTR_TYPE) &&
            hasEqualAttribute(currentView, temporaryView, c, t, SAI_HOSTIF_ATTR_NAME))
        {
            return c;
        }
    }

    return nullptr;
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForVirtualRouter(/*{{{*/
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    auto virtualrouters = currentView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_VIRTUAL_ROUTER);

    for (const auto &c: virtualrouters)
    {
        if (hasEqualAttribute(currentView, temporaryView, c, t, SAI_VIRTUAL_ROUTER_ATTR_VIOLATION_IP_OPTIONS) &&
            hasEqualAttribute(currentView, temporaryView, c, t, SAI_VIRTUAL_ROUTER_ATTR_VIOLATION_TTL1_ACTION))
        {
            return c;
        }
    }

    return nullptr;
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForRouterInterface(/*{{{*/
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    auto routerinterfaces = curv.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_ROUTER_INTERFACE);

    for (const auto &c: routerinterfaces)
    {
        if (hasEqualAttribute(curv, tmpv, c, t, SAI_ROUTER_INTERFACE_ATTR_TYPE))
        {
            if (hasEqualAttribute(curv, tmpv, c, t, SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID))
            {
                int32_t type = t->getAttr(SAI_ROUTER_INTERFACE_ATTR_TYPE)->getSaiAttr()->value.s32;

                if (type == SAI_ROUTER_INTERFACE_TYPE_PORT)
                {
                    /*
                     * When type is PORT, then we need to check port ID as well.
                     */

                    if (hasEqualAttribute(curv, tmpv, c, t, SAI_ROUTER_INTERFACE_ATTR_PORT_ID))
                    {
                        return c;
                    }
                }
                else if (type == SAI_ROUTER_INTERFACE_TYPE_LOOPBACK)
                {
                    /*
                     * When type is defined as LOOPBACK, then PORT attribute is
                     * not present If multiple loopbacks will be defined then
                     * this is first that we hit, so to make it better we need
                     * then extend this logic to look deeper into dependency
                     * graph and find similarities.
                     */

                    return c;
                }
            }
        }
    }

    return nullptr;
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForNextHop(/*{{{*/
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    auto nexthops = curv.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_NEXT_HOP);

    for (const auto &c: nexthops)
    {
        if (hasEqualAttribute(curv, tmpv, c, t, SAI_NEXT_HOP_ATTR_TYPE) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_NEXT_HOP_ATTR_IP) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID))
        {
            return c;
        }
    }

    return nullptr;
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForBufferPool(/*{{{*/
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    auto bufferpools = curv.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_BUFFER_POOL);

    for (const auto &c: bufferpools)
    {
        if (hasEqualAttribute(curv, tmpv, c, t, SAI_BUFFER_POOL_ATTR_SIZE) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_BUFFER_POOL_ATTR_TH_MODE) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_BUFFER_POOL_ATTR_TYPE))
        {
            // we will need heuristics since we can have multiple same objects
            return c;
        }
    }

    return nullptr;
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForBufferProfile(/*{{{*/
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    auto bufferprofiles = curv.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_BUFFER_PROFILE);

    for (const auto &c: bufferprofiles)
    {
        if (hasEqualAttribute(curv, tmpv, c, t, SAI_BUFFER_PROFILE_ATTR_POOL_ID) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_BUFFER_PROFILE_ATTR_BUFFER_SIZE))
        {
            // we will need heuristics since we can have multiple same objects
            if (hasEqualAttribute(curv, tmpv, c, t, SAI_BUFFER_PROFILE_ATTR_SHARED_DYNAMIC_TH))
            {
                return c;
            }

            if (hasEqualAttribute(curv, tmpv, c, t, SAI_BUFFER_PROFILE_ATTR_SHARED_STATIC_TH))
            {
                return c;
            }
        }
    }

    return nullptr;
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForWred(/*{{{*/
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    auto wreds = curv.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_WRED);

    for (const auto &c: wreds)
    {
        if (hasEqualAttribute(curv, tmpv, c, t, SAI_WRED_ATTR_ECN_MARK_MODE) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_WRED_ATTR_GREEN_MIN_THRESHOLD) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_WRED_ATTR_GREEN_MAX_THRESHOLD) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_WRED_ATTR_YELLOW_MIN_THRESHOLD) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_WRED_ATTR_YELLOW_MAX_THRESHOLD) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_WRED_ATTR_RED_MIN_THRESHOLD) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_WRED_ATTR_RED_MAX_THRESHOLD))
        {
                return c;
        }
    }

    return nullptr;
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForNextHopGroup(/*{{{*/
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    auto nexthopgroups = curv.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_NEXT_HOP_GROUP);

    for (const auto &c: nexthopgroups)
    {
        if (hasEqualAttribute(curv, tmpv, c, t, SAI_NEXT_HOP_GROUP_ATTR_TYPE) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_NEXT_HOP_GROUP_ATTR_NEXT_HOP_LIST))
        {
            return c;
        }
    }

    return nullptr;
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForPolicer(/*{{{*/
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    auto policers = curv.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_POLICER);

    for (const auto &c: policers)
    {
        if (hasEqualAttribute(curv, tmpv, c, t, SAI_POLICER_ATTR_METER_TYPE) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_POLICER_ATTR_MODE))
        {
            // we will need heuristics since we can have multiple same objects
            return c;
        }
    }

    return nullptr;
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForHostifTrapGroup(/*{{{*/
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    auto hostiftrapgroups = curv.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_TRAP_GROUP);

    for (const auto &c: hostiftrapgroups)
    {
        if (hasEqualAttribute(curv, tmpv, c, t, SAI_HOSTIF_TRAP_GROUP_ATTR_QUEUE) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER))
        {
            return c;
        }

        if (hasEqualAttribute(curv, tmpv, c, t, SAI_HOSTIF_TRAP_GROUP_ATTR_QUEUE) &&
                !t->hasAttr(SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER))
        {
            return c;
        }
    }

    return nullptr;
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForAclTable(/*{{{*/
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    auto acltables = curv.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_ACL_TABLE);

    for (const auto &c: acltables)
    {
        if (hasEqualAttribute(curv, tmpv, c, t, SAI_ACL_TABLE_ATTR_STAGE) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_ACL_TABLE_ATTR_PRIORITY))
        {
            return c;
        }
    }

    return nullptr;
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForAclEntry(/*{{{*/
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    auto aclentries = curv.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_ACL_ENTRY);

    for (const auto &c: aclentries)
    {
        if (hasEqualAttribute(curv, tmpv, c, t, SAI_ACL_ENTRY_ATTR_TABLE_ID) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_ACL_ENTRY_ATTR_PRIORITY) &&
                hasEqualAttribute(curv, tmpv, c, t, SAI_ACL_ENTRY_ATTR_ACTION_SET_COLOR))
        {
            return c;
        }
    }

    return nullptr;
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForNeighborEntry(/*{{{*/
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    /*
     * For Neighbor we don't need to iterate via all current
     * neighbors, we can do dictionary lookup, but we need
     * to do smart trick, since temporary object was processed
     * we just need to check whether VID in neighbor_entry
     * struct is matched/final and it has RID assigned from
     * current view. If, RID exists, we can use that RID
     * to get VID of current view, exchange in neighbor_entry
     * struct and do dictionary lookup on serialized neighbor_entry.
     *
     * With this approach for many entries this is the quickest
     * possible way. In case when RID don't exist, that means
     * we have invalid neighbor entry, so we must return null.
     */

    /*
     * Make a copy here to not destroy object data, later
     * on this data should be read only.
     */

    sai_neighbor_entry_t ne = t->meta_key.key.neighbor_entry;

    sai_object_id_t temporaryRouterInterfaceVid = ne.rif_id;

    auto temporaryIt = tmpv.vidToRid.find(temporaryRouterInterfaceVid);

    if (temporaryIt == tmpv.vidToRid.end())
    {
        /*
         * RID for router interface in neighbor_entry struct was not assigned,
         * so we didn't matched it in previous processing router interfaces, or
         * router interface will be created later on. This mean we can't
         * match neighbor because even if all attributes are matchind, RID in
         * neighbor struct after create will not match so at the end of
         * processing current neighbor will need to be destroyed and new one
         * needs to be created.
         */

        return nullptr;
    }

    /*
     * We found RID which should be in current view, now we need to
     * get current view VID to replace is in our neighbor_entry copy.
     */

    sai_object_id_t temporaryRid = temporaryIt->second;

    auto currentIt = curv.ridToVid.find(temporaryRid);

    if (currentIt == curv.ridToVid.end())
    {
        /*
         * This is just sanity check, should never happen.
         */

        SWSS_LOG_ERROR("found tempoary RID 0x%lx but current VID don't exists, FATAL", temporaryRid);

        throw std::runtime_error("found temporary RID but current VID don't exists, FATAL");
    }

    /*
     * This vid is vid of current view router interface, it may or may not be
     * equal to temporaryVid, but we need to this step to not guess vids.
     */

    sai_object_id_t currentRouterInterfaceVid = currentIt->second;

    ne.rif_id = currentRouterInterfaceVid;

    std::string str_neighbor_entry = sai_serialize_neighbor_entry(ne);

    /*
     * Now when we have serialized neighbor entry with temporary rif_if VID
     * replaced to current rif_id VID we can do dictionary lookup for neighbor.
     */

    auto currentNeighborIt = curv.soNeighbors.find(str_neighbor_entry);

    if (currentNeighborIt == curv.soNeighbors.end())
    {
        SWSS_LOG_DEBUG("unable to find neighbor entry %s in current asic view", str_neighbor_entry.c_str());

        return nullptr;
    }

    /*
     * We found the same neighbor entry in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentNeighborObj = currentNeighborIt->second;

    if (currentNeighborObj->object_status == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentNeighborObj;
    }

    /*
     * If we are here, that means this neighbor was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_ERROR("found neighbor entry %s in current view, but it status is %d, FATAL",
            str_neighbor_entry.c_str(), currentNeighborObj->object_status);

    throw std::runtime_error("found neighbor entry in current view, but it status was processed");
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForRouteEntry(/*{{{*/
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    /*
     * For Route we don't need to iterate via all current
     * routes, we can do dictionary lookup, but we need
     * to do smart trick, since temporary object was processed
     * we just need to check whether VID in route_entry
     * struct is matched/final and it has RID assigned from
     * current view. If, RID exists, we can use that RID
     * to get VID of current view, exchange in route_entry
     * struct and do dictionary lookup on serialized route_entry.
     *
     * With this approach for many entries this is the quickest
     * possible way. In case when RID don't exist, that means
     * we have invalid route entry, so we must return null.
     */

    /*
     * Make a copy here to not destroy object data, later
     * on this data should be read only.
     */

    sai_unicast_route_entry_t re = t->meta_key.key.route_entry;

    sai_object_id_t temporaryVirtualRouterVid = re.vr_id;

    auto temporaryIt = tmpv.vidToRid.find(temporaryVirtualRouterVid);

    if (temporaryIt == tmpv.vidToRid.end())
    {
        /*
         * RID for virtual router in route_entry struct was not assigned,
         * so we didn't matched it in previous processing virtual routers, or
         * virtual router will be created later on. This mean we can't
         * match route because even if all attributes are matchind, RID in
         * route struct after create will not match so at the end of
         * processing current route will need to be destroyed and new one
         * needs to be created.
         */

        return nullptr;
    }

    /*
     * We found RID which should be in current view, now we need to
     * get current view VID to replace is in our route_entry copy.
     */

    sai_object_id_t temporaryRid = temporaryIt->second;

    auto currentIt = curv.ridToVid.find(temporaryRid);

    if (currentIt == curv.ridToVid.end())
    {
        /*
         * This is just sanity check, should never happen.
         */

        SWSS_LOG_ERROR("found tempoary RID 0x%lx but current VID don't exists, FATAL", temporaryRid);

        throw std::runtime_error("found temporary RID but current VID don't exists, FATAL");
    }

    /*
     * This vid is vid of current view virtual router, it may or may not be
     * equal to temporaryVid, but we need to this step to not guess vids.
     */

    sai_object_id_t currentVirtualRouterVid = currentIt->second;

    re.vr_id = currentVirtualRouterVid;

    std::string str_route_entry = sai_serialize_route_entry(re);

    /*
     * Now when we have serialized route entry with temporary vr_id VID
     * replaced to current vr_id VID we can do dictionary lookup for route.
     */

    auto currentRouteIt = curv.soRoutes.find(str_route_entry);

    if (currentRouteIt == curv.soRoutes.end())
    {
        SWSS_LOG_DEBUG("unable to find route entry %s in current asic view", str_route_entry.c_str());

        return nullptr;
    }

    /*
     * We found the same route entry in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentRouteObj = currentRouteIt->second;

    if (currentRouteObj->object_status == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentRouteObj;
    }

    /*
     * If we are here, that means this route was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_ERROR("found route entry %s in current view, but it status is %d, FATAL",
            str_route_entry.c_str(), currentRouteObj->object_status);

    throw std::runtime_error("found route entry in current view, but it status was processed");
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForSwitch(/*{{{*/
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    /*
     * On compare logic we checked that we have one switch
     * so we can just get first.
     */

    auto currentSwitchIt = curv.soSwitches.begin();

    if (currentSwitchIt == curv.soSwitches.end())
    {
        SWSS_LOG_DEBUG("unable to find switch object in current view");

        return nullptr;
    }

    /*
     * We found switch object in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentSwitchObj = currentSwitchIt->second;

    if (currentSwitchObj->object_status == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentSwitchObj;
    }

    /*
     * If we are here, that means this switch was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_ERROR("found switch object %s in current view, but it status is %d, FATAL",
            currentSwitchObj->str_object_id.c_str(), currentSwitchObj->object_status);

    throw std::runtime_error("found switch object in current view, but it status was processed");
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForFdbEntry(/*{{{*/
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    /*
     * For fdb we don't need to iterate via all current
     * fdbs, we can do dictionary lookup, we don't even
     * do OID translation since fdb entry don't contain
     * any oids, so simple direct lookup is fine. This
     * will change in SAI 1.0 since oids are added there.
     */

    sai_fdb_entry_t fe = t->meta_key.key.fdb_entry;

    std::string str_fdb_entry = sai_serialize_fdb_entry(fe);

    /*
     * Now when we have serialized fdb entry can do dictionary lookup for fdb.
     */

    auto currentFdbIt = curv.soFdbs.find(str_fdb_entry);

    if (currentFdbIt == curv.soFdbs.end())
    {
        SWSS_LOG_DEBUG("unable to find fdb entry %s in current asic view", str_fdb_entry.c_str());

        return nullptr;
    }

    /*
     * We found the same fdb entry in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentFdbObj = currentFdbIt->second;

    if (currentFdbObj->object_status == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentFdbObj;
    }

    /*
     * If we are here, that means this fdb was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_ERROR("found fdb entry %s in current view, but it status is %d, FATAL",
            str_fdb_entry.c_str(), currentFdbObj->object_status);

    throw std::runtime_error("found fdb entry in current view, but it status was processed");
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForTrap(/*{{{*/
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    /*
     * For traps in 0.9.4 traps are enum values, and we have
     * their exact names so we can do direct dictionary lookup.
     */

    sai_hostif_trap_id_t trap_id = t->meta_key.key.trap_id;

    std::string str_trap_id = sai_serialize_hostif_trap_id(trap_id);

    /*
     * Now when we have serialized trap id, we can do dictionary lookup for trap.
     */

    auto currentTrapIt = curv.soTraps.find(str_trap_id);

    if (currentTrapIt == curv.soTraps.end())
    {
        SWSS_LOG_DEBUG("unable to find trap id %s in current asic view", str_trap_id.c_str());

        return nullptr;
    }

    /*
     * We found the same trap id in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentTrapObj = currentTrapIt->second;

    if (currentTrapObj->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentTrapObj;
    }

    /*
     * If we are here, that means this trap was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_ERROR("found trap id %s in current view, but it status is %d, FATAL",
            str_trap_id.c_str(), currentTrapObj->object_status);

    throw std::runtime_error("found trap id in current view, but it status was processed");
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForVlan(/*{{{*/
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    /*
     * For vlan in 0.9.4 vlans are numbers values, and we have
     * their exact values so we can do direct dictionary lookup.
     */

    sai_vlan_id_t vlan_id = t->meta_key.key.vlan_id;

    std::string str_vlan_id = sai_serialize_vlan_id(vlan_id);

    /*
     * Now when we have serialized vlan id, we can do dictionary lookup for vlan.
     */

    auto currentVlanIt = curv.soVlans.find(str_vlan_id);

    if (currentVlanIt == curv.soVlans.end())
    {
        SWSS_LOG_DEBUG("unable to find vlan id %s in current asic view", str_vlan_id.c_str());

        return nullptr;
    }

    /*
     * We found the same vlan id in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentVlanObj = currentVlanIt->second;

    if (currentVlanObj->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentVlanObj;
    }

    /*
     * If we are here, that means this vlan was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_ERROR("found vlan id %s in current view, but it status is %d, FATAL",
            str_vlan_id.c_str(), currentVlanObj->object_status);

    throw std::runtime_error("found vlan id in current view, but it status was processed");
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatch(
        _In_ const AsicView &curv,
        _In_ const AsicView &tmpv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    if (t->oidObject)
    {
        /*
         * This approach will not work for neighbor but it could work for
         * routes in SAI 0.9.4 with default virtual router. But make no sense
         * to invest time since in SAI 1.0 it will not work for routes as well.
         */

        if (t->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED)
        {
            // object status is matched so current and temp VID are the same
            // so we can just take object directly

            SWSS_LOG_INFO("found best match for %s %s since object status is MATCHED",
                t->str_object_type.c_str(),
                t->str_object_id.c_str());

            return curv.oOids.at(t->getVid());
        }
    }

    // NOTE: later on best match find can be done based on present CREATE_ONLY
    // attributes or on most matched CREATE_AND_SET attributes, but
    // for now we will keep separate logic for different object type

    switch (t->getObjectType())
    {
        case SAI_OBJECT_TYPE_QOS_MAPS:
            //return findCurrentBestMatchForQosMap(curv, tmpv, t);
            return findCurrentBestMatchForGenericObject(curv, tmpv, t);

        case SAI_OBJECT_TYPE_SCHEDULER:
            //return findCurrentBestMatchForScheduler(curv, tmpv, t);
            return findCurrentBestMatchForGenericObject(curv, tmpv, t);

        case SAI_OBJECT_TYPE_HOST_INTERFACE:
            //return findCurrentBestMatchForHostif(curv, tmpv, t);
            return findCurrentBestMatchForGenericObject(curv, tmpv, t);

        case SAI_OBJECT_TYPE_VIRTUAL_ROUTER:
            //return findCurrentBestMatchForVirtualRouter(curv, tmpv, t);
            return findCurrentBestMatchForGenericObject(curv, tmpv, t);

        case SAI_OBJECT_TYPE_ROUTER_INTERFACE:
            //return findCurrentBestMatchForRouterInterface(curv, tmpv, t);
            return findCurrentBestMatchForGenericObject(curv, tmpv, t);

        case SAI_OBJECT_TYPE_NEXT_HOP:
            //return findCurrentBestMatchForNextHop(curv, tmpv, t);
            return findCurrentBestMatchForGenericObject(curv, tmpv, t);

        case SAI_OBJECT_TYPE_WRED:
            //return findCurrentBestMatchForWred(curv, tmpv, t);
            return findCurrentBestMatchForGenericObject(curv, tmpv, t);

        case SAI_OBJECT_TYPE_NEXT_HOP_GROUP:
            //return findCurrentBestMatchForNextHopGroup(curv, tmpv, t);
            return findCurrentBestMatchForGenericObject(curv, tmpv, t);

        case SAI_OBJECT_TYPE_ACL_TABLE:
            //return findCurrentBestMatchForAclTable(curv, tmpv, t);
            return findCurrentBestMatchForGenericObject(curv, tmpv, t);

        case SAI_OBJECT_TYPE_ACL_ENTRY:
            //return findCurrentBestMatchForAclEntry(curv, tmpv, t);
            return findCurrentBestMatchForGenericObject(curv, tmpv, t);

        case SAI_OBJECT_TYPE_BUFFER_POOL:
            //return findCurrentBestMatchForBufferPool(curv, tmpv, t);
            return findCurrentBestMatchForGenericObject(curv, tmpv, t);

        case SAI_OBJECT_TYPE_BUFFER_PROFILE:
            return findCurrentBestMatchForBufferProfile(curv, tmpv, t);

        case SAI_OBJECT_TYPE_POLICER:
            return findCurrentBestMatchForPolicer(curv, tmpv, t);

        case SAI_OBJECT_TYPE_TRAP_GROUP:
            return findCurrentBestMatchForHostifTrapGroup(curv, tmpv, t);

            /*
             * Non object id cases
             */

        case SAI_OBJECT_TYPE_NEIGHBOR:
            return findCurrentBestMatchForNeighborEntry(curv, tmpv, t);

        case SAI_OBJECT_TYPE_ROUTE:
            return findCurrentBestMatchForRouteEntry(curv, tmpv, t);

        case SAI_OBJECT_TYPE_FDB:
            return findCurrentBestMatchForFdbEntry(curv, tmpv, t);

        case SAI_OBJECT_TYPE_SWITCH:
            return findCurrentBestMatchForSwitch(curv, tmpv, t);

        case SAI_OBJECT_TYPE_TRAP:
            return findCurrentBestMatchForTrap(curv, tmpv, t);

        case SAI_OBJECT_TYPE_VLAN:
            return findCurrentBestMatchForVlan(curv, tmpv, t);

        default:

            SWSS_LOG_ERROR("%s is not supported yet for find", t->str_object_type.c_str());

            return nullptr;
    }

    // TODO we could also implement generic best match method
    // based on attribute types and metadata, this should the way
    // to go, and have only specific find methods for special cases
    //
    // TODO we need to implement for identical object per object
    // heuristic like going along the path and counding how many
    // times object is used, or on which "matched" port/object
    // is used, or on how many routes etc, of course in both
    // trees
}

/**
 * @brief Process SAI object for ASIC view transition
 *
 * Purpose of this function is to find matching SAI
 * object in current view corresponding to new temporary
 * view for which we want to make switch current ASIC
 * configuration.
 *
 * This function is recursive since it checks all object
 * attributes including attributes that contain other
 * objects which at this stage may not be processed yet.
 *
 * Processing may result in different actions:
 *
 * - no action is taken if objects are the same
 * - update existing object for new attributes if possible
 * - remove current object and create new object if
 *   updating current attributes is not possible or
 *   best matching object was not fount in current view
 *
 * All those actions will be generated "in memory" no actual
 * SAI ASIC operations will be performed at this stage.
 * After entire object dependency graph will be processed
 * and consistent, list of generated actions will be executed
 * on actual ASIC.
 * This approach is safer than making changes right away
 * since if some object is not supported we will return
 * return but ASIC still will be in consistent state.
 *
 *
 * NOTE: Development is in progress, not all corner cases
 * are supported yet.
 */
void processObjectForViewTransition(
        _In_ AsicView &curv,
        _In_ AsicView &tmpv,
        _In_ std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    if (t->object_status == SAI_OBJECT_STATUS_FINAL)
    {
        // object is in final state, no need to process it
        //
        // TODO what about removed objects? we should not get temp removed object's here
        return;
    }

    SWSS_LOG_INFO("processing temp %s %s", t->str_object_type.c_str(), t->str_object_id.c_str());

    // first we need to make sure if all attributes of this temp object are
    // in FINAL or MATCHED state, then we can process this object and find
    // best match in current asic view

    // TODO for non object id, we need also process object id present in object id struct entry

    for (auto &at: t->attrs)
    {
        auto &attribute = at.second;

        SWSS_LOG_INFO("attr %s", attribute->getStrAttrId().c_str());

        const auto meta = attribute->getAttrMetadata();

        const sai_attribute_t &attr = *attribute->getSaiAttr();

        uint32_t count = 0;
        const sai_object_id_t *objectIdList = NULL;

        // TODO move this to separate method for reuse

        switch (meta->serializationtype)
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

            default:
                // attribute is not object id
                continue;
        }

        // for each object id in attributes go recursively to match those objects

        for (uint32_t j = 0; j < count; j++)
        {
            sai_object_id_t vid = objectIdList[j];

            if (vid == SAI_NULL_OBJECT_ID)
            {
                continue;
            }

            SWSS_LOG_INFO("- processing attr VID 0x%lx", vid);

            auto tempParent = tmpv.oOids.at(vid);

            processObjectForViewTransition(curv, tmpv, tempParent); // recursion

            /*
             * Temporary object here is never changed, even if we do recursion
             * here all that could been removed are current objects in current
             * view tree so we don't need to worry about any temporary object
             * removal.
             */
        }
    }

    /*
     * For non object id types like neighbor or route they have object id
     * inside object struct, they also need to be processed, in SAI 1.0 this
     * can be automated since we have metadata for those structs.
     */

    if (t->getObjectType() == SAI_OBJECT_TYPE_NEIGHBOR)
    {
        auto structObject = tmpv.oOids.at(t->meta_key.key.neighbor_entry.rif_id);

        processObjectForViewTransition(curv, tmpv, structObject); // recursion
    }
    else if (t->getObjectType() == SAI_OBJECT_TYPE_ROUTE)
    {
        auto structObject = tmpv.oOids.at(t->meta_key.key.route_entry.vr_id);

        processObjectForViewTransition(curv, tmpv, structObject); // recursion
    }
    else if (t->getObjectType() == SAI_OBJECT_TYPE_FDB)
    {
        /*
         * In SAI 0.9.4 FDB don't contains any object IDs so no extra work
         * required at this stage. As SAI 1.0 this will change.
         */
    }

    /*
     * now since all object ids (VIDs) were processed for current object
     * we can try to find current best match
     */

    std::shared_ptr<SaiObj> currentBestMatch = findCurrentBestMatch(curv, tmpv, t);

    /*
     * TODO XXX
     *
     * So there will be interesting problem, when we don't find best matching
     * object, but actual object will exist, and it will have the same KEY attribute
     * like, and some other attribute will be create_only and it will be differerent
     * then we can't just create temporary object because of key will match and we
     * have conflict, so we need first in this case remove prebious object with key
     *
     * so there are 2 options here:
     *
     * - eather our finding best match function will always include matching keys
     *   (if more keys, they all should (or any?) match should be right away taken) and this
     *   should be only object returned on list possible candidates as best match
     *   then below logic should figure out that we cant do "set" and we need to
     *   destroy object and destroy it now, and create temporary later
     *   this means that finding best logic can return objects that are not
     *   upgradable "cant do set"
     *
     * - or secnd solution, in this case, finding best match will not include
     *   this object on list, since other create only attribute will be different
     *   which also means we need to destroy and recreate, but in  this case
     *   finding best match will figure this out and will destroy object
     *   on the way since it will know that set transition is not possible
     *   so here when we find best match (other object) in that
     *   case we always will be able to do "SET", or when no object
     *   will be returned then we can just righ away create this object
     *   since all keys objects were removed by finding best match
     *
     * in both cases logic is the same, someone needs to figure out wheter
     * updating object and set is possible or wheter we leave object alone,
     * or we delete it before creating new one object
     *
     * preferebly this logic could be in both but that duplicates compare actions
     * i would rather want this in finding best match, but that would modify
     * current view which i dont like much
     * and here it seems like to much hassle since after finding best match
     * here we would have simple operations
     *
     * currently KEY are in: vlan, queue, port, hostif_trap
     * - port - hw lane, we don't need to worry since w match them all
     * - queue - all other attributes can be set
     * - vlan - all ather attributes can be set
     * - hostif_trap - all other attributes can be set
     *
     *   we could add a check for that in sanity
     */

    if (currentBestMatch == nullptr)
    {
        SWSS_LOG_INFO("failed to find best match %s in current view", t->str_object_type.c_str());

        // TODO in this case we need to produce operations to CREATE this object
        // since we were not able to match current one

        SWSS_LOG_ERROR("finding current best match for %s %s failed, not supported yet, FIXME",
                t->str_object_type.c_str(), t->str_object_id.c_str());

        throw std::runtime_error("finding current best match failed, not supported yet, FIXME");
    }

    // we need to 2 two runs, for not matched parameters since if first can be updated
    // but second can't then we will end up modyfying current view and there will be not going back

    SWSS_LOG_INFO("found best match %s: current: %s temp: %s",
            currentBestMatch->str_object_type.c_str(),
            currentBestMatch->str_object_id.c_str(),
            t->str_object_id.c_str());

    // all parents (if any) are in final state here, we could now search for best match against current view
    // that some of the objects in temp final stat could been created so they will not
    // exist in current view (since all creation is done after all matching, so it may cause
    // problems for finding RID for compare

    // after finding best match we need to determine whether current object can be updated to
    // "this temp" object or whether current needs to be destroyed and recreated according to temp

    std::set<sai_attr_id_t> processedAttributes;

    // matched objects can have different attributes

    // This is first pass of temporary object to see what action is needed

    // TODO even with current best matching, update maybe not possible
    // so we are actually will be double checking here attributes
    // so actual logic needs to be changed so first pass will actually
    // find best match that is "possible" to update to temporary view
    // if update will be needed

    for (auto &at: t->attrs)
    {
        auto &ta = at.second;

        SWSS_LOG_INFO("first pass (temp): attr %s", ta->getStrAttrId().c_str());

        const auto meta = ta->getAttrMetadata();

        const sai_attribute_t &attr = *ta->getSaiAttr();

        processedAttributes.insert(attr.id); // mark id as processed

        // on all non object id we can compare attributes just by compare serialized value

        if (currentBestMatch->hasAttr(attr.id))
        {
            // same attribute exists on current and temp view, check if it's the same

            auto ca = currentBestMatch->getAttr(attr.id);

            if (hasEqualAttribute(curv, tmpv, currentBestMatch, t, attr.id))
            {
                /*
                 * Attributes are equal so go for next attribute
                 */

                continue;
            }

            // here we need to check if it's possible to update attribute value
            // ot whether current object needs to be destroyed

            SWSS_LOG_ERROR("attr %s value current %s vs temp %s, FIXME",
                    meta->attridname,
                    ca->getStrAttrValue().c_str(),
                    ta->getStrAttrValue().c_str());

            throw std::runtime_error("attr value current vs temp is different, FIXME");
        }
        else
        {
            // this attribute is missing from current object but is present in temporary one
            //
            // NOTE: we can also check if missing attribute has default value and whether this
            // default value is the same as in temporary object, then we can skip this update

            // TODO check if it's possible to set this attribute or whether current
            // best match object must be removed
            SWSS_LOG_ERROR("attr %s is missing from best match, FIXME", meta->attridname);

            throw std::runtime_error("attr is missing from best match, FIXME");
        }
    }

    // best match can have more attributes than current temporary object
    // let see if we can bring them to default value if possible

    for (auto &ac: currentBestMatch->attrs)
    {
        auto &ca = ac.second;

        const auto meta = ca->getAttrMetadata();

        const sai_attribute_t &attr = *ca->getSaiAttr();

        if (processedAttributes.find(attr.id) != processedAttributes.end())
        {
            // this attibute was processed in previous temporary attributes processing
            continue;
        }

        SWSS_LOG_INFO("first pass (curr): attr %s", ca->getStrAttrId().c_str());

        // this attrbute is not present in temporary object, but is present in current
        // object, we should see if we can bring it to default value or whether
        // current object should be removed if this attribute is CREATE_ONLY
        //
        // NOTE: this can also be object id attribute
        //
        // TODO put those objects also in temp view?

        SWSS_LOG_ERROR("attr %s is present in current object but missing from temporary, FIXME", meta->attridname);

        throw std::runtime_error("attr is present in current object but missing from temporary, FIXME");
    }

    // NOTE if we are here this means that match was sucessfull and we can update from
    // current to temp, some actions maybe required, also current object may be in
    // removed state, so new object will need to be created

    // TODO generate update actions and updates if required
    //
    // TODO we probably need second stage to do the update in current tree view if matching will be successfull

    if ((t->object_status == SAI_OBJECT_STATUS_NOT_PROCESSED &&
        currentBestMatch->object_status == SAI_OBJECT_STATUS_NOT_PROCESSED) ||
        (t->object_status == SAI_OBJECT_STATUS_MATCHED &&
        currentBestMatch->object_status == SAI_OBJECT_STATUS_MATCHED))
    {
        // if we are here, move object status to final

        t->object_status = SAI_OBJECT_STATUS_FINAL;
        currentBestMatch->object_status = SAI_OBJECT_STATUS_FINAL;

        if (t->oidObject)
        {
            // populate rid/vid map
            // TODO support case when RID don't exists

            sai_object_id_t tvid = t->getVid();
            sai_object_id_t cvid = currentBestMatch->getVid();
            sai_object_id_t rid = curv.vidToRid.at(cvid);

            SWSS_LOG_INFO("remapped current VID 0x%lx to temp VID 0x%lx using RID 0x%lx", cvid, tvid, rid);

            tmpv.ridToVid[rid] = tvid;
            tmpv.vidToRid[tvid] = rid;
        }
        else
        {
            // TODO this can be optimized
            // by having double map by object type and by string map

            if (t->getObjectType() == SAI_OBJECT_TYPE_NEIGHBOR)
            {
                SWSS_LOG_INFO("remapped %s current %s to temp %s",
                        sai_serialize_object_type(t->getObjectType()).c_str(),
                        currentBestMatch->str_object_id.c_str(),
                        t->str_object_id.c_str());

                tmpv.neighborIdMap[t->str_object_id] = currentBestMatch->str_object_id;
                curv.neighborIdMap[currentBestMatch->str_object_id] = t->str_object_id;

                return;
            }
            else if (t->getObjectType() == SAI_OBJECT_TYPE_ROUTE)
            {
                SWSS_LOG_INFO("remapped %s current %s to temp %s",
                        sai_serialize_object_type(t->getObjectType()).c_str(),
                        currentBestMatch->str_object_id.c_str(),
                        t->str_object_id.c_str());

                tmpv.routeIdMap[t->str_object_id] = currentBestMatch->str_object_id;
                curv.routeIdMap[currentBestMatch->str_object_id] = t->str_object_id;

                return;
            }
            else if (t->getObjectType() == SAI_OBJECT_TYPE_FDB)
            {
                SWSS_LOG_INFO("remapped %s current %s to temp %s",
                        sai_serialize_object_type(t->getObjectType()).c_str(),
                        currentBestMatch->str_object_id.c_str(),
                        t->str_object_id.c_str());

                tmpv.fdbIdMap[t->str_object_id] = currentBestMatch->str_object_id;
                curv.fdbIdMap[currentBestMatch->str_object_id] = t->str_object_id;

                return;
            }
            else if (t->getObjectType() == SAI_OBJECT_TYPE_SWITCH)
            {
                SWSS_LOG_INFO("remapped %s current %s to temp %s",
                        sai_serialize_object_type(t->getObjectType()).c_str(),
                        currentBestMatch->str_object_id.c_str(),
                        t->str_object_id.c_str());

                tmpv.switchIdMap[t->str_object_id] = currentBestMatch->str_object_id;
                curv.switchIdMap[currentBestMatch->str_object_id] = t->str_object_id;

                return;
            }
            else if (t->getObjectType() == SAI_OBJECT_TYPE_TRAP)
            {
                SWSS_LOG_INFO("remapped %s current %s to temp %s",
                        sai_serialize_object_type(t->getObjectType()).c_str(),
                        currentBestMatch->str_object_id.c_str(),
                        t->str_object_id.c_str());

                /*
                 * We don't need actual matching since traps have the exact values
                 */

                return;
            }
            else if (t->getObjectType() == SAI_OBJECT_TYPE_VLAN)
            {
                SWSS_LOG_INFO("remapped %s current %s to temp %s",
                        sai_serialize_object_type(t->getObjectType()).c_str(),
                        currentBestMatch->str_object_id.c_str(),
                        t->str_object_id.c_str());

                /*
                 * We don't need actual matching since vlans have the exact values
                 */

                return;
            }

            SWSS_LOG_ERROR("object %s is not oid object, not supported yet, FIXME", t->str_object_type.c_str());

            throw std::runtime_error("object %s is not oid object, not supported yet, FIXME");
        }

        return;
    }
    else
    {
        SWSS_LOG_ERROR("unexpected temporary object status: %d", t->object_status);

        throw std::runtime_error("unexpected temporary object status");
    }

    // TODO we need to compare readl id's here (if object were created then hmm how?)
    // and set object status to final, if object was created we need extra flag to mention that
    // object was created and RID don't exist's yet
    //
    // TODO when SET operation is required for objet ID we also need to update references
    // in current view

    // NOTE: after processing each object in attribute we need to check if this object was
    // removed (because it could - NO this is wrong, since in temp view nothig is removed
    // temp view is const, only current view can be changed

    // TODO how to keep RID which are not created yet?

    // TODO check all attributes first ? if all attributes are in final/matched state ?
    // in generic case current object can be removed if parents also will be removed

    // TODO for processing non object id we need to check objects in entry types

    SWSS_LOG_ERROR("implement ME here, find best match!, FIXME");

    throw std::runtime_error("implement ME here, find best match!, FIXME");

    // and then generate diff etc, create new if not existing
    //
    // TODO set state here as final since object was fully processed (on both temp and current
    // even if current was created dynamically in memory)
    // hmm, maybe creation can be done right away ? since it will populate everything ?
    // and in case on failure in next comparison it will be taken into account anyway

    // TODO when we compare attributes, and one attribute is missing, we can check if
    // it have default value, and if this value is the same in current and temp view
    // this will save doing "set" operation or remove/create if attribute is create only

    // TODO we need 2 passes - first to check if all objects are in
    // matched / final state on temp view, that will determine
    // if object can be updated
    //
    // second pass actual fix pass for current view
    //
    // TODO if some object can't be removed from current and it's missing from
    // temp, then it needs to be transfered to temp as well
}

void processObjectType(
        _In_ AsicView &curv,
        _In_ AsicView &tmpv,
        _In_ sai_object_type_t object_type)
{
    SWSS_LOG_ENTER();

    auto objects = tmpv.getObjectsByObjectType(object_type);

    for (const auto &o: objects)
    {
        processObjectForViewTransition(curv, tmpv, o);
    }

    SWSS_LOG_NOTICE("processed %s", sai_serialize_object_type(object_type).c_str());
}

void checkSwitch(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView)
{
    SWSS_LOG_ENTER();

    /*
     * Current logic supports only 1 instance of switch
     * so in both view we should have only 1 instance
     * of switch and they must match.
     */

    if (currentView.soSwitches.size() != 1)
    {
        SWSS_LOG_ERROR("current view don't contain any switch");

        throw std::runtime_error("current view don't contain any switch object");
    }

    if (temporaryView.soSwitches.size() != 1)
    {
        SWSS_LOG_ERROR("temporary view don't contain any switch object");

        throw std::runtime_error("temporary view don't contain any switch object");
    }

    /*
     * In current SAI 0.9.4 switch is non object id so we don't
     * need to ma VID/RID for it, but in SAI 1.0 it will be
     * object id so we need to change our strategy here.
     *
     * Later on we can support in 0.9.4 case where one or both
     * switches can be missing since no attributes were set.
     */
}

sai_status_t applyViewTransition(
        _In_ AsicView &current,
        _In_ AsicView &temp)
{
    SWSS_LOG_ENTER();

    // match all oids that are the same in previous and current view

    matchOids(current, temp);

    checkSwitch(current, temp);

    checkMatchedPorts(temp);

    /*
     * Process all objects
     */

//    for (auto &obj: temp.soAll)
//    {
//        processObjectForViewTransition(current, temp, obj.second);
//    }
//
//    return SAI_STATUS_SUCCESS;

    // we start processing from ports since they are all matched
    // but this is not necessary

    processObjectType(current, temp, SAI_OBJECT_TYPE_PORT);
    processObjectType(current, temp, SAI_OBJECT_TYPE_SWITCH);
    processObjectType(current, temp, SAI_OBJECT_TYPE_HOST_INTERFACE);
    processObjectType(current, temp, SAI_OBJECT_TYPE_VIRTUAL_ROUTER);
    processObjectType(current, temp, SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    processObjectType(current, temp, SAI_OBJECT_TYPE_NEXT_HOP);
    processObjectType(current, temp, SAI_OBJECT_TYPE_BUFFER_POOL);
    processObjectType(current, temp, SAI_OBJECT_TYPE_BUFFER_PROFILE);
    processObjectType(current, temp, SAI_OBJECT_TYPE_PRIORITY_GROUP);
    processObjectType(current, temp, SAI_OBJECT_TYPE_QUEUE);
    processObjectType(current, temp, SAI_OBJECT_TYPE_NEXT_HOP_GROUP);
    processObjectType(current, temp, SAI_OBJECT_TYPE_WRED);
    processObjectType(current, temp, SAI_OBJECT_TYPE_SCHEDULER);
    processObjectType(current, temp, SAI_OBJECT_TYPE_SCHEDULER_GROUP);
    processObjectType(current, temp, SAI_OBJECT_TYPE_QOS_MAPS);
    processObjectType(current, temp, SAI_OBJECT_TYPE_POLICER);
    processObjectType(current, temp, SAI_OBJECT_TYPE_TRAP_GROUP);
    processObjectType(current, temp, SAI_OBJECT_TYPE_ACL_TABLE);
    processObjectType(current, temp, SAI_OBJECT_TYPE_ACL_ENTRY);
    processObjectType(current, temp, SAI_OBJECT_TYPE_NEIGHBOR);
    processObjectType(current, temp, SAI_OBJECT_TYPE_ROUTE);
    processObjectType(current, temp, SAI_OBJECT_TYPE_FDB);
    processObjectType(current, temp, SAI_OBJECT_TYPE_TRAP);
    processObjectType(current, temp, SAI_OBJECT_TYPE_VLAN);

    /*
     * Iterate via all object types to make sure all objects were processed.
     * We could also just get list of all objects from temp asic view
     * and iterate through them.
     */

    for (int ot = SAI_OBJECT_TYPE_NULL; ot < SAI_OBJECT_TYPE_MAX; ot++)
    {
        /*
         * We don't need to skip hostif packet and fdb flush since those are
         * not real objects and they never will be in object list.
         */

        processObjectType(current, temp, (sai_object_type_t)ot);
    }

    // TODO we will need in temp view option str_matched_id
    // string if empty than it mean it's the same
    // and then we will need second pass on all objects to see
    // if we have the same attributes

    return SAI_STATUS_SUCCESS;
    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t internalSyncdApplyView()
{
    sai_status_t status;

    SWSS_LOG_ENTER();

    /*
     * Initialize rand for future candidate object selection if necessary.
     */

    std::srand((unsigned int)std::time(0));

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

    status = checkObjectsStatus(temp);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("object status failed on temp view: %s", sai_serialize_status(status).c_str());

        return status;
    }

    SWSS_LOG_NOTICE("all temporary view objects were processed to FINAL state");

    // TODO this check must be different
    // for objects not processed they needs to be removed
    // and actions needs to be generated!
    status = checkObjectsStatus(current);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("object status failed on current view: %s", sai_serialize_status(status).c_str());

        return status;
    }

    // since we matched all rid/vid, number should be the same in previous and next view

    /*
     * TODO: This check will not be valid at this point, since some objects
     * could be removed from current view, and new objects could be not created
     * yet on temporary view. This check needs to be removed later.
     */

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
        SWSS_LOG_ERROR("Exception: %s", e.what());
    }

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    return status;
}


// TODO we need to be careful since not all "SET" attrubutes can be set,
// some objects could be destroyed
