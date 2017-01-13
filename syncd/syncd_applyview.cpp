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

void checkMatchedPorts(
        _In_ const AsicView &temp)
{
    SWSS_LOG_ENTER();

    auto ports = temp.getObjectsByObjectType(SAI_OBJECT_TYPE_PORT);

    for (const auto &p: ports)
    {
        // we expect to all ports status to be matched since
        // this is our entry point of compare logic

        if (p->object_status != OBJECT_STATUS_MATCHED)
        {
            SWSS_LOG_ERROR("port %s object status is not matched", p->str_object_id.c_str());

            throw std::runtime_error("not all ports objects status are matched");
        }
    }

    SWSS_LOG_NOTICE("all ports are matched");
}

bool hasEqualAttribute(
        _In_ const std::shared_ptr<SaiObj> &current,
        _In_ const std::shared_ptr<SaiObj> &temporary,
        _In_ sai_attr_id_t id)
{
    SWSS_LOG_ENTER();

    // NOTE: this function should be used only to compare primitive attributes

    if (current->hasAttr(id) && temporary->hasAttr(id))
    {
        return current->getAttr(id)->getStrAttrValue() == temporary->getAttr(id)->getStrAttrValue();
    }

    return false;
}

// TODO best match could find multiple matches, and then in
// main processing loop we could select the one that can
// be the most apriopriate

std::shared_ptr<SaiObj> findCurrentBestMatchForQosMap(
        _In_ const AsicView &curv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    auto qosmaps = curv.getObjectsByObjectType(SAI_OBJECT_TYPE_QOS_MAPS);

    for (const auto &c: qosmaps)
    {
        if (c->object_status != OBJECT_STATUS_NOT_PROCESSED)
        {
            // we are interested only in not processed objects
            // TODO move this to separate method in class
            continue;
        }

        if (!hasEqualAttribute(c, t, SAI_QOS_MAP_ATTR_TYPE))
        {
            continue;
        }

        // TODO this later can be complicated since it can will contain bridge id object id

        if (!hasEqualAttribute(c, t, SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST))
        {
            continue;
        }

        SWSS_LOG_INFO("found best match %s: current: %s temp: %s",
                c->str_object_type.c_str(),
                c->str_object_id.c_str(),
                t->str_object_id.c_str());

        return c;
    }

    SWSS_LOG_INFO("failed to find best match %s in current view", t->str_object_type.c_str());

    return nullptr;
}

