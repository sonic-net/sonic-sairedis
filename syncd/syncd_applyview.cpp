#include "syncd.h"
#include "sairedis.h"
#include "swss/table.h"
#include "swss/logger.h"
#include "swss/dbconnector.h"

#include <algorithm>

/*
 * NOTE: all methods taking current and temporary view could be moved to
 * transition class etc to just use class members instead of passing those
 * everywhere.
 */

typedef enum _sai_object_status_t
{
    /**
     * @brief Object was not processed at all
     *
     * This enum must be declared first.
     */
    SAI_OBJECT_STATUS_NOT_PROCESSED = 0,

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
     *
     * Only current view objects can be set to this status.
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

        SaiAttr(/*{{{*/
                _In_ const std::string &str_attr_id,
                _In_ const std::string &str_attr_value):
            m_str_attr_id(str_attr_id),
            m_str_attr_value(str_attr_value),
            m_meta(NULL)
        {
            SWSS_LOG_ENTER();

            /*
             * We perform deserialize here to have attribute value when we need
             * it, this can include allocated lists, so on destructor we ned to
             * free this memory.
             */

            sai_deserialize_attr_id(str_attr_id, &m_meta);

            m_attr.id = m_meta->attrid;

            sai_deserialize_attr_value(str_attr_value, *m_meta, m_attr, false);

            m_is_object_id_attr = m_meta->allowedobjecttypes.size() > 0;
        }/*}}}*/

        ~SaiAttr()/*{{{*/
        {
            SWSS_LOG_ENTER();

            sai_deserialize_free_attribute_value(m_meta->serializationtype, m_attr);
        }/*}}}*/

        sai_attribute_t* getRWSaiAttr()/*{{{*/
        {
            return &m_attr;
        }/*}}}*/

        const sai_attribute_t* getSaiAttr() const/*{{{*/
        {
            return &m_attr;
        }/*}}}*/

        bool isObjectIdAttr() const/*{{{*/
        {
            return m_is_object_id_attr;
        }/*}}}*/

        const std::string& getStrAttrId() const/*{{{*/
        {
            return m_str_attr_id;
        }/*}}}*/

        const std::string& getStrAttrValue() const/*{{{*/
        {
            return m_str_attr_value;
        }/*}}}*/

        const sai_attr_metadata_t* getAttrMetadata() const/*{{{*/
        {
            return m_meta;
        }/*}}}*/

        void UpdateValue()/*{{{*/
        {
            m_str_attr_value = sai_serialize_attr_value(*m_meta, m_attr);
        }/*}}}*/

        /**
         * @brief Get OID list from attribute
         *
         * Based on serialization type attribute may be oid attribute, oid list
         * attribute or non oid attribute. This method will extract all those oids from
         * this attribute and return as vector.  This is handy when we nedd processin
         * oids per attribute.
         *
         * TODO: this could be SaiAttr method
         */
        std::vector<sai_object_id_t> getOidListFromAttribute() const/*{{{*/
        {
            SWSS_LOG_ENTER();

            const sai_attribute_t &attr = m_attr;

            uint32_t count = 0;

            const sai_object_id_t *objectIdList = NULL;

            /*
             * For ACL fields and actions we need to use enable flag as indicator,
             * since when attribute is disabled then parameter can be garbage.
             */

            switch (m_meta->serializationtype)
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

                    if (attr.value.aclfield.enable)
                    {
                        count = 1;
                        objectIdList = &attr.value.aclfield.data.oid;
                    }

                    break;

                case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:

                    if (attr.value.aclfield.enable)
                    {
                        count = attr.value.aclfield.data.objlist.count;
                        objectIdList = attr.value.aclfield.data.objlist.list;
                    }

                    break;

                case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:

                    if (attr.value.aclaction.enable)
                    {
                        count = 1;
                        objectIdList = &attr.value.aclaction.parameter.oid;
                    }

                    break;

                case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:

                    if (attr.value.aclaction.enable)
                    {
                        count = attr.value.aclaction.parameter.objlist.count;
                        objectIdList = attr.value.aclaction.parameter.objlist.list;
                    }

                    break;

                default:

                    /*
                     * Attribute not contain any object ids.
                     */

                    break;
            }

            // TODO check will it work with NULL pointer

            std::vector<sai_object_id_t> result(objectIdList, objectIdList + count);

            return result;
        }/*}}}*/

    private:

        SaiAttr(const SaiAttr&);
        SaiAttr& operator=(const SaiAttr&);

        const std::string m_str_attr_id;
        std::string m_str_attr_value;

        const sai_attr_metadata_t* m_meta;
        sai_attribute_t m_attr;
        bool m_is_object_id_attr;
};

class SaiObj
{
    public:

        SaiObj():/*{{{*/
            oidObject(false),
            createdObject(false),
            defaultObject(false),
            m_object_status(SAI_OBJECT_STATUS_NOT_PROCESSED)
        {
        }/*}}}*/

        // TODO move to methods
        std::string str_object_type;
        std::string str_object_id;

        sai_object_meta_key_t meta_key;

        bool oidObject;

        /**
         * @brief This will indicate whether obect was created and it will
         * indicate that currently there is no RID for it.
         */
        bool createdObject;

        /**
         * @brief This will indicate wheter object is default object, like
         * default STP instance, default Virtual Router, default Trap Group etc
         * so it can't be removed or created, it attributes can be brought to
         * default values.
         */
        bool defaultObject;

        const std::unordered_map<sai_attr_id_t, std::shared_ptr<SaiAttr>>& getAllAttributes() const/*{{{*/
        {
            return m_attrs;
        }/*}}}*/

        std::shared_ptr<SaiAttr> getSaiAttr(/*{{{*/
                _In_ sai_attr_id_t id) const
        {
            auto it = m_attrs.find(id);

            if (it == m_attrs.end())
            {
                SWSS_LOG_ERROR("object %s has no attribute %d", str_object_id.c_str(), id);

                for (const auto &ita: m_attrs)
                {
                    const auto &a = ita.second;

                    SWSS_LOG_ERROR("%s: %s", a->getStrAttrId().c_str(), a->getStrAttrValue().c_str());
                }

                throw std::runtime_error("object has no attribute");
            }

            return it->second;
        }/*}}}*/

        void setObjectStatus(/*{{{*/
                _In_ sai_object_status_t os)
        {
            SWSS_LOG_ENTER();

            m_object_status = os;

        }/*}}}*/

        sai_object_status_t getObjectStatus() const/*{{{*/
        {
            return m_object_status;
        }/*}}}*/

        sai_object_type_t getObjectType() const/*{{{*/
        {
            return meta_key.object_type;
        }/*}}}*/

        void setAttr(/*{{{*/
                _In_ std::shared_ptr<SaiAttr> a)
        {
            m_attrs[a->getSaiAttr()->id] = a;
        }/*}}}*/

        bool hasAttr(/*{{{*/
                _In_ sai_attr_id_t id) const
        {
            return m_attrs.find(id) != m_attrs.end();
        }/*}}}*/

        sai_object_id_t getVid() const/*{{{*/
        {
            if (oidObject)
            {
                return meta_key.key.object_id;
            }

            SWSS_LOG_ERROR("object %s it not object id type", str_object_id.c_str());

            throw std::runtime_error("object is not object id type");
        }/*}}}*/

        /*
         * TODO we need dependency tree if we want to remove objects which have
         * reference count not zero.
         */

    private:

        sai_object_status_t m_object_status;

        std::unordered_map<sai_attr_id_t, std::shared_ptr<SaiAttr>> m_attrs;

        SaiObj(const SaiObj&);
        SaiObj& operator=(const SaiObj&);
};

typedef std::unordered_map<sai_object_id_t, sai_object_id_t> ObjectIdMap;
typedef std::unordered_map<std::string, std::shared_ptr<SaiObj>> StrObjectIdToSaiObjectHash;
typedef std::unordered_map<sai_object_id_t, std::shared_ptr<SaiObj>> ObjectIdToSaiObjectHash;

class AsicView
{
    public:

        AsicView()/*{{{*/
        {
            // empty intentionally
        }/*}}}*/

        // TODO should this be static method that returns AsicViewObject?
        void fromDump(/*{{{*/
                _In_ const swss::TableDump &dump)
        {
            SWSS_LOG_ENTER();

            /*
             * Input should be also existing obejcts, so they could be created
             * here right away but we would need VIDs as well.
             */

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

                        /*
                         * Since neighbor struct object contains RIF ID, we
                         * need to increase vid reference With new metadata for
                         * SAI 1.0 this can be done in generic way for all non
                         * object ids.
                         */

                        m_vidReference[o->meta_key.key.neighbor_entry.rif_id] += 1;

                        break;

                    case SAI_OBJECT_TYPE_ROUTE:
                        sai_deserialize_route_entry(o->str_object_id, o->meta_key.key.route_entry);
                        soRoutes[o->str_object_id] = o;

                        /*
                         * Since route struct object contains VR ID, we need to
                         * increase vid reference.
                         */

                        m_vidReference[o->meta_key.key.route_entry.vr_id] += 1;

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

                        // TODO move to metadata
                        o->oidObject = true;

                        /*
                         * Here is only object VID declaration, since we don't
                         * know what objects were processed previously but on
                         * some of previous object attributes this VID could be
                         * used, so value can be already greater than zero, but
                         * here we need to just mark that vid exists in
                         * vidReference is it has not been set yet at all.
                         */

                        m_vidReference[o->meta_key.key.object_id] += 0;

                        break;
                }

                soAll[o->str_object_id] = o;
                sotAll[o->meta_key.object_type][o->str_object_id] = o;

                populateAttributes(o, key.second);
            }

            /*
             * If tere is no switch, then create one by default with all empty
             * values.
             */

            if (soSwitches.size() == 0)
            {
                const auto &o = createSwitchObject();

                soAll[o->str_object_id] = o;
                soSwitches[o->str_object_id] = o;
                sotAll[o->meta_key.object_type][o->str_object_id] = o;
            }

            {
                // VLAN 1 also needs to be present

                std::shared_ptr<SaiObj> o = std::make_shared<SaiObj>();

                o->str_object_type  = sai_serialize_object_type(SAI_OBJECT_TYPE_VLAN);
                o->str_object_id    = sai_serialize_vlan_id(1);

                o->meta_key.object_type = SAI_OBJECT_TYPE_VLAN;
                o->meta_key.key.vlan_id = 1;

                if (soVlans.find(o->str_object_id) == soVlans.end())
                {
                    /*
                     * Vlan 1 don't exist in current view, save it as default
                     */

                    soVlans[o->str_object_id] = o;
                    soAll[o->str_object_id] = o;
                    sotAll[o->meta_key.object_type][o->str_object_id] = o;
                }
            }

            {
                // create all default traps in both views

                for (int index = 0; metadata_enum_sai_hostif_trap_type_t.valuesnames[index] != NULL; ++index)
                {
                    sai_hostif_trap_id_t trap_id = (sai_hostif_trap_id_t)metadata_enum_sai_hostif_trap_type_t.values[index];

                    std::shared_ptr<SaiObj> o = std::make_shared<SaiObj>();

                    o->str_object_type  = sai_serialize_object_type(SAI_OBJECT_TYPE_TRAP);
                    o->str_object_id    = sai_serialize_hostif_trap_id(trap_id);

                    o->meta_key.object_type = SAI_OBJECT_TYPE_TRAP;
                    o->meta_key.key.trap_id = trap_id;

                    if (soTraps.find(o->str_object_id) == soTraps.end())
                    {
                        soTraps[o->str_object_id] = o;
                        soAll[o->str_object_id] = o;
                        sotAll[o->meta_key.object_type][o->str_object_id] = o;
                    }
                }
            }

        }/*}}}*/

        std::shared_ptr<SaiObj> createTrapObject(/*{{{*/
                _In_ const std::string &str_object_type,
                _In_ const std::string &str_object_id)
        {
            SWSS_LOG_ENTER();

            std::shared_ptr<SaiObj> o = std::make_shared<SaiObj>();

            o->str_object_type  = str_object_type;
            o->str_object_id    = str_object_id;

            sai_deserialize_object_type(o->str_object_type, o->meta_key.object_type);

            sai_deserialize_hostif_trap_id(o->str_object_id, o->meta_key.key.trap_id);
            soTraps[o->str_object_id] = o;

            if (soAll.find(o->str_object_id) != soAll.end())
            {
                SWSS_LOG_ERROR("FATAL: trap %s already exists in current view", o->str_object_id.c_str());

                throw std::runtime_error("FATAL: trap already exists in current view");
            }

            soAll[o->str_object_id] = o;
            sotAll[o->meta_key.object_type][o->str_object_id] = o;

            return o;
        }/*}}}*/

        void releaseVidReference(/*{{{*/
                _In_ sai_object_id_t vid)
        {
            SWSS_LOG_ENTER();

            if (vid == SAI_NULL_OBJECT_ID)
            {
                return;
            }

            auto it = m_vidReference.find(vid);

            if (it == m_vidReference.end())
            {
                SWSS_LOG_ERROR("vid 0x%lx don't exist in reference map", vid);

                throw std::runtime_error("vid don't exist in reference map");
            }

            int referenceCount = --(it->second);

            if (referenceCount < 0)
            {
                SWSS_LOG_ERROR("vid 0x%lx decreased reference too many times: %d, referenceCount, BUG", vid, referenceCount);

                throw std::runtime_error("vid decreased reference too many times, BUG");
            }

            SWSS_LOG_INFO("released vid 0x%lx refrence from %d to %d", vid, referenceCount + 1, referenceCount);
        }/*}}}*/

        void bindNewVidReference(/*{{{*/
                _In_ sai_object_id_t vid)
        {
            SWSS_LOG_ENTER();

            if (vid == SAI_NULL_OBJECT_ID)
            {
                return;
            }

            /*
             * If we are doing bind on new vid reference that VID needs to
             * exist int current view, eather object was matched or new object
             * was created.
             *
             * TODO not sure if this will have impact on some other object
             * processing since if object was matched or created then this VID
             * can be found in other attribures compring them, and it will get
             * NULL instead RID.
             */

            auto it = m_vidReference.find(vid);

            if (it == m_vidReference.end())
            {
                SWSS_LOG_ERROR("vid 0x%lx don't exist in reference map", vid);

                throw std::runtime_error("vid don't exist in reference map");
            }

            int referenceCount = ++(it->second);

            SWSS_LOG_INFO("increased vid 0x%lx refrence from %d to %d", vid, referenceCount - 1, referenceCount);
        }/*}}}*/

        int getVidReferenceCount(/*{{{*/
                _In_ sai_object_id_t vid) const
        {
            SWSS_LOG_ENTER();

            auto it = m_vidReference.find(vid);

            if (it != m_vidReference.end())
            {
                return it->second;
            }

            return -1;
        }/*}}}*/

        void insertNewVidReference(/*{{{*/
                _In_ sai_object_id_t vid)
        {
            SWSS_LOG_ENTER();

            auto it = m_vidReference.find(vid);

            if (it != m_vidReference.end())
            {
                SWSS_LOG_ERROR("vid 0x%lx already exist in reference map, BUG", vid);

                throw std::runtime_error("vid already exist in reference map, BUG");
            }

            m_vidReference[vid] = 0;

            SWSS_LOG_INFO("inserted vid 0x%lx as reference", vid);
        }/*}}}*/

        // TODO convert to something like nonObjectIdMap
        StrObjectIdToSaiObjectHash soSwitches;
        StrObjectIdToSaiObjectHash soFdbs;
        StrObjectIdToSaiObjectHash soNeighbors;
        StrObjectIdToSaiObjectHash soRoutes;
        StrObjectIdToSaiObjectHash soVlans;
        StrObjectIdToSaiObjectHash soTraps;
        StrObjectIdToSaiObjectHash soOids;
        StrObjectIdToSaiObjectHash soAll;

    private:
        std::map<sai_object_type_t, StrObjectIdToSaiObjectHash> sotAll;
    public:

        ObjectIdToSaiObjectHash oOids;

        // on temp view this needs to be used for actual NEW rids created
        // and then reused with rid mapping to create new rid/vid map
        ObjectIdMap ridToVid;
        ObjectIdMap vidToRid;
        ObjectIdMap removedVidToRid;

        std::map<sai_object_type_t, std::unordered_map<std::string,std::string>> nonObjectIdMap;

        sai_object_id_t cpuPortRid;
        sai_object_id_t defaultVirtualRouterRid;
        sai_object_id_t defaultTrapGroupRid;
        sai_object_id_t defaultStpInstanceRid;

        std::vector<std::shared_ptr<SaiObj>> getObjectsByObjectType(/*{{{*/
                _In_ sai_object_type_t object_type) const
        {
            SWSS_LOG_ENTER();

            // order on list is random
            std::vector<std::shared_ptr<SaiObj>> list;

            // we need to use find, since object type may not exist
            auto it = sotAll.find(object_type);

            if (it == sotAll.end())
            {
                return list;
            }

            for (const auto &p: it->second)
            {
                list.push_back(p.second);
            }

            return list;
        }/*}}}*/

        /*
         * TODO instead of iterating via all list all the time lets have this
         * as set, and then when we set object status we just remove it from
         * set, but there is problem since status in set on object not on view
         */

        std::vector<std::shared_ptr<SaiObj>> getNotProcessedObjectsByObjectType(/*{{{*/
                _In_ sai_object_type_t object_type) const
        {
            SWSS_LOG_ENTER();

            // order on list is random
            std::vector<std::shared_ptr<SaiObj>> list;

            // we need to use find, since object type may not exist
            auto it = sotAll.find(object_type);

            if (it == sotAll.end())
            {
                return list;
            }

            for (const auto &p: it->second)
            {
                if (p.second->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
                {
                    list.push_back(p.second);
                }
            }

            return list;
        }/*}}}*/

        std::vector<std::shared_ptr<SaiObj>> getAllNotProcessedObjects() const/*{{{*/
        {
            SWSS_LOG_ENTER();

            // order on list is random
            std::vector<std::shared_ptr<SaiObj>> list;

            for (const auto &p: soAll)
            {
                if (p.second->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
                {
                    list.push_back(p.second);
                }
            }

            return list;
        }/*}}}*/

        void createDummy(/*{{{*/
                _In_ sai_object_id_t rid,
                _In_ sai_object_id_t vid)
        {
            SWSS_LOG_ENTER();

            /*
             * Function creates dummy objects, which is used to indicate
             * that this OID object exist in current view. This is used
             * for existing objects, like cpu port, default trap group.
             */

            sai_object_type_t object_type = getObjectTypeFromVid(vid);

            if (object_type == SAI_OBJECT_TYPE_NULL)
            {
                SWSS_LOG_ERROR("null object type from vid 0x%lx", vid);

                throw std::runtime_error("null object type from vid");
            }

            std::shared_ptr<SaiObj> o = std::make_shared<SaiObj>();

            o->str_object_type  = sai_serialize_object_type(object_type);
            o->str_object_id    = sai_serialize_object_id(vid);

            o->meta_key.object_type = object_type;
            o->meta_key.key.object_id = vid;

            soOids[o->str_object_id] = o;
            oOids[o->meta_key.key.object_id] = o;

            o->oidObject = true;

            m_vidReference[o->meta_key.key.object_id] += 0;

            soAll[o->str_object_id] = o;
            sotAll[o->meta_key.object_type][o->str_object_id] = o;

            ridToVid[rid] = vid;
            vidToRid[vid] = rid;
        }/*}}}*/

        ~AsicView()/*{{{*/
        {
            // empty intentionally
        }/*}}}*/

        void asicSetAttribute(/*{{{*/
                _In_ const std::shared_ptr<SaiObj> &currentObj,
                _In_ const std::shared_ptr<SaiAttr> &attr)
        {
            SWSS_LOG_ENTER();

            /*
             * This method will generate ASIC set operation on current existing
             * object similar like SAI REDIS is doing.
             *
             * NOTE: in long run, this is serialize, and then we call deserialize
             * to execute them on actual asic, maybe this is not necessary
             * and could be optimized later.
             *
             * TODO set on object id should do release of links (currently done
             * outside) and modify dependency tree.
             */

            std::vector<swss::FieldValueTuple> entry = SaiAttributeList::serialize_attr_list(
                    currentObj->getObjectType(),
                    1,
                    attr->getSaiAttr(),
                    false);

            std::string key = currentObj->str_object_type + ":" + currentObj->str_object_id;

            std::shared_ptr<swss::KeyOpFieldsValuesTuple> kco = std::make_shared<swss::KeyOpFieldsValuesTuple>(key, "set", entry);

            m_asicOperations.push_back(kco);
        }/*}}}*/

        void asicCreateObject(/*{{{*/
                _In_ const std::shared_ptr<SaiObj> &currentObj)
        {
            SWSS_LOG_ENTER();

            if (!currentObj->oidObject)
            {
                SWSS_LOG_ERROR("non object id not supported yet %s:%s FIXME",
                        currentObj->str_object_type.c_str(),
                        currentObj->str_object_id.c_str());

                throw std::runtime_error("non object id not supported yet");
            }

            soOids[currentObj->str_object_id] = currentObj;
            oOids[currentObj->meta_key.key.object_id] = currentObj;

            m_vidReference[currentObj->meta_key.key.object_id] += 0;

            soAll[currentObj->str_object_id] = currentObj;
            sotAll[currentObj->meta_key.object_type][currentObj->str_object_id] = currentObj;

            /*
             * This method will generate ASIC create operation on current
             * existing object similar like SAI REDIS is doing.
             *
             * NOTE: in long run, this is serialize, and then we call
             * deserialize to execute them on actual asic, maybe this is not
             * necessary and could be optimized later.
             *
             * TODO create on object id attributes should do bind references to
             * used VIDs of of links (currently done outside) and modify
             * dependency tree.
             */

            std::vector<swss::FieldValueTuple> entry;

            for (auto const &pair: currentObj->getAllAttributes())
            {
                const auto &attr = pair.second;

                swss::FieldValueTuple fvt(attr->getStrAttrId(), attr->getStrAttrValue());

                entry.push_back(fvt);
            }

            if (entry.size() == 0)
            {
                // make sure that we put object into db
                // even if there are no attributes set
                swss::FieldValueTuple null("NULL", "NULL");

                entry.push_back(null);
            }

            std::string key = currentObj->str_object_type + ":" + currentObj->str_object_id;

            std::shared_ptr<swss::KeyOpFieldsValuesTuple> kco = std::make_shared<swss::KeyOpFieldsValuesTuple>(key, "create", entry);

            m_asicOperations.push_back(kco);
        }/*}}}*/

        // TODO combine with creating object id
        void asicCreateNonObjectIdObject(/*{{{*/
                _In_ const std::shared_ptr<SaiObj> &currentObj)
        {
            SWSS_LOG_ENTER();

            if (currentObj->oidObject)
            {
                SWSS_LOG_ERROR("object id is not supported yet %s:%s FIXME",
                        currentObj->str_object_type.c_str(),
                        currentObj->str_object_id.c_str());

                throw std::runtime_error("object id is not supported yet");
            }

            switch (currentObj->getObjectType())
            {
                case SAI_OBJECT_TYPE_FDB:
                    sai_deserialize_fdb_entry(currentObj->str_object_id, currentObj->meta_key.key.fdb_entry);
                    soFdbs[currentObj->str_object_id] = currentObj;
                    break;

                case SAI_OBJECT_TYPE_NEIGHBOR:
                    sai_deserialize_neighbor_entry(currentObj->str_object_id, currentObj->meta_key.key.neighbor_entry);
                    soNeighbors[currentObj->str_object_id] = currentObj;

                    /*
                     * Since neighbor struct object contains RIF ID, we
                     * need to increase vid reference With new metadata for
                     * SAI 1.0 this can be done in generic way for all non
                     * object ids.
                     */

                    m_vidReference[currentObj->meta_key.key.neighbor_entry.rif_id] += 1;

                    break;

                case SAI_OBJECT_TYPE_ROUTE:
                    sai_deserialize_route_entry(currentObj->str_object_id, currentObj->meta_key.key.route_entry);
                    soRoutes[currentObj->str_object_id] = currentObj;

                    /*
                     * Since route struct object contains VR ID, we need to
                     * increase vid reference.
                     */

                    m_vidReference[currentObj->meta_key.key.route_entry.vr_id] += 1;

                    break;

                case SAI_OBJECT_TYPE_VLAN:
                    sai_deserialize_vlan_id(currentObj->str_object_id, currentObj->meta_key.key.vlan_id);
                    soVlans[currentObj->str_object_id] = currentObj;
                    break;

                default:

                    SWSS_LOG_ERROR("unsupported object type: %s",
                            sai_serialize_object_type(currentObj->getObjectType()).c_str());

                    throw std::runtime_error("unsupported object type");
            }

            // TODO fix this

            soAll[currentObj->str_object_id] = currentObj;
            sotAll[currentObj->meta_key.object_type][currentObj->str_object_id] = currentObj;

            /*
             * This method will generate ASIC create operation on current
             * existing object similar like SAI REDIS is doing.
             *
             * NOTE: in long run, this is serialize, and then we call
             * deserialize to execute them on actual asic, maybe this is not
             * necessary and could be optimized later.
             *
             * TODO create on object id attributes should do bind references to
             * used VIDs of of links (currently done outside) and modify
             * dependency tree.
             */

            std::vector<swss::FieldValueTuple> entry;

            for (auto const &pair: currentObj->getAllAttributes())
            {
                const auto &attr = pair.second;

                swss::FieldValueTuple fvt(attr->getStrAttrId(), attr->getStrAttrValue());

                entry.push_back(fvt);
            }

            if (entry.size() == 0)
            {
                // make sure that we put object into db
                // even if there are no attributes set
                swss::FieldValueTuple null("NULL", "NULL");

                entry.push_back(null);
            }

            std::string key = currentObj->str_object_type + ":" + currentObj->str_object_id;

            std::shared_ptr<swss::KeyOpFieldsValuesTuple> kco = std::make_shared<swss::KeyOpFieldsValuesTuple>(key, "create", entry);

            m_asicOperations.push_back(kco);
        }/*}}}*/

        // TODO combine to 1 method
        void asicRemoveNonObjectIdObject(/*{{{*/
                _In_ const std::shared_ptr<SaiObj> &currentObj)
        {
            SWSS_LOG_ENTER();

            if (currentObj->oidObject)
            {
                SWSS_LOG_ERROR("object id is not supported yet %s:%s FIXME",
                        currentObj->str_object_type.c_str(),
                        currentObj->str_object_id.c_str());

                throw std::runtime_error("object id is not supported yet");
            }

            switch (currentObj->getObjectType())
            {
                case SAI_OBJECT_TYPE_FDB:

                    soFdbs.erase(currentObj->str_object_id);

                    break;

                case SAI_OBJECT_TYPE_NEIGHBOR:

                    /*
                     * Since neighbor struct object contains RIF ID, we
                     * need to decrease vid reference, with new metadata for
                     * SAI 1.0 this can be done in generic way for all non
                     * object ids.
                     */

                    soNeighbors.erase(currentObj->str_object_id);

                    m_vidReference[currentObj->meta_key.key.neighbor_entry.rif_id] -= 1;

                    break;

                case SAI_OBJECT_TYPE_ROUTE:

                    /*
                     * Since route struct object contains VR ID, we need to
                     * decrease vid reference.
                     */

                    soRoutes.erase(currentObj->str_object_id);

                    m_vidReference[currentObj->meta_key.key.route_entry.vr_id] -= 1;

                    break;

                case SAI_OBJECT_TYPE_VLAN:

                    soVlans.erase(currentObj->str_object_id);

                    break;

                default:

                    SWSS_LOG_ERROR("unsupported object type: %s",
                            sai_serialize_object_type(currentObj->getObjectType()).c_str());

                    throw std::runtime_error("unsupported object type");
            }

            soAll.erase(currentObj->str_object_id);
            sotAll[currentObj->meta_key.object_type].erase(currentObj->str_object_id);

            /*
             * Generate asic commands.
             */

            std::vector<swss::FieldValueTuple> entry;

            std::string key = currentObj->str_object_type + ":" + currentObj->str_object_id;

            std::shared_ptr<swss::KeyOpFieldsValuesTuple> kco = std::make_shared<swss::KeyOpFieldsValuesTuple>(key, "remove", entry);

            m_asicOperations.push_back(kco);
        }/*}}}*/

        void asicRemoveObject(/*{{{*/
                _In_ const std::shared_ptr<SaiObj> &currentObj)
        {
            SWSS_LOG_ENTER();

            if (!currentObj->oidObject)
            {
                SWSS_LOG_ERROR("non object id is not supported yet %s:%s FIXME",
                        currentObj->str_object_type.c_str(),
                        currentObj->str_object_id.c_str());

                throw std::runtime_error("non object id is not supported yet");
            }

            /*
             * Reference count is already check externally, but we can move
             * that check here also as sanity check.
             */

            soOids.erase(currentObj->str_object_id);
            oOids.erase(currentObj->meta_key.key.object_id);

            m_vidReference[currentObj->meta_key.key.object_id] -= 1;

            soAll.erase(currentObj->str_object_id);
            sotAll[currentObj->meta_key.object_type].erase(currentObj->str_object_id);

            /*
             * Clear object also from rid/vid maps.
             */

            sai_object_id_t vid = currentObj->getVid();
            sai_object_id_t rid = vidToRid.at(vid);

            /*
             * This will have impact on translate_vid_to_rid, we need to put this in other view
             */

            ridToVid.erase(rid);
            vidToRid.erase(vid);

            removedVidToRid[vid] = rid;

            // TODO m_vidReference remove also ?

            /*
             * Generate asic commands.
             */

            std::vector<swss::FieldValueTuple> entry;

            std::string key = currentObj->str_object_type + ":" + currentObj->str_object_id;

            std::shared_ptr<swss::KeyOpFieldsValuesTuple> kco = std::make_shared<swss::KeyOpFieldsValuesTuple>(key, "remove", entry);

            m_asicOperations.push_back(kco);
        }/*}}}*/

        const std::vector<std::shared_ptr<swss::KeyOpFieldsValuesTuple>>& asicGetOperations() const/*{{{*/
        {
            SWSS_LOG_ENTER();

            return m_asicOperations;
        }/*}}}*/

        size_t asicGetOperationsCount() const/*{{{*/
        {
            SWSS_LOG_ENTER();

            return m_asicOperations.size();
        }/*}}}*/

        bool hasRid(/*{{{*/
                _In_ sai_object_id_t rid) const
        {
            return ridToVid.find(rid) != ridToVid.end();
        }/*}}}*/

        bool hasVid(/*{{{*/
                _In_ sai_object_id_t vid) const
        {
            return vidToRid.find(vid) != vidToRid.end();
        }/*}}}*/

    private:

        std::map<sai_object_id_t, int> m_vidReference;

        void populateAttributes(/*{{{*/
                _In_ std::shared_ptr<SaiObj> &o,
                _In_ const swss::TableMap &map)
        {
            SWSS_LOG_ENTER();

            for (const auto &field: map)
            {
                std::shared_ptr<SaiAttr> a = std::make_shared<SaiAttr>(field.first, field.second);

                o->setAttr(a);

                for (auto const &vid: a->getOidListFromAttribute())
                {
                    if (vid != SAI_NULL_OBJECT_ID)
                    {
                        m_vidReference[vid] += 1;
                    }
                }
            }
        }/*}}}*/

        std::shared_ptr<SaiObj> createSwitchObject()/*{{{*/
        {
            SWSS_LOG_ENTER();

            /*
             * Switch object is using default value which is zero for object id.  In
             * SAI 1.0 it will use actual value, and this method here will not be
             * needed.
             */

            sai_object_id_t object_id = 0;

            std::shared_ptr<SaiObj> sw = std::make_shared<SaiObj>();

            sw->str_object_id        = sai_serialize_object_id(object_id);
            sw->str_object_type      = "SAI_OBJECT_TYPE_SWITCH";
            sw->meta_key.object_type = SAI_OBJECT_TYPE_SWITCH;
            sw->meta_key.key.object_id = object_id;

            return sw;
        }/*}}}*/

        /*
         * Not sure if there is sense to make this shared, ony reason is to skip
         * copy expensive KeyOpFieldsValuesTuple objects.
         */

        std::vector<std::shared_ptr<swss::KeyOpFieldsValuesTuple>> m_asicOperations;

        AsicView(const SaiAttr&);
        AsicView& operator=(const SaiAttr&);
};

void redisGetAsicView(/*{{{*/
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
}/*}}}*/

sai_status_t checkObjectsStatus(/*{{{*/
        _In_ const AsicView &view)
{
    SWSS_LOG_ENTER();

    int count = 0;

    for (const auto &p: view.soAll)
    {
        if (p.second->getObjectStatus() != SAI_OBJECT_STATUS_FINAL)
        {
            const auto &o = *p.second;

            SWSS_LOG_ERROR("object was not processed: %s %s, status: %d (ref: %d)",
                    o.str_object_type.c_str(),
                    o.str_object_id.c_str(),
                    o.getObjectStatus(),
                    o.oidObject ? view.getVidReferenceCount(o.getVid()): -1);

            count++;
        }
    }

    if (count > 0)
    {
        SWSS_LOG_ERROR("%d objects were not processed", count);

        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}/*}}}*/

ObjectIdToSaiObjectHash vectorToHash(/*{{{*/
        _In_ const std::vector<std::shared_ptr<SaiObj>> &vec)
{
    SWSS_LOG_ENTER();

    ObjectIdToSaiObjectHash hash;

    for (const auto &i: vec)
    {
        hash[i->getVid()] = i;
    }

    return hash;
}/*}}}*/

void matchOids(/*{{{*/
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
}/*}}}*/

void checkMatchedPorts(/*{{{*/
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
}/*}}}*/

/**
 * @brief Check if both list contains the same objects
 *
 * Function returns TRUE only when all list contain exact the same objects
 * compared by RID values and with exact same order.
 *
 * TODO Currently order on the list matters, but we need to update this logic
 * so order will not matter, just values of object will need to be considered.
 * We need to have extra index of processed objects and not processed yet.  We
 * should also cover NULL case and duplicated objects.  Normally we should not
 * have duplicated object id's on the list, and we can easy check that using
 * hash.
 *
 * In case of really long list, easier way to solve this can be getting all the
 * RIDs from current view (they must exist), getting all the matched RIDs from
 * temporary list (if one of them don't exists then lists are not equal) sort
 * both list nlog(n) and then compare sequentially.
 */
bool hasEqualObjectList(/*{{{*/
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
}/*}}}*/

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
bool hasEqualAttribute(/*{{{*/
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
        const auto &currentAttr = current->getSaiAttr(id);
        const auto &temporaryAttr = temporary->getSaiAttr(id);

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

        /*
         * We can use getOidListFromAttribute for extracting those oid lists
         * since we know that attribute type is oid type.
         */

        const auto &temporaryObjList = temporaryAttr->getOidListFromAttribute();
        const auto &currentObjList = currentAttr->getOidListFromAttribute();

        /*
         * This function already supports enable flags for acl field and action
         * so we don't need to worry about it here.
         *
         * TODO: for acl list/action this maybe missleading since one can be
         * enabled and list is empty and other one can be disabled and this
         * function also return empty, so this will mean they are equal but
         * they don't sinec they differ with enable flag. We probably won't hit
         * this, since we probably always have some oids on the list.
         */

        return hasEqualObjectList(
                currentView,
                temporaryView,
                (uint32_t)currentObjList.size(),
                currentObjList.data(),
                (uint32_t)temporaryObjList.size(),
                temporaryObjList.data());
    }

    /*
     * Currently we don't support case where only one attribute is present, but
     * we should consider that if other attribute has default value.
     */

    return false;
}/*}}}*/

typedef struct _sai_object_compare_info_t/*{{{*/
{
    size_t equal_attributes;

    std::shared_ptr<SaiObj> obj;

} sai_object_compare_info_t;/*}}}*/

bool compareByEqualAttributes(/*{{{*/
        _In_ const sai_object_compare_info_t &a,
        _In_ const sai_object_compare_info_t &b)
{
    /*
     * NOTE: this function will sort in descending order
     */

    return a.equal_attributes > b.equal_attributes;
}/*}}}*/

/**
 * @brief this function will select random sai object from best candidates we
 * found.
 *
 * Input list should contain at least one candidate.
 */
std::shared_ptr<SaiObj> selectRandomCandidate(/*{{{*/
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    size_t candidateCount = candidateObjects.size();

    SWSS_LOG_INFO("selecting random candidate from pool size %zu", candidateCount);

    size_t index = std::rand() % candidateCount;

    return candidateObjects.at(index).obj;
}/*}}}*/

bool isOidOnAttrValue(/*{{{*/
        _In_ const std::shared_ptr<SaiAttr> &attr,
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    for (auto const &vid: attr->getOidListFromAttribute())
    {
        if (vid == oid)
        {
            return true;
        }
    }

    return false;
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForGenericObjectUsingHeuristic(/*{{{*/
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
        default:

            SWSS_LOG_WARN("%s is not supported for heuristic, will select best match object at random",
                    sai_serialize_object_type(object_type).c_str());

            break;
    }

    return selectRandomCandidate(candidateObjects);
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForGenericObject(/*{{{*/
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
     */

    if (!temporaryObj->oidObject)
    {
        SWSS_LOG_ERROR("non object id %s is used in generic method, please implement special case, FIXME", temporaryObj->str_object_type.c_str());

        throw std::runtime_error("non object id is used in generic method, implement special case, FIXME");
    }

    /*
     * Get not processed objects of temporary object type, and all attributes
     * that are set on that object. This function should be used only on oid
     * object ids, since for non object id findinf best match is based on
     * struct entry of object id.
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

                SWSS_LOG_INFO("ob equal %s %s, %s: %s",
                        temporaryObj->str_object_id.c_str(),
                        currentObj->str_object_id.c_str(),
                        attr.second->getStrAttrId().c_str(),
                        attr.second->getStrAttrValue().c_str());
            }
            else
            {
                SWSS_LOG_INFO("ob not equal %s %s, %s: %s",
                        temporaryObj->str_object_id.c_str(),
                        currentObj->str_object_id.c_str(),
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
     * TODO if we have more than 1 object matched actually more preffered
     * object would be the object with most CREATE_ONLY attributes matching
     * since that will reduce risk of removind and recreateing that object in
     * current view.
     */

    /*
     * Sort candidate objects by equal attributes in descending order, we know
     * here that we have at least 2 candidates.
     *
     * NOTE: maybe at this point we should be using heuristics?
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
            [equalAttributes](const sai_object_compare_info_t &candidate)
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
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForNeighborEntry(/*{{{*/
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> temporaryObj)
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

    sai_neighbor_entry_t ne = temporaryObj->meta_key.key.neighbor_entry;

    sai_object_id_t temporaryRouterInterfaceVid = ne.rif_id;

    auto temporaryIt = temporaryView.vidToRid.find(temporaryRouterInterfaceVid);

    if (temporaryIt == temporaryView.vidToRid.end())
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

    auto currentIt = currentView.ridToVid.find(temporaryRid);

    if (currentIt == currentView.ridToVid.end())
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

    auto currentNeighborIt = currentView.soNeighbors.find(str_neighbor_entry);

    if (currentNeighborIt == currentView.soNeighbors.end())
    {
        SWSS_LOG_DEBUG("unable to find neighbor entry %s in current asic view", str_neighbor_entry.c_str());

        return nullptr;
    }

    /*
     * We found the same neighbor entry in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentNeighborObj = currentNeighborIt->second;

    if (currentNeighborObj->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentNeighborObj;
    }

    /*
     * If we are here, that means this neighbor was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_ERROR("found neighbor entry %s in current view, but it status is %d, FATAL",
            str_neighbor_entry.c_str(), currentNeighborObj->getObjectStatus());

    throw std::runtime_error("found neighbor entry in current view, but it status was processed");
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForRouteEntry(/*{{{*/
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> temporaryObj)
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

    sai_unicast_route_entry_t re = temporaryObj->meta_key.key.route_entry;

    sai_object_id_t temporaryVirtualRouterVid = re.vr_id;

    auto temporaryIt = temporaryView.vidToRid.find(temporaryVirtualRouterVid);

    if (temporaryIt == temporaryView.vidToRid.end())
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

    auto currentIt = currentView.ridToVid.find(temporaryRid);

    if (currentIt == currentView.ridToVid.end())
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
    auto currentRouteIt = currentView.soRoutes.find(str_route_entry);

    if (currentRouteIt == currentView.soRoutes.end())
    {
        SWSS_LOG_DEBUG("unable to find route entry %s in current asic view", str_route_entry.c_str());

        return nullptr;
    }

    /*
     * We found the same route entry in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentRouteObj = currentRouteIt->second;

    if (currentRouteObj->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentRouteObj;
    }

    /*
     * If we are here, that means this route was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_ERROR("found route entry %s in current view, but it status is %d, FATAL",
            str_route_entry.c_str(), currentRouteObj->getObjectStatus());

    throw std::runtime_error("found route entry in current view, but it status was processed");
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForSwitch(/*{{{*/
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * On compare logic we checked that we have one switch
     * so we can just get first.
     */

    auto currentSwitchIt = currentView.soSwitches.begin();

    if (currentSwitchIt == currentView.soSwitches.end())
    {
        SWSS_LOG_DEBUG("unable to find switch object in current view");

        return nullptr;
    }

    /*
     * We found switch object in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentSwitchObj = currentSwitchIt->second;

    if (currentSwitchObj->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentSwitchObj;
    }

    /*
     * If we are here, that means this switch was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_ERROR("found switch object %s in current view, but it status is %d, FATAL",
            currentSwitchObj->str_object_id.c_str(), currentSwitchObj->getObjectStatus());

    throw std::runtime_error("found switch object in current view, but it status was processed");
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForFdbEntry(/*{{{*/
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * For fdb we don't need to iterate via all current
     * fdbs, we can do dictionary lookup, we don't even
     * do OID translation since fdb entry don't contain
     * any oids, so simple direct lookup is fine. This
     * will change in SAI 1.0 since oids are added there.
     */

    sai_fdb_entry_t fe = temporaryObj->meta_key.key.fdb_entry;

    std::string str_fdb_entry = sai_serialize_fdb_entry(fe);

    /*
     * Now when we have serialized fdb entry can do dictionary lookup for fdb.
     */

    auto currentFdbIt = currentView.soFdbs.find(str_fdb_entry);

    if (currentFdbIt == currentView.soFdbs.end())
    {
        SWSS_LOG_DEBUG("unable to find fdb entry %s in current asic view", str_fdb_entry.c_str());

        return nullptr;
    }

    /*
     * We found the same fdb entry in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentFdbObj = currentFdbIt->second;

    if (currentFdbObj->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentFdbObj;
    }

    /*
     * If we are here, that means this fdb was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_ERROR("found fdb entry %s in current view, but it status is %d, FATAL",
            str_fdb_entry.c_str(), currentFdbObj->getObjectStatus() );

    throw std::runtime_error("found fdb entry in current view, but it status was processed");
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForTrap(/*{{{*/
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * For traps in 0.9.4 traps are enum values, and we have
     * their exact names so we can do direct dictionary lookup.
     */

    sai_hostif_trap_id_t trap_id = temporaryObj->meta_key.key.trap_id;

    std::string str_trap_id = sai_serialize_hostif_trap_id(trap_id);

    /*
     * Now when we have serialized trap id, we can do dictionary lookup for trap.
     */

    auto currentTrapIt = currentView.soTraps.find(str_trap_id);

    if (currentTrapIt == currentView.soTraps.end())
    {
        SWSS_LOG_WARN("unable to find trap id %s in current asic view", str_trap_id.c_str());

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
            str_trap_id.c_str(), currentTrapObj->getObjectStatus());

    throw std::runtime_error("found trap id in current view, but it status was processed");
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatchForVlan(/*{{{*/
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * For vlan in 0.9.4 vlans are numbers values, and we have
     * their exact values so we can do direct dictionary lookup.
     */

    sai_vlan_id_t vlan_id = temporaryObj->meta_key.key.vlan_id;

    std::string str_vlan_id = sai_serialize_vlan_id(vlan_id);

    /*
     * Now when we have serialized vlan id, we can do dictionary lookup for vlan.
     */

    auto currentVlanIt = currentView.soVlans.find(str_vlan_id);

    if (currentVlanIt == currentView.soVlans.end())
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
            str_vlan_id.c_str(), currentVlanObj->getObjectStatus());

    throw std::runtime_error("found vlan id in current view, but it status was processed");
}/*}}}*/