std::shared_ptr<SaiObj> findCurrentBestMatch(
        _In_ const AsicView &curv,
        _In_ const std::shared_ptr<SaiObj> t)
{
    SWSS_LOG_ENTER();

    if (t->oidObject)
    {
        if (t->object_status == OBJECT_STATUS_MATCHED)
        {
            // object status is matched so current and tem VID are the same
            // so we can just take object directly

            SWSS_LOG_INFO("found best match for %s %s since object status is MATCHED",
                t->str_object_type.c_str(),
                t->str_object_id.c_str());

            return curv.oOids.at(t->getVid());
        }
    }

    switch (t->meta_key.object_type)
    {
        case SAI_OBJECT_TYPE_QOS_MAPS:

            return findCurrentBestMatchForQosMap(curv, t);

        default:

            SWSS_LOG_ERROR("%s is not supported yet for find", t->str_object_type.c_str());

            return nullptr;
    }
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

    if (t->object_status == OBJECT_STATUS_FINAL)
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

    for (auto &at: t->attrs)
    {
        auto &attribute = at.second;

        SWSS_LOG_INFO(" attr %s", attribute->getStrAttrId().c_str());

        const auto meta = attribute->getAttrMetadata();

        const sai_attribute_t &attr = *attribute->getSaiAttr();

        uint32_t count = 0;
        const sai_object_id_t *objectIdList;

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

            SWSS_LOG_INFO(" - processing attr VID 0x%lx", vid);

            auto tempParent = tmpv.oOids.at(vid);

            processObjectForViewTransition(curv, tmpv, tempParent); // recursion
        }
    }

    std::shared_ptr<SaiObj> currentBestMatch = findCurrentBestMatch(curv, t);

    if (currentBestMatch == nullptr)
    {
        // TODO in this case we need to produce operations to CREATE this object
        // since we were not able to match current one

        SWSS_LOG_ERROR("finding current best match for %s %s failed, not supported yet, FIXME",
                t->str_object_type.c_str(), t->str_object_id.c_str());

        throw std::runtime_error("finding current best match failed, not supported yet, FIXME");
    }

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

    for (auto &at: t->attrs)
    {
        auto &ta = at.second;

        SWSS_LOG_INFO(" first pass (temp): attr %s", ta->getStrAttrId().c_str());

        const auto meta = ta->getAttrMetadata();

        const sai_attribute_t &attr = *ta->getSaiAttr();

        processedAttributes.insert(attr.id); // mark id as processed

        // on all non object id we can compare attributes just by compare serialized value

        if (currentBestMatch->hasAttr(attr.id))
        {
            // same attribute exists on current and temp view, check if it's the same

            auto ca = currentBestMatch->getAttr(attr.id);

            switch (meta->serializationtype)
            {
                case SAI_SERIALIZATION_TYPE_OBJECT_ID:

                    {
                        // TODO we need to check if both RID exist on current and temp

                        sai_object_id_t cvid = ca->getSaiAttr()->value.oid;
                        sai_object_id_t tvid = ta->getSaiAttr()->value.oid;

                        if (cvid == SAI_NULL_OBJECT_ID && tvid == SAI_NULL_OBJECT_ID)
                        {
                            // both id are null, thats find, no need to do any action
                            continue;
                        }

                        if (cvid != SAI_NULL_OBJECT_ID && tvid != SAI_NULL_OBJECT_ID)
                        {
                            sai_object_id_t crid = curv.vidToRid.at(cvid);

                            auto it = tmpv.vidToRid.find(tvid);

                            if (it == tmpv.vidToRid.end())
                            {
                                // TODO this object was created in previous processing and RID
                                // is not yet created, we need to support this scenario

                                SWSS_LOG_ERROR("current VID 0x%lx RID 0x%lx temp VID 0x%lx RID don't exist yet, FIXME", cvid, crid, tvid);

                                throw std::runtime_error("temporary object only have VID, RID was not created yet, FIXME");
                            }

                            sai_object_id_t trid = it->second;

                            if (trid == crid)
                            {
                                // both RID on current and temporary objects are the same, no need to perform any action
                                continue;
                            }

                            SWSS_LOG_ERROR("current VID 0x%lx RID 0x%lx temp VID 0x%lx RID 0x%lx are different, not supported yet, FIXME",
                                    cvid, crid, tvid,trid);

                            throw std::runtime_error("current RID and temp RID are different, FIXME");

                            continue;
                        }

                        SWSS_LOG_ERROR("scenario where current VID 0x%lx and temp VID 0x%lx is not supported, FIXME", cvid, tvid);

                        throw std::runtime_error("scenario where one of current/tmep vid is NULL is not supported, FIXME");
                    }

                    break;

                case SAI_SERIALIZATION_TYPE_OBJECT_LIST:
                case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:

                    // all those object's are in FINAL state but not all of them
                    // may be created yet, so RID maybe missing

                    // TODO here we need to compare RID's

                    SWSS_LOG_ERROR(" - %s is not supported yet for compare",
                            get_attr_info(*meta).c_str());

                    throw std::runtime_error("serialization type is not supported yet");

                default:

                    // for non object id we can just compare serialized value of the attributes

                    if (ca->getStrAttrValue() == ta->getStrAttrValue())
                    {
                        // both attributes values on current and temp view are the same
                        // there is no need for any update
                        continue;
                    }

                    // here we need to check if it's possible to update attribute value
                    // ot whether current object needs to be destroyed

                    SWSS_LOG_ERROR("attr %s value current %s vs temp %s, FIXME",
                            meta->attridname,
                            ca->getStrAttrValue().c_str(),
                            ta->getStrAttrValue().c_str());

                    throw std::runtime_error("attr value current vs temp is different, FIXME");

                    break;
            }
        }
        else
        {
            // this attribute is missing from current object but is present in temporary one

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

        SWSS_LOG_INFO(" first pass (curr): attr %s", ca->getStrAttrId().c_str());

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

    if ((t->object_status == OBJECT_STATUS_NOT_PROCESSED &&
        currentBestMatch->object_status == OBJECT_STATUS_NOT_PROCESSED) ||
        (t->object_status == OBJECT_STATUS_MATCHED &&
        currentBestMatch->object_status == OBJECT_STATUS_MATCHED))
    {
        // if we are here, move object status to final

        t->object_status = OBJECT_STATUS_FINAL;
        currentBestMatch->object_status = OBJECT_STATUS_FINAL;

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
}

void processPorts(
        _In_ AsicView &curv,
        _In_ AsicView &tmpv)
{
    SWSS_LOG_ENTER();

    // we start processing from ports since they are all matched
    // but this is not necessary

    auto ports = tmpv.getObjectsByObjectType(SAI_OBJECT_TYPE_PORT);

    for (const auto &p: ports)
    {
        processObjectForViewTransition(curv, tmpv, p);
    }

    // TODO we need 2 passes - first to check if all objects are in
    // matched / final state on temp view, that will determine
    // if object can be updated
    //
    // second pass actual fix pass for current view
    //
    // TODO if some object can't be removed from current and it's missing from
    // temp, then it needs to be transfered to temp as well
}

sai_status_t applyViewTransition(
        _In_ AsicView &current,
        _In_ AsicView &temp)
{
    SWSS_LOG_ENTER();

    // match all oids that are the same in previous and current view

    matchOids(current, temp);

    checkMatchedPorts(temp);

    processPorts(current, temp);

    SWSS_LOG_NOTICE("ports mapped");

    // TODO start processing matched objects (from ports)

    // TODO we will need in temp view option str_matched_id
    // string if empty than it mean it's the same
    // and then we will need second pass on all objects to see
    // if we have the same attributes

    return SAI_STATUS_NOT_IMPLEMENTED;
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
        SWSS_LOG_ERROR("Exception: %s", e.what());
    }

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    return status;
}


// TODO we need to be careful since not all "SET" attrubutes can be set,
// some objects could be destroyed