std::shared_ptr<SaiObj> findCurrentBestMatch(/*{{{*/
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> temporaryObj)
{
    SWSS_LOG_ENTER();

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
             * Object status is matched so current and temp VID are the same so
             * we can just take object directly.
             */

            SWSS_LOG_INFO("found best match for %s %s since object status is MATCHED",
                    temporaryObj->str_object_type.c_str(),
                    temporaryObj->str_object_id.c_str());

            return currentView.oOids.at(temporaryObj->getVid());
        }
    }

    /*
     * NOTE: later on best match find can be done based on present CREATE_ONLY
     * attributes or on most matched CREATE_AND_SET attributes.
     */

    switch (temporaryObj->getObjectType())
    {
        /*
         * Non object id cases
         */

        case SAI_OBJECT_TYPE_NEIGHBOR:
            return findCurrentBestMatchForNeighborEntry(currentView, temporaryView, temporaryObj);

        case SAI_OBJECT_TYPE_ROUTE:
            return findCurrentBestMatchForRouteEntry(currentView, temporaryView, temporaryObj);

        case SAI_OBJECT_TYPE_FDB:
            return findCurrentBestMatchForFdbEntry(currentView, temporaryView, temporaryObj);

        case SAI_OBJECT_TYPE_SWITCH:
            return findCurrentBestMatchForSwitch(currentView, temporaryView, temporaryObj);

        case SAI_OBJECT_TYPE_TRAP:
            return findCurrentBestMatchForTrap(currentView, temporaryView, temporaryObj);

        case SAI_OBJECT_TYPE_VLAN:
            return findCurrentBestMatchForVlan(currentView, temporaryView, temporaryObj);

        default:

            /*
             * Here we support only object id object types.
             */

            return findCurrentBestMatchForGenericObject(currentView, temporaryView, temporaryObj);
    }

    /*
     * TODO: We need to implement for multiple object matches per object
     * heuristic like going along the path and counting how many times object
     * is used, or on which "matched" port/object is used, or on how many
     * routes etc, of course in both trees.
     */
}/*}}}*/

void processObjectForViewTransition(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ std::shared_ptr<SaiObj> temporayObj);

/**
 * @brief Process object attributes for view transition
 *
 * Since each object can contain attributes (or non id struct object can
 * contain oids inside struct) they need to be processed first recursivly and
 * they need to be in FINAL state before processing temporary view further.
 * This processing may result in changes in current view like removing and
 * recreateing or adding new objects depends on transitions.
 */
void procesObjectAttributesForViewTransition(/*{{{*/
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ std::shared_ptr<SaiObj> temporaryObj)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("%s %s", temporaryObj->str_object_type.c_str(), temporaryObj->str_object_id.c_str());

    /*
     * First we need to make sure if all attributes of this temporary object
     * are in FINAL or MATCHED state, then we can process this object and find
     * best match in current asic view.
     */

    for (auto &at: temporaryObj->getAllAttributes())
    {
        auto &attribute = at.second;

        SWSS_LOG_INFO("attr %s", attribute->getStrAttrId().c_str());

        // for each object id in attributes go recursively to match those objects

        for (auto vid: attribute->getOidListFromAttribute())
        {
            if (vid == SAI_NULL_OBJECT_ID)
            {
                continue;
            }

            SWSS_LOG_INFO("- processing attr VID 0x%lx", vid);

            auto tempParent = temporaryView.oOids.at(vid);

            processObjectForViewTransition(currentView, temporaryView, tempParent); // recursion

            /*
             * Temporary object here is never changed, even if we do recursion
             * here all that could been removed are objects in current view
             * tree so we don't need to worry about any temporary object
             * removal.
             */
        }
    }

    if (temporaryObj->oidObject)
    {
        return;
    }

    /*
     * For non object id types like NEIGHBOR or ROUTE they have object id
     * inside object struct entry, they also need to be processed, in SAI 1.0
     * this can be automated since we have metadata for those structs.
     */

    switch (temporaryObj->getObjectType())
    {
        case SAI_OBJECT_TYPE_NEIGHBOR:

            {
                sai_object_id_t vid = temporaryObj->meta_key.key.neighbor_entry.rif_id;

                auto structObject = temporaryView.oOids.at(vid);

                SWSS_LOG_INFO("- processing NEIGHBOR rif_id VID 0x%lx", vid);

                processObjectForViewTransition(currentView, temporaryView, structObject); // recursion
            }

            break;

        case SAI_OBJECT_TYPE_ROUTE:

            {
                sai_object_id_t vid = temporaryObj->meta_key.key.route_entry.vr_id;

                auto structObject = temporaryView.oOids.at(vid);

                SWSS_LOG_INFO("- processing ROUTE vr_id VID 0x%lx", vid);

                processObjectForViewTransition(currentView, temporaryView, structObject); // recursion
            }

            break;

        case SAI_OBJECT_TYPE_TRAP:
        case SAI_OBJECT_TYPE_SWITCH:
        case SAI_OBJECT_TYPE_VLAN:
        case SAI_OBJECT_TYPE_FDB:

            {
                /*
                 * In SAI 0.9.4 FDB don't contains any object IDs so no extra work
                 * required at this stage. As SAI 1.0 this will change.
                 */
            }

            break;

        default:

            SWSS_LOG_ERROR("non object id %s is not supported for processing yet, FIXME", temporaryObj->str_object_type.c_str());

            throw std::runtime_error("non object id is not supported for processing yet, FIXME");
    }
}/*}}}*/

std::shared_ptr<SaiAttr> getSaiAttrFromDefaultValue(
        _In_ const AsicView &currentView,
        _In_ const sai_attr_metadata_t &meta);

void releaseExisgingLinks(
        _In_ AsicView &view,
        _In_ std::shared_ptr<SaiAttr> attr);

void bindNewLinks(/*{{{*/
        _In_ AsicView &view,
        _In_ std::shared_ptr<SaiAttr> attr)
{
    SWSS_LOG_ENTER();

    /*
     * For each VID on that attribute (it can be single oid or oid list,
     * bind new link on current view.
     *
     * Notice that this list can contain VIDs from temporary view or default
     * view, so they may not exists in current view. But that should also be
     * not the case since we eather created new object on current view or
     * eather we matched current object to temporary object so RID can be the
     * same.
     *
     * TODO also if we want to keep track of object reverse dependency
     * this action can be more complicated.
     */

    for (auto const &vid: attr->getOidListFromAttribute())
    {
        view.bindNewVidReference(vid);
    }
}/*}}}*/

void bringNonRemovableObjectToDefaultState(/*{{{*/
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ std::shared_ptr<SaiObj> currentObj)
{
    SWSS_LOG_ENTER();

    for (const auto &it: currentObj->getAllAttributes())
    {
        const auto &attr = it.second;

        const auto &meta = attr->getAttrMetadata();

        if (!HAS_FLAG_CREATE_AND_SET(meta->flags))
        {
            SWSS_LOG_ERROR("attribute %s is not CREATE_AND_SET, bug?", meta->attridname);

            throw std::runtime_error("attribute is not create and set");
        }

        if (meta->defaultvaluetype == SAI_DEFAULT_VALUE_TYPE_NONE)
        {
            SWSS_LOG_ERROR("attribute %s default value type is NONE, bug?", meta->attridname);

            throw std::runtime_error("attribute default value type is none");
        }

        auto defaultValueAttr = getSaiAttrFromDefaultValue(currentView, *meta);

        if (defaultValueAttr == nullptr)
        {
            SWSS_LOG_ERROR("Can't get default value for present current attr %s:%s, FIXME",
                    meta->attridname,
                    attr->getStrAttrValue().c_str());

            throw std::runtime_error("cant get default value for attribute");
        }

        if (attr->getStrAttrValue() == defaultValueAttr->getStrAttrValue())
        {
            /*
             * If current value is the same as default value no need to
             * generate ASIC update command.
             */

            continue;
        }

        if (attr->isObjectIdAttr())
        {
            /*
             * If attribute is OID attribute, we need to release previous links
             * and bind new links for new OID.
             */

            releaseExisgingLinks(currentView, attr);

            currentObj->setAttr(defaultValueAttr);

            bindNewLinks(currentView, defaultValueAttr);
        }
        else
        {
            /*
             * Attribute is non oid attribute, so no links operation is needed.
             */

            currentObj->setAttr(defaultValueAttr);
        }

        currentView.asicSetAttribute(currentObj, defaultValueAttr);
    }

    currentObj->setObjectStatus(SAI_OBJECT_STATUS_FINAL);

    /*
     * Problem here is that default trap group is used and we build references
     * for it, so it will not be "removed" when processing, since it's default
     * object so it will not be processed, and also it can't be removed since
     * it's default object.
     */
}/*}}}*/


void releaseExisgingLinks(/*{{{*/
        _In_ AsicView &currentView,
        _In_ std::shared_ptr<SaiObj> currentObj)
{
    SWSS_LOG_ENTER();

    for (const auto &ita: currentObj->getAllAttributes())
    {
        releaseExisgingLinks(currentView, ita.second);
    }
}/*}}}*/

void removeExistingObjectFromCurrentView(/*{{{*/
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ std::shared_ptr<SaiObj> currentObj)
{
    SWSS_LOG_ENTER();

    /*
     * This decreaseing VID reference will be hard when actual reference will
     * be one of default objects like CPU or default trap group when we "bring
     * defaul value" or maybe in oid case we will not bring default values and
     * just remove and recreate.
     */

    if (currentObj->oidObject)
    {
        int count = currentView.getVidReferenceCount(currentObj->getVid());

        if (count != 0)
        {
            /*
             * If references count is not zero, we need to remove child
             * first for that we need dependency tree, not supported yet.
             */

            SWSS_LOG_ERROR("can't remove existing object %s:%s since reference count is %d, FIXME",
                    currentObj->str_object_type.c_str(),
                    currentObj->str_object_id.c_str(),
                    count);

            throw std::runtime_error("can't remove existing object since reference count is not zero, FIXME");
        }
    }

    /*
     * If some object can't be removed from current and it's missing from temp,
     * then it needs to be transfered to temp as well and bring to default
     * values.
     *
     * First we need to check if object can be removed, like port's cant be
     * removed, vlan 1, queues, ingress_pg etc.
     */

    switch (currentObj->getObjectType())
    {
        case SAI_OBJECT_TYPE_NEIGHBOR:
        case SAI_OBJECT_TYPE_FDB:
        case SAI_OBJECT_TYPE_ROUTE:

            /*
             * For ROUTE/FDB/NEIGHBOR we don't need to worry about dependencies
             * since those are struct object id and no object is usind it. But
             * we need to release it reference id for useda OIDs in structs
             * like rif_id and vr_id.
             */

            currentView.asicRemoveNonObjectIdObject(currentObj);

            releaseExisgingLinks(currentView, currentObj);
            currentObj->setObjectStatus(SAI_OBJECT_STATUS_REMOVED);

            return;

        case SAI_OBJECT_TYPE_VLAN:

            if (currentObj->meta_key.key.vlan_id == 1)
            {
                /*
                 * VLAN 1 can't be removed.
                 */

                bringNonRemovableObjectToDefaultState(currentView, temporaryView, currentObj);
            }
            else
            {
                currentView.asicRemoveNonObjectIdObject(currentObj);

                releaseExisgingLinks(currentView, currentObj);
                currentObj->setObjectStatus(SAI_OBJECT_STATUS_REMOVED);
            }

            return;

        case SAI_OBJECT_TYPE_SWITCH:
        case SAI_OBJECT_TYPE_TRAP:

            /*
             * All traps are inserted so we should alwasy find current best
             * match.
             */

            bringNonRemovableObjectToDefaultState(currentView, temporaryView, currentObj);
            return;

        case SAI_OBJECT_TYPE_SCHEDULER:
        case SAI_OBJECT_TYPE_POLICER:
        case SAI_OBJECT_TYPE_QOS_MAPS:
        case SAI_OBJECT_TYPE_NEXT_HOP_GROUP:
        case SAI_OBJECT_TYPE_BUFFER_POOL:
        case SAI_OBJECT_TYPE_WRED:
        case SAI_OBJECT_TYPE_BUFFER_PROFILE:
        case SAI_OBJECT_TYPE_NEXT_HOP:
        case SAI_OBJECT_TYPE_ROUTER_INTERFACE:
        case SAI_OBJECT_TYPE_ACL_ENTRY:
        case SAI_OBJECT_TYPE_HOST_INTERFACE:
        case SAI_OBJECT_TYPE_ACL_TABLE:

            currentView.asicRemoveObject(currentObj);

            releaseExisgingLinks(currentView, currentObj);
            currentObj->setObjectStatus(SAI_OBJECT_STATUS_REMOVED);

            return;

            /*
             * Remove for those objects is not supported now lets bring back
             * object to default state.
             *
             * For oid values we can figure out if we would have list of
             * defaults which can be removed and which can't and use list same
             * for queu schedulers etc.
             */

        case SAI_OBJECT_TYPE_TRAP_GROUP:
        case SAI_OBJECT_TYPE_VIRTUAL_ROUTER:

            if (currentView.vidToRid.at(currentObj->getVid()) == currentView.defaultVirtualRouterRid ||
                currentView.vidToRid.at(currentObj->getVid()) == currentView.defaultTrapGroupRid)
            {
                /*
                 * Default trap group and default virtual router can't be removed.
                 */

                bringNonRemovableObjectToDefaultState(currentView, temporaryView, currentObj);
            }
            else
            {
                currentView.asicRemoveObject(currentObj);

                releaseExisgingLinks(currentView, currentObj);
                currentObj->setObjectStatus(SAI_OBJECT_STATUS_REMOVED);
            }

            return;

        /*
         * NOTE: Some queues can be user created and then they can be removed.
         * Also user can create scheduler groups as well.
         */

        case SAI_OBJECT_TYPE_PORT:
        case SAI_OBJECT_TYPE_QUEUE:
        case SAI_OBJECT_TYPE_PRIORITY_GROUP:
        case SAI_OBJECT_TYPE_SCHEDULER_GROUP:
            bringNonRemovableObjectToDefaultState(currentView, temporaryView, currentObj);
            return;

        default:
            break;
    }

    SWSS_LOG_ERROR("remove existing object %s:%s from current view is not supported yet, FIXME",
            currentObj->str_object_type.c_str(),
            currentObj->str_object_id.c_str());

    throw std::runtime_error("remove existing object from current view is not supported yet, FIXME");
}/*}}}*/

void releaseExisgingLinks(/*{{{*/
        _In_ AsicView &view,
        _In_ std::shared_ptr<SaiAttr> attr)
{
    SWSS_LOG_ENTER();

    /*
     * For each VID on that attribute (it can be single oid or oid list,
     * release link on current view.
     *
     * Second operation after could increase links of setting new attribute, or
     * nothing if object was removed.
     *
     * TODO also if we want to keep track of object reverse dependency
     * this action can be more complicated.
     */

    for (auto const &vid: attr->getOidListFromAttribute())
    {
        view.releaseVidReference(vid);
    }
}/*}}}*/

sai_object_id_t translateTemporaryVidToCurrentVid(/*{{{*/
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ sai_object_id_t tvid)
{
    SWSS_LOG_ENTER();

    /*
     * This method is used to translate temporary VID to current VID using RID
     * which should be present in both views.  If RID don't exist, then we
     * check whether object was created if so, then we return temporary VID
     * instead of createing new current VID and we don't need to track mapping
     * of those vids not having actual RID. This function should be used only
     * when we creating new objects in current view.
     */

    auto temporaryIt = temporaryView.vidToRid.find(tvid);

    if (temporaryIt == temporaryView.vidToRid.end())
    {
        /*
         * This can also happen when this VID object was created dynamically,
         * and don't have RID assigned yet, in that case we should use exact
         * same VID. We need to get temporary object for that VID and see if
         * flag created is set, and same VID must then exists in current view.
         */

        /*
         * We need to find object that has this vid
         */

        auto tempIt = temporaryView.oOids.find(tvid);

        if (tempIt == temporaryView.oOids.end())
        {
            SWSS_LOG_ERROR("temporary VID 0x%lx not found in temporary view", tvid);

            throw std::runtime_error("temporary VID not found in temporary view");
        }

        const auto &tempObj = tempIt->second;

        if (tempObj->createdObject)
        {
            // TODO we can also double check if this vid exists in current view.
            SWSS_LOG_DEBUG("translated temp VID 0x%lx to current, since object was created", tvid);

            return tvid;
        }

        SWSS_LOG_ERROR("VID 0x%lx was not found in temporary view, was object created? FIXME", tvid);

        throw std::runtime_error("VID was not found in temp view, object was created? FIXME");
    }

    sai_object_id_t rid = temporaryIt->second;

    auto currentIt = currentView.ridToVid.find(rid);

    if (currentIt == currentView.ridToVid.end())
    {
        SWSS_LOG_ERROR("RID 0x%lx was not found in current view", rid);

        throw std::runtime_error("RID was not found in current view");
    }

    sai_object_id_t cvid = currentIt->second;

    SWSS_LOG_DEBUG("translated temp VID 0x%lx using RID 0x%lx to current VID 0x%lx", tvid, rid, cvid);

    return cvid;
}/*}}}*/

std::shared_ptr<SaiAttr> translateTemporaryVidsToCurrentVids(/*{{{*/
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> &currentObj,
        _In_ const std::shared_ptr<SaiAttr> &inattr)
{
    SWSS_LOG_ENTER();

    /*
     * We are creating copy here since we will modyify contents of that
     * attribute.
     */

    auto attr = std::make_shared<SaiAttr>(inattr->getStrAttrId(), inattr->getStrAttrValue());

    if (!attr->isObjectIdAttr())
    {
        return attr;
    }

    /*
     * We need temporary VID translation to current view RID.
     *
     * TODO: we also need simpler version that will translate simple VID for
     * oids present inside non object id structs.
     */

    uint32_t count = 0;

    sai_object_id_t *objectIdList = NULL;

    auto &at = *attr->getRWSaiAttr();

    switch (attr->getAttrMetadata()->serializationtype)
    {
        case SAI_SERIALIZATION_TYPE_OBJECT_ID:
            count = 1;
            objectIdList = &at.value.oid;
            break;

        case SAI_SERIALIZATION_TYPE_OBJECT_LIST:
            count = at.value.objlist.count;
            objectIdList = at.value.objlist.list;
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:

            if (at.value.aclfield.enable)
            {
                count = 1;
                objectIdList = &at.value.aclfield.data.oid;
            }

            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:

            if (at.value.aclfield.enable)
            {
                count = at.value.aclfield.data.objlist.count;
                objectIdList = at.value.aclfield.data.objlist.list;
            }

            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:

            if (at.value.aclaction.enable)
            {
                count = 1;
                objectIdList = &at.value.aclaction.parameter.oid;
            }

            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:

            if (at.value.aclaction.enable)
            {
                count = at.value.aclaction.parameter.objlist.count;
                objectIdList = at.value.aclaction.parameter.objlist.list;
            }

            break;

        default:
            break;
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        sai_object_id_t tvid = objectIdList[i];

        if (tvid == SAI_NULL_OBJECT_ID)
        {
            continue;
        }

        /*
         * Do actual translation.
         */

        objectIdList[i] =  translateTemporaryVidToCurrentVid(currentView, temporaryView, tvid);
    }

    /*
     * Since we probably performed updates on this attribute, since VID was
     * eschanged, we need to update value string of that attribute.
     */

    attr->UpdateValue();

    return attr;
}/*}}}*/

/**
 * @brief Set attribute on current object
 *
 * This function will set given attribute to current object adding new
 * attribute or replacing exising one. Given attribute can be eather one
 * temporary attribute missing or different from current object object or
 * default attribute value if we need to bring some attribute to default value.
 */
void setAttributeOnCurrentObject(/*{{{*/
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> &currentObj,
        _In_ const std::shared_ptr<SaiAttr> &inattr)
{
    SWSS_LOG_ENTER();

    /*
     * At the beginning just small assert to check if object is create and set.
     */

    const auto meta = inattr->getAttrMetadata();

    if (!HAS_FLAG_CREATE_AND_SET(meta->flags))
    {
        SWSS_LOG_ERROR("can't set attribute %s on current object %s:%s since it's not CREATE_AND_SET",
                meta->attridname,
                currentObj->str_object_type.c_str(),
                currentObj->str_object_id.c_str());

        throw std::runtime_error("can't set attribute on current object since it's not CREATE_AND_SET");
    }

    /*
     * TODO ok there is a problem here, since attribute we SET can be OID
     * attribute, and that can be a VID or list of VIDs from temporary view. So
     * after this "set" operation we will end up with MIXED VIDs on current
     * view, some object will contain VIDs from temporary view, and this can
     * lead to fail in translate_vid_to_rid since we will need to look inside
     * two views to find right RID.
     *
     * We need a flag to say that VID is for object that was created
     * and we can mix only those VID's for objects that was created
     *
     * In here we need to translate attribute VIDs to current VIDs to keep
     * track in dependency tree, since if we use temporary VID that will point
     * to the same RID then we will lost track in dependency tree of that VID
     * reference count.
     *
     * TODO this should also be done for object id's inside non object ids
     */

    std::shared_ptr<SaiAttr> attr = translateTemporaryVidsToCurrentVids(currentView, temporaryView, currentObj, inattr);

    if (attr->isObjectIdAttr())
    {
        if (currentObj->hasAttr(meta->attrid))
        {
            /*
             * Since previous attribute exists, lets release previous links if
             * they are not NULL.
             *
             * TODO maybe release links should also modify dependency tree
             */

            releaseExisgingLinks(currentView, currentObj->getSaiAttr(meta->attrid));
        }

        currentObj->setAttr(attr);

        bindNewLinks(currentView, currentObj->getSaiAttr(meta->attrid));
    }
    else
    {
        /*
         * This SET don't contain any OIDs so no extra operations are required,
         * we don't need to break any references and decrease any
         * reference count.
         *
         * Also this attribute may exist already on this object or it will be
         * just set now, so just in case lets make copy of it.
         *
         * TODO: making copy here is not maybe necessary since default
         * attribute will be created dynamically anyway, and temporary
         * attributes will not change also.
         */

        currentObj->setAttr(attr);
    }

    currentView.asicSetAttribute(currentObj, attr);
}/*}}}*/

void createNewOidObjectFromTemporaryObject(/*{{{*/
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ std::shared_ptr<SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    if (!temporaryObj->oidObject)
    {
        SWSS_LOG_ERROR("expected OID object type, got: %s:%s",
                temporaryObj->str_object_type.c_str(),
                temporaryObj->str_object_id.c_str());

        throw std::runtime_error("expected OID object type");
    }

    /*
     * Ok there is a problem here, since attributes we pass to create can be
     * OID attributes, and that can be a VID or list of VIDs from temporary
     * view. So after this create operation we will end up with MIXED VIDs on
     * current view, some object will contain VIDs from temporary view, and
     * this can lead to fail in translate_vid_to_rid since we will need to look
     * inside two views to find right RID.
     *
     * We need a flag to say that VID is for object that was created and we can
     * mix only those VID's for objects that was created
     *
     * In here we need to translate attribute VIDs to current VIDs to keep
     * track in dependency tree, since if we use temporary VID that will point
     * to the same RID then we will lost track in dependency tree of that VID
     * reference count.
     *
     * TODO this should also be done for object id's inside non object ids
     */

    /*
     * We need to loop through all attributes and create copy of this object
     * and translate all attributes for getting correct VIDs like in set
     * operation, we can't use VID's from temporary view to not mix them with
     * current view, except created object ID's, since we won't be creating any
     * new VID on the way (but we could) maybe we can do that later - but for
     * that we will need some kind of RID which would later need to be removed.
     */

    std::shared_ptr<SaiObj> currentObj = std::make_shared<SaiObj>();

    /*
     * TODO Find out better way to do this, copy operator ?
     */

    currentObj->str_object_type  = temporaryObj->str_object_type;
    currentObj->str_object_id    = temporaryObj->str_object_id;      // temporary VID
    currentObj->meta_key         = temporaryObj->meta_key;           // temporary VID
    currentObj->defaultObject    = temporaryObj->defaultObject;
    currentObj->oidObject        = temporaryObj->oidObject;

    /*
     * CreateObject flag is set to true, so when we we will be looking to
     * translate VID between temporary view and current view (since status will
     * be FINAL) then we will know that there is no actual RID yet, and both
     * VIDs are the same. So this is how we cam match both objects between
     * views.
     */

    currentObj->createdObject = true;
    temporaryObj->createdObject = true;

    for (const auto &pair: temporaryObj->getAllAttributes())
    {
        const auto &tmpattr = pair.second;

        std::shared_ptr<SaiAttr> attr = translateTemporaryVidsToCurrentVids(currentView, temporaryView, currentObj, tmpattr);

        if (attr->isObjectIdAttr())
        {
            /*
             * This is new attribute so we don't need to release any new links.
             * But we need to bind new links on current object.
             */

            currentObj->setAttr(attr);

            bindNewLinks(currentView, currentObj->getSaiAttr(pair.first)); // we can use attr here
        }
        else
        {
            /*
             * This create attribute don't contain any OIDs so no extra
             * operations are required, we don't need to break any references
             * and decrease any reference count.
             */

            currentObj->setAttr(attr);
        }
    }

    /*
     * Asic create object inserts new fererence to track inside asic view.
     */

    currentView.asicCreateObject(currentObj);

    /*
     * Move both object status to FINAL since both objects were processed
     * succesfully and object was created.
     */

    currentObj->setObjectStatus(SAI_OBJECT_STATUS_FINAL);
    temporaryObj->setObjectStatus(SAI_OBJECT_STATUS_FINAL);
}/*}}}*/

// TODO combine those 2 methdos to 1
void createNewNonOidObjectFromTemporaryObject(/*{{{*/
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ std::shared_ptr<SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    if (temporaryObj->oidObject)
    {
        SWSS_LOG_ERROR("expected non OID object type, got: %s:%s",
                temporaryObj->str_object_type.c_str(),
                temporaryObj->str_object_id.c_str());

        throw std::runtime_error("expected OID object type");
    }

    /*
     * Ok there is a problem here, since attributes we pass to create can be
     * OID attributes, and that can be a VID or list of VIDs from temporary
     * view. So after this create operation we will end up with MIXED VIDs on
     * current view, some object will contain VIDs from temporary view, and
     * this can lead to fail in translate_vid_to_rid since we will need to look
     * inside two views to find right RID.
     *
     * We need a flag to say that VID is for object that was created and we can
     * mix only those VID's for objects that was created
     *
     * In here we need to translate attribute VIDs to current VIDs to keep
     * track in dependency tree, since if we use temporary VID that will point
     * to the same RID then we will lost track in dependency tree of that VID
     * reference count.
     */

    /*
     * We need to loop through all attributes and create copy of this object
     * and translate all attributes for getting correct VIDs like in set
     * operation, we can't use VID's from temporary view to not mix them with
     * current view, except created object ID's, since we won't be creating any
     * new VID on the way (but we could) maybe we can do that later - but for
     * that we will need some kind of RID which would later need to be removed.
     */

    std::shared_ptr<SaiObj> currentObj = std::make_shared<SaiObj>();

    /*
     * TODO Find out better way to do this, copy operator ?
     */

    currentObj->str_object_type  = temporaryObj->str_object_type;
    currentObj->str_object_id    = temporaryObj->str_object_id;      // non object id
    currentObj->meta_key         = temporaryObj->meta_key;           // non object id
    currentObj->defaultObject    = temporaryObj->defaultObject;
    currentObj->oidObject        = temporaryObj->oidObject;

    switch (temporaryObj->getObjectType())
    {
        case SAI_OBJECT_TYPE_ROUTE:

            /*
             * Since ROUTE object contains OID inside struct, this OID may
             * already exist in current view, so we need to do translation
             * here.
             */

            {
                sai_object_id_t vid = currentObj->meta_key.key.route_entry.vr_id;

                vid = translateTemporaryVidToCurrentVid(currentView, temporaryView, vid);

                /*
                 * Since it's possible that object id may had been changed, we need
                 * to update string as well.
                 */

                currentObj->meta_key.key.route_entry.vr_id = vid;

                // TODO this needs to be done in more generic way
                // TODO should this vid be updated in vr_id ?
                currentObj->str_object_id = sai_serialize_route_entry(currentObj->meta_key.key.route_entry);

                currentView.bindNewVidReference(vid);
            }

            break;

        case SAI_OBJECT_TYPE_NEIGHBOR:

            /*
             * Since NEIGHBOR object contains OID inside struct, this OID may
             * already exist in current view, so we need to do translation
             * here.
             */

            {
                sai_object_id_t vid = currentObj->meta_key.key.neighbor_entry.rif_id;

                vid = translateTemporaryVidToCurrentVid(currentView, temporaryView, vid);

                /*
                 * Since it's possible that object id may had been changed, we need
                 * to update string as well.
                 */

                currentObj->meta_key.key.neighbor_entry.rif_id = vid;

                // TODO this needs to be done in more generic way
                // TODO should this vid be updated in rif_id ?
                currentObj->str_object_id = sai_serialize_neighbor_entry(currentObj->meta_key.key.neighbor_entry);

                currentView.bindNewVidReference(vid);
            }

            break;

        case SAI_OBJECT_TYPE_FDB:
        case SAI_OBJECT_TYPE_VLAN:

            /*
             * Objectd FDB and VLAN they don't contain OID values inside
             * object id struct, so no special translation is required.
             */

            break;

        default:

            SWSS_LOG_ERROR("unexpected non object id type: %s",
                    sai_serialize_object_type(temporaryObj->getObjectType()).c_str());

            throw std::runtime_error("unexpected non object id type");
    }


    /*
     * CreateObject flag is set to true, so when we we will be looking to
     * translate VID between temporary view and current view (since status will
     * be FINAL) then we will know that there is no actual RID yet, and both
     * VIDs are the same. So this is how we cam match both objects between
     * views.
     */

    currentObj->createdObject = true;
    temporaryObj->createdObject = true;

    for (const auto &pair: temporaryObj->getAllAttributes())
    {
        const auto &tmpattr = pair.second;

        std::shared_ptr<SaiAttr> attr = translateTemporaryVidsToCurrentVids(currentView, temporaryView, currentObj, tmpattr);

        if (attr->isObjectIdAttr())
        {
            /*
             * This is new attribute so we don't need to release any new links.
             * But we need to bind new links on current object.
             */

            currentObj->setAttr(attr);

            bindNewLinks(currentView, currentObj->getSaiAttr(pair.first)); // we can use attr here
        }
        else
        {
            /*
             * This create attribute don't contain any OIDs so no extra
             * operations are required, we don't need to break any references
             * and decrease any reference count.
             */

            currentObj->setAttr(attr);
        }
    }

    /*
     * Asic create object inserts new fererence to track inside asic view.
     */

    currentView.asicCreateNonObjectIdObject(currentObj);

    /*
     * Move both object status to FINAL since both objects were processed
     * succesfully and object was created.
     */

    currentObj->setObjectStatus(SAI_OBJECT_STATUS_FINAL);
    temporaryObj->setObjectStatus(SAI_OBJECT_STATUS_FINAL);
}/*}}}*/

void createTrapAndSetAttributes(/*{{{*/
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ std::shared_ptr<SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    if (temporaryObj->getObjectType() != SAI_OBJECT_TYPE_TRAP)
    {
        SWSS_LOG_ERROR("expected TRAP, got %s:%s",
                temporaryObj->str_object_type.c_str(),
                temporaryObj->str_object_id.c_str());

        throw std::runtime_error("expected TRAP object");
    }

    /*
     * Ok since traps exist in 0.9.4 SAI then we don't create them we can just
     * set all attributes, but since we don't populate trap objects directly,
     * we need to create in current view empty trap object that will represent
     * temporary object trap.
     */

    const auto &currentObj = currentView.createTrapObject(
            temporaryObj->str_object_type,
            temporaryObj->str_object_id);

    for(const auto &it: temporaryObj->getAllAttributes())
    {
        setAttributeOnCurrentObject(currentView, temporaryView, currentObj, it.second);
    }

    currentObj->setObjectStatus(SAI_OBJECT_STATUS_FINAL);
    temporaryObj->setObjectStatus(SAI_OBJECT_STATUS_FINAL);
}/*}}}*/

/**
 * @brief Create new object from temporary object
 *
 * Since best match was not found we need to create a brand new object and put
 * it into current view as well.
 */
void createNewObjectFromTemporaryObject(/*{{{*/
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ std::shared_ptr<SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("creating object %s:%s",
                    temporaryObj->str_object_type.c_str(),
                    temporaryObj->str_object_id.c_str());

    switch (temporaryObj->getObjectType())
    {
        /*
         * This trap can be default trap group, so let's check if it's,
         * then we can't create it, we need to set new attributes if
         * that possible. But this case should not happen
         * since on syncd start we are putting default trap group to
         * asic view, so if user will query default one, they will
         * be matched by RID.
         *
         * Default trap group is transfered to view on inif if it don't
         * exist.
         */

        case SAI_OBJECT_TYPE_TRAP_GROUP:
        case SAI_OBJECT_TYPE_POLICER:
        case SAI_OBJECT_TYPE_SCHEDULER:
        case SAI_OBJECT_TYPE_WRED:
        case SAI_OBJECT_TYPE_ACL_TABLE:
        case SAI_OBJECT_TYPE_ACL_ENTRY:
        case SAI_OBJECT_TYPE_QOS_MAPS:
        case SAI_OBJECT_TYPE_ROUTER_INTERFACE:
        case SAI_OBJECT_TYPE_NEXT_HOP:
        case SAI_OBJECT_TYPE_NEXT_HOP_GROUP:
        case SAI_OBJECT_TYPE_HOST_INTERFACE:
        case SAI_OBJECT_TYPE_BUFFER_POOL:
        case SAI_OBJECT_TYPE_BUFFER_PROFILE:

            createNewOidObjectFromTemporaryObject(currentView, temporaryView, temporaryObj);
            return;

        case SAI_OBJECT_TYPE_PORT:
        case SAI_OBJECT_TYPE_SWITCH:
        case SAI_OBJECT_TYPE_VIRTUAL_ROUTER:
        case SAI_OBJECT_TYPE_STP_INSTANCE:
        case SAI_OBJECT_TYPE_QUEUE:
        case SAI_OBJECT_TYPE_PRIORITY_GROUP:
        case SAI_OBJECT_TYPE_SCHEDULER_GROUP:
            break;

        case SAI_OBJECT_TYPE_TRAP:

            /*
             * Traps in 0.9.4 exists, and they can be only set, there is no
             * create for them.
             *
             * All traps are defined right away in both views so we should never hit this
             */

            // createTrapAndSetAttributes(currentView, temporaryView, temporaryObj);
            break;

        case SAI_OBJECT_TYPE_VLAN:  // vlan 1 can't be created since it exist by default
        case SAI_OBJECT_TYPE_ROUTE:
        case SAI_OBJECT_TYPE_NEIGHBOR:
        case SAI_OBJECT_TYPE_FDB:

            createNewNonOidObjectFromTemporaryObject(currentView, temporaryView, temporaryObj);
            return;

        default:
            break;
    }

    SWSS_LOG_ERROR("create new object %s:%s from temporay is not supported yet, FIXME",
            temporaryObj->str_object_type.c_str(),
            temporaryObj->str_object_id.c_str());

    throw std::runtime_error("finding current best match failed, not supported yet, FIXME");
}/*}}}*/

void UpdateObjectStatus(/*{{{*/
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ std::shared_ptr<SaiObj> currentBestMatch,
        _In_ std::shared_ptr<SaiObj> temporaryObj)
{
    SWSS_LOG_ENTER();

    bool notprocessed = ((temporaryObj->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED) &&
            (currentBestMatch->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED));

    bool matched = ((temporaryObj->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED) &&
            (currentBestMatch->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED));

    if (notprocessed || matched)
    {
        /*
         * Since everything was processed, then move those object status to
         * FINAL.
         */

        temporaryObj->setObjectStatus(SAI_OBJECT_STATUS_FINAL);
        currentBestMatch->setObjectStatus(SAI_OBJECT_STATUS_FINAL);

        if (temporaryObj->oidObject)
        {
            /*
             * When objects are object id type, we need to update map in
             * temporary view for correct VID/RID.
             *
             * In here RID should alwasy exists, since when objects are MATCHED
             * then RID exists and both VID's are the same, and when both
             * objects are not processed, then RID also exists since current
             * object was selected as current best match. Other options are
             * object was removed, but the it could not be selected, or object
             * was created, but then is in FINAL state so also couldn't be
             * selected here.
             */

            sai_object_id_t tvid = temporaryObj->getVid();
            sai_object_id_t cvid = currentBestMatch->getVid();
            sai_object_id_t rid = currentView.vidToRid.at(cvid);

            SWSS_LOG_INFO("remapped current VID 0x%lx to temp VID 0x%lx using RID 0x%lx", cvid, tvid, rid);

            temporaryView.ridToVid[rid] = tvid;
            temporaryView.vidToRid[tvid] = rid;

            /*
             * TODO set new VID if it don't exist in current view with NULL RID
             * that will mean we created new object, this VID will be later
             * used to count references and as a sanity check if we are
             * increasing valid reference.
             */
        }
        else
        {
            /*
             * For non object id we will create map by object type and map
             * current to temporary and vice versa. For some objects like TRAP
             * or VLAN values will be the same but for ROUTES or NEIGHBORS can
             * be different since they contain VID which in temporary view can
             * be different and because of that we will need a map.
             */

            sai_object_type_t objectType = temporaryObj->getObjectType();

            SWSS_LOG_INFO("remapped %s current %s to temp %s",
                    sai_serialize_object_type(temporaryObj->getObjectType()).c_str(),
                    currentBestMatch->str_object_id.c_str(),
                    temporaryObj->str_object_id.c_str());

            temporaryView.nonObjectIdMap[objectType][temporaryObj->str_object_id] = currentBestMatch->str_object_id;
            currentView.nonObjectIdMap[objectType][currentBestMatch->str_object_id] = temporaryObj->str_object_id;
        }
    }
    else
    {
        /*
         * This method should be used only for objects that have status MATCHED
         * or NON_PROCESSED, other combinations are not suppported since other
         * actions will be eather create new object from temporary object or
         * remove existing current object.
         */

        SWSS_LOG_ERROR("unexpected status combination: current %d, temporary %d",
                currentBestMatch->getObjectStatus(),
                temporaryObj->getObjectStatus());

        throw std::runtime_error("unexpected status combination");
    }
}/*}}}*/

std::shared_ptr<SaiAttr> getSaiAttrFromDefaultValue(/*{{{*/
        _In_ const AsicView &currentView,
        _In_ const sai_attr_metadata_t &meta)
{
    SWSS_LOG_ENTER();

    // TODO move this method to asicview class

    switch (meta.defaultvaluetype)
    {
        case SAI_DEFAULT_VALUE_TYPE_CONST:

            /*
             * only primitives can be supported on CONST
             */

            switch (meta.serializationtype)
            {
                case SAI_SERIALIZATION_TYPE_BOOL:
                case SAI_SERIALIZATION_TYPE_UINT8:
                case SAI_SERIALIZATION_TYPE_INT8:
                case SAI_SERIALIZATION_TYPE_UINT16:
                case SAI_SERIALIZATION_TYPE_INT16:
                case SAI_SERIALIZATION_TYPE_UINT32:
                case SAI_SERIALIZATION_TYPE_INT32:
                case SAI_SERIALIZATION_TYPE_UINT64:
                case SAI_SERIALIZATION_TYPE_INT64:
                case SAI_SERIALIZATION_TYPE_OBJECT_ID:

                    {
                        sai_attribute_t attr;

                        attr.id = meta.attrid;
                        attr.value = meta.defaultvalue;

                        std::string str_attr_value = sai_serialize_attr_value(meta, attr, false);

                        return std::make_shared<SaiAttr>(meta.attridname, str_attr_value);
                    }

                default:

                    SWSS_LOG_ERROR("serialization type %d is not supported yet, FIXME", meta.serializationtype);
                    break;
            }

            break;

        case SAI_DEFAULT_VALUE_TYPE_ATTR_VALUE:

            /*
             * TODO normally we need check defaultobject type and value but
             * this is only available in metadata sai 1.0.
             */

            if (meta.objecttype == SAI_OBJECT_TYPE_TRAP && meta.attrid == SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP)
            {
                /*
                 * Default trap group is set on traps by default, so to bring them to
                 * default state we need to explicitly set this.
                 */

                const auto &tg = currentView.ridToVid.find(currentView.defaultTrapGroupRid);

                if (tg == currentView.ridToVid.end())
                {
                    SWSS_LOG_ERROR("default trap group RID 0x%lx don't exist in current view", currentView.defaultTrapGroupRid);

                    throw std::runtime_error("default trap group don't exist in current view");
                }

                sai_attribute_t at;

                at.id = meta.attrid;
                at.value.oid = tg->second; // default trap group VID

                std::string str_attr_value = sai_serialize_attr_value(meta, at, false);

                return std::make_shared<SaiAttr>(meta.attridname, str_attr_value);
            }

            SWSS_LOG_ERROR("default value type %d is not supported yet for %s, FIXME",
                    meta.defaultvaluetype,
                    meta.attridname);

            break;

        default:

            SWSS_LOG_ERROR("default value type %d is not supported yet for %s, FIXME",
                    meta.defaultvaluetype,
                    meta.attridname);
            break;
    }

    return nullptr;
}/*}}}*/

bool performObjectSetTransition(/*{{{*/
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ std::shared_ptr<SaiObj> currentBestMatch,
        _In_ std::shared_ptr<SaiObj> temporaryObj,
        _In_ bool performTransition)
{
    SWSS_LOG_ENTER();

    /*
     * All parents (if any) are in final state here, we could now search for
     * best match in current view that some of the objects in temp final state
     * could been created so they should exist in current view but without
     * actual RID since all creation and RID mapping is done after all
     * matching, so it may cause problems for finding RID for compare.
     */

    /*
     * When we have best match we need to determine whether current object can
     * be updated to "this temporary" object or whether current needs to be
     * destroyed and recreated according to temporary.
     */

    std::set<sai_attr_id_t> processedAttributes;

    /*
     * Matched objects can have different attributes so we need to mark in
     * processed attributes which one were processed so if current object has
     * more attributes then we need to bring them back to default values if
     * possible.
     */

    /*
     * Depending on performTransition flag this method used in first pass will
     * determine whether current object can be updated to temporary one.  If
     * first pass was successfull second pass when flag is set to true, actual
     * SET operations will be generated and current view will be modified.  No
     * actual ASIC operations will be performed, all ASIC changes will be done
     * after all object will be moved to final state.
     */

    for (auto &at: temporaryObj->getAllAttributes())
    {
        auto &temporaryAttr = at.second;

        SWSS_LOG_INFO("first pass (temp): attr %s", temporaryAttr->getStrAttrId().c_str());

        const auto meta = temporaryAttr->getAttrMetadata();

        const sai_attribute_t &attr = *temporaryAttr->getSaiAttr();

        processedAttributes.insert(attr.id); // mark attr id as processed

        if (currentBestMatch->hasAttr(attr.id))
        {
            /*
             * Same attribute exists on current and temp view, check if it's
             * the same.  Previously we used hasEqualAttribute method to find
             * best match but now we are looking for different attribute
             * values.
             */

            auto currentAttr = currentBestMatch->getSaiAttr(attr.id);

            if (hasEqualAttribute(currentView, temporaryView, currentBestMatch, temporaryObj, attr.id))
            {
                /*
                 * Attributes are equal so go for next attribute
                 */

                continue;
            }

            /*
             * Now we know that attribute values are different.
             */

            /*
             * Here we don't need to check if attribute is mandatory on create
             * or conditional since attribute is present on both objects. If
             * attribute is CREATE_AND_SET that means we can update attribute
             * value on current best match object.
             */

            if (HAS_FLAG_CREATE_AND_SET(meta->flags))
            {
                // TODO move to INFO

                SWSS_LOG_DEBUG("Attr %s can be updated from %s to %s",
                        meta->attridname,
                        currentAttr->getStrAttrValue().c_str(),
                        temporaryAttr->getStrAttrValue().c_str());

                /*
                 * Generate action and update current view in second pass
                 * and continue for next attribute.
                 */

                if (performTransition)
                {
                    setAttributeOnCurrentObject(currentView, temporaryView, currentBestMatch, temporaryAttr);
                }

                continue;
            }

            /*
             * In this place we know attribute is CREATE_ONLY and it's value is
             * different on both objects. Object must be destroyed and new
             * object must be created. In this case does not matter whether
             * attribute is mandatory on create or conditional since attribute
             * is present on both objects.
             */

            SWSS_LOG_WARN("Attr %s CAN'T be updated from %s to %s since it's CREATE_ONLY",
                    meta->attridname,
                    currentAttr->getStrAttrValue().c_str(),
                    temporaryAttr->getStrAttrValue().c_str());

            /*
             * We return false since object can't be updated.  Object creation
             * is in different place when current best match is not found.
             */

            return false;
        }

        /*
         * In this case attribute exists only on temporary object.  Because of
         * different flags, and conditions this maybe not easy task to
         * determine what should happen.
         *
         * Depends on attribute order processing, we may process mandatory on
         * create conditional attribute first, before finding out that
         * condition attribute is different, but if condition would be the same
         * then this conditional attribute would be also present on current
         * best match.
         *
         * There are also default values here that come into play.  We can
         * expand this logic in the future.
         */

        bool conditional = meta->conditions.size() > 0;

        /*
         * If attribute is CREATE_AND_SET and not conditional then it's
         * defenetly safe to make SET operation.
         *
         * XXX previously we had (meta->flags == SAI_ATTR_FLAGS_CREATE_AND_SET)
         * If it's not conditional current HAS_FLAG should not matter. But it
         * can also be mandatory on create but this does not matter since if
         * it's mandtory on create then current object already exists co we can
         * stil perform update on this attribute because it was passed during
         * creation.
         */

        if (HAS_FLAG_CREATE_AND_SET(meta->flags) && !conditional)
        {
            SWSS_LOG_INFO("Missing current attr %s can be set to %s",
                    meta->attridname,
                    temporaryAttr->getStrAttrValue().c_str());

            /*
             * Generate action and update current view in second pass
             * and continue for next attribute.
             */

            if (performTransition)
            {
                setAttributeOnCurrentObject(currentView, temporaryView, currentBestMatch, temporaryAttr);
            }

            continue;
        }

        /*
         * This is the most interesting case, we currently leave it here and we
         * will support it later. Some other cases here also can be considered
         * as success, for example if default value is the same as current
         * value.
         */

        SWSS_LOG_WARN("Missing current attr %s (conditional: %d) CAN'T be set to %s, flags: 0x%x, FIXME",
                meta->attridname,
                conditional,
                temporaryAttr->getStrAttrValue().c_str(),
                meta->flags);

        /*
         * We can't continue with update in that case, so return false.
         */

        return false;
    }

    /*
     * Current best match can have more attributes than temporary object.
     * let see if we can bring them to default value if possible.
     */

    for (auto &ac: currentBestMatch->getAllAttributes())
    {
        auto &currentAttr = ac.second;

        const auto meta = currentAttr->getAttrMetadata();

        const sai_attribute_t &attr = *currentAttr->getSaiAttr();

        if (processedAttributes.find(attr.id) != processedAttributes.end())
        {
            /*
             * This attibute was processed in previous temporary attributes processing so skip it here.
             */

            continue;
        }

        SWSS_LOG_INFO("first pass (curr): attr %s", currentAttr->getStrAttrId().c_str());

        /*
         * We should not have MANDATORY_ON_CREATE attributes here since all
         * mandatory on create (even conditional) should be present in in
         * previous loop and they are matching, so we should get here
         * CREATE_ONLY or CREATE_AND_SET attributes only. So we should not get
         * conditional attributes here also, but lets take extra care about that
         * just as sanity check.
         */

        bool conditional = meta->conditions.size() > 0;

        if (conditional || HAS_FLAG_MANDATORY_ON_CREATE(meta->flags))
        {
            if (currentBestMatch->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED &&
                    HAS_FLAG_CREATE_AND_SET(meta->flags))
            {
               //   SAI_QUEUE_ATTR_PARENT_SCHEDULER_NODE
               //   SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID
               //   SAI_SCHEDULER_GROUP_ATTR_PARENT_NODE
               //   SAI_BRIDGE_PORT_ATTR_BRIDGE_ID
               //
               //   TODO matched by ID (MATCHED state) should always be updatable
               //   except those 4 above (at least for those above since they can have
               //   default value present after switch creation

                // TODO SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID is mandatory on create but also SET
                // if attribute is set we and objest is in MATCHED state then that means we are able to
                // bring this attribute to default state not for all attributes!

                SWSS_LOG_ERROR("current attribute is mandatory on create, crate and set, and object MATCHED, FIXME %s:%s",
                        meta->attridname,
                        currentAttr->getStrAttrValue().c_str());

                return false;
            }

            SWSS_LOG_ERROR("Present current attr %s:%s is conditional or MANDATORY_ON_CREATE, we don't expect this here, FIXME",
                    meta->attridname,
                    currentAttr->getStrAttrValue().c_str());

            /*
             * We don't expect conditional or mandatory on create attributes,
             * since in previous loop all attributes were matching they also
             * had to match.  If we hit this case we need to take a closer look
             * since it will mean we have a bug somewhere.
             */

            return false;
        }

        /*
         * If attribute is CREATE_AND_SET or CREATE_ONLY, they may have a
         * default value, for create and set we maybe able to set it and for
         * create only we just need to make sure its expected value, if not
         * then it can't be updated and we need to return false.
         *
         * TODO Currently we will support limited default value types.
         *
         * TODO Later this comparison of default value we need to extract to
         * separate functions. Maybe create SaiAttr from default value or
         * nullptr if it's not supported yet.
         */

        if (meta->flags == SAI_ATTR_FLAGS_CREATE_AND_SET || meta->flags == SAI_ATTR_FLAGS_CREATE_ONLY)
        {
            const auto defaultValueAttr = getSaiAttrFromDefaultValue(currentView, *meta);

            if (defaultValueAttr == nullptr)
            {
                if (meta->objecttype == SAI_OBJECT_TYPE_SWITCH &&
                        meta->attrid == SAI_SWITCH_ATTR_SRC_MAC_ADDRESS)
                {
                    /*
                     * TODO: we need to store somewhere default value for
                     * switch MAC.  Also not sure if this check should be here
                     * ? or should it be in remove phase. Problem here is that
                     * switch will be matched (current best match) but since we
                     * can't get default value, then what will happen we will
                     * try to remove switch and recreate it.
                     *
                     * Same will apply for default values which are pointing to
                     * different attributes.
                     */

                    SWSS_LOG_WARN("can't bring default value for %s, FIXME", meta->attridname);
                    continue;
                }

                SWSS_LOG_WARN("Can't get default value for present current attr %s:%s, FIXME",
                        meta->attridname,
                        currentAttr->getStrAttrValue().c_str());

                /*
                 * If we can't get default value then we can't do set, because
                 * we don't know with what, so we need to destroy current
                 * object and recreate new one from temporary.
                 */

                return false;
            }

            if (currentAttr->getStrAttrValue() == defaultValueAttr->getStrAttrValue())
            {
                // TODO convert to info
                SWSS_LOG_INFO("Present current attr %s value %s is the same as default value, no action needed",
                    meta->attridname,
                    currentAttr->getStrAttrValue().c_str());

                continue;
            }

            if (meta->flags == SAI_ATTR_FLAGS_CREATE_ONLY)
            {
                SWSS_LOG_WARN("Present current attr %s:%s has default that CAN'T be set to %s since it's CREATE_ONLY",
                        meta->attridname,
                        currentAttr->getStrAttrValue().c_str(),
                        defaultValueAttr->getStrAttrValue().c_str());

                return false;
            }

            SWSS_LOG_INFO("Present current attr %s:%s has default that can be set to %s",
                    meta->attridname,
                    currentAttr->getStrAttrValue().c_str(),
                    defaultValueAttr->getStrAttrValue().c_str());

            /*
             * Generate action and update current view in second pass
             * and continue for next attribute.
             */

            if (performTransition)
            {
                setAttributeOnCurrentObject(currentView, temporaryView, currentBestMatch, defaultValueAttr);
            }

            continue;
        }

        SWSS_LOG_ERROR("we should not get here, we have a bug, current present attribute %s:%s has some wrong flags 0x%x",
                    meta->attridname,
                    currentAttr->getStrAttrValue().c_str(),
                    meta->flags);

        throw std::runtime_error("unexpected current preset attribute flags, FIXME");
    }

    /*
     * All attributes were processed, and ether no changes are required or all
     * changes can be performed or some missing attributes has exact value as
     * default value.
     */

    return true;
}/*}}}*/

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
 * NOTE: Development is in progress, not all corner cases
 * are supported yet.
 */
void processObjectForViewTransition(/*{{{*/
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ std::shared_ptr<SaiObj> temporaryObj)
{
    SWSS_LOG_ENTER();

    if (temporaryObj->getObjectStatus() == SAI_OBJECT_STATUS_FINAL)
    {
        /*
         * If object is in final state, no need to process it again, nothing
         * about this object will change.
         */

        return;
    }

    procesObjectAttributesForViewTransition(currentView, temporaryView, temporaryObj);

    /*
     * Now since all object ids (VIDs) were processed for temporary object we
     * can try to find current best match.
     */

    std::shared_ptr<SaiObj> currentBestMatch = findCurrentBestMatch(currentView, temporaryView, temporaryObj);

    /*
     * So there will be interesting problem, when we don't find best matching
     * object, but actual object will exist, and it will have the same KEY
     * attribute like, and some other attribute will be create_only and it will
     * be differerent then we can't just create temporary object because of key
     * will match and we have conflict, so we need first in this case remove
     * prebious object with key
     *
     * so there are 2 options here:
     *
     * - eather our finding best match function will always include matching
     *   keys (if more keys, they all should (or any?) match should be right
     *   away taken) and this should be only object returned on list possible
     *   candidates as best match then below logic should figure out that we
     *   cant do "set" and we need to destroy object and destroy it now, and
     *   create temporary later this means that finding best logic can return
     *   objects that are not upgradable "cant do set"
     *
     * - or secnd solution, in this case, finding best match will not include
     *   this object on list, since other create only attribute will be
     *   different which also means we need to destroy and recreate, but in
     *   this case finding best match will figure this out and will destroy
     *   object on the way since it will know that set transition is not
     *   possible so here when we find best match (other object) in that case
     *   we always will be able to do "SET", or when no object will be returned
     *   then we can just righ away create this object since all keys objects
     *   were removed by finding best match
     *
     * in both cases logic is the same, someone needs to figure out wheter
     * updating object and set is possible or wheter we leave object alone, or
     * we delete it before creating new one object
     *
     * preferebly this logic could be in both but that duplicates compare
     * actions i would rather want this in finding best match, but that would
     * modify current view which i dont like much and here it seems like to
     * much hassle since after finding best match here we would have simple
     * operations
     *
     * currently KEY are in: vlan, queue, port, hostif_trap
     * - port - hw lane, we don't need to worry since we match them all
     * - queue - all other attributes can be set
     * - vlan - all ather attributes can be set
     * - hostif_trap - all other attributes can be set
     *
     * We could add a check for that in sanity
     */

    if (currentBestMatch == nullptr)
    {
        /*
         * Since we didn't found current best match, just create this object as
         * a new object, and put it also in current view in final state.
         */

        SWSS_LOG_INFO("failed to find best match %s %s in current view, will create new object",
                temporaryObj->str_object_type.c_str(),
                temporaryObj->str_object_id.c_str());

        createNewObjectFromTemporaryObject(currentView, temporaryView, temporaryObj);
        return;
    }

    SWSS_LOG_INFO("found best match %s: current: %s temporary: %s",
            currentBestMatch->str_object_type.c_str(),
            currentBestMatch->str_object_id.c_str(),
            temporaryObj->str_object_id.c_str());

    /*
     * We need to two passes, for not matched parameters since if first
     * atttribute will modify current object like SET operation, but second
     * attribute will not be able to do update becasue of CREATE_ONLY etc, then
     * we will end up with half modified current object and VIEW. So we have
     * two choices here when update is not possible:
     *
     * - remove current object and it's childs and create temporary one, or
     * - leave current object as unprocessed (dry run) for maybe future best
     *   match, and just create temporary object
     *
     * We will choose approach witch leaving object untouched if something will
     * go wrong, we can always switch back to destroy current best match if
     * that would be better approach. It could be actually param of comparison
     * logic.
     *
     * NOTE: this function is called twice if first time will be successfull
     * then logs will be doubled in syslog.
     */

    bool passed = performObjectSetTransition(currentView, temporaryView, currentBestMatch, temporaryObj, false);

    if (!passed)
    {
        /*
         * First pass was a failure, so we can't update existing object with
         * temporary one, probably because of CRATE_ONLY attributes.
         *
         * Our strategy here will be to leave untouched current object (second
         * strategy will be to remove it and it's childeren if link can't be
         * broken, Objects with non id like ROUTE FDB NEIGHBOR they need to be
         * removed first anyway, since we got best match, and for those best
         * match is via struct elements so we can't create next route with the
         * same elements in the struct.
         *
         * Then we create new object from temporary one and set it state to
         * final.
         */

        switch (temporaryObj->getObjectType())
        {
            case SAI_OBJECT_TYPE_ROUTE:
            case SAI_OBJECT_TYPE_FDB:
            case SAI_OBJECT_TYPE_NEIGHBOR:
            case SAI_OBJECT_TYPE_VLAN: // may be not possible may have dependencies
            case SAI_OBJECT_TYPE_SWITCH: // switch can't be removed
            case SAI_OBJECT_TYPE_TRAP: // traps are always defined and should only support set

                removeExistingObjectFromCurrentView(currentView, temporaryView, currentBestMatch);
                break;

            default:

                /*
                 * Later on if we decide we want to remove objects before
                 * creating new one's we need to put here this action, or just
                 * remove this entire switch and call remove.
                 */

                break;
        }

        createNewObjectFromTemporaryObject(currentView, temporaryView, temporaryObj);

        return;
    }

    /*
     * First pass was successfull, so we can do update on current object, lets do that now!
     */

    if (temporaryObj->oidObject && (temporaryObj->getObjectStatus() != SAI_OBJECT_STATUS_MATCHED))
    {
        /*
         * We track all oid object references, and since this objest was
         * matched we need to insert it's vid to reference map since when we
         * will update attributes in current view from temporary view
         * attributes, then this object must be in reference map. This is
         * sanity check. Object can't be in matched state since matched objects
         * have the same RID and VID in both views, so VID already exists in
         * reference map.
         *
         * This object VID will be remapped inside UpdateObjectStatus to
         * rid/vid map since if we are here then we matched one of current
         * objects to this temporary obejct and update can be performed.
         */

        // TODO this is temporary VID, it's not available in current view
        // so we need to remove this code here, since we will translate
        // this VID in temporary view to current view

        sai_object_id_t vid = temporaryObj->getVid();

        currentView.insertNewVidReference(vid);
    }

    performObjectSetTransition(currentView, temporaryView, currentBestMatch, temporaryObj, true);

    /*
     * Since we got here, that means matching or updating current best match
     * object was successful, and we can now set their state to final.
     */

    UpdateObjectStatus(currentView, temporaryView, currentBestMatch, temporaryObj);
}/*}}}*/

void processObjectType(/*{{{*/
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ sai_object_type_t object_type)
{
    SWSS_LOG_ENTER();

    auto objects = temporaryView.getObjectsByObjectType(object_type);

    for (const auto &o: objects)
    {
        processObjectForViewTransition(currentView, temporaryView, o);
    }

    SWSS_LOG_NOTICE("processed %s", sai_serialize_object_type(object_type).c_str());
}/*}}}*/

void checkSwitch(/*{{{*/
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView)
{
    SWSS_LOG_ENTER();

    size_t csize = currentView.soSwitches.size();
    size_t tsize = temporaryView.soSwitches.size();

    if (csize == 0 && tsize == 0)
    {
        return;
    }

    if (csize == 1 && tsize == 1)
    {
        return;
    }

    /*
     * Current logic supports only 1 instance of switch so in both views we
     * should have only 1 instance of switch and they must match.
     */

    SWSS_LOG_ERROR("unsupported number of switches: current %zu, temp %zu", csize, tsize);

    throw std::runtime_error("unsupported number of switches");

    /*
     * In current SAI 0.9.4 switch is non object id so we don't need to make
     * VID/RID for it, but in SAI 1.0 it will be object id so we need to change
     * our strategy here.
     *
     * Later on we can support in 0.9.4 case where one or both switches can be
     * missing since no attributes were set.
     */
}/*}}}*/

void bringDefaultTrapGroupToFinalState(/*{{{*/
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView)
{
    SWSS_LOG_ENTER();

    sai_object_id_t rid = currentView.defaultTrapGroupRid;

    if (temporaryView.hasRid(rid))
    {
        /*
         * Default trap group is defined inside temporary view
         * so it will be matched by RID, no need to process.
         */

        return;
    }

    sai_object_id_t vid = currentView.ridToVid.at(rid);

    auto const &dtgObj = currentView.oOids.at(vid);

    if (dtgObj->getObjectStatus() != SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        /*
         * Object was processed, no further actions are required.
         */

        return;
    }

    /*
     * Trap group was not processed, it can't be removed so bring it to default
     * state and release existing references.
     */

    bringNonRemovableObjectToDefaultState(currentView, temporaryView, dtgObj);
}/*}}}*/

sai_status_t applyViewTransition(/*{{{*/
        _In_ AsicView &current,
        _In_ AsicView &temp)
{
    SWSS_LOG_ENTER();

    checkSwitch(current, temp);

    checkMatchedPorts(temp);

    /*
     * Process all objects
     */

    /*
     * During iteration no object from temp view are removed, so no need to
     * worry about any iterator issues here since removed objects are only from
     * current view.
     */

    for (auto &obj: temp.soAll)
    {
        processObjectForViewTransition(current, temp, obj.second);
    }

    /*
     * There is a problem here with default trap group, since when other trap
     * groups are created and used in traps, then when removing them we reset
     * trap group on traps to default one, and this increases reference count
     * so in this loop, default trap group will not be considered to be removed
     * and it will stay not processed since it will hold reference count on
     * traps. So what we can do here, we can explicitly move default trap group
     * to FINAL state if it's not defined in temporary view, but this problem
     * should go away when we will put all existing objects to temporary view
     * if they don't exist.
     *
     * Similar thing can happen when we start using PROFILE_ID on
     * SCHEDULER_GROUP.
     */

    bringDefaultTrapGroupToFinalState(current, temp);

    /*
     * Removing needs to be done from leaf with no references and it can be
     * multiple passes since if in first pass object had non zero references,
     * it can have zero in next pass so it is safe to remove.
     */

    for (int removed = 1; removed != 0 ;)
    {
        removed = 0;

        for (const auto &obj: current.getAllNotProcessedObjects())
        {
            /*
             * What can happen during this processing some object state during
             * processing can change from not processed to removed, if it have
             * references, currently we are only removing objects with zero
             * references, so this will not happen but in general case this
             * will be the case.
             */

            if (obj->oidObject)
            {
                if (current.getVidReferenceCount(obj->getVid()) == 0)
                {
                    /*
                     * Reference count on this VID is zero, so it's safe to
                     * remove this object.
                     */

                    removeExistingObjectFromCurrentView(current, temp, obj);
                    removed++;
                }
            }
            else
            {
                /*
                 * Non object id objects don't have references count, they are leafs
                 * so we can remove them right away
                 */

                removeExistingObjectFromCurrentView(current, temp, obj);
                removed++;
            }
        }

        SWSS_LOG_NOTICE("- loop removed (%d)", removed);
    }

    return SAI_STATUS_SUCCESS;
}/*}}}*/

void executeOperationsOnAsic(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView);

void populateExistingObjects(/*{{{*/
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ const std::set<sai_object_id_t> rids)
{
    SWSS_LOG_ENTER();

    /*
     * If some objects that are existing objects on switch are not present in
     * temporary view, just populate them with empty values.  Since vid2rid
     * maps must match on both view after apply view.
     */

    for (const auto rid: rids)
    {
        if (rid == SAI_NULL_OBJECT_ID)
        {
            continue;
        }

        /*
         * Rid's are not matched yet, but if VID is the same then we have the
         * same object.
         */

        if (temporaryView.hasRid(rid))
        {
            continue;
        }

        auto it = currentView.ridToVid.find(rid);

        if (it == currentView.ridToVid.end())
        {
            SWSS_LOG_ERROR("unable to find existing object RID 0x%lx in current view", rid);

            throw std::runtime_error("unable to find existing object RID in current view");
        }

        sai_object_id_t vid = it->second;

        temporaryView.createDummy(rid, vid);

        SWSS_LOG_DEBUG("populate existing RID/VID 0x%lx 0x%lx", rid, vid);

        /*
         * Move both objects to matched state since match oids was already
         * called, and here we created some new objects that should be matched.
         */

        currentView.oOids.at(vid)->setObjectStatus(SAI_OBJECT_STATUS_MATCHED);
        temporaryView.oOids.at(vid)->setObjectStatus(SAI_OBJECT_STATUS_MATCHED);
    }
}/*}}}*/

void updateRedisDatabase(/*{{{*/
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView)
{
    SWSS_LOG_ENTER();

    /*
     * TODO we can make lua script for this which will be much faster.
     */

    SWSS_LOG_TIMER("redis update");

    /*
     * Remove Asic State Table
     */

    const auto &asicStateKeys = g_redisClient->keys(ASIC_STATE_TABLE + std::string(":*"));

    for (const auto &key: asicStateKeys)
    {
        g_redisClient->del(key);
    }

    /*
     * Remove Temp Asic State Table
     */

    const auto &tempAsicStateKeys = g_redisClient->keys(TEMP_PREFIX ASIC_STATE_TABLE + std::string(":*"));

    for (const auto &key: tempAsicStateKeys)
    {
        g_redisClient->del(key);
    }

    /*
     * Save temporary view as current view in redis database.
     */

    for (const auto &pair: temporaryView.soAll)
    {
        const auto &obj = pair.second;

        const auto &attr = obj->getAllAttributes();

        std::string key = std::string(ASIC_STATE_TABLE) + ":" + obj->str_object_type + ":" + obj->str_object_id;

        if (attr.size() == 0)
        {
            /*
             * Object has no attributes, so populate using NULL just to
             * indicate that object exists.
             */

            g_redisClient->hset(key, "NULL", "NULL");
        }
        else
        {
            for (const auto &ap: attr)
            {
                const auto saiAttr = ap.second;

                g_redisClient->hset(key, saiAttr->getStrAttrId(), saiAttr->getStrAttrValue());
            }
        }
    }

    /*
     * Remove previous RID2VID maps and apply new map.
     */

    redisClearVidToRidMap();
    redisClearRidToVidMap();

    for (auto &kv: temporaryView.ridToVid)
    {
        std::string strVid = sai_serialize_object_id(kv.first);
        std::string strRid = sai_serialize_object_id(kv.second);

        g_redisClient->hset(VIDTORID, strVid, strRid);
        g_redisClient->hset(RIDTOVID, strRid, strVid);
    }
}/*}}}*/

sai_status_t internalSyncdApplyView()/*{{{*/
{
    sai_status_t status;

    SWSS_LOG_ENTER();

    /*
     * Initialize rand for future candidate object selection if necessary.
     *
     * NOTE: should this be deterministic? So we could repeat random choice
     * when something bad happen or we hit a bug, so in that case it will be
     * easier for reproduce, we could at least log valur returned from time().
     *
     * TODO: to make it stable, we also need to make stable redisGetAsicView
     * sinec now order of items is random. Also redis result needs to be
     * sorted.
     */

    std::srand((unsigned int)std::time(0));

    AsicView current;
    AsicView temp;

    ObjectIdMap vidToRidMap = redisGetVidToRidMap();
    ObjectIdMap ridToVidMap = redisGetRidToVidMap();

    current.ridToVid = ridToVidMap;
    current.vidToRid = vidToRidMap;

    /*
     * Those calls could be calls to SAI, but when this will be separate lib
     * then we would like to limit sai to minimum or reimplement getting those.
     */

    current.cpuPortRid              = redisGetCpuId();
    current.defaultVirtualRouterRid = redisGetDefaultVirtualRouterId();
    current.defaultTrapGroupRid     = redisGetDefaultTrapGroupId();
    current.defaultStpInstanceRid   = redisGetDefaultStpInstanceId();

    temp.cpuPortRid                 = current.cpuPortRid;
    temp.defaultVirtualRouterRid    = current.defaultVirtualRouterRid;
    temp.defaultTrapGroupRid        = current.defaultTrapGroupRid;
    temp.defaultStpInstanceRid      = current.defaultStpInstanceRid;

    /*
     * Read current and temporary view from REDIS.
     */

    redisGetAsicView(ASIC_STATE_TABLE, current);
    redisGetAsicView(TEMP_PREFIX ASIC_STATE_TABLE, temp);

    std::set<sai_object_id_t> existingObjects;

    existingObjects.insert(current.cpuPortRid);
    existingObjects.insert(current.defaultVirtualRouterRid);
    existingObjects.insert(current.defaultTrapGroupRid);
    existingObjects.insert(current.defaultStpInstanceRid);

    /*
     * Match oids before calling populate existing objects since after matching
     * oids rid and vid maps will be populated.
     */

    matchOids(current, temp);

    /*
     * Pupulate existing objects to current and temp view if they don't exist
     * sinc we are populating them when syncd starts, and when we switch view
     * we don't wan to loose any of those objects since during syncd rungime is
     * counting on that that those objects exists.
     */

    populateExistingObjects(current, temp, existingObjects);
    populateExistingObjects(current, temp, g_defaultQueuesRids);
    populateExistingObjects(current, temp, g_defaultPriorityGroupsRids);
    populateExistingObjects(current, temp, g_defaultSchedulerGroupsRids);
    populateExistingObjects(current, temp, g_defaultPortsRids);

    status = applyViewTransition(current, temp);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("apply view transition failed: %s", sai_serialize_status(status).c_str());

        return status;
    }

    SWSS_LOG_WARN("asic operations to execute: %zu", current.asicGetOperationsCount());

    status = checkObjectsStatus(temp);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("object status failed on temp view: %s", sai_serialize_status(status).c_str());

        return status;
    }

    SWSS_LOG_NOTICE("all temporary view objects were processed to FINAL state");

    status = checkObjectsStatus(current);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("object status failed on current view: %s", sai_serialize_status(status).c_str());

        return status;
    }

    SWSS_LOG_NOTICE("all current view objects were processed to FINAL state");

    /*
     * After all operations both views should look the same so number of
     * rid/vid should look the same.
     */

    if ((current.ridToVid.size() != temp.ridToVid.size()) ||
            (current.vidToRid.size() != temp.vidToRid.size()))
    {
        // TODO for debug we will need to display differences

        SWSS_LOG_ERROR("wrong number of vid/rid items in map, forgot to translate? R2V: %zu:%zu, V2R: %zu:%zu, display which, FIXME",
                current.ridToVid.size(),
                temp.ridToVid.size(),
                current.vidToRid.size(),
                temp.vidToRid.size());

        return SAI_STATUS_FAILURE;
    }

    /*
     * At the end number of soAll objects must be equal on both view if some on
     * temporary views are missing, we need to transport empty objects to
     * temproary view, like queus, scheduler groups, virtual router, trap
     * groups etc.
     */

    if (current.soAll.size() != temp.soAll.size())
    {
        // TODO for debug we will need to display differences

        /*
         * If this will happen that means non object id values are different
         * since number of RID/VID maps is identical (previous check).
         *
         * Unlikely to be routes/neighbors/fdbs, can be traps, switch, vlan.
         */

        SWSS_LOG_ERROR("wrong number of all objects current: %zu vs temp %zu, display which, FIXME", current.soAll.size(), temp.soAll.size());

        return SAI_STATUS_FAILURE;
    }

    executeOperationsOnAsic(current, temp);

    updateRedisDatabase(current, temp);

    return status;
}/*}}}*/

sai_status_t syncdApplyView()/*{{{*/
{
    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    sai_status_t status = SAI_STATUS_FAILURE;

    {
        SWSS_LOG_TIMER("apply"); // TODO we need this as notice

        try
        {
            status = internalSyncdApplyView();
        }
        catch (const std::runtime_error &e)
        {
            SWSS_LOG_ERROR("Exception: %s", e.what());
        }

        swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_INFO);
    }

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    return status;
}/*}}}*/

/*
 * Below we will duplicate asic execution logic for asic operations.
 *
 * Since we have objects on our disposal and actual attributes, we don't need
 * to do serialize/deserialize twice, we can take advantage of that.
 * Only VID to RID translation is needed, and GET api is not supported
 * anyway since comparison logic will do only SET/REMOVE/CREATE then
 * all objects can be set to consts.
 *
 * We can address that when actual asic switch will be more time
 * consuming than we expect.
 *
 * NOTE: Also instead of passing views everywhere we could close this in a
 * class that will have 2 members current and temporary view. This could be
 * helpfull when we will have to manage multiple switches at the same time.
 */

sai_object_id_t asic_translate_vid_to_rid(/*{{{*/
        _In_ const AsicView &current,
        _In_ const AsicView &temporary,
        _In_ sai_object_id_t vid)
{
    SWSS_LOG_ENTER();

    if (vid == SAI_NULL_OBJECT_ID)
    {
        return SAI_NULL_OBJECT_ID;
    }

    auto currentIt = current.vidToRid.find(vid);

    if (currentIt == current.vidToRid.end())
    {
        /*
         * If object was removed it RID was moved to removed map.  This is
         * required since at the end we need to check whether rid/vid maps are
         * the same for both views.
         */

        currentIt = current.removedVidToRid.find(vid);

        if (currentIt == current.removedVidToRid.end())
        {
            SWSS_LOG_ERROR("unable to find VID 0x%lx in current view", vid);

            throw std::runtime_error("unable to find VID in current view");
        }
    }

    sai_object_id_t rid = currentIt->second;

    return rid;
}/*}}}*/

void asic_translate_list_vid_to_rid(/*{{{*/
        _In_ const AsicView &current,
        _In_ const AsicView &temporary,
        _Inout_ sai_object_list_t &element)
{
    for (uint32_t i = 0; i < element.count; i++)
    {
        element.list[i] = asic_translate_vid_to_rid(current, temporary, element.list[i]);
    }
}/*}}}*/

void asic_translate_vid_to_rid_list(/*{{{*/
        _In_ const AsicView &current,
        _In_ const AsicView &temporary,
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // all id's reseived from view should be virtual, so
    // lets translate them to real id's

    for (uint32_t i = 0; i < attr_count; i++)
    {
        sai_attribute_t &attr = attr_list[i];

        auto meta = get_attribute_metadata(object_type, attr.id);

        // this should not happen we should get list right away from SaiAttr

        if (meta == NULL)
        {
            SWSS_LOG_ERROR("unable to get metadata for object type %x, attribute %d", object_type, attr.id);

            // asic will be in inconsistent state at this point

            exit_and_notify(EXIT_FAILURE);
            throw std::runtime_error("unable to get metadata");
        }

        switch (meta->serializationtype)
        {
            case SAI_SERIALIZATION_TYPE_OBJECT_ID:
                attr.value.oid = asic_translate_vid_to_rid(current, temporary, attr.value.oid);
                break;

            case SAI_SERIALIZATION_TYPE_OBJECT_LIST:
                asic_translate_list_vid_to_rid(current, temporary, attr.value.objlist);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:

                if (attr.value.aclfield.enable)
                {
                    attr.value.aclfield.data.oid = asic_translate_vid_to_rid(current, temporary, attr.value.aclfield.data.oid);
                }

                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:

                if (attr.value.aclfield.enable)
                {
                    asic_translate_list_vid_to_rid(current, temporary, attr.value.aclfield.data.objlist);
                }

                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:

                if (attr.value.aclaction.enable)
                {
                    attr.value.aclaction.parameter.oid = asic_translate_vid_to_rid(current, temporary, attr.value.aclaction.parameter.oid);
                }

                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:

                if (attr.value.aclaction.enable)
                {
                    asic_translate_list_vid_to_rid(current, temporary, attr.value.aclaction.parameter.objlist);
                }

                break;

            default:
                break;
        }
    }
}/*}}}*/

sai_status_t asic_handle_generic(/*{{{*/
        _In_ AsicView &current,
        _In_ AsicView &temporary,
        _In_ sai_object_type_t object_type,
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_object_id_t object_id;
    sai_deserialize_object_id(str_object_id, object_id);

    SWSS_LOG_DEBUG("common generic api: %d", api);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            {
                SWSS_LOG_DEBUG("generic create for object type %s", sai_serialize_object_type(object_type).c_str());

                create_fn create = common_create[object_type];

                sai_object_id_t real_object_id;
                sai_status_t status = create(&real_object_id, attr_count, attr_list);

                if (status == SAI_STATUS_SUCCESS)
                {
                    current.ridToVid[real_object_id] = object_id;
                    current.vidToRid[object_id] = real_object_id;

                    temporary.ridToVid[real_object_id] = object_id;
                    temporary.vidToRid[object_id] = real_object_id;

                    std::string str_vid = sai_serialize_object_id(object_id);
                    std::string str_rid = sai_serialize_object_id(real_object_id);

                    SWSS_LOG_NOTICE("saved VID %s to RID %s", str_vid.c_str(), str_rid.c_str());
                }
                else
                {
                    SWSS_LOG_ERROR("failed to create %s %s",
                            sai_serialize_object_type(object_type).c_str(),
                            sai_serialize_status(status).c_str());
                }

                return status;
            }

        case SAI_COMMON_API_REMOVE:
            {
                SWSS_LOG_DEBUG("generic remove for object type %s", sai_serialize_object_type(object_type).c_str());

                remove_fn remove = common_remove[object_type];

                sai_object_id_t rid = asic_translate_vid_to_rid(current, temporary, object_id);

                std::string str_vid = sai_serialize_object_id(object_id);
                std::string str_rid = sai_serialize_object_id(rid);

                /*
                 * Since object was removed, then we also need to remove it
                 * from removedVidToRid map jsut in case if there is some bug.
                 */

                current.removedVidToRid.erase(object_id);

                return remove(rid);
            }

        case SAI_COMMON_API_SET:
            {
                SWSS_LOG_DEBUG("generic set for object type %s", sai_serialize_object_type(object_type).c_str());

                set_attribute_fn set = common_set_attribute[object_type];

                sai_object_id_t rid = asic_translate_vid_to_rid(current, temporary, object_id);

                return set(rid, attr_list);
            }

        default:
            SWSS_LOG_ERROR("generic other apis not implemented");
            return SAI_STATUS_FAILURE;
    }
}/*}}}*/

sai_status_t asic_handle_fdb(/*{{{*/
        _In_ const AsicView &current,
        _In_ const AsicView &temporary,
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_fdb_entry_t fdb_entry;
    sai_deserialize_fdb_entry(str_object_id, fdb_entry);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return sai_fdb_api->create_fdb_entry(&fdb_entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return sai_fdb_api->remove_fdb_entry(&fdb_entry);

        case SAI_COMMON_API_SET:
            return sai_fdb_api->set_fdb_entry_attribute(&fdb_entry, attr_list);

        default:
            SWSS_LOG_ERROR("fdb other apis not implemented");
            return SAI_STATUS_FAILURE;
    }
}/*}}}*/

sai_status_t asic_handle_switch(/*{{{*/
        _In_ const AsicView &current,
        _In_ const AsicView &temporary,
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    switch (api)
    {
        case SAI_COMMON_API_SET:
            return sai_switch_api->set_switch_attribute(attr_list);

        default:
            SWSS_LOG_ERROR("switch other apis not implemented");
            return SAI_STATUS_FAILURE;
    }
}/*}}}*/

sai_status_t asic_handle_neighbor(/*{{{*/
        _In_ const AsicView &current,
        _In_ const AsicView &temporary,
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_neighbor_entry_t neighbor_entry;
    sai_deserialize_neighbor_entry(str_object_id, neighbor_entry);

    neighbor_entry.rif_id = asic_translate_vid_to_rid(current, temporary, neighbor_entry.rif_id);

    SWSS_LOG_DEBUG("neighbor: %s", str_object_id.c_str());

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return sai_neighbor_api->create_neighbor_entry(&neighbor_entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return sai_neighbor_api->remove_neighbor_entry(&neighbor_entry);

        case SAI_COMMON_API_SET:
            return sai_neighbor_api->set_neighbor_attribute(&neighbor_entry, attr_list);

        default:
            SWSS_LOG_ERROR("neighbor other apis not implemented");
            return SAI_STATUS_FAILURE;
    }
}/*}}}*/

sai_status_t asic_handle_route(/*{{{*/
        _In_ const AsicView &current,
        _In_ const AsicView &temporary,
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_unicast_route_entry_t route_entry;
    sai_deserialize_route_entry(str_object_id, route_entry);

    route_entry.vr_id = asic_translate_vid_to_rid(current, temporary, route_entry.vr_id);

    SWSS_LOG_DEBUG("route: %s", str_object_id.c_str());

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return sai_route_api->create_route(&route_entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return sai_route_api->remove_route(&route_entry);

        case SAI_COMMON_API_SET:
            return sai_route_api->set_route_attribute(&route_entry, attr_list);

        default:
            SWSS_LOG_ERROR("route other apis not implemented");
            return SAI_STATUS_FAILURE;
    }
}/*}}}*/

sai_status_t asic_handle_vlan(/*{{{*/
        _In_ const AsicView &current,
        _In_ const AsicView &temporary,
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_vlan_id_t vlan_id;
    sai_deserialize_vlan_id(str_object_id, vlan_id);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return sai_vlan_api->create_vlan(vlan_id);

        case SAI_COMMON_API_REMOVE:
            return sai_vlan_api->remove_vlan(vlan_id);

        case SAI_COMMON_API_SET:
            return sai_vlan_api->set_vlan_attribute(vlan_id, attr_list);

        default:
            SWSS_LOG_ERROR("vlan other apis not implemented");
            return SAI_STATUS_FAILURE;
    }
}/*}}}*/

sai_status_t asic_handle_trap(/*{{{*/
        _In_ const AsicView &current,
        _In_ const AsicView &temporary,
        _In_ std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_hostif_trap_id_t trap_id;
    sai_deserialize_hostif_trap_id(str_object_id, trap_id);

    switch (api)
    {
        case SAI_COMMON_API_SET:
            return sai_hostif_api->set_trap_attribute(trap_id, attr_list);

        default:
            SWSS_LOG_ERROR("trap other apis not implemented");
            return SAI_STATUS_FAILURE;
    }
}/*}}}*/

sai_status_t asic_process_event(/*{{{*/
        _In_ AsicView &current,
        _In_ AsicView &temporary,
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    /*
     * Ssince we have stored our keys in asic view, we can actually keep api
     * type right away so we don't have to serialize and deserialize everything
     * twice. We can take a look into that if update execution time will not be
     * sufficient.
     */

    const std::string &key = kfvKey(kco);
    const std::string &op = kfvOp(kco);

    std::string str_object_type = key.substr(0, key.find(":"));
    std::string str_object_id = key.substr(key.find(":") + 1);

    SWSS_LOG_INFO("key: %s op: %s", key.c_str(), op.c_str());

    sai_common_api_t api = SAI_COMMON_API_MAX;

    if (op == "set")
    {
        /*
         * Most common operation will probably be set so let put it first
         */

        api = SAI_COMMON_API_SET;
    }
    else if (op == "create")
    {
        api = SAI_COMMON_API_CREATE;
    }
    else if (op == "remove")
    {
        api = SAI_COMMON_API_REMOVE;
    }
    else
    {
        SWSS_LOG_ERROR("api %s is not implemented", op.c_str());

        return SAI_STATUS_NOT_SUPPORTED;
    }

    std::stringstream ss;

    sai_object_type_t object_type;
    sai_deserialize_object_type(str_object_type, object_type);

    if (object_type >= SAI_OBJECT_TYPE_MAX)
    {
        SWSS_LOG_ERROR("undefined object type %d", object_type);

        return SAI_STATUS_NOT_SUPPORTED;
    }

    const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

    SaiAttributeList list(object_type, values, false);

    sai_attribute_t *attr_list = list.get_attr_list();
    uint32_t attr_count = list.get_attr_count();

    asic_translate_vid_to_rid_list(current, temporary, object_type, attr_count, attr_list);

    sai_status_t status;

    switch (object_type)
    {
        case SAI_OBJECT_TYPE_FDB:
            status = asic_handle_fdb(current, temporary, str_object_id, api, attr_count, attr_list);
            break;

        case SAI_OBJECT_TYPE_SWITCH:
            status = asic_handle_switch(current, temporary, str_object_id, api, attr_count, attr_list);
            break;

        case SAI_OBJECT_TYPE_NEIGHBOR:
            status = asic_handle_neighbor(current, temporary, str_object_id, api, attr_count, attr_list);
            break;

        case SAI_OBJECT_TYPE_ROUTE:
            status = asic_handle_route(current, temporary, str_object_id, api, attr_count, attr_list);
            break;

        case SAI_OBJECT_TYPE_VLAN:
            status = asic_handle_vlan(current, temporary, str_object_id, api, attr_count, attr_list);
            break;

        case SAI_OBJECT_TYPE_TRAP:
            status = asic_handle_trap(current, temporary, str_object_id, api, attr_count, attr_list);
            break;

        default:
            status = asic_handle_generic(current, temporary, object_type, str_object_id, api, attr_count, attr_list);
            break;
    }

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to execute api: %s, key: %s, status: %s", op.c_str(), key.c_str(), sai_serialize_status(status).c_str());

        for (const auto &v: values)
        {
            SWSS_LOG_ERROR("field: %s, value: %s", fvField(v).c_str(), fvValue(v).c_str());
        }

        // asic here will be in inconsistent state

        exit_and_notify(EXIT_FAILURE);
    }

    return status;
}/*}}}*/

void executeOperationsOnAsic(/*{{{*/
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("operations to execute on ASIC: %zu", currentView.asicGetOperationsCount());

    {
        SWSS_LOG_TIMER("asic apply");

        try
        {
            for (const auto &op: currentView.asicGetOperations())
            {
                /*
                 * It is possible that this method will throw exception in that case we
                 * also should exit syncd since we can be in the middle of executing
                 * operations and if some problems will happen and we continue to stay
                 * alive and next apply view, we will be in inconsistent state which
                 * will lead to unexpected behaviour.
                 */

                sai_status_t status = asic_process_event(currentView, temporaryView, *op);

                if (status != SAI_STATUS_SUCCESS)
                {
                    SWSS_LOG_ERROR("status of last operation was: %s, ASIC will be in inconsistent state, exiting",
                            sai_serialize_status(status).c_str());

                    exit_and_notify(EXIT_FAILURE);
                }
            }
        }
        catch (const std::exception &e)
        {
            SWSS_LOG_ERROR("Error while executing asic operations, ASIC is in inconsistent state: %s", e.what());

            exit_and_notify(EXIT_FAILURE);
        }
    }

    SWSS_LOG_NOTICE("performed all operations on asic succesfully");
}/*}}}*/
