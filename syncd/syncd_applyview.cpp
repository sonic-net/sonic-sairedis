#include "syncd.h"
#include "sairedis.h"
#include "swss/table.h"
#include "swss/logger.h"
#include "swss/dbconnector.h"

#include <algorithm>
#include <list>

/*
 * NOTE: All methods taking current and temporary view could be moved to
 * transition class etc to just use class members instead of passing those
 * parameters to every function.
 */

/**
 * When set to true extra logging will be added for tracking references.  This
 * is useful for debugging, but for production operations this will produce too
 * much noise in logs, and we still can replay scenario using recordings.
 */
bool enableRefernceCountLogs = false;

/**
 * @brief Represents SAI object status during comparison
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
     * Previous VID was matched to temp VID since it's the same. Objects exists
     * in both previous and next view and have the save VID/RID values.
     *
     * However attributes of that object could be different and may be not
     * matched yet. This object still needs processing for attributes.
     *
     * Since only attributes can be updated then this object may not be removed
     * at all, it must be possible to update only attribute values.
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
     * This means object was matched/set/created and proper actions were
     * generated as diff data to be executed on ASIC.
     */
    SAI_OBJECT_STATUS_FINAL,

} sai_object_status_t;

struct AsicOperation
{
    AsicOperation(int id, sai_object_id_t vID, bool remove,
           std::shared_ptr<swss::KeyOpFieldsValuesTuple>& operation):
        opId(id), vid(vID), isRemove(remove), op(operation)
    {
    }

    int opId;

    sai_object_id_t vid;

    bool isRemove;

    std::shared_ptr<swss::KeyOpFieldsValuesTuple> op;
};

/**
 * @brief Class represents single attribute
 *
 * Some attributes are lists, and have allocated memory, this class will help to
 * handle memory management and also will keep metadata for attribute.
 */
class SaiAttr
{
    public:

        /**
         * @brief Constructor
         *
         * @param[in] str_attr_id Attribute is as string
         * @param[in] str_attr_value Attribute value as string
         */
        SaiAttr(
                _In_ const std::string &str_attr_id,
                _In_ const std::string &str_attr_value):
            m_str_attr_id(str_attr_id),
            m_str_attr_value(str_attr_value),
            m_meta(NULL)
        {
            SWSS_LOG_ENTER();

            /*
             * We perform deserialize here to have attribute value when we need
             * it, this can include allocated lists, so on destructor we need
             * to free this memory.
             */

            sai_deserialize_attr_id(str_attr_id, &m_meta);

            m_attr.id = m_meta->attrid;

            sai_deserialize_attr_value(str_attr_value, *m_meta, m_attr, false);
        }

        /**
         * @brief Destructor
         */
        ~SaiAttr()
        {
            SWSS_LOG_ENTER();

            sai_deserialize_free_attribute_value(m_meta->attrvaluetype, m_attr);
        }

        sai_attribute_t* getRWSaiAttr()
        {
            SWSS_LOG_ENTER();

            return &m_attr;
        }

        const sai_attribute_t* getSaiAttr() const
        {
            SWSS_LOG_ENTER();
            return &m_attr;
        }

        /**
         * @brief Gets value.oid of attribute value.
         *
         * If attribute is not OID exception will be thrown.
         *
         * @return Oid field of attribute value.
         */
        sai_object_id_t getOid() const
        {
            SWSS_LOG_ENTER();

            if (m_meta->attrvaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID)
            {
                SWSS_LOG_THROW("attribute %s is not OID attribute", m_meta->attridname);
            }

            return m_attr.value.oid;
        }

        /**
         * @brief Tells whether attribute contains OIDs
         *
         * @return True if attribute contains OIDs, false otherwise
         */
        bool isObjectIdAttr() const
        {
            SWSS_LOG_ENTER();
            return m_meta->isoidattribute;
        }

        const std::string& getStrAttrId() const
        {
            SWSS_LOG_ENTER();
            return m_str_attr_id;
        }

        const std::string& getStrAttrValue() const
        {
            SWSS_LOG_ENTER();
            return m_str_attr_value;
        }

        const sai_attr_metadata_t* getAttrMetadata() const
        {
            SWSS_LOG_ENTER();
            return m_meta;
        }

        void UpdateValue()
        {
            SWSS_LOG_ENTER();

            m_str_attr_value = sai_serialize_attr_value(*m_meta, m_attr);
        }

        /**
         * @brief Get OID list from attribute
         *
         * Based on serialization type attribute may be oid attribute, oid list
         * attribute or non oid attribute. This method will extract all those oids from
         * this attribute and return as vector.  This is handy when we need processing
         * oids per attribute.
         *
         * @return Object list used in attribute
         */
        std::vector<sai_object_id_t> getOidListFromAttribute() const
        {
            SWSS_LOG_ENTER();

            const sai_attribute_t &attr = m_attr;

            uint32_t count = 0;

            const sai_object_id_t *objectIdList = NULL;

            /*
             * For ACL fields and actions we need to use enable flag as
             * indicator, since when attribute is disabled then parameter can
             * be garbage.
             */

            switch (m_meta->attrvaluetype)
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

                    if (m_meta->isoidattribute)
                    {
                        SWSS_LOG_THROW("attribute %s is oid attrubute but not handled", m_meta->attridname);
                    }

                    /*
                     * Attribute not contain any object ids.
                     */

                    break;
            }

            std::vector<sai_object_id_t> result(objectIdList, objectIdList + count);

            return result;
        }

    private:

        /*
         * Copy constructor and assignment operator are marked as private to
         * prevent copy of this object, since attribute can contain pointers to
         * list which can lead to double free when object copy.
         *
         * This can be solved by making proper implementations of those
         * methods, currently this is not required.
         */

        SaiAttr(const SaiAttr&);
        SaiAttr& operator=(const SaiAttr&);

        const std::string m_str_attr_id;
        std::string m_str_attr_value;

        const sai_attr_metadata_t* m_meta;
        sai_attribute_t m_attr;
};

/**
 * @brief Class represents SAI object
 */
class SaiObj
{
    public:

        /**
         * @brief Constructor
         */
        SaiObj():
            createdObject(false),
            m_object_status(SAI_OBJECT_STATUS_NOT_PROCESSED)
        {
            SWSS_LOG_ENTER();

            /* empty intentionally */
        }

        // TODO move to methods or constructor ?
        std::string str_object_type;
        std::string str_object_id;

        sai_object_meta_key_t meta_key;

        const sai_object_type_info_t *info;

        /**
         * @brief This will indicate whether object was created and it will
         * indicate that currently there is no RID for it.
         */
        bool createdObject;

        bool isOidObject() const
        {
            SWSS_LOG_ENTER();
            // XXX return info->isobjectid;
            return !info->isnonobjectid;
        }

        const std::unordered_map<sai_attr_id_t, std::shared_ptr<SaiAttr>>& getAllAttributes() const
        {
            SWSS_LOG_ENTER();

            return m_attrs;
        }

        std::shared_ptr<const SaiAttr> getSaiAttr(
                _In_ sai_attr_id_t id) const
        {
            SWSS_LOG_ENTER();

            auto it = m_attrs.find(id);

            if (it == m_attrs.end())
            {
                for (const auto &ita: m_attrs)
                {
                    const auto &a = ita.second;

                    SWSS_LOG_ERROR("%s: %s", a->getStrAttrId().c_str(), a->getStrAttrValue().c_str());
                }

                SWSS_LOG_THROW("object %s has no attribute %d", str_object_id.c_str(), id);
            }

            return it->second;
        }

        std::shared_ptr<const SaiAttr> tryGetSaiAttr(
                _In_ sai_attr_id_t id) const
        {
            SWSS_LOG_ENTER();

            auto it = m_attrs.find(id);

            if (it == m_attrs.end())
                return nullptr;

            return it->second;
        }

        /**
         * @brief Sets object status
         *
         * @param[in] object_status New object status
         */
        void setObjectStatus(
                _In_ sai_object_status_t object_status)
        {
            SWSS_LOG_ENTER();

            m_object_status = object_status;
        }

        /**
         * @brief Gets current object status
         *
         * @return Current object status
         */
        sai_object_status_t getObjectStatus() const
        {
            SWSS_LOG_ENTER();

            return m_object_status;
        }

        /**
         * @brief Gets object type
         *
         * @return Object type
         */
        sai_object_type_t getObjectType() const
        {
            SWSS_LOG_ENTER();

            return meta_key.objecttype;
        }

        // TODO should be private, and we should have some friends from AsicView class
        void setAttr(
                _In_ const std::shared_ptr<SaiAttr> &attr)
        {
            SWSS_LOG_ENTER();

            m_attrs[attr->getSaiAttr()->id] = attr;
        }

        bool hasAttr(
                _In_ sai_attr_id_t id) const
        {
            SWSS_LOG_ENTER();

            return m_attrs.find(id) != m_attrs.end();
        }

        sai_object_id_t getVid() const
        {
            SWSS_LOG_ENTER();

            if (isOidObject())
            {
                return meta_key.objectkey.key.object_id;
            }

            SWSS_LOG_THROW("object %s it not object id type", str_object_id.c_str());
        }

        /*
         * NOTE: We need dependency tree if we want to remove objects which
         * have reference count not zero. Currently we just iterate on removed
         * objects as many times as their reference will get down to zero.
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

/**
 * @brief Class represents ASIC view
 */
class AsicView
{
    public:

        /**
         * @brief Constructor
         */
        AsicView():
            m_asicOperationId(0)
        {
            SWSS_LOG_ENTER();

            /* empty intentionally */
        }

        /**
         * @brief Populates ASIC view from REDIS table dump
         *
         * @param[in] dump Redis table dump
         *
         * NOTE: Could be static method that returns AsicView object.
         */
        void fromDump(
                _In_ const swss::TableDump &dump)
        {
            SWSS_LOG_ENTER();

            /*
             * Input should be also existing objects, so they could be created
             * here right away but we would need VIDs as well.
             */

            for (const auto &key: dump)
            {
                auto start = key.first.find_first_of(":");

                if (start == std::string::npos)
                {
                    SWSS_LOG_THROW("failed to find colon in %s", key.first.c_str());
                }

                std::shared_ptr<SaiObj> o = std::make_shared<SaiObj>();

                // TODO we could use sai deserialize object meta key

                o->str_object_type  = key.first.substr(0, start);
                o->str_object_id    = key.first.substr(start + 1);

                sai_deserialize_object_type(o->str_object_type, o->meta_key.objecttype);

                o->info = sai_metadata_get_object_type_info(o->meta_key.objecttype);

                /*
                 * Since neighbor/route/fdb structs objects contains OIDs, we
                 * need to increase vid reference. With new metadata for SAI
                 * 1.0 this can be done in generic way for all non object ids.
                 */

                switch (o->meta_key.objecttype)
                {
                    case SAI_OBJECT_TYPE_FDB_ENTRY:
                        sai_deserialize_fdb_entry(o->str_object_id, o->meta_key.objectkey.key.fdb_entry);
                        soFdbs[o->str_object_id] = o;
                        break;

                    case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
                        sai_deserialize_neighbor_entry(o->str_object_id, o->meta_key.objectkey.key.neighbor_entry);
                        soNeighbors[o->str_object_id] = o;
                        break;

                    case SAI_OBJECT_TYPE_ROUTE_ENTRY:
                        sai_deserialize_route_entry(o->str_object_id, o->meta_key.objectkey.key.route_entry);
                        soRoutes[o->str_object_id] = o;

                        break;

                    default:

                        if (o->info->isnonobjectid)
                        {
                            SWSS_LOG_THROW("object %s is non object id, not handled, FIXME", key.first.c_str());
                        }

                        sai_deserialize_object_id(o->str_object_id, o->meta_key.objectkey.key.object_id);

                        soOids[o->str_object_id] = o;
                        oOids[o->meta_key.objectkey.key.object_id] = o;

                        break;
                }

                soAll[o->str_object_id] = o;
                sotAll[o->meta_key.objecttype][o->str_object_id] = o;

                if (o->info->isnonobjectid)
                {
                    updateNonObjectIdVidReferenceCountByValue(o, 1);
                }
                else
                {
                    /*
                     * Here is only object VID declaration, since we don't
                     * know what objects were processed previously but on
                     * some of previous object attributes this VID could be
                     * used, so value can be already greater than zero, but
                     * here we need to just mark that vid exists in
                     * vidReference.
                     */

                    m_vidReference[o->meta_key.objectkey.key.object_id] += 0;
                }

                populateAttributes(o, key.second);
            }
        }

    private:

        /**
         * @brief Release existing VID links (references) based on given attribute.
         *
         * @param[in] attr Attribute which will be used to obtain oids.
         */
        void releaseExisgingLinks(
                _In_ const std::shared_ptr<const SaiAttr> &attr)
        {
            SWSS_LOG_ENTER();

            /*
             * For each VID on that attribute (it can be single oid or oid list,
             * release link on current view.
             *
             * Second operation after could increase links of setting new attribute, or
             * nothing if object was removed.
             *
             * Also if we want to keep track of object reverse dependency
             * this action can be more complicated.
             */

            for (auto const &vid: attr->getOidListFromAttribute())
            {
                releaseVidReference(vid);
            }
        }

        /**
         * @brief Release existing VID links (references) based on given object.
         *
         * All OID attributes will be scanned and released.
         *
         * @param[in] obj Object which will be used to obtain attributes and oids
         */
        void releaseExisgingLinks(
                _In_ const std::shared_ptr<const SaiObj> &obj)
        {
            SWSS_LOG_ENTER();

            for (const auto &ita: obj->getAllAttributes())
            {
                releaseExisgingLinks(ita.second);
            }
        }

        /**
         * @brief Release VID reference.
         *
         * If SET operation was performed on attribute, and attribute was OID
         * attribute, then we need to release previous reference to that VID,
         * and bind new reference to next OID if present.
         *
         * @param[in] vid Virtual ID to be released.
         */
        void releaseVidReference(
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
                SWSS_LOG_THROW("vid %s doesn't exist in reference map",
                        sai_serialize_object_id(vid).c_str());
            }

            int referenceCount = --(it->second);

            if (referenceCount < 0)
            {
                SWSS_LOG_THROW("vid %s decreased reference count too many times: %d, BUG",
                        sai_serialize_object_id(vid).c_str(),
                        referenceCount);
            }

            SWSS_LOG_INFO("decreased vid %s refrence to %d",
                    sai_serialize_object_id(vid).c_str(),
                    referenceCount);

            if (referenceCount == 0)
            {
                m_vidToAsicOperationId[vid] = m_asicOperationId;
            }
        }

        /**
         * @brief Bind new links (references) based on attribute
         *
         * @param[in] attr Attribute to obtain oids to bind references
         */
        void bindNewLinks(
                _In_ const std::shared_ptr<const SaiAttr> &attr)
        {
            SWSS_LOG_ENTER();

            /*
             * For each VID on that attribute (it can be single oid or oid list,
             * bind new link on current view.
             *
             * Notice that this list can contain VIDs from temporary view or default
             * view, so they may not exists in current view. But that should also be
             * not the case since we either created new object on current view or
             * either we matched current object to temporary object so RID can be the
             * same.
             *
             * Also if we want to keep track of object reverse dependency
             * this action can be more complicated.
             */

            for (auto const &vid: attr->getOidListFromAttribute())
            {
                bindNewVidReference(vid);
            }
        }

        /**
         * @brief Bind existing VID links (references) based on given object.
         *
         * All OID attributes will be scanned and bound.
         *
         * @param[in] obj Object which will be used to obtain attributes and oids
         */
        void bindNewLinks(
                _In_ const std::shared_ptr<const SaiObj> &obj)
        {
            SWSS_LOG_ENTER();

            for (const auto &ita: obj->getAllAttributes())
            {
                bindNewLinks(ita.second);
            }
        }

        /**
         * @brief Bind new VID reference
         *
         * If attribute is OID attribute then we need to increase reference
         * count on that VID to mark it that it is in use.
         *
         * @param[in] vid Virtual ID reference to be bind
         */
        void bindNewVidReference(
                _In_ sai_object_id_t vid)
        {
            SWSS_LOG_ENTER();

            if (vid == SAI_NULL_OBJECT_ID)
            {
                return;
            }

            /*
             * If we are doing bind on new VID reference, that VID needs to
             * exist int current view, either object was matched or new object
             * was created.
             *
             * TODO: Not sure if this will have impact on some other object
             * processing since if object was matched or created then this VID
             * can be found in other attributes comparing them, and it will get
             * NULL instead RID.
             */

            auto it = m_vidReference.find(vid);

            if (it == m_vidReference.end())
            {
                SWSS_LOG_THROW("vid %s doesn't exist in reference map",
                        sai_serialize_object_id(vid).c_str());
            }

            int referenceCount = ++(it->second);

            SWSS_LOG_INFO("increased vid %s refrence to %d",
                    sai_serialize_object_id(vid).c_str(),
                    referenceCount);
        }

    public:

        /**
         * @brief Gets VID reference count.
         *
         * @param vid Virtual ID to obtain reference count.
         *
         * @return Reference count or -1 if VID was not found.
         */
        int getVidReferenceCount(
                _In_ sai_object_id_t vid) const
        {
            SWSS_LOG_ENTER();

            auto it = m_vidReference.find(vid);

            if (it != m_vidReference.end())
            {
                return it->second;
            }

            return -1;
        }

        /**
         * @brief Insert new VID reference.
         *
         * Inserts new reference to be tracked. This also make sure that
         * reference doesn't exist yet, as a sanity check if same reference
         * would be inserted twice.
         *
         * @param[in] vid Virtual ID reference to be inserted.
         */
        void insertNewVidReference(
                _In_ sai_object_id_t vid)
        {
            SWSS_LOG_ENTER();

            auto it = m_vidReference.find(vid);

            if (it != m_vidReference.end())
            {
                SWSS_LOG_THROW("vid %s already exist in reference map, BUG",
                        sai_serialize_object_id(vid).c_str());
            }

            m_vidReference[vid] = 0;

            SWSS_LOG_INFO("inserted vid %s as reference",
                    sai_serialize_object_id(vid).c_str());
        }

        // TODO convert to something like nonObjectIdMap

        StrObjectIdToSaiObjectHash soFdbs;
        StrObjectIdToSaiObjectHash soNeighbors;
        StrObjectIdToSaiObjectHash soRoutes;
        StrObjectIdToSaiObjectHash soOids;
        StrObjectIdToSaiObjectHash soAll;

    private:

        std::map<sai_object_type_t, StrObjectIdToSaiObjectHash> sotAll;

    public:

        ObjectIdToSaiObjectHash oOids;

        /*
         * On temp view this needs to be used for actual NEW rids created and
         * then reused with rid mapping to create new rid/vid map.
         */

        ObjectIdMap ridToVid;
        ObjectIdMap vidToRid;
        ObjectIdMap removedVidToRid;

        std::map<sai_object_type_t, std::unordered_map<std::string,std::string>> nonObjectIdMap;

        sai_object_id_t defaultTrapGroupRid;

        /**
         * @brief Gets objects by object type.
         *
         * @param object_type Object type to be used as filter.
         *
         * @return List of objects with requested object type.
         * Order on list is random.
         */
        std::vector<std::shared_ptr<SaiObj>> getObjectsByObjectType(
                _In_ sai_object_type_t object_type) const
        {
            SWSS_LOG_ENTER();

            std::vector<std::shared_ptr<SaiObj>> list;

            /*
             * We need to use find, since object type may not exist.
             */

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
        }

        /**
         * @brief Gets not processed objects by object type.
         *
         * Call to this method can be expensive, since every time we iterate
         * entire list. This list can contain even 10k elements if view will be
         * very large.
         *
         * @param object_type Object type to be used as filter.
         *
         * @return List of objects with requested object type and marked
         * as not processed. Order on list is random.
         */
        std::vector<std::shared_ptr<SaiObj>> getNotProcessedObjectsByObjectType(
                _In_ sai_object_type_t object_type) const
        {
            SWSS_LOG_ENTER();

            std::vector<std::shared_ptr<SaiObj>> list;

            /*
             * We need to use find, since object type may not exist.
             */

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
        }

        /**
         * @brief Gets all not processed objects
         *
         * @return List of all not processed objects. Order on list is random.
         */
        std::vector<std::shared_ptr<SaiObj>> getAllNotProcessedObjects() const
        {
            SWSS_LOG_ENTER();

            std::vector<std::shared_ptr<SaiObj>> list;

            for (const auto &p: soAll)
            {
                if (p.second->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
                {
                    list.push_back(p.second);
                }
            }

            return list;
        }

        /**
         * @brief Create dummy existing object
         *
         * Function creates dummy object, which is used to indicate that
         * this OID object exist in current view. This is used for existing
         * objects, like CPU port, default trap group.
         *
         * @param[in] rid Real ID
         * @param[in] vid Virtual ID
         */
        void createDummyExistingObject(
                _In_ sai_object_id_t rid,
                _In_ sai_object_id_t vid)
        {
            SWSS_LOG_ENTER();

            sai_object_type_t object_type = getObjectTypeFromVid(vid);

            if (object_type == SAI_OBJECT_TYPE_NULL)
            {
                SWSS_LOG_THROW("got null object type from VID %s",
                        sai_serialize_object_id(vid).c_str());
            }

            std::shared_ptr<SaiObj> o = std::make_shared<SaiObj>();

            o->str_object_type = sai_serialize_object_type(object_type);
            o->str_object_id   = sai_serialize_object_id(vid);

            o->meta_key.objecttype = object_type;
            o->meta_key.objectkey.key.object_id = vid;

            o->info = sai_metadata_get_object_type_info(object_type);

            soOids[o->str_object_id] = o;
            oOids[vid] = o;

            m_vidReference[vid] += 0;

            soAll[o->str_object_id] = o;
            sotAll[o->meta_key.objecttype][o->str_object_id] = o;

            ridToVid[rid] = vid;
            vidToRid[vid] = rid;
        }

        /**
         * @brief Destructor
         */
        ~AsicView()
        {
            /* empty intentionally */
        }

        /**
         * @brief Generate ASIC set operation on current existing object.
         *
         * NOTE: In long run, this is serialize, and then we call deserialize
         * to execute them on actual ASIC, maybe this is not necessary
         * and could be optimized later.
         *
         * TODO: Set on object id should do release of links (currently done
         * outside) and modify dependency tree.
         *
         * @param currentObj Current object.
         * @param attr Attribute to be set on current object.
         */
        void asicSetAttribute(
                _In_ const std::shared_ptr<SaiObj> &currentObj,
                _In_ const std::shared_ptr<SaiAttr> &attr)
        {
            SWSS_LOG_ENTER();

            SWSS_LOG_INFO("%s: %s -> %s:%s", currentObj->str_object_type.c_str(), currentObj->str_object_id.c_str(),
                    attr->getStrAttrId().c_str(), attr->getStrAttrValue().c_str());

            m_asicOperationId++;

            /*
             * Release previous references if attribute is object id and bind
             * new reference in that place.
             */

            auto meta = attr->getAttrMetadata();

            if (attr->isObjectIdAttr())
            {
                if (currentObj->hasAttr(meta->attrid))
                {
                    /*
                     * Since previous attribute exists, lets release previous links if
                     * they are not NULL.
                     */

                    releaseExisgingLinks(currentObj->getSaiAttr(meta->attrid));
                }

                currentObj->setAttr(attr);

                bindNewLinks(currentObj->getSaiAttr(meta->attrid));
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
                 * Making copy here is not necessary since default attribute will be
                 * created dynamically anyway, and temporary attributes will not change
                 * also.
                 */

                currentObj->setAttr(attr);
            }

            std::vector<swss::FieldValueTuple> entry = SaiAttributeList::serialize_attr_list(
                    currentObj->getObjectType(),
                    1,
                    attr->getSaiAttr(),
                    false);

            std::string key = currentObj->str_object_type + ":" + currentObj->str_object_id;

            std::shared_ptr<swss::KeyOpFieldsValuesTuple> kco =
                std::make_shared<swss::KeyOpFieldsValuesTuple>(key, "set", entry);

            sai_object_id_t vid = (currentObj->isOidObject()) ? currentObj->getVid() : SAI_NULL_OBJECT_ID;

            m_asicOperations.push_back(AsicOperation(m_asicOperationId, vid, false, kco));

            dumpRef("set");
        }

        /**
         * @brief Generate ASIC create operation for current object.
         *
         * NOTE: In long run, this is serialize, and then we call
         * deserialize to execute them on actual asic, maybe this is not
         * necessary and could be optimized later.
         *
         * TODO: Create on object id attributes should bind references to
         * used VIDs of of links (currently done outside) and modify
         * dependency tree.
         *
         * @param currentObject Current object to be created.
         */
        void asicCreateObject(
                _In_ const std::shared_ptr<SaiObj> &currentObj)
        {
            SWSS_LOG_ENTER();

            SWSS_LOG_INFO("%s: %s", currentObj->str_object_type.c_str(), currentObj->str_object_id.c_str());

            m_asicOperationId++;

            if (currentObj->isOidObject())
            {
                soOids[currentObj->str_object_id] = currentObj;
                oOids[currentObj->meta_key.objectkey.key.object_id] = currentObj;

                soAll[currentObj->str_object_id] = currentObj;
                sotAll[currentObj->meta_key.objecttype][currentObj->str_object_id] = currentObj;

                /*
                 * Since we are creating object, we just need to mark that
                 * reference is there.  But at this point object is not used
                 * anywhere.
                 */

                m_vidReference[currentObj->meta_key.objectkey.key.object_id] += 0;
            }
            else
            {
                /*
                 * Since neighbor/route/fdb structs objects contains OIDs, we
                 * need to increase vid reference. With new metadata for SAI
                 * 1.0 this can be done in generic way for all non object ids.
                 */

                // TODO why we are doing deserialize here? meta key should be populated already

                switch (currentObj->getObjectType())
                {
                    case SAI_OBJECT_TYPE_FDB_ENTRY:
                        //sai_deserialize_fdb_entry(currentObj->str_object_id, currentObj->meta_key.objectkey.key.fdb_entry);
                        soFdbs[currentObj->str_object_id] = currentObj;
                        break;

                    case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
                        //sai_deserialize_neighbor_entry(currentObj->str_object_id, currentObj->meta_key.objectkey.key.neighbor_entry);
                        soNeighbors[currentObj->str_object_id] = currentObj;
                        break;

                    case SAI_OBJECT_TYPE_ROUTE_ENTRY:
                        //sai_deserialize_route_entry(currentObj->str_object_id, currentObj->meta_key.objectkey.key.route_entry);
                        soRoutes[currentObj->str_object_id] = currentObj;
                        break;

                    default:

                        SWSS_LOG_THROW("unsupported object type: %s",
                                sai_serialize_object_type(currentObj->getObjectType()).c_str());
                }

                soAll[currentObj->str_object_id] = currentObj;
                sotAll[currentObj->meta_key.objecttype][currentObj->str_object_id] = currentObj;

                updateNonObjectIdVidReferenceCountByValue(currentObj, 1);
            }

            bindNewLinks(currentObj); // handle attribute references

            /*
             * Generate asic commands.
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
                /*
                 * Make sure that we put object into db even if there are no
                 * attributes set.
                 */

                swss::FieldValueTuple null("NULL", "NULL");

                entry.push_back(null);
            }

            std::string key = currentObj->str_object_type + ":" + currentObj->str_object_id;

            std::shared_ptr<swss::KeyOpFieldsValuesTuple> kco =
                std::make_shared<swss::KeyOpFieldsValuesTuple>(key, "create", entry);

            sai_object_id_t vid = (currentObj->isOidObject()) ? currentObj->getVid() : SAI_NULL_OBJECT_ID;

            m_asicOperations.push_back(AsicOperation(m_asicOperationId, vid, false, kco));

            dumpRef("create");
        }

        /**
         * @brief Generate ASIC remove operation for current existing object.
         *
         * @param currentObj Current existing object to be removed.
         */
        void asicRemoveObject(
                _In_ const std::shared_ptr<SaiObj> &currentObj)
        {
            SWSS_LOG_ENTER();

            SWSS_LOG_INFO("%s: %s", currentObj->str_object_type.c_str(), currentObj->str_object_id.c_str());

            m_asicOperationId++;

            if (currentObj->isOidObject())
            {
                /*
                 * Reference count is already check externally, but we can move
                 * that check here also as sanity check.
                 */

                soOids.erase(currentObj->str_object_id);
                oOids.erase(currentObj->meta_key.objectkey.key.object_id);

                soAll.erase(currentObj->str_object_id);
                sotAll.at(currentObj->meta_key.objecttype).erase(currentObj->str_object_id);

                m_vidReference[currentObj->meta_key.objectkey.key.object_id] -= 1;

                /*
                 * Clear object also from rid/vid maps.
                 */

                sai_object_id_t vid = currentObj->getVid();
                sai_object_id_t rid = vidToRid.at(vid);

                /*
                 * This will have impact on translate_vid_to_rid, we need to put
                 * this in other view.
                 */

                ridToVid.erase(rid);
                vidToRid.erase(vid);

                /*
                 * We could remove this VID also from m_vidReference, but it's not
                 * required.
                 */

                removedVidToRid[vid] = rid;
            }
            else
            {
                /*
                 * Since neighbor/route/fdb structs objects contains OIDs, we
                 * need to decrease VID reference. With new metadata for SAI
                 * 1.0 this can be done in generic way for all non object ids.
                 */

                switch (currentObj->getObjectType())
                {
                    case SAI_OBJECT_TYPE_FDB_ENTRY:
                        soFdbs.erase(currentObj->str_object_id);
                        break;

                    case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
                        soNeighbors.erase(currentObj->str_object_id);
                        break;

                    case SAI_OBJECT_TYPE_ROUTE_ENTRY:
                        soRoutes.erase(currentObj->str_object_id);
                        break;

                    default:

                        SWSS_LOG_THROW("unsupported object type: %s",
                                sai_serialize_object_type(currentObj->getObjectType()).c_str());
                }

                soAll.erase(currentObj->str_object_id);
                sotAll.at(currentObj->meta_key.objecttype).erase(currentObj->str_object_id);

                updateNonObjectIdVidReferenceCountByValue(currentObj, -1);
            }

            releaseExisgingLinks(currentObj); // handle attribute references

            /*
             * Generate asic commands.
             */

            std::vector<swss::FieldValueTuple> entry;

            std::string key = currentObj->str_object_type + ":" + currentObj->str_object_id;

            std::shared_ptr<swss::KeyOpFieldsValuesTuple> kco =
                std::make_shared<swss::KeyOpFieldsValuesTuple>(key, "remove", entry);

            sai_object_id_t vid = (currentObj->isOidObject()) ? currentObj->getVid() : SAI_NULL_OBJECT_ID;

            if (currentObj->isOidObject())
            {
                m_asicOperations.push_back(AsicOperation(m_asicOperationId, vid, true, kco));
            }
            else
            {
                /*
                 * When doing remove of non object id, let's put it on front,
                 * since when removing next hop group from group last member,
                 * and group is in use by some route, then remove fails since
                 * group cant be empty.
                 *
                 * Of course this doesn't guarantee that remove all routes will
                 * be at the beginning, also it may happen that default route
                 * will be removed first which maybe not allowed.
                 */

                m_asicRemoveOperationsNonObjectId.push_back(AsicOperation(m_asicOperationId, vid, true, kco));
            }

            dumpRef("remove");
        }

        std::vector<AsicOperation> asicGetWithOptimizedRemoveOperations() const
        {
            SWSS_LOG_ENTER();

            SWSS_LOG_TIMER("optimizing asic remove operations");

            std::vector<AsicOperation> v;

            /*
             * First push remove operations on non object id at the beginning of list.
             */

            for (const auto &n: m_asicRemoveOperationsNonObjectId)
            {
                v.push_back(n);
            }

            size_t index = v.size();

            size_t moved = 0;

            for (auto opit = m_asicOperations.begin(); opit != m_asicOperations.end(); ++opit)
            {
                const auto &op = *opit;

                if (!op.isRemove)
                {
                    /*
                     * This is create or set operation, put it at the list end.
                     */

                    v.push_back(op);

                    continue;
                }

                /*
                 * NOTE: When operation is SET on OID object it can release
                 * references on that OID, and if that OID is later to be
                 * removed a wrong order of operations will happen:
                 *
                 * not optimized scenario:
                 * - create object A
                 * - set object B attr (releases reference on C)
                 * - remove object D (release reference on C)
                 * - remove object C
                 *
                 * this logic could result with wrong order:
                 * - remove D
                 * - remove C (will break reference count on C, since used in B)
                 *   since D is considered last OP releasing C reference
                 * - create A
                 * - set object B with A
                 *
                 * correct optimized logic:
                 * - remove D
                 * - create A
                 * - set object B with A
                 * - remove C
                 *
                 * This is fixed by checking if last operation to release reference was REMOVE
                 */

                if (op.vid == SAI_NULL_OBJECT_ID)
                {
                    SWSS_LOG_THROW("non object id remove not exected here");
                }

                auto mit = m_vidToAsicOperationId.find(op.vid);

                if (mit == m_vidToAsicOperationId.end())
                {
                    /*
                     * This vid is not present in map, so we can move remove
                     * operation all the way to the top since there is no
                     * operation that decreased this vid reference.
                     *
                     * This can be NHG member, vlan member etc.
                     */

                    v.insert(v.begin() + index, op);

                    SWSS_LOG_INFO("move 0x%lx all way up (not in map): %s to index: %zu", op.vid,
                            sai_serialize_object_type(redis_sai_object_type_query(op.vid)).c_str(),index);

                    index++;

                    moved++;

                    continue;
                }

                /*
                 * This last operation id that decreased VID reference to zero
                 * can be before or after current iterator, so it may be not
                 * found on list from current iterator to list end.  This will
                 * mean that we can insert this remove at current iterator
                 * position.
                 *
                 * If operation is found then we need to insert this op after
                 * current iterator and forward iterator.
                 *
                 * But there is a catch here, if that last operation was REMOVE,
                 * then it was forwarded all the way to the UP, but in the
                 * middle we could have some "SET" operations that would
                 * release this reference also we in this case, we can't just
                 * move this remove just after previous remove.
                 */

                int lastOpIdDecRef = mit->second;

                auto itr = find_if(v.begin(), v.end(), [lastOpIdDecRef] (const AsicOperation& ao) { return ao.opId == lastOpIdDecRef; } );

                if (itr == v.end())
                {
                    SWSS_LOG_THROW("something wrong, vid %s in map, but not found on list!",
                            sai_serialize_object_id(op.vid).c_str());
                }

                if (itr->isRemove)
                {
                    SWSS_LOG_INFO("previous operation to release reference %s was also REMOVE, we push current REMOVE op at the list end",
                            sai_serialize_object_id(mit->first).c_str());

                    /*
                     * If previous operation was REMOVE (it could be pushed to
                     * the top) so push current operation to the list here,
                     * instead of possible break reference count on SET
                     * operations.
                     */

                    v.push_back(op);

                    continue;
                }

                /*
                 * We add +1 since we need to insert current object AFTER the one that we found
                 */

                size_t lastOpIdDecRefIndex = itr - v.begin() + 1;

                if (lastOpIdDecRefIndex > index)
                {
                    SWSS_LOG_INFO("index update from %zu to %zu", index, lastOpIdDecRefIndex);

                    index = lastOpIdDecRefIndex;
                }

                v.insert(v.begin() + index, op);

                SWSS_LOG_INFO("move 0x%lx in the middle up: %s (last: %zu curr: %zu)", op.vid,
                            sai_serialize_object_type(redis_sai_object_type_query(op.vid)).c_str(), lastOpIdDecRefIndex, index);

                index++;

                moved++;
            }

            SWSS_LOG_NOTICE("moved %zu REMOVE operations upper in stack from total %zu operations", moved, v.size());

            return v;
        }

        std::vector<AsicOperation> asicGetOperations() const
        {
            SWSS_LOG_ENTER();

            // XXX it will copy entire vector :/, expensive, but
            // in most cases we will have very small number operations to execute

            /*
             * We need to put remove operations of non object id first because
             * of removing last next hop group member if group is in use.
             */

            auto sum = m_asicRemoveOperationsNonObjectId;

            sum.insert(sum.end(), m_asicOperations.begin(), m_asicOperations.end());

            return sum;
        }

        size_t asicGetOperationsCount() const
        {
            SWSS_LOG_ENTER();

            return m_asicOperations.size() + m_asicRemoveOperationsNonObjectId.size();
        }

        bool hasRid(
                _In_ sai_object_id_t rid) const
        {
            SWSS_LOG_ENTER();

            return ridToVid.find(rid) != ridToVid.end();
        }

        bool hasVid(
                _In_ sai_object_id_t vid) const
        {
            SWSS_LOG_ENTER();

            return vidToRid.find(vid) != vidToRid.end();
        }

        void dumpRef(const std::string & asicName)
        {
            SWSS_LOG_ENTER();

            if (enableRefernceCountLogs == false)
                return;

            SWSS_LOG_NOTICE("dump references in ASIC VIEW: %s", asicName.c_str());

            for (auto kvp: m_vidReference)
            {
                sai_object_id_t oid = kvp.first;

                sai_object_type_t ot = redis_sai_object_type_query(oid);

                switch (ot)
                {
                    case SAI_OBJECT_TYPE_LAG:
                    case SAI_OBJECT_TYPE_NEXT_HOP:
                    case SAI_OBJECT_TYPE_NEXT_HOP_GROUP:
                    case SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER:
                    case SAI_OBJECT_TYPE_ROUTE_ENTRY:
                    case SAI_OBJECT_TYPE_ROUTER_INTERFACE:
                        break;

                        // skip object we have no interest into
                    default:
                        continue;
                }

                SWSS_LOG_NOTICE("ref %s: %s: %d",
                        sai_serialize_object_type(ot).c_str(),
                        sai_serialize_object_id(oid).c_str(),
                        kvp.second);
            }
        }

        void dumpVidToAsicOperatioId()
        {
            SWSS_LOG_ENTER();

            for (auto a:  m_vidToAsicOperationId)
            {
                SWSS_LOG_WARN("%d: %s:%s", a.second,
                        sai_serialize_object_type(redis_sai_object_type_query(a.first)).c_str(),
                        sai_serialize_object_id(a.first).c_str());
            }
        }

    private:

        /**
         * @brief Virtual ID reference map.
         *
         * VID is key, reference count is value.
         */
        std::map<sai_object_id_t, int> m_vidReference;

        /**
         * @brief Asic operation ID.
         *
         * Since asic resources are limited like number of routes or buffer
         * pools, so if we don't match object, at first we created new object
         * and then remove previous one. This scenario may not be possible in
         * case of limited resources. Advantage here is that this approach is
         * making sure that asic data plane disruption will be minimal. But we
         * need to switch to remove object first and then create new one. This
         * will make sure that we will be able to remove first and then create,
         * but in this case we can have some asic data plane disruption.
         *
         * This asic operation id will be used to figure out what operation
         * reduced object reference to zero, so we could move remove operation
         * right after this operation instead of the executing all remove
         * actions after all set/create.
         *
         */
        int m_asicOperationId;

        /**
         * @brief VID to asic operation id map.
         *
         * Map where key is VID that points to last asic operation id that
         * decreased reference on that VID to zero. This mean that if object
         * with that VID will be removed, we can move remove operation right
         * after asic operation id pointed by this VID.
         */
        std::map<sai_object_id_t, int> m_vidToAsicOperationId;

        void populateAttributes(
                _In_ std::shared_ptr<SaiObj> &obj,
                _In_ const swss::TableMap &map)
        {
            SWSS_LOG_ENTER();

            for (const auto &field: map)
            {
                std::shared_ptr<SaiAttr> attr = std::make_shared<SaiAttr>(field.first, field.second);

                obj->setAttr(attr);

                /*
                 * Since attributes can contain OIDs we need to update
                 * reference count on them.
                 */

                for (auto const &vid: attr->getOidListFromAttribute())
                {
                    if (vid != SAI_NULL_OBJECT_ID)
                    {
                        m_vidReference[vid] += 1;
                    }
                }
            }
        }

        /**
         * @brief Update non object id VID reference count by specified value.
         *
         * Method will iterate via all OID struct members in non object id and
         * update reference count by specified value.
         *
         * @param currentObj Current object to be processed.
         * @param value Value by which reference will be updated. Can be negative.
         */
        void updateNonObjectIdVidReferenceCountByValue(
                _In_ const std::shared_ptr<SaiObj> &currentObj,
                _In_ int value)
        {
            SWSS_LOG_ENTER();

            for (size_t j = 0; j < currentObj->info->structmemberscount; ++j)
            {
                const sai_struct_member_info_t *m = currentObj->info->structmembers[j];

                if (m->membervaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_ID)
                {
                    sai_object_id_t vid = m->getoid(&currentObj->meta_key);

                    m_vidReference[vid] += value;

                    if (enableRefernceCountLogs)
                    {
                        SWSS_LOG_WARN("updated vid %s refrence to %d",
                                sai_serialize_object_id(vid).c_str(),
                                m_vidReference[vid]);
                    }

                    if (m_vidReference[vid] == 0)
                    {
                        m_vidToAsicOperationId[vid] = m_asicOperationId;
                    }
                }
            }
        }

        /**
         * @brief ASIC operation list.
         *
         * List contains ASIC operation generated in the same way as SAIREDIS.
         *
         * KeyOpFieldsValuesTuple is shared to prevent expensive copy when
         * adding to vector.
         */
        std::vector<AsicOperation> m_asicOperations;
        std::vector<AsicOperation> m_asicRemoveOperationsNonObjectId;

        /*
         * Copy constructor and assignment operator are marked as private to
         * avoid copy of data of entire view. This is not needed for now.
         */

        AsicView(const SaiAttr&);
        AsicView& operator=(const SaiAttr&);
};

void redisGetAsicView(
        _In_ const std::string &tableName,
        _In_ AsicView &view)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("get asic view from %s", tableName.c_str());

    swss::DBConnector db(ASIC_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);

    swss::Table table(&db, tableName);

    swss::TableDump dump;

    table.dump(dump);

    view.fromDump(dump);

    SWSS_LOG_NOTICE("objects count for %s: %zu", tableName.c_str(), view.soAll.size());
}

void checkObjectsStatus(
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
                    o.isOidObject() ? view.getVidReferenceCount(o.getVid()): -1);

            count++;
        }
    }

    if (count > 0)
    {
        SWSS_LOG_THROW("%d objects were not processed", count);
    }
}

/**
 * @brief Match OIDs on temporary and current view using VIDs.
 *
 * For all OID objects in temporary view we match all VID's in the current
 * view, some objects will have the same VID's in both views (eg. ports), in
 * that case their RID's also are the same, so in that case we mark both
 * objects in both views as "MATCHED" which will speed up search comparison
 * logic for those type of objects.
 *
 * @param currentView Current view.
 * @param temporaryView Temporary view.
 */
void matchOids(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView)
{
    SWSS_LOG_ENTER();

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

        SWSS_LOG_INFO("matched %s RID %s VID %s",
                currentIt->second->str_object_type.c_str(),
                sai_serialize_object_id(rid).c_str(),
                sai_serialize_object_id(vid).c_str());
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
         * ruled out.
         */

        SWSS_LOG_THROW("port %s object status is not MATCHED (%d)", p->str_object_id.c_str(), p->getObjectStatus());
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
 * We need to have extra index of processed objects and not processed yet.  We
 * should also cover NULL case and duplicated objects.  Normally we should not
 * have duplicated object id's on the list, and we can easy check that using
 * hash.
 *
 * In case of really long list, easier way to solve this can be getting all the
 * RIDs from current view (they must exist), getting all the matched RIDs from
 * temporary list (if one of them doesn't exist then lists are not equal) sort
 * both list nlog(n) and then compare sequentially.
 *
 * @param currentView Current view.
 * @param temporaryView Temporary view.
 * @param current_count Object list count from current view.
 * @param current_list Object list from current view.
 * @param temporary_count Object list count from temporary view.
 * @param temporary_list Object list from temporary view.
 *
 * @return True if object list are equal, false otherwise.
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

                SWSS_LOG_THROW("temporary object type is %s and current object type is %s, FATAL",
                        sai_serialize_object_type(temporaryObjectType).c_str(),
                        sai_serialize_object_type(currentObjectType).c_str());
            }

            if (temporaryObjectType != currentObjectType)
            {
                /*
                 * Compared object types are different, so they can't be equal.
                 * No need to check other objects on list.
                 */

                return false;
            }

            auto temporaryIt = temporaryView.vidToRid.find(temporaryVid);

            if (temporaryIt == temporaryView.vidToRid.end())
            {
                /*
                 * Temporary RID doesn't exist yet for this object, so it means
                 * this object will be created in the future after all
                 * comparison logic finishes.
                 *
                 * Here we know that this temporary object is not processed yet
                 * but during recursive processing we know that this OID value
                 * was already processed, and two things could happen:
                 *
                 * - we matched existing current object for this VID and actual
                 *   RID was assigned, or
                 *
                 * - during matching we didn't matched current object so RID is
                 *   not assigned, and this object will be created later on
                 *   which will assign new RID
                 *
                 * Since we are here where RID doesn't exist this is the second
                 * case, also we know that current object VID exists so his RID
                 * also exists, so those RID's can't be equal, we need return
                 * false here.
                 *
                 * Fore more strong verification we can introduce and extra
                 * flag in SaiObj indicating that object was processed and it
                 * needs to be created.
                 */

                SWSS_LOG_INFO("temporary RID doesn't exist (VID %s), attributes are not equal",
                        sai_serialize_object_id(temporaryVid).c_str());

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
                SWSS_LOG_THROW("current VID %s exists but current RID is missing, FATAL",
                        sai_serialize_object_id(currentVid).c_str());
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
 * @brief Compare qos map list attributes order insensitive.
 *
 * @param current Current object qos map attribute.
 * @param temporary Temporary object qos map attribute.
 *
 * @return True if attributes are equal, false otherwise.
 */
bool hasEqualQosMapList(
        _In_ const sai_qos_map_list_t& c,
        _In_ const sai_qos_map_list_t& t)
{
    SWSS_LOG_ENTER();

    if (c.count != t.count)
        return false;

    if (c.list == NULL || t.list == NULL)
        return false;

    std::vector<std::string> citems;
    std::vector<std::string> titems;

    for (uint32_t i = 0; i < c.count; i++)
    {
        citems.push_back(sai_serialize_qos_map_item(c.list[i]));
        titems.push_back(sai_serialize_qos_map_item(t.list[i]));
    }

    std::sort(citems.begin(), citems.end());
    std::sort(titems.begin(), titems.end());

    for (uint32_t i = 0; i < c.count; i++)
    {
        if (citems.at(i) != titems.at(i))
        {
            return false;
        }
    }

    SWSS_LOG_NOTICE("qos map are equal, but has different order");

    // all items in both attributes are equal
    return true;
}

bool hasEqualQosMapList(
        _In_ const std::shared_ptr<const SaiAttr> &current,
        _In_ const std::shared_ptr<const SaiAttr> &temporary)
{
    SWSS_LOG_ENTER();

    auto c = current->getSaiAttr()->value.qosmap;
    auto t = temporary->getSaiAttr()->value.qosmap;

    return hasEqualQosMapList(c, t);
}

/**
 * @brief Check if current and temporary object has
 * the same attribute and attribute has the same value on both.
 *
 * This also includes object ID attributes, that's why we need
 * current and temporary view to compare RID values.
 *
 * NOTE: both objects must be the same object type, otherwise
 * this compare make no sense.
 *
 * NOTE: this function does not check if attributes are
 * different, whether we can update existing one to new one,
 * for that we will need different method
 *
 * @param currentView Current view.
 * @param temporaryView Temporary view.
 * @param current Current object.
 * @param temporary Temporary object.
 * @param id Attribute id to be checked.
 *
 * @return True if attributes are equal, false otherwise.
 */
bool hasEqualAttribute(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &current,
        _In_ const std::shared_ptr<const SaiObj> &temporary,
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
             * Worth noticing here that acl entry field/action have enable
             * flag, and if it's disabled then parameter value don't matter.
             * But this is fine here since serialized value don't contain
             * parameter value if it's disabled so we don't need to take extra
             * care about this case here.
             */

            return true;
        }

        if (currentAttr->getAttrMetadata()->attrvaluetype == SAI_ATTR_VALUE_TYPE_QOS_MAP_LIST)
        {
            /*
             * In case of qos map list, order of list does not matter, so
             * compare only entries.
             */

            return hasEqualQosMapList(currentAttr, temporaryAttr);
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
         * TODO: For acl list/action this maybe wrong since one can be
         * enabled and list is empty and other one can be disabled and this
         * function also return empty, so this will mean they are equal but
         * they don't since they differ with enable flag. We probably won't hit
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
    SWSS_LOG_ENTER();

    /*
     * NOTE: this function will sort in descending order
     */

    return a.equal_attributes > b.equal_attributes;
}

/**
 * @brief Select random SAI object from best candidates we found.
 *
 * Input list must contain at least one candidate.
 *
 * @param candidateObjects List of candidate objects.
 *
 * @return Random selected object from provided list.
 */
std::shared_ptr<SaiObj> selectRandomCandidate(
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    size_t candidateCount = candidateObjects.size();

    SWSS_LOG_INFO("selecting random candidate from %zu objects", candidateCount);

    size_t index = std::rand() % candidateCount;

    return candidateObjects.at(index).obj;
}

std::vector<std::shared_ptr<const SaiObj>> findUsageCount(
        _In_ const AsicView &view,
        _In_ const std::shared_ptr<const SaiObj> &obj,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id)
{
    SWSS_LOG_ENTER();

    std::vector<std::shared_ptr<const SaiObj>> filtered;

    const auto &members = view.getObjectsByObjectType(object_type);

    for (auto &m: members)
    {
        if (m->hasAttr(attr_id))
        {
            const auto &attr = m->getSaiAttr(attr_id);

            if (attr->getAttrMetadata()->attrvaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID)
            {
                SWSS_LOG_THROW("attribute %s is not oid attribute, or not oid attr, bug!",
                        attr->getAttrMetadata()->attridname);
            }

            if (attr->getSaiAttr()->value.oid == obj->getVid())
            {
                filtered.push_back(m);
            }
        }
    }

    return filtered;
}

int findAllChildsInDependencyTreeCount(
        _In_ const AsicView &view,
        _In_ const std::shared_ptr<const SaiObj> &obj)
{
    SWSS_LOG_ENTER();

    int count = 0;

    const auto &info = obj->info;

    if (info->revgraphmembers == NULL)
    {
        return count;
    }

    for (int idx = 0; info->revgraphmembers[idx] != NULL; ++idx)
    {
        auto &member = info->revgraphmembers[idx];

        if (member->attrmetadata == NULL)
        {
            /*
             * Skip struct members for now but we need support.
             */

            continue;
        }

        auto &meta = member->attrmetadata;

        if (SAI_HAS_FLAG_READ_ONLY(meta->flags))
        {
            /*
             * Skip read only attributes.
             */

            continue;
        }

        if (meta->attrvaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID)
        {
            /*
             * For now skip lists and acl.
             */

            continue;
        }

        SWSS_LOG_DEBUG("looking for %s on %s", obj->str_object_type.c_str(), meta->attridname);

        auto usageObjects = findUsageCount(view, obj, meta->objecttype, meta->attrid);

        count += (int)usageObjects.size();

        if (meta->objecttype == obj->getObjectType())
        {
            /*
             * Skip loops on scheduler group.
             * Mirror session has loop on port, but we break port in next condition.
             */

            continue;
        }

        if (meta->objecttype == SAI_OBJECT_TYPE_PORT ||
                meta->objecttype == SAI_OBJECT_TYPE_SWITCH)
        {
            /*
             * Ports and switch have a lot of members lets just stop here.
             */
            continue;
        }

        /*
         * For each object that is using input object run recursion to find all
         * nodes in the tree.
         *
         * NOTE: if we would have actual tree then this task would be much
         * easier.
         */

        for (auto &uo: usageObjects)
        {
            count += findAllChildsInDependencyTreeCount(view, uo);
        }
    }

    return count;
}

std::shared_ptr<SaiObj> findCurrentBestMatchForLag(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For lag, let's try find matching LAG member which will be using the same
     * port, since we expect that port object can belong only to 1 lag.
     */

    /*
     * Find not processed LAG members, in both views, since lag member contains
     * LAG and PORT, then it should not be processed before LAG itself. But
     * since PORT objects on LAG members should be matched at the beginning of
     * comparison logic, then we can find batching LAG members based on the
     * same VID, since it will be the same in both views.
     */

    const auto tmpLagMembersNP = temporaryView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_LAG_MEMBER);

    /*
     * First we need to find at least 1 LAG member that belongs to temporary
     * object so we could extract port object.
     */

    sai_object_id_t tmpLagVid = temporaryObj->getVid();

    sai_object_id_t temporaryLagMemberPortVid = SAI_NULL_OBJECT_ID;

    for (const auto &lagMember: tmpLagMembersNP)
    {
        // lag member attr lag should always exists on
        const auto lagMemberLagAttr = lagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_LAG_ID);

        if (lagMemberLagAttr->getSaiAttr()->value.oid == tmpLagVid)
        {
            SWSS_LOG_NOTICE("found temp LAG member %s which uses temp LAG %s",
                    temporaryObj->str_object_id.c_str(),
                    lagMember->str_object_id.c_str());

            temporaryLagMemberPortVid = lagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_PORT_ID)->getSaiAttr()->value.oid;

            break;
        }
    }

    if (temporaryLagMemberPortVid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_WARN("failed to find temporary LAG member for LAG %s", sai_serialize_object_id(tmpLagVid).c_str());

        return nullptr;
    }

    /*
     * Now since we have port VID which should be the same in both current and
     * temporary view, let's find LAG member object that uses this port on
     * current view.
     */

    const auto curLagMembersNP = currentView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_LAG_MEMBER);

    for (const auto &lagMember: curLagMembersNP)
    {
        // lag member attr lag should always exists on
        const auto lagMemberPortAttr = lagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_PORT_ID);

        if (lagMemberPortAttr->getSaiAttr()->value.oid == temporaryLagMemberPortVid)
        {
            SWSS_LOG_NOTICE("found current LAG member %s which uses PORT %s",
                    lagMember->str_object_id.c_str(),
                    sai_serialize_object_id(temporaryLagMemberPortVid).c_str());

            /*
             * We found LAG member which uses the same PORT VID, let's extract
             * LAG and check if this LAG is on the candidate list.
             */

            sai_object_id_t currentLagVid = lagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_LAG_ID)->getSaiAttr()->value.oid;

            for (auto &c: candidateObjects)
            {
                if (c.obj->getVid() == currentLagVid)
                {
                    SWSS_LOG_NOTICE("found best candidate for temp LAG %s which is current LAG %s using PORT %s",
                            temporaryObj->str_object_id.c_str(),
                            sai_serialize_object_id(currentLagVid).c_str(),
                            sai_serialize_object_id(temporaryLagMemberPortVid).c_str());

                    return c.obj;
                }
            }
        }
    }

    SWSS_LOG_WARN("failed to find best candidate for LAG using LAG member and port id: tmp %s", temporaryObj->str_object_id.c_str());

    return nullptr;
}

std::shared_ptr<SaiObj> findCurrentBestMatchForNextHopGroup(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For next hop group, let's try find matching NHG which will be based on
     * NHG set as SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID in route_entry.  We assume
     * that each class IPv4 and IPv6 will have different NHG, and each IP
     * prefix will only be assigned to one NHG.
     */

    /*
     * First find route entries on which temporary NHG is assigned.
     */

    const auto tmpRouteEntries = temporaryView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_ROUTE_ENTRY);

    std::shared_ptr<SaiObj> tmpRouteCandidate = nullptr;

    for (auto tmpRoute: tmpRouteEntries)
    {
        if (!tmpRoute->hasAttr(SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID))
            continue;

        const auto routeNH = tmpRoute->getSaiAttr(SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID);

        if (routeNH->getSaiAttr()->value.oid != temporaryObj->getVid())
            continue;

        tmpRouteCandidate = tmpRoute;

        SWSS_LOG_NOTICE("Found route candidate for NHG: %s: %s",
                temporaryObj->str_object_id.c_str(),
                tmpRoute->str_object_id.c_str());

        break;
    }

    if (tmpRouteCandidate == nullptr)
    {
        SWSS_LOG_NOTICE("failed to find route candidate for NHG: %s",
                temporaryObj->str_object_id.c_str());

        return false;
    }

    /*
     * We found route candidate, then let's find the same route on the candidate side.
     * But we will only compare prefix value.
     */

    const auto curRouteEntries = currentView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_ROUTE_ENTRY);

    for (auto curRoute: curRouteEntries)
    {
        std::string tmpPrefix = sai_serialize_ip_prefix(tmpRouteCandidate->meta_key.objectkey.key.route_entry.destination);
        std::string curPrefix = sai_serialize_ip_prefix(curRoute->meta_key.objectkey.key.route_entry.destination);

        if (tmpPrefix != curPrefix)
            continue;

        /*
         * Prefixes are equal, now find candidate NHG.
         */

        if (!curRoute->hasAttr(SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID))
            continue;

        const auto routeNH = curRoute->getSaiAttr(SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID);

        sai_object_id_t curNextHopId = routeNH->getSaiAttr()->value.oid;

        for (auto candidate: candidateObjects)
        {
            if (curNextHopId != candidate.obj->getVid())
                continue;

            SWSS_LOG_NOTICE("Found NHG candidate %s", candidate.obj->str_object_id.c_str());

            return candidate.obj;
        }
    }

    SWSS_LOG_NOTICE("failed to find best candidate for next hop group using route_entry");

    return nullptr;
}

std::shared_ptr<SaiObj> findCurrentBestMatchForAclTableGroup(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For acl table group let's try find matching INGRESS_ACL on matched PORT.
     */

    /*
     * Since we know that ports are matched, they have same VID/RID in both
     * temporary and current view.
     */

    const auto ports = temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_PORT);

    for (auto port: ports)
    {
        if (!port->hasAttr(SAI_PORT_ATTR_INGRESS_ACL))
            continue;

        auto inACL = port->getSaiAttr(SAI_PORT_ATTR_INGRESS_ACL);

        if (inACL->getSaiAttr()->value.oid != temporaryObj->getVid())
            continue;

        SWSS_LOG_DEBUG("found port candidate %s for ACL table group",
                port->str_object_id.c_str());


        auto curPort = currentView.oOids.at(port->getVid());

        if (!curPort->hasAttr(SAI_PORT_ATTR_INGRESS_ACL))
            continue;

        inACL = curPort->getSaiAttr(SAI_PORT_ATTR_INGRESS_ACL);

        sai_object_id_t atgVid = inACL->getSaiAttr()->value.oid;

        for (auto c: candidateObjects)
        {
            if (c.obj->getVid() == atgVid)
            {
                SWSS_LOG_INFO("found ALC table group candidate %s using port %s",
                        c.obj->str_object_id.c_str(),
                        port->str_object_id.c_str());

                return c.obj;
            }
        }
    }

    /*
     * Port didn't work, try to find match by LAG, but lag will be tricky,
     * since it will be not matched since if this unprocessed acl table group
     * is processed right now, then if it's assigned to lag then by design we
     * go recursively be attributes to match attributes first.
     */

    // TODO this could be helper method, since we will need this for router interface

    const auto tmpLags = temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_LAG);

    for (auto tmpLag: tmpLags)
    {
        if (!tmpLag->hasAttr(SAI_LAG_ATTR_INGRESS_ACL))
            continue;

        auto inACL = tmpLag->getSaiAttr(SAI_LAG_ATTR_INGRESS_ACL);

        if (inACL->getSaiAttr()->value.oid != temporaryObj->getVid())
            continue;

        /*
         * We found LAG on which this ACL is present, but this object status is
         * not processed so we need to trace back to port using LAG member.
         */

        SWSS_LOG_INFO("found LAG candidate: lag status %d", tmpLag->getObjectStatus());

        const auto tmpLagMembers = temporaryView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_LAG_MEMBER);

        for (auto tmpLagMember: tmpLagMembers)
        {
            const auto tmpLagMemberLagAttr = tmpLagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_LAG_ID);

            if (tmpLagMemberLagAttr->getSaiAttr()->value.oid != tmpLag->getVid())
                continue;

            const auto tmpLagMemberPortAttr = tmpLagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_PORT_ID);

            sai_object_id_t tmpPortVid = tmpLagMemberPortAttr->getSaiAttr()->value.oid;

            SWSS_LOG_INFO("found tmp LAG member port! %s", sai_serialize_object_id(tmpPortVid).c_str());

            sai_object_id_t portRid = temporaryView.vidToRid.at(tmpPortVid);

            sai_object_id_t curPortVid = currentView.ridToVid.at(portRid);

            const auto curLagMembers = currentView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_LAG_MEMBER);

            for (auto curLagMember: curLagMembers)
            {
                const auto curLagMemberPortAttr = curLagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_PORT_ID);

                if (curLagMemberPortAttr->getSaiAttr()->value.oid != curPortVid)
                    continue;

                const auto curLagMemberLagAttr = curLagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_LAG_ID);

                sai_object_id_t curLagId = curLagMemberLagAttr->getSaiAttr()->value.oid;

                SWSS_LOG_INFO("found current LAG member: %s", curLagMember->str_object_id.c_str());

                auto curLag = currentView.oOids.at(curLagId);

                if (!curLag->hasAttr(SAI_LAG_ATTR_INGRESS_ACL))
                    continue;

                inACL = curLag->getSaiAttr(SAI_LAG_ATTR_INGRESS_ACL);

                for (auto c: candidateObjects)
                {
                    if (c.obj->getVid() != inACL->getSaiAttr()->value.oid)
                        continue;

                    SWSS_LOG_INFO("found best ACL table group match based on LAG ingress acl: %s", c.obj->str_object_id.c_str());

                    return c.obj;
                }
            }
        }
    }

    SWSS_LOG_NOTICE("failed to find best candidate for ACL table group using port");

    return nullptr;
}

std::shared_ptr<SaiObj> findCurrentBestMatchForAclTable(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For acl table we can go to acl table group member and then to acl table group and port.
     */

    const auto tmpAclTableGroupMembers = temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER);

    for (auto tmpAclTableGroupMember: tmpAclTableGroupMembers)
    {
        auto tmpAclTableId = tmpAclTableGroupMember->getSaiAttr(SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_ID);

        if (tmpAclTableId->getOid() != temporaryObj->getVid())
        {
            // this is not the expected acl table group member
            continue;
        }

        auto tmpAclTableGroupId = tmpAclTableGroupMember->getSaiAttr(SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_GROUP_ID);

        /*
         * We have acl table group id, search on which port it's set, not let's
         * find on which port it's set.
         */

        const auto tmpPorts = temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_PORT);

        for (auto tmpPort: tmpPorts)
        {
            if (!tmpPort->hasAttr(SAI_PORT_ATTR_INGRESS_ACL))
                continue;

            auto tmpInACL = tmpPort->getSaiAttr(SAI_PORT_ATTR_INGRESS_ACL);

            if (tmpInACL->getOid() != tmpAclTableGroupId->getOid())
                continue;

            auto curPort = currentView.oOids.at(tmpPort->getVid());

            if (!curPort->hasAttr(SAI_PORT_ATTR_INGRESS_ACL))
                continue;

            auto curInACL = curPort->getSaiAttr(SAI_PORT_ATTR_INGRESS_ACL);

            /*
             * We found current ingress acl, now let's find acl table group members
             * that use this acl table group.
             */

            const auto curAclTableGroupMembers = currentView.getObjectsByObjectType(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER);

            for (auto curAclTableGroupMember: curAclTableGroupMembers)
            {
                auto curAclTableGroupId = curAclTableGroupMember->getSaiAttr(SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_GROUP_ID);

                if (curAclTableGroupId->getOid() != curInACL->getOid())
                {
                    // this member uses different acl table group
                    continue;
                }

                auto curAclTableId = curAclTableGroupMember->getSaiAttr(SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_ID);

                /*
                 * We found possible current acl table ID, let's see if it's on
                 * candidate list. Note, it could be possible that many
                 * different paths will lead multiple possible candidates.
                 */

                for (auto c: candidateObjects)
                {
                    if (c.obj->getVid() == curAclTableId->getOid())
                    {
                        SWSS_LOG_INFO("found ALC table candidate %s using port %s",
                                c.obj->str_object_id.c_str(),
                                tmpPort->str_object_id.c_str());

                        return c.obj;
                    }
                }
            }
        }
    }

    SWSS_LOG_NOTICE("failed to find best candidate for ACL table using port");

    return nullptr;
}

std::shared_ptr<SaiObj> findCurrentBestMatchForRouterInterface(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For router interface which is LOOPBACK, we could trace them to TUNNEL
     * object on which they are used. It could be not obvious, when multiple
     * tunnels will be used.
     */

    const auto typeAttr = temporaryObj->getSaiAttr(SAI_ROUTER_INTERFACE_ATTR_TYPE);

    if (typeAttr->getSaiAttr()->value.s32 != SAI_ROUTER_INTERFACE_TYPE_LOOPBACK)
    {
        SWSS_LOG_WARN("RIF %s is not LOOPBACK", temporaryObj->str_object_id.c_str());

        return nullptr;
    }

    const auto tmpTunnels = temporaryView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_TUNNEL);

    for (auto tmpTunnel: tmpTunnels)
    {
        /*
         * Try match tunnel by src encap IP address.
         */

        if (!tmpTunnel->hasAttr(SAI_TUNNEL_ATTR_ENCAP_SRC_IP))
        {
            // not encap src attribute, skip
            continue;
        }

        const std::string tmpSrcIP = tmpTunnel->getSaiAttr(SAI_TUNNEL_ATTR_ENCAP_SRC_IP)->getStrAttrValue();

        const auto curTunnels = currentView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_TUNNEL);

        for (auto curTunnel: curTunnels)
        {
            if (!tmpTunnel->hasAttr(SAI_TUNNEL_ATTR_ENCAP_SRC_IP))
            {
                // not encap src attribute, skip
                continue;
            }

            const std::string curSrcIP = tmpTunnel->getSaiAttr(SAI_TUNNEL_ATTR_ENCAP_SRC_IP)->getStrAttrValue();

            if (curSrcIP != tmpSrcIP)
            {
                continue;
            }

            /*
             * At this point we have both tunnels which ip matches.
             */

            if (tmpTunnel->hasAttr(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE) &&
                curTunnel->hasAttr(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE))
            {
                auto tmpRif = tmpTunnel->getSaiAttr(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE);
                auto curRif = curTunnel->getSaiAttr(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE);

                if (tmpRif->getSaiAttr()->value.oid == temporaryObj->getVid())
                {
                    for (auto c: candidateObjects)
                    {
                        if (c.obj->getVid() != curRif->getSaiAttr()->value.oid)
                            continue;

                        SWSS_LOG_INFO("found best ROUTER_INTERFACE based on TUNNEL underlay interface %s", c.obj->str_object_id.c_str());

                        return c.obj;
                    }
                }
            }

            if (tmpTunnel->hasAttr(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE) &&
                curTunnel->hasAttr(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE))
            {
                auto tmpRif = tmpTunnel->getSaiAttr(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE);
                auto curRif = curTunnel->getSaiAttr(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE);

                if (tmpRif->getSaiAttr()->value.oid == temporaryObj->getVid())
                {
                    for (auto c: candidateObjects)
                    {
                        if (c.obj->getVid() != curRif->getSaiAttr()->value.oid)
                            continue;

                        SWSS_LOG_INFO("found best ROUTER_INTERFACE based on TUNNEL overlay interface %s", c.obj->str_object_id.c_str());

                        return c.obj;
                    }
                }
            }
        }
    }

    // try find tunnel by TUNNEL_TERM_TABLE_ENTRY using SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP

    for (auto tmpTunnel: tmpTunnels)
    {
        const auto curTunnels = currentView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_TUNNEL);

        for (auto curTunnel: curTunnels)
        {
            const auto tmpTunnelTermTableEtnries = temporaryView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY);

            for (auto tmpTunnelTermTableEntry: tmpTunnelTermTableEtnries)
            {
                auto tmpTunnelId = tmpTunnelTermTableEntry->tryGetSaiAttr(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID);

                if (tmpTunnelId == nullptr)
                    continue;

                auto tmpDstIp = tmpTunnelTermTableEntry->tryGetSaiAttr(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP);

                if (tmpDstIp == nullptr)
                    continue;

                if (tmpTunnelId->getOid() != tmpTunnel->getVid())   // not this tunnel
                    continue;

                const auto curTunnelTermTableEtnries = currentView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY);

                for (auto curTunnelTermTableEntry: curTunnelTermTableEtnries)
                {
                    auto curTunnelId = curTunnelTermTableEntry->tryGetSaiAttr(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID);

                    if (curTunnelId == nullptr)
                        continue;

                    auto curDstIp = curTunnelTermTableEntry->tryGetSaiAttr(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP);

                    if (curDstIp == nullptr)
                        continue;

                    if (curTunnelId->getOid() != curTunnel->getVid())   // not this tunnel
                        continue;

                    if (curDstIp->getStrAttrValue() != tmpDstIp->getStrAttrValue())
                        continue;

                    if (tmpTunnel->hasAttr(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE) &&
                            curTunnel->hasAttr(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE))
                    {
                        auto tmpRif = tmpTunnel->getSaiAttr(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE);
                        auto curRif = curTunnel->getSaiAttr(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE);

                        if (tmpRif->getSaiAttr()->value.oid == temporaryObj->getVid())
                        {
                            for (auto c: candidateObjects)
                            {
                                if (c.obj->getVid() != curRif->getSaiAttr()->value.oid)
                                    continue;

                                SWSS_LOG_INFO("found best ROUTER_INTERFACE based on TUNNEL underlay interface %s", c.obj->str_object_id.c_str());

                                return c.obj;
                            }
                        }
                    }

                    if (tmpTunnel->hasAttr(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE) &&
                            curTunnel->hasAttr(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE))
                    {
                        auto tmpRif = tmpTunnel->getSaiAttr(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE);
                        auto curRif = curTunnel->getSaiAttr(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE);

                        if (tmpRif->getSaiAttr()->value.oid == temporaryObj->getVid())
                        {
                            for (auto c: candidateObjects)
                            {
                                if (c.obj->getVid() != curRif->getSaiAttr()->value.oid)
                                    continue;

                                SWSS_LOG_INFO("found best ROUTER_INTERFACE based on TUNNEL overlay interface %s", c.obj->str_object_id.c_str());

                                return c.obj;
                            }
                        }
                    }
                }
            }
        }
    }

    SWSS_LOG_NOTICE("failed to find best candidate for LOOPBACK ROUTER_INTERFACE using TUNNEL");

    return nullptr;
}

std::shared_ptr<SaiObj> findCurrentBestMatchForPolicer(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For policer we can see on which hostif trap group is set, and on which
     * hostif trap.  Hostif trap have SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE attribute
     * which is KEY and there can be only one trap of that type. we can use
     * that to match hostif trap group and later on policer.
     *
     * NOTE: policer can be set on default hostif trap group. In this case we
     * are getting processed and not processed objects into account.
     */

    const auto tmpTrapGroups = temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP);

    for (auto tmpTrapGroup: tmpTrapGroups)
    {
        auto tmpTrapGroupPolicerAttr = tmpTrapGroup->tryGetSaiAttr(SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER);

        if (tmpTrapGroupPolicerAttr == nullptr)
        {
            // no policer attribute
            continue;
        }

        if (tmpTrapGroupPolicerAttr->getOid() != temporaryObj->getVid())
        {
            // not this policer
            continue;
        }

        /*
         * Found hostif trap group which have this policer, now find hostif
         * trap type with this trap group.
         */

        const auto tmpTraps = temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_HOSTIF_TRAP);

        for (auto tmpTrap: tmpTraps)
        {
            auto tmpTrapGroupAttr = tmpTrap->tryGetSaiAttr(SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP);

            if (tmpTrapGroupAttr == nullptr)
                continue;

            if (tmpTrapGroupAttr->getOid() != tmpTrapGroup->getVid())
                continue;

            auto tmpTrapTypeAttr = tmpTrap->getSaiAttr(SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE);

            SWSS_LOG_INFO("trap type: %s", tmpTrapTypeAttr->getStrAttrValue().c_str());

            /*
             * We have temporary trap type, let's find that trap in current view.
             */

            const auto curTraps = currentView.getObjectsByObjectType(SAI_OBJECT_TYPE_HOSTIF_TRAP);

            for (auto curTrap: curTraps)
            {
                auto curTrapTypeAttr = curTrap->getSaiAttr(SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE);

                if (curTrapTypeAttr->getStrAttrValue() != tmpTrapTypeAttr->getStrAttrValue())
                    continue;

                /*
                 * We have that trap, let's extract trap group.
                 */

                auto curTrapGroupAttr = curTrap->tryGetSaiAttr(SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP);

                if (curTrapGroupAttr == nullptr)
                    continue;

                sai_object_id_t curTrapGroupVid = curTrapGroupAttr->getOid();

                /*
                 * If value is not set, it should point to SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP
                 */

                if (curTrapGroupVid == SAI_NULL_OBJECT_ID)
                    continue;

                auto curTrapGroup = currentView.oOids.at(curTrapGroupVid);

                auto curTrapGroupPolicerAttr = curTrapGroup->tryGetSaiAttr(SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER);

                if (curTrapGroupPolicerAttr == nullptr)
                {
                    // no policer attribute
                    continue;
                }

                for (auto c: candidateObjects)
                {
                    if (c.obj->getVid() != curTrapGroupPolicerAttr->getOid())
                        continue;

                    SWSS_LOG_INFO("found best POLICER based on hostif trap group %s", c.obj->str_object_id.c_str());

                    return c.obj;
                }
            }
        }
    }

    SWSS_LOG_NOTICE("failed to find best candidate for POLICER using hostif trap group");

    return nullptr;
}

std::shared_ptr<SaiObj> findCurrentBestMatchForHostifTrapGroup(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For hostif trap group we can see on which hostif trap group is set.
     * Hostif trap have SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE attribute which is KEY
     * and there can be only one trap of that type. we can use that to match
     * hostif trap group.
     */

    const auto tmpTraps = temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_HOSTIF_TRAP);

    for (auto tmpTrap: tmpTraps)
    {
        auto tmpTrapGroupAttr = tmpTrap->tryGetSaiAttr(SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP);

        if (tmpTrapGroupAttr == nullptr)
            continue;

        if (tmpTrapGroupAttr->getOid() != temporaryObj->getVid())
            continue;

        auto tmpTrapTypeAttr = tmpTrap->getSaiAttr(SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE);

        SWSS_LOG_INFO("trap type: %s", tmpTrapTypeAttr->getStrAttrValue().c_str());

        /*
         * We have temporary trap type, let's find that trap in current view.
         */

        const auto curTraps = currentView.getObjectsByObjectType(SAI_OBJECT_TYPE_HOSTIF_TRAP);

        for (auto curTrap: curTraps)
        {
            auto curTrapTypeAttr = curTrap->getSaiAttr(SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE);

            if (curTrapTypeAttr->getStrAttrValue() != tmpTrapTypeAttr->getStrAttrValue())
                continue;

            /*
             * We have that trap, let's extract trap group.
             */

            auto curTrapGroupAttr = curTrap->tryGetSaiAttr(SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP);

            if (curTrapGroupAttr == nullptr)
                continue;

            for (auto c: candidateObjects)
            {
                if (c.obj->getVid() != curTrapGroupAttr->getOid())
                    continue;

                SWSS_LOG_INFO("found best HOSTIF TRAP GROUP based on hostif trap %s", c.obj->str_object_id.c_str());

                return c.obj;
            }
        }
    }

    SWSS_LOG_NOTICE("failed to find best candidate for HOSTIF TRAP GROUP using hostif trap");

    return nullptr;
}

std::shared_ptr<SaiObj> findCurrentBestMatchForBufferPool(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    /*
     * For buffer pool using buffer profile which could be set on ingress
     * priority group or queue. Those two should be already matched.
     */

    const auto tmpBufferProfiles = temporaryView.getNotProcessedObjectsByObjectType(SAI_OBJECT_TYPE_BUFFER_PROFILE);

    for (auto tmpBufferProfile: tmpBufferProfiles)
    {
        auto tmpPoolIdAttr = tmpBufferProfile->tryGetSaiAttr(SAI_BUFFER_PROFILE_ATTR_POOL_ID);

        if (tmpPoolIdAttr == nullptr)
            continue;

        if (tmpPoolIdAttr->getOid() != temporaryObj->getVid())
            continue;

        /*
         * We have temporary buffer profile which uses this buffer pool, let's
         * find ingress priority group or queue on which it could be set.
         */

        auto tmpQueues = temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_QUEUE);

        for (auto tmpQueue: tmpQueues)
        {
            auto tmpBufferProfileIdAttr = tmpQueue->tryGetSaiAttr(SAI_QUEUE_ATTR_BUFFER_PROFILE_ID);

            if (tmpBufferProfileIdAttr == nullptr)
                continue;

            if (tmpBufferProfileIdAttr->getOid() != tmpBufferProfile->getVid())
                continue;

            if (tmpQueue->getObjectStatus() != SAI_OBJECT_STATUS_MATCHED)
                continue;

            // we can use tmp VID since object is matched and both vids are the same
            auto curQueue = currentView.oOids.at(tmpQueue->getVid());

            auto curBufferProfileIdAttr = curQueue->tryGetSaiAttr(SAI_QUEUE_ATTR_BUFFER_PROFILE_ID);

            if (curBufferProfileIdAttr == nullptr)
                continue;

            if (curBufferProfileIdAttr->getOid() == SAI_NULL_OBJECT_ID)
                continue;

            // we have buffer profile

            auto curBufferProfile = currentView.oOids.at(curBufferProfileIdAttr->getOid());

            auto curPoolIdAttr = curBufferProfile->tryGetSaiAttr(SAI_BUFFER_PROFILE_ATTR_POOL_ID);

            if (curPoolIdAttr == nullptr)
                continue;

            for (auto c: candidateObjects)
            {
                if (c.obj->getVid() != curPoolIdAttr->getOid())
                    continue;

                SWSS_LOG_INFO("found best BUFFER POOL based on buffer profile and queue %s", c.obj->str_object_id.c_str());

                return c.obj;
            }
        }

        /*
         * Queues didn't worked, lets try to use ingress priority groups.
         */

        auto tmpIngressPriorityGroups = temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP);

        for (auto tmpIngressPriorityGroup: tmpIngressPriorityGroups)
        {
            auto tmpBufferProfileIdAttr = tmpIngressPriorityGroup->tryGetSaiAttr(SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE);

            if (tmpBufferProfileIdAttr == nullptr)
                continue;

            if (tmpBufferProfileIdAttr->getOid() != tmpBufferProfile->getVid())
                continue;

            if (tmpIngressPriorityGroup->getObjectStatus() != SAI_OBJECT_STATUS_MATCHED)
                continue;

            // we can use tmp VID since object is matched and both vids are the same
            auto curIngressPriorityGroup = currentView.oOids.at(tmpIngressPriorityGroup->getVid());

            auto curBufferProfileIdAttr = curIngressPriorityGroup->tryGetSaiAttr(SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE);

            if (curBufferProfileIdAttr == nullptr)
                continue;

            if (curBufferProfileIdAttr->getOid() == SAI_NULL_OBJECT_ID)
                continue;

            // we have buffer profile

            auto curBufferProfile = currentView.oOids.at(curBufferProfileIdAttr->getOid());

            auto curPoolIdAttr = curBufferProfile->tryGetSaiAttr(SAI_BUFFER_PROFILE_ATTR_POOL_ID);

            if (curPoolIdAttr == nullptr)
                continue;

            for (auto c: candidateObjects)
            {
                if (c.obj->getVid() != curPoolIdAttr->getOid())
                    continue;

                SWSS_LOG_INFO("found best BUFFER POOL based on buffer profile and ingress priority group %s", c.obj->str_object_id.c_str());

                return c.obj;
            }
        }

    }

    SWSS_LOG_NOTICE("failed to find best candidate for BUFFER POOL using buffer profile, ipg and queue");

    return nullptr;
}

std::shared_ptr<SaiObj> findCurrentBestMatchForGenericObjectUsingGraph(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    std::shared_ptr<SaiObj> candidate = nullptr;

    switch (temporaryObj->getObjectType())
    {
        case SAI_OBJECT_TYPE_LAG:
            candidate = findCurrentBestMatchForLag(currentView, temporaryView, temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_NEXT_HOP_GROUP:
            candidate = findCurrentBestMatchForNextHopGroup(currentView, temporaryView, temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_ACL_TABLE_GROUP:
            candidate = findCurrentBestMatchForAclTableGroup(currentView, temporaryView, temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_ROUTER_INTERFACE:
            candidate = findCurrentBestMatchForRouterInterface(currentView, temporaryView, temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_POLICER:
            candidate = findCurrentBestMatchForPolicer(currentView, temporaryView, temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP:
            candidate = findCurrentBestMatchForHostifTrapGroup(currentView, temporaryView, temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_ACL_TABLE:
            candidate = findCurrentBestMatchForAclTable(currentView, temporaryView, temporaryObj, candidateObjects);
            break;

        case SAI_OBJECT_TYPE_BUFFER_POOL:
            candidate = findCurrentBestMatchForBufferPool(currentView, temporaryView, temporaryObj, candidateObjects);
            break;

        default:
            break;
    }

    return candidate;
}

std::shared_ptr<SaiObj> findCurrentBestMatchForGenericObjectUsingHeuristic(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
        _In_ const std::vector<sai_object_compare_info_t> &candidateObjects)
{
    SWSS_LOG_ENTER();

    std::shared_ptr<SaiObj> candidate = findCurrentBestMatchForGenericObjectUsingGraph(
            currentView,
            temporaryView,
            temporaryObj,
            candidateObjects);

    if (candidate != nullptr)
        return candidate;

    /*
     * Idea is to count all dependencies that uses this object.  this may not
     * be a good approach since logic may choose wrong candidate.
     */

    int tempCount = findAllChildsInDependencyTreeCount(temporaryView, temporaryObj);

    SWSS_LOG_DEBUG("%s count usage: %d", temporaryObj->str_object_type.c_str(), tempCount);

    std::vector<int> counts;

    int exact = 0;
    std::shared_ptr<SaiObj> matched;

    for (auto &c: candidateObjects)
    {
        int count = findAllChildsInDependencyTreeCount(currentView, c.obj);

        SWSS_LOG_DEBUG("candidate count usage: %d", count);

        counts.push_back(count);

        if (count == tempCount)
        {
            exact++;
            matched = c.obj;
        }
    }

    if (exact == 1)
    {
        return matched;
    }

    SWSS_LOG_WARN("heuristic failed for %s, selecting at random (count: %d, exact match: %d)",
            temporaryObj->str_object_type.c_str(),
            tempCount,
            exact);

    return selectRandomCandidate(candidateObjects);
}

std::shared_ptr<SaiAttr> getSaiAttrFromDefaultValue(
        _In_ const AsicView &currentView,
        _In_ const sai_attr_metadata_t &meta);

std::shared_ptr<SaiObj> findCurrentBestMatchForGenericObject(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
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
     * Since our system design is to restart orchagent without restarting syncd
     * and recreating objects and reassign new VIDs created inside orchagent,
     * in our most cases values of objects will not change.  This will cause to
     * make our comparison logic here fairly simple:
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
     *   destroyed because we need to set new CREATE_ONLY attributes
     *
     * - there are also cases with default values of attributes where attribute
     *   is present only in one object but on other one it have default value
     *   and this default value is the same as the one in attribute
     *
     * - another case is for objects that needs to be removed since there are
     *   no corresponding objects in temporary view, but they can't be removed,
     *   objects like PORT or default QUEUEs or INGRESS_PRIORITY_GROUPs, then
     *   we need to bring their current set values to default ones, which also
     *   can be challenging since we need to know previous default value and it
     *   could be assigned by switch internally, like default MAC address or
     *   default TRAP group etc
     *
     * - there is also interesting case with KEYs attributes which when
     *   doing remove/create they needs to be removed first since
     *   we can't have 2 identical cases
     *
     * There are a lot of aspects to consider here, in here we will cake only
     * couple of them in consideration, and other will be taken care inside
     * processObjectForViewTransition method which will handle all other cases
     * not mentioned here.
     */

    /*
     * First check if object is oid object, if yes, check if it status is
     * matched.
     */

    if (!temporaryObj->isOidObject())
    {
        SWSS_LOG_THROW("non object id %s is used in generic method, please implement special case, FIXME",
                temporaryObj->str_object_type.c_str());
    }

    /*
     * Get not processed objects of temporary object type, and all attributes
     * that are set on that object. This function should be used only on oid
     * object ids, since for non object id finding best match is based on
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

        /*
         * NOTE: we only iterate by attributes that are present in temporary
         * view. It may happen that current view has some additional attributes
         * set that are create only and value can't be updated then, so in that
         * case such object must be disqualified from being candidate.
         */

        for (const auto &attr: attrs)
        {
            sai_attr_id_t attrId = attr.first;

            /*
             * Function hasEqualAttribute check if attribute exists on both objects.
             */

            if (hasEqualAttribute(currentView, temporaryView, currentObj, temporaryObj, attrId))
            {
                soci.equal_attributes++;

                SWSS_LOG_DEBUG("ob equal %s %s, %s: %s",
                        temporaryObj->str_object_id.c_str(),
                        currentObj->str_object_id.c_str(),
                        attr.second->getStrAttrId().c_str(),
                        attr.second->getStrAttrValue().c_str());
            }
            else
            {
                SWSS_LOG_DEBUG("ob not equal %s %s, %s: %s",
                        temporaryObj->str_object_id.c_str(),
                        currentObj->str_object_id.c_str(),
                        attr.second->getStrAttrId().c_str(),
                        attr.second->getStrAttrValue().c_str());

                /*
                 * Function hasEqualAttribute returns true only when both
                 * attributes are existing and both are equal, so here it
                 * returned false, so it may mean 2 things:
                 *
                 * - attribute doesn't exist in current view, or
                 * - attributes are different
                 *
                 * If we check if attribute also exists in current view and has
                 * CREATE_ONLY flag then attributes are different and we
                 * disqualify this object since new temporary object needs to
                 * pass new different attribute with CREATE_ONLY flag.
                 *
                 * Case when attribute doesn't exist is much more complicated
                 * since it maybe conditional and have default value, we will
                 * do that check when we select best match.
                 */

                /*
                 * Get attribute metadata to see if contains CREATE_ONLY flag.
                 */

                const sai_attr_metadata_t* meta = attr.second->getAttrMetadata();

                if (SAI_HAS_FLAG_CREATE_ONLY(meta->flags) && currentObj->hasAttr(attrId))
                {
                    has_different_create_only_attr = true;

                    SWSS_LOG_INFO("obj has not equal create only attributes %s",
                            temporaryObj->str_object_id.c_str());

                    /*
                     * In this case there is no need to compare other
                     * attributes since we won't be able to update them anyway.
                     */

                    break;
                }

                if (SAI_HAS_FLAG_CREATE_ONLY(meta->flags) && !currentObj->hasAttr(attrId))
                {
                    /*
                     * This attribute exists only on temporary view and it's
                     * create only.  If it has default value, check if it's the
                     * same as current.
                     */

                    auto curDefault = getSaiAttrFromDefaultValue(currentView, *meta);

                    if (curDefault != nullptr)
                    {
                        if (curDefault->getStrAttrValue() != attr.second->getStrAttrValue())
                        {
                            has_different_create_only_attr = true;

                            SWSS_LOG_INFO("obj has not equal create only attributes %s (default): %s",
                                    temporaryObj->str_object_id.c_str(),
                                    meta->attridname);
                            break;
                        }
                        else
                        {
                            SWSS_LOG_INFO("obj has equal create only value %s (default): %s",
                                    temporaryObj->str_object_id.c_str(),
                                    meta->attridname);
                        }
                    }
                }
            }
        }

        /*
         * Before we add this object as candidate, see if there are some create
         * only attributes which are not present in temporary object but
         * present in current, and if there is default value that is the same.
         */

        const auto curAttrs = currentObj->getAllAttributes();

        for (auto curAttr: curAttrs)
        {
            if (attrs.find(curAttr.first) != attrs.end())
            {
                // attr exists in both objects.
                continue;
            }

            const sai_attr_metadata_t* meta = curAttr.second->getAttrMetadata();

            if (SAI_HAS_FLAG_CREATE_ONLY(meta->flags) && !temporaryObj->hasAttr(curAttr.first))
            {
                /*
                 * This attribute exists only on current view and it's
                 * create only.  If it has default value, check if it's the
                 * same as current.
                 */

                auto tmpDefault = getSaiAttrFromDefaultValue(temporaryView, *meta);

                if (tmpDefault != nullptr)
                {
                    if (tmpDefault->getStrAttrValue() != curAttr.second->getStrAttrValue())
                    {
                        has_different_create_only_attr = true;

                        SWSS_LOG_INFO("obj has not equal create only attributes %s (default): %s",
                                currentObj->str_object_id.c_str(),
                                meta->attridname);
                        break;
                    }
                    else
                    {
                        SWSS_LOG_INFO("obj has equal create only value %s (default): %s",
                                temporaryObj->str_object_id.c_str(),
                                meta->attridname);
                    }
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

    SWSS_LOG_INFO("number candidate objects for %s is %zu",
            temporaryObj->str_object_id.c_str(),
            candidateObjects.size());

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
     * If we have more than 1 object matched actually more preferred
     * object would be the object with most CREATE_ONLY attributes matching
     * since that will reduce risk of removing and recreating that object in
     * current view.
     */

    /*
     * But at this point also let's try find best candidate using graph paths,
     * since if some attributes are mismatched (like for example more ACLs are
     * created) this can lead to choose wrong LAG and have implications on
     * router interface and so on.  So matching by graph path here could be
     * more precise.
     */

    auto graphCandidate = findCurrentBestMatchForGenericObjectUsingGraph(
            currentView,
            temporaryView,
            temporaryObj,
            candidateObjects);

    if (graphCandidate != nullptr)
        return graphCandidate;

    /*
     * Sort candidate objects by equal attributes in descending order, we know
     * here that we have at least 2 candidates.
     *
     * NOTE: maybe at this point we should be using heuristics?
     */

    std::sort(candidateObjects.begin(), candidateObjects.end(), compareByEqualAttributes);

    if (candidateObjects.at(0).equal_attributes > candidateObjects.at(1).equal_attributes)
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
     * Smart lookup would be for example searching whether current object is
     * pointing to the same PORT as temporary object (since ports are matched
     * at the beginning). For different types of objects we need different type
     * of logic and we can start adding that when needed and when missing we
     * will just choose at random possible causing some remove/recreate but
     * this logic is not perfect at this point.
     */

    /*
     * Lets also remove candidates with less equal attributes
     */

    size_t previousCandidates = candidateObjects.size();

    size_t equalAttributes = candidateObjects.at(0).equal_attributes;

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
}

bool exchangeTemporaryVidToCurrentVid(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _Inout_ sai_object_meta_key_t &meta_key)
{
    SWSS_LOG_ENTER();

    auto info = sai_metadata_get_object_type_info(meta_key.objecttype);

    // XXX if (info->isobjectid)

    if (!info->isnonobjectid)
    {
        SWSS_LOG_THROW("expected non object id, but got: %s",
                sai_serialize_object_meta_key(meta_key).c_str());
    }

    for (size_t j = 0; j < info->structmemberscount; ++j)
    {
        const sai_struct_member_info_t *m = info->structmembers[j];

        if (m->membervaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID)
        {
            continue;
        }

        sai_object_id_t tempVid = m->getoid(&meta_key);

        auto temporaryIt = temporaryView.vidToRid.find(tempVid);

        if (temporaryIt == temporaryView.vidToRid.end())
        {
            /*
             * RID for temporary VID was not assigned, so we didn't matched it
             * in previous processing, or VID will be created later on. This
             * mean we can't match because even if all attributes are matching,
             * RID in struct after create will not match so at the end of
             * processing current object will need to be destroyed and new one
             * needs to be created.
             */

            return false;
        }

        sai_object_id_t temporaryRid = temporaryIt->second;

        auto currentIt = currentView.ridToVid.find(temporaryRid);

        if (currentIt == currentView.ridToVid.end())
        {
            /*
             * This is just sanity check, should never happen.
             */

            SWSS_LOG_THROW("found tempoary RID %s but current VID doesn't exist, FATAL",
                    sai_serialize_object_id(temporaryRid).c_str());
        }

        /*
         * This vid is vid of current view, it may or may not be
         * equal to temporaryVid, but we need to this step to not guess vids.
         */

        sai_object_id_t currentVid = currentIt->second;

        m->setoid(&meta_key, currentVid);
    }

    return true;
}

// TODO we can do this in generic way, we need serialize

/**
 * @brief Find current best match for neighbor.
 *
 * For Neighbor we don't need to iterate via all current neighbors, we can do
 * dictionary lookup, but we need to do smart trick, since temporary object was
 * processed we just need to check whether VID in neighbor_entry struct is
 * matched/final and it has RID assigned from current view. If, RID exists, we
 * can use that RID to get VID of current view, exchange in neighbor_entry
 * struct and do dictionary lookup on serialized neighbor_entry.
 *
 * With this approach for many entries this is the quickest possible way. In
 * case when RID doesn't exist, that means we have invalid neighbor entry, so we
 * must return null.
 *
 * @param currentView Current view.
 * @param temporaryView Temporary view.
 * @param temporaryObj Temporary object.
 *
 * @return Best match object if found or nullptr.
 */
std::shared_ptr<SaiObj> findCurrentBestMatchForNeighborEntry(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * Make a copy here to not destroy object data, later
     * on this data should be read only.
     */

    sai_object_meta_key_t mk = temporaryObj->meta_key;

    if (!exchangeTemporaryVidToCurrentVid(currentView, temporaryView, mk))
    {
        /*
         * Not all oids inside struct object were translated, so there is no
         * matching object in current view, we need to return null.
         */

        return nullptr;
    }

    std::string str_neighbor_entry = sai_serialize_neighbor_entry(mk.objectkey.key.neighbor_entry);

    /*
     * Now when we have serialized neighbor entry with temporary rif_if VID
     * replaced to current rif_id VID we can do dictionary lookup for neighbor.
     */

    auto currentNeighborIt = currentView.soNeighbors.find(str_neighbor_entry);

    if (currentNeighborIt == currentView.soNeighbors.end())
    {
        SWSS_LOG_DEBUG("unable to find neighbor entry %s in current asic view",
                str_neighbor_entry.c_str());

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

    SWSS_LOG_THROW("found neighbor entry %s in current view, but it status is %d, FATAL",
            str_neighbor_entry.c_str(),
            currentNeighborObj->getObjectStatus());
}

/**
 * @brief Find current best match for route.
 *
 * For Route we don't need to iterate via all current routes, we can do
 * dictionary lookup, but we need to do smart trick, since temporary object was
 * processed we just need to check whether VID in route_entry struct is
 * matched/final and it has RID assigned from current view. If, RID exists, we
 * can use that RID to get VID of current view, exchange in route_entry struct
 * and do dictionary lookup on serialized route_entry.
 *
 * With this approach for many entries this is the quickest possible way. In
 * case when RID doesn't exist, that means we have invalid route entry, so we
 * must return null.
 *
 * @param currentView Current view.
 * @param temporaryView Temporary view.
 * @param temporaryObj Temporary object.
 *
 * @return Best match object if found or nullptr.
 */
std::shared_ptr<SaiObj> findCurrentBestMatchForRouteEntry(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * Make a copy here to not destroy object data, later
     * on this data should be read only.
     */

    sai_object_meta_key_t mk = temporaryObj->meta_key;

    if (!exchangeTemporaryVidToCurrentVid(currentView, temporaryView, mk))
    {
        /*
         * Not all oids inside struct object were translated, so there is no
         * matching object in current view, we need to return null.
         */

        return nullptr;
    }

    std::string str_route_entry = sai_serialize_route_entry(mk.objectkey.key.route_entry);

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

    SWSS_LOG_THROW("found route entry %s in current view, but it status is %d, FATAL",
            str_route_entry.c_str(),
            currentRouteObj->getObjectStatus());
}

/**
 * @brief Find current best match for FDB.
 *
 * For FDB we don't need to iterate via all current FDBs, we can do dictionary
 * lookup, but we need to do smart trick, since temporary object was processed
 * we just need to check whether VID in fdb_entry struct is matched/final and
 * it has RID assigned from current view. If, RID exists, we can use that RID
 * to get VID of current view, exchange in fdb_entry struct and do dictionary
 * lookup on serialized fdb_entry.
 *
 * With this approach for many entries this is the quickest possible way. In
 * case when RID doesn't exist, that means we have invalid fdb entry, so we must
 * return null.
 *
 * @param currentView Current view.
 * @param temporaryView Temporary view.
 * @param temporaryObj Temporary object.
 *
 * @return Best match object if found or nullptr.
 */
std::shared_ptr<SaiObj> findCurrentBestMatchForFdbEntry(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * Make a copy here to not destroy object data, later
     * on this data should be read only.
     */

    sai_object_meta_key_t mk = temporaryObj->meta_key;

    if (!exchangeTemporaryVidToCurrentVid(currentView, temporaryView, mk))
    {
        /*
         * Not all oids inside struct object were translated, so there is no
         * matching object in current view, we need to return null.
         */

        return nullptr;
    }

    std::string str_fdb_entry = sai_serialize_fdb_entry(mk.objectkey.key.fdb_entry);

    /*
     * Now when we have serialized fdb entry with temporary VIDs
     * replaced to current VIDs we can do dictionary lookup for fdb.
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

    SWSS_LOG_THROW("found fdb entry %s in current view, but it status is %d, FATAL",
            str_fdb_entry.c_str(),
            currentFdbObj->getObjectStatus());
}

std::shared_ptr<SaiObj> findCurrentBestMatchForSwitch(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * On compare logic we checked that we have one switch
     * so we can just get first.
     */

    auto currentSwitches = currentView.getObjectsByObjectType(SAI_OBJECT_TYPE_SWITCH);

    if (currentSwitches.size() == 0)
    {
        SWSS_LOG_NOTICE("unable to find switch object in current view");

        return nullptr;
    }

    if (currentSwitches.size() > 1)
    {
        SWSS_LOG_THROW("found %zu switches declared in current view, not supported",
                currentSwitches.size());
    }

    /*
     * We found switch object in current view! Just one extra check
     * of object status if it's not processed yet.
     */

    auto currentSwitchObj = currentSwitches.at(0);

    if (currentSwitchObj->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        return currentSwitchObj;
    }

    /*
     * If we are here, that means this switch was already processed, which
     * can indicate a bug or somehow duplicated entries.
     */

    SWSS_LOG_THROW("found switch object %s in current view, but it status is %d, FATAL",
            currentSwitchObj->str_object_id.c_str(),
            currentSwitchObj->getObjectStatus());
}

std::shared_ptr<SaiObj> findCurrentBestMatch(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    if (temporaryObj->isOidObject() &&
            temporaryObj->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED)
    {
        /*
         * Object status is matched so current and temp VID are the same so we
         * can just take object directly.
         */

        SWSS_LOG_INFO("found best match for %s %s since object status is MATCHED",
                temporaryObj->str_object_type.c_str(),
                temporaryObj->str_object_id.c_str());

        return currentView.oOids.at(temporaryObj->getVid());
    }

    /*
     * NOTE: Later on best match find can be done based on present CREATE_ONLY
     * attributes or on most matched CREATE_AND_SET attributes.
     */

    switch (temporaryObj->getObjectType())
    {
        /*
         * Non object id cases
         */

        case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
            return findCurrentBestMatchForNeighborEntry(currentView, temporaryView, temporaryObj);

        case SAI_OBJECT_TYPE_ROUTE_ENTRY:
            return findCurrentBestMatchForRouteEntry(currentView, temporaryView, temporaryObj);

        case SAI_OBJECT_TYPE_FDB_ENTRY:
            return findCurrentBestMatchForFdbEntry(currentView, temporaryView, temporaryObj);

            /*
             * We can have special case for switch since we know there should
             * be only one switch.
             */

        case SAI_OBJECT_TYPE_SWITCH:
            return findCurrentBestMatchForSwitch(currentView, temporaryView, temporaryObj);

        default:

            if (!temporaryObj->isOidObject())
            {
                SWSS_LOG_THROW("object %s:%s is non object id, not handled yet, FIXME",
                        temporaryObj->str_object_type.c_str(),
                        temporaryObj->str_object_id.c_str());
            }

            /*
             * Here we support only object id object types.
             */

            return findCurrentBestMatchForGenericObject(currentView, temporaryView, temporaryObj);
    }
}

void processObjectForViewTransition(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> &temporayObj);

/**
 * @brief Process object attributes for view transition.
 *
 * Since each object can contain attributes (or non id struct object can
 * contain oids inside struct) they need to be processed first recursively and
 * they need to be in FINAL state before processing temporary view further.
 *
 * This processing may result in changes in current view like removing and
 * recreating or adding new objects depends on transitions.
 *
 * @param currentView Current view.
 * @param temporaryView Temporary view.
 * @param temporaryObj Temporary object.
 */
void procesObjectAttributesForViewTransition(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> &temporaryObj)
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

        /*
         * For each object id in attributes go recursively to match those
         * objects.
         */

        for (auto vid: attribute->getOidListFromAttribute())
        {
            if (vid == SAI_NULL_OBJECT_ID)
            {
                continue;
            }

            SWSS_LOG_INFO("- processing attr VID %s", sai_serialize_object_id(vid).c_str());

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

    if (temporaryObj->isOidObject())
    {
        return;
    }

    /*
     * For non object id types like NEIGHBOR or ROUTE they have object id
     * inside object struct entry, they also need to be processed, in SAI 1.0
     * this can be automated since we have metadata for those structs.
     */

    for (size_t j = 0; j < temporaryObj->info->structmemberscount; ++j)
    {
        const sai_struct_member_info_t *m = temporaryObj->info->structmembers[j];

        if (m->membervaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID)
        {
            continue;
        }

        sai_object_id_t vid = m->getoid(&temporaryObj->meta_key);

        SWSS_LOG_INFO("- processing %s (%s) VID %s",
                temporaryObj->str_object_type.c_str(),
                m->membername,
                sai_serialize_object_id(vid).c_str());

        auto structObject = temporaryView.oOids.at(vid);

        processObjectForViewTransition(currentView, temporaryView, structObject); // recursion
    }
}

void bringNonRemovableObjectToDefaultState(
        _In_ AsicView &currentView,
        _In_ const std::shared_ptr<SaiObj> &currentObj)
{
    SWSS_LOG_ENTER();

    for (const auto &it: currentObj->getAllAttributes())
    {
        const auto &attr = it.second;

        const auto &meta = attr->getAttrMetadata();

        if (!SAI_HAS_FLAG_CREATE_AND_SET(meta->flags))
        {
            SWSS_LOG_THROW("attribute %s is not CREATE_AND_SET, bug?", meta->attridname);
        }

        if (meta->defaultvaluetype == SAI_DEFAULT_VALUE_TYPE_NONE)
        {
            SWSS_LOG_THROW("attribute %s default value type is NONE, bug?", meta->attridname);
        }

        auto defaultValueAttr = getSaiAttrFromDefaultValue(currentView, *meta);

        if (defaultValueAttr == nullptr)
        {
            SWSS_LOG_THROW("Can't get default value for present current attr %s:%s, FIXME",
                    meta->attridname,
                    attr->getStrAttrValue().c_str());
        }

        if (attr->getStrAttrValue() == defaultValueAttr->getStrAttrValue())
        {
            /*
             * If current value is the same as default value no need to
             * generate ASIC update command.
             */

            continue;
        }

        currentView.asicSetAttribute(currentObj, defaultValueAttr);
    }

    currentObj->setObjectStatus(SAI_OBJECT_STATUS_FINAL);

    /*
     * TODO Revisit.
     *
     * Problem here is that default trap group is used and we build references
     * for it, so it will not be "removed" when processing, since it's default
     * object so it will not be processed, and also it can't be removed since
     * it's default object.
     */
}

/**
 * @brief Indicates whether object can be removed.
 *
 * This method should be used on oid objects, all non oid objects (like route,
 * neighbor, etc.) can be safely removed.
 *
 * @param currentObj Current object to be examined.
 *
 * @return True if object can be removed, false otherwise.
 */
bool isNonRemovableObject(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &currentObj)
{
    SWSS_LOG_ENTER();

    if (currentObj->isOidObject())
    {
        sai_object_id_t rid = currentView.vidToRid.at(currentObj->getVid());

        // XXX we have only 1 switch, so we can get away with this

        auto sw = switches.begin()->second;

        return sw->isNonRemovableRid(rid);
    }

    /*
     * Object is non object id, like ROUTE_ENTRY etc, can be removed.
     */

    return false;
}

void removeExistingObjectFromCurrentView(
        _In_ AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> &currentObj)
{
    SWSS_LOG_ENTER();

    /*
     * This decreasing VID reference will be hard when actual reference will
     * be one of default objects like CPU or default trap group when we "bring
     * default value" or maybe in oid case we will not bring default values and
     * just remove and recreate.
     */

    if (currentObj->isOidObject())
    {
        int count = currentView.getVidReferenceCount(currentObj->getVid());

        if (count != 0)
        {
            /*
             * If references count is not zero, we need to remove child
             * first for that we need dependency tree, not supported yet.
             *
             * NOTE: Object can be non removable and can have reference on it.
             */

            SWSS_LOG_THROW("can't remove existing object %s:%s since reference count is %d, FIXME",
                    currentObj->str_object_type.c_str(),
                    currentObj->str_object_id.c_str(),
                    count);
        }
    }

    /*
     * If some object can't be removed from current view and it's missing from
     * temp, then it needs to be transferred to temp as well and bring to
     * default values.
     *
     * First we need to check if object can be removed, like port's cant be
     * removed, vlan 1, queues, ingress pg etc.
     *
     * Remove for those objects is not supported now lets bring back
     * object to default state.
     *
     * For oid values we can figure out if we would have list of
     * defaults which can be removed and which can't and use list same
     * for queues, schedulers etc.
     *
     * We could use switch here and non created objects to make this simpler.
     */


    if (currentObj->getObjectType() == SAI_OBJECT_TYPE_SWITCH)
    {
        /*
         * Remove switch is much simpler, we just need check if switch doesn't
         * exist in temporary view, and we can just remove it. In here when
         * attributes don't match it's much complicated.
         */

        SWSS_LOG_THROW("switch can't be removed from current view, not supported yet");
    }

    if (isNonRemovableObject(currentView, temporaryView, currentObj))
    {
        bringNonRemovableObjectToDefaultState(currentView, currentObj);
    }
    else
    {
        /*
         * Asic remove object is decreasing reference count on non object ID if
         * current object is non object id, release existing links only looks
         * into attributes.
         */

        currentView.asicRemoveObject(currentObj);

        currentObj->setObjectStatus(SAI_OBJECT_STATUS_REMOVED);
    }
}

sai_object_id_t translateTemporaryVidToCurrentVid(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ sai_object_id_t tvid)
{
    SWSS_LOG_ENTER();

    /*
     * This method is used to translate temporary VID to current VID using RID
     * which should be present in both views. If RID doesn't exist, then we
     * check whether object was created if so, then we return temporary VID
     * instead of creating new current VID and we don't need to track mapping
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
            SWSS_LOG_THROW("temporary VID %s not found in temporary view",
                    sai_serialize_object_id(tvid).c_str());
        }

        const auto &tempObj = tempIt->second;

        if (tempObj->createdObject)
        {
            SWSS_LOG_DEBUG("translated temp VID %s to current, since object was created",
                    sai_serialize_object_id(tvid).c_str());

            return tvid;
        }

        SWSS_LOG_THROW("VID %s was not found in temporary view, was object created? FIXME",
                sai_serialize_object_id(tvid).c_str());
    }

    sai_object_id_t rid = temporaryIt->second;

    auto currentIt = currentView.ridToVid.find(rid);

    if (currentIt == currentView.ridToVid.end())
    {
        SWSS_LOG_THROW("RID %s was not found in current view",
                sai_serialize_object_id(rid).c_str());
    }

    sai_object_id_t cvid = currentIt->second;

    SWSS_LOG_DEBUG("translated temp VID %s using RID %s to current VID %s",
            sai_serialize_object_id(tvid).c_str(),
            sai_serialize_object_id(rid).c_str(),
            sai_serialize_object_id(cvid).c_str());

    return cvid;
}

std::shared_ptr<SaiAttr> translateTemporaryVidsToCurrentVids(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> &currentObj,
        _In_ const std::shared_ptr<SaiAttr> &inattr)
{
    SWSS_LOG_ENTER();

    /*
     * We are creating copy here since we will modify contents of that
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
     * We also need simpler version that will translate simple VID for
     * oids present inside non object id structs.
     */

    uint32_t count = 0;

    sai_object_id_t *objectIdList = NULL;

    auto &at = *attr->getRWSaiAttr();

    switch (attr->getAttrMetadata()->attrvaluetype)
    {
        case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
            count = 1;
            objectIdList = &at.value.oid;
            break;

        case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
            count = at.value.objlist.count;
            objectIdList = at.value.objlist.list;
            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:

            if (at.value.aclfield.enable)
            {
                count = 1;
                objectIdList = &at.value.aclfield.data.oid;
            }

            break;

        case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:

            if (at.value.aclfield.enable)
            {
                count = at.value.aclfield.data.objlist.count;
                objectIdList = at.value.aclfield.data.objlist.list;
            }

            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:

            if (at.value.aclaction.enable)
            {
                count = 1;
                objectIdList = &at.value.aclaction.parameter.oid;
            }

            break;

        case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:

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
     * exchanged, we need to update value string of that attribute.
     */

    attr->UpdateValue();

    return attr;
}

/**
 * @brief Set attribute on current object
 *
 * This function will set given attribute to current object adding new
 * attribute or replacing existing one. Given attribute can be either one
 * temporary attribute missing or different from current object object or
 * default attribute value if we need to bring some attribute to default value.
 *
 * @param currentView Current view.
 * @param temporaryView Temporary view.
 * @param currentObj Current object on which we set attribute.
 * @param inattr Attribute to be set on current object.
 */
void setAttributeOnCurrentObject(
        _In_ AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> &currentObj,
        _In_ const std::shared_ptr<SaiAttr> &inattr)
{
    SWSS_LOG_ENTER();

    /*
     * At the beginning just small assert to check if object is create and set.
     */

    const auto meta = inattr->getAttrMetadata();

    if (!SAI_HAS_FLAG_CREATE_AND_SET(meta->flags))
    {
        SWSS_LOG_THROW("can't set attribute %s on current object %s:%s since it's not CREATE_AND_SET",
                meta->attridname,
                currentObj->str_object_type.c_str(),
                currentObj->str_object_id.c_str());
    }

    /*
     * NOTE: Since attribute we SET can be OID attribute, and that can be a VID
     * or list of VIDs from temporary view. So after this "set" operation we
     * will end up with MIXED VIDs on current view, some object will contain
     * VIDs from temporary view, and this can lead to fail in
     * translate_vid_to_rid since we will need to look inside two views to find
     * right RID.
     *
     * We need a flag to say that VID is for object that was created
     * and we can mix only those VID's for objects that was created.
     *
     * In here we need to translate attribute VIDs to current VIDs to keep
     * track in dependency tree, since if we use temporary VID that will point
     * to the same RID then we will lost track in dependency tree of that VID
     * reference count.
     *
     * This should also done for object id's inside non object ids.
     */

    std::shared_ptr<SaiAttr> attr = translateTemporaryVidsToCurrentVids(currentView, temporaryView, currentObj, inattr);

    currentView.asicSetAttribute(currentObj, attr);
}

/**
 * @brief Create new object from temporary object.
 *
 * Since best match was not found we need to create a brand new object and put
 * it into current view as well.
 *
 * @param currentView Current view.
 * @param temporaryView Temporary view.
 * @param temporaryObj Temporary object to be cloned to current view.
 */
void createNewObjectFromTemporaryObject(
        _In_ AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("creating object %s:%s",
                    temporaryObj->str_object_type.c_str(),
                    temporaryObj->str_object_id.c_str());

    /*
     * This trap can be default trap group, so let's check if it's, then we
     * can't create it, we need to set new attributes if that possible. But
     * this case should not happen since on syncd start we are putting default
     * trap group to asic view, so if user will query default one, they will be
     * matched by RID.
     *
     * Default trap group is transferred to view on init if it doesn't exist.
     *
     * There should be no such action here, since this scenario would mean that
     * there is default switch object in temporary view, but not in current
     * view. All default objects are put to ASIC state table when switch is
     * created. So what can happen is opposite scenario.
     */

    /*
     * NOTE: Since attributes we pass to create can be OID attributes, and that
     * can be a VID or list of VIDs from temporary view. So after this create
     * operation we will end up with MIXED VIDs on current view, some object
     * will contain VIDs from temporary view, and this can lead to fail in
     * translate_vid_to_rid since we will need to look inside two views to find
     * right RID.
     *
     * We need a flag to say that VID is for object that was created and we can
     * mix only those VID's for objects that was created.
     *
     * In here we need to translate attribute VIDs to current VIDs to keep
     * track in dependency tree, since if we use temporary VID that will point
     * to the same RID then we will lost track in dependency tree of that VID
     * reference count.
     *
     * This should also be done for object id's inside non object ids.
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
    currentObj->str_object_id    = temporaryObj->str_object_id;      // temporary VID / non object id
    currentObj->meta_key         = temporaryObj->meta_key;           // temporary VID / non object id
    currentObj->info             = temporaryObj->info;

    if (!temporaryObj->isOidObject())
    {
        /*
         * Since object contains OID inside struct, this OID may already exist
         * in current view, so we need to do translation here.
         */

        for (size_t j = 0; j < currentObj->info->structmemberscount; ++j)
        {
            const sai_struct_member_info_t *m = currentObj->info->structmembers[j];

            if (m->membervaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID)
            {
                continue;
            }

            sai_object_id_t vid = m->getoid(&currentObj->meta_key);

            vid = translateTemporaryVidToCurrentVid(currentView, temporaryView, vid);

            m->setoid(&currentObj->meta_key, vid);

            /*
             * Bind new vid reference is already done inside asicCreateObject.
             */
        }

        /*
         * Since it's possible that object id may had been changed, we need to
         * update string as well.
         */

        switch (temporaryObj->getObjectType())
        {
            case SAI_OBJECT_TYPE_ROUTE_ENTRY:

                currentObj->str_object_id = sai_serialize_route_entry(currentObj->meta_key.objectkey.key.route_entry);
                break;

            case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:

                currentObj->str_object_id = sai_serialize_neighbor_entry(currentObj->meta_key.objectkey.key.neighbor_entry);
                break;

            case SAI_OBJECT_TYPE_FDB_ENTRY:
                currentObj->str_object_id = sai_serialize_fdb_entry(currentObj->meta_key.objectkey.key.fdb_entry);
                break;

            default:

                SWSS_LOG_THROW("unexpected non object id type: %s",
                        sai_serialize_object_type(temporaryObj->getObjectType()).c_str());
        }
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
     * Asic create object inserts new reference to track inside asic view.
     */

    currentView.asicCreateObject(currentObj);

    /*
     * Move both object status to FINAL since both objects were processed
     * successfully and object was created.
     */

    currentObj->setObjectStatus(SAI_OBJECT_STATUS_FINAL);
    temporaryObj->setObjectStatus(SAI_OBJECT_STATUS_FINAL);
}

void UpdateObjectStatus(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> &currentBestMatch,
        _In_ const std::shared_ptr<SaiObj> &temporaryObj)
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

        if (temporaryObj->isOidObject())
        {
            /*
             * When objects are object id type, we need to update map in
             * temporary view for correct VID/RID.
             *
             * In here RID should always exists, since when objects are MATCHED
             * then RID exists and both VID's are the same, and when both
             * objects are not processed, then RID also exists since current
             * object was selected as current best match. Other options are
             * object was removed, but the it could not be selected, or object
             * was created, but then is in FINAL state so also could not be
             * selected here.
             */

            sai_object_id_t tvid = temporaryObj->getVid();
            sai_object_id_t cvid = currentBestMatch->getVid();
            sai_object_id_t rid = currentView.vidToRid.at(cvid);

            SWSS_LOG_INFO("remapped current VID %s to temp VID %s using RID %s",
                    sai_serialize_object_id(cvid).c_str(),
                    sai_serialize_object_id(tvid).c_str(),
                    sai_serialize_object_id(rid).c_str());

            temporaryView.ridToVid[rid] = tvid;
            temporaryView.vidToRid[tvid] = rid;

            /*
             * TODO: Set new VID if it doesn't exist in current view with NULL RID
             * that will mean we created new object, this VID will be later
             * used to count references and as a sanity check if we are
             * increasing valid reference.
             *
             * Revisit. We have create flag now.
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
         * or NON_PROCESSED, other combinations are not supported since other
         * actions will be either create new object from temporary object or
         * remove existing current object.
         */

        SWSS_LOG_THROW("unexpected status combination: current %d, temporary %d",
                currentBestMatch->getObjectStatus(),
                temporaryObj->getObjectStatus());
    }
}

std::shared_ptr<SaiAttr> getSaiAttrFromDefaultValue(
        _In_ const AsicView &currentView,
        _In_ const sai_attr_metadata_t &meta)
{
    SWSS_LOG_ENTER();

    /*
     * Worth notice, that this is only helper, since metadata on attributes
     * tell default value for example for oid object as SAI_NULL_OBJECT_ID but
     * maybe on the switch vendor actually assigned some value, so default
     * value will not be NULL after creation.
     *
     * We can check that by using SAI discovery.
     *
     * TODO Default value also must depend on dependency tree !
     * This will be tricky, we need to revisit that !
     */

    if (meta.objecttype == SAI_OBJECT_TYPE_SWITCH &&
            meta.attrid == SAI_SWITCH_ATTR_SRC_MAC_ADDRESS)
    {
        /*
         * Same will apply for default values which are pointing to
         * different attributes.
         *
         * Default value is stored in SaiSwitch class.
         */

        // XXX we have only 1 switch, so we can get away with this

        auto sw = switches.begin()->second;

        sai_attribute_t attr;

        memset(&attr, 0, sizeof(sai_attribute_t));

        attr.id = meta.attrid;

        sw->getDefaultMacAddress(attr.value.mac);

        std::string str_attr_value = sai_serialize_attr_value(meta, attr, false);

        SWSS_LOG_NOTICE("bringing default %s", meta.attridname);

        return std::make_shared<SaiAttr>(meta.attridname, str_attr_value);
    }

    /*
     * Move this method to asicview class.
     */

    switch (meta.defaultvaluetype)
    {
        case SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST:

            {
                /*
                 * For empty list we can use trick here, just set attr to all
                 * zeros, then all count's will be zeros and all lists pointers
                 * will be NULL.
                 *
                 * TODO: If this is executed on existing attribute then we need
                 * Dependency tree since initially that list may not be empty!
                 */

                sai_attribute_t attr;

                memset(&attr, 0, sizeof(sai_attribute_t));

                attr.id = meta.attrid;

                std::string str_attr_value = sai_serialize_attr_value(meta, attr, false);

                return std::make_shared<SaiAttr>(meta.attridname, str_attr_value);
            }


        case SAI_DEFAULT_VALUE_TYPE_CONST:

            /*
             * Only primitives can be supported on CONST.
             */

            switch (meta.attrvaluetype)
            {
                case SAI_ATTR_VALUE_TYPE_BOOL:
                case SAI_ATTR_VALUE_TYPE_UINT8:
                case SAI_ATTR_VALUE_TYPE_INT8:
                case SAI_ATTR_VALUE_TYPE_UINT16:
                case SAI_ATTR_VALUE_TYPE_INT16:
                case SAI_ATTR_VALUE_TYPE_UINT32:
                case SAI_ATTR_VALUE_TYPE_INT32:
                case SAI_ATTR_VALUE_TYPE_UINT64:
                case SAI_ATTR_VALUE_TYPE_INT64:
                case SAI_ATTR_VALUE_TYPE_POINTER:
                case SAI_ATTR_VALUE_TYPE_OBJECT_ID:

                    {
                        sai_attribute_t attr;

                        attr.id = meta.attrid;
                        attr.value = *meta.defaultvalue;

                        std::string str_attr_value = sai_serialize_attr_value(meta, attr, false);

                        return std::make_shared<SaiAttr>(meta.attridname, str_attr_value);
                    }

                default:

                    SWSS_LOG_ERROR("serialization type %s is not supported yet, FIXME",
                            sai_serialize_attr_value_type(meta.attrvaluetype).c_str());
                    break;
            }

            /*
             * NOTE: default for acl flags or action is disabled.
             */

            break;

        case SAI_DEFAULT_VALUE_TYPE_ATTR_VALUE:

            /*
             * TODO normally we need check default object type and value but
             * this is only available in metadata sai 1.0.
             *
             * We need to look at attrvalue in metadata and object type if it's switch.
             * then get it from switch
             *
             * And all those values we should keep in double map object type
             * and attribute id, and auto select from attr value.
             */

            // TODO double check that, to make it generic (check attr value type is oid)
            if (meta.objecttype == SAI_OBJECT_TYPE_HOSTIF_TRAP && meta.attrid == SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP)
            {
                /*
                 * Default trap group is set on traps by default, so to bring them to
                 * default state we need to explicitly set this.
                 */

                SWSS_LOG_NOTICE("bring default trap group on %s", meta.attridname);

                const auto &tg = currentView.ridToVid.find(currentView.defaultTrapGroupRid);

                if (tg == currentView.ridToVid.end())
                {
                    SWSS_LOG_THROW("default trap group RID 0x%lx doesn't exist in current view", currentView.defaultTrapGroupRid);
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

            return nullptr;

        case SAI_DEFAULT_VALUE_TYPE_NONE:

            /*
             * No default value present.
             */

            return nullptr;

        default:

            SWSS_LOG_ERROR("default value type %d is not supported yet for %s, FIXME",
                    meta.defaultvaluetype,
                    meta.attridname);

            return nullptr;
    }

    return nullptr;
}

bool performObjectSetTransition(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> &currentBestMatch,
        _In_ const std::shared_ptr<const SaiObj> &temporaryObj,
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
     * first pass was successful second pass when flag is set to true, actual
     * SET operations will be generated and current view will be modified.  No
     * actual ASIC operations will be performed, all ASIC changes will be done
     * after all object will be moved to final state.
     */

    /*
     * XXX If objects are matched (same vid/rid on object id) then this
     * function must return true, just skip all create only attributes.
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

            if (SAI_HAS_FLAG_CREATE_AND_SET(meta->flags))
            {
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

            if (currentBestMatch->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED)
            {
                /*
                 * This should not happen, since this mean, that attribute is
                 * create only, object is matched, and attribute value is
                 * different! DB is broken?
                 */

                SWSS_LOG_THROW("Attr %s CAN'T be updated from %s to %s on VID %s when MATCHED and CREATE_ONLY, FATAL",
                        meta->attridname,
                        currentAttr->getStrAttrValue().c_str(),
                        temporaryAttr->getStrAttrValue().c_str(),
                        temporaryObj->str_object_id.c_str());
            }

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

        bool conditional = meta->isconditional;

        /*
         * If attribute is CREATE_AND_SET and not conditional then it's
         * safe to make SET operation.
         *
         * XXX previously we had (meta->flags == SAI_ATTR_FLAGS_CREATE_AND_SET)
         * If it's not conditional current SAI_HAS_FLAG should not matter. But it
         * can also be mandatory on create but this does not matter since if
         * it's mandatory on create then current object already exists co we can
         * still perform update on this attribute because it was passed during
         * creation.
         */

        if (SAI_HAS_FLAG_CREATE_AND_SET(meta->flags) && !conditional)
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

        if (currentBestMatch->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED)
        {
            if (SAI_HAS_FLAG_CREATE_ONLY(meta->flags))
            {
                /*
                 * Attribute is create only attribute on matched object. This
                 * can happen when we are have create only attributes in asic
                 * view, those attributes were put by snoop logic. Since we
                 * skipping only read-only attributes then we snoop create-only
                 * also, but on "existing" objects this will cause problem and
                 * during apply logic we need to skip this attribute since we
                 * won't be able to SET it anyway on matched object, and value
                 * is the same as current object.
                 */

                SWSS_LOG_INFO("Skipping create only attr on matched object: %s:%s",
                        meta->attridname,
                        temporaryAttr->getStrAttrValue().c_str());

                continue;
            }
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

        const auto &meta = currentAttr->getAttrMetadata();

        const sai_attribute_t &attr = *currentAttr->getSaiAttr();

        if (processedAttributes.find(attr.id) != processedAttributes.end())
        {
            /*
             * This attribute was processed in previous temporary attributes processing so skip it here.
             */

            continue;
        }

        SWSS_LOG_INFO("first pass (curr): attr %s", currentAttr->getStrAttrId().c_str());

        /*
         * Check if we are bringing one of the default created objects to
         * default value, since for that we will need dependency TREE.  Most of
         * the time user should just query configuration and just set new
         * values based on get results, so this will not be needed.
         *
         * And even if we will have dependency tree, those values may not be
         * synced because of remove etc, so we will need to check if default
         * values actually exists.
         */

        if (currentBestMatch->isOidObject())
        {
            sai_object_id_t vid = currentBestMatch->getVid();

            /*
             * Current best match may be created, check if vid/rid exist.
             */

            if (currentView.hasVid(vid))
            {
                sai_object_id_t rid = currentView.vidToRid.at(vid);

                // XXX we have only 1 switch, so we can get away with this

                auto sw = switches.begin()->second;

                if (sw->isDiscoveredRid(rid))
                {
                    SWSS_LOG_INFO("performing default on existing object VID %s: %s: %s, we need default dependency TREE, FIXME",
                            sai_serialize_object_id(vid).c_str(),
                            meta->attridname,
                            currentAttr->getStrAttrValue().c_str());
                }
            }
        }

        /*
         * We should not have MANDATORY_ON_CREATE attributes here since all
         * mandatory on create (even conditional) should be present in in
         * previous loop and they are matching, so we should get here
         * CREATE_ONLY or CREATE_AND_SET attributes only. So we should not get
         * conditional attributes here also, but lets take extra care about that
         * just as sanity check.
         */

        bool conditional = meta->isconditional;

        if (conditional || SAI_HAS_FLAG_MANDATORY_ON_CREATE(meta->flags))
        {
            if (currentBestMatch->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED &&
                    SAI_HAS_FLAG_CREATE_AND_SET(meta->flags))
            {
                if (meta->objecttype == SAI_OBJECT_TYPE_PORT &&
                        meta->attrid == SAI_PORT_ATTR_SPEED)
                {
                    /*
                     * NOTE: for SPEED we could query each port at start and
                     * save it's speed to recover here, or even we could query
                     * each attribute on existing object during discovery
                     * process.
                     */

                    SWSS_LOG_WARN("No previous value specified on %s (VID), can't bring to default, leaving attr unchanged: %s:%s",
                            sai_serialize_object_id(currentBestMatch->getVid()).c_str(),
                            meta->attridname,
                            currentAttr->getStrAttrValue().c_str());

                    continue;
                }

                if (meta->objecttype == SAI_OBJECT_TYPE_SCHEDULER_GROUP &&
                        meta->attrid == SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID)
                {
                    /*
                     * This attribute can hold reference to user created
                     * objects which maybe required to be destroyed, that's why
                     * we need to bring real value. What if real value were
                     * removed?
                     */

                    sai_object_id_t def = SAI_NULL_OBJECT_ID;

                    auto sw = switches.begin()->second;

                    sai_object_id_t vid = currentBestMatch->getVid();

                    if (currentView.hasVid(vid))
                    {
                        // scheduler_group RID
                        sai_object_id_t rid = currentView.vidToRid.at(vid);

                        rid = sw->getDefaultValueForOidAttr(rid, SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID);

                        if (rid != SAI_NULL_OBJECT_ID && currentView.hasRid(rid))
                        {
                            /*
                             * We found default value
                             */

                            SWSS_LOG_DEBUG("found default rid %s, vid %s for %s",
                                    sai_serialize_object_id(rid).c_str(),
                                    sai_serialize_object_id(vid).c_str(),
                                    meta->attridname);

                            def = currentView.ridToVid.at(rid);
                        }
                    }

                    sai_attribute_t defattr;

                    defattr.id = meta->attrid;
                    defattr.value.oid = def;

                    std::string str_attr_value = sai_serialize_attr_value(*meta, defattr, false);

                    auto defaultValueAttr = std::make_shared<SaiAttr>(meta->attridname, str_attr_value);

                    if (performTransition)
                    {
                        setAttributeOnCurrentObject(currentView, temporaryView, currentBestMatch, defaultValueAttr);
                    }

                    continue;
                }

               // SAI_QUEUE_ATTR_PARENT_SCHEDULER_NODE
               // SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID*
               // SAI_SCHEDULER_GROUP_ATTR_PARENT_NODE
               // SAI_BRIDGE_PORT_ATTR_BRIDGE_ID
               //
               // TODO matched by ID (MATCHED state) should always be updatable
               // except those 4 above (at least for those above since they can have
               // default value present after switch creation

                // TODO SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID is mandatory on create but also SET
                // if attribute is set we and object is in MATCHED state then that means we are able to
                // bring this attribute to default state not for all attributes!
                // *SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID - is not any more mandatory on create, so default should be NULL

                SWSS_LOG_ERROR("current attribute is mandatory on create, crate and set, and object MATCHED, FIXME %s %s:%s",
                        currentBestMatch->str_object_id.c_str(),
                        meta->attridname,
                        currentAttr->getStrAttrValue().c_str());

                return false;
            }

            if (currentBestMatch->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED)
            {
                if (SAI_HAS_FLAG_CREATE_ONLY(meta->flags))
                {
                    /*
                     * Attribute is create only attribute on matched object. This
                     * can happen when we are have create only attributes in asic
                     * view, those attributes were put by snoop logic. Since we
                     * skipping only read-only attributes then we snoop create-only
                     * also, but on "existing" objects this will cause problem and
                     * during apply logic we need to skip this attribute since we
                     * won't be able to SET it anyway on matched object, and value
                     * is the same as current object.
                     */

                    SWSS_LOG_INFO("Skipping create only attr on matched object: %s:%s",
                            meta->attridname,
                            currentAttr->getStrAttrValue().c_str());

                    continue;
                }
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
         * Later this comparison of default value we need to extract to
         * separate functions. Maybe create SaiAttr from default value or
         * nullptr if it's not supported yet.
         */

        if (meta->flags == SAI_ATTR_FLAGS_CREATE_AND_SET || meta->flags == SAI_ATTR_FLAGS_CREATE_ONLY)
        {
            // TODO default value for existing objects needs dependency tree

            const auto defaultValueAttr = getSaiAttrFromDefaultValue(currentView, *meta);

            if (defaultValueAttr == nullptr)
            {
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

        SWSS_LOG_THROW("we should not get here, we have a bug, current present attribute %s:%s has some wrong flags 0x%x",
                    meta->attridname,
                    currentAttr->getStrAttrValue().c_str(),
                    meta->flags);
    }

    /*
     * All attributes were processed, and ether no changes are required or all
     * changes can be performed or some missing attributes has exact value as
     * default value.
     */

    return true;
}

/**
 * @brief Process SAI object for ASIC view transition
 *
 * Purpose of this function is to find matching SAI object in current view
 * corresponding to new temporary view for which we want to make switch current
 * ASIC configuration.
 *
 * This function is recursive since it checks all object attributes including
 * attributes that contain other objects which at this stage may not be
 * processed yet.
 *
 * Processing may result in different actions:
 *
 * - no action is taken if objects are the same
 * - update existing object for new attributes if possible
 * - remove current object and create new object if updating current attributes
 *   is not possible or best matching object was not fount in current view
 *
 * All those actions will be generated "in memory" no actual SAI ASIC
 * operations will be performed at this stage.  After entire object dependency
 * graph will be processed and consistent, list of generated actions will be
 * executed on actual ASIC.  This approach is safer than making changes right
 * away since if some object is not supported we will return return but ASIC
 * still will be in consistent state.
 *
 * NOTE: Development is in progress, not all corner cases are supported yet.
 *
 * @param currentView Current view.
 * @param temporaryView Temporary view.
 * @param temporaryObj Temporary object to be processed.
 */
void processObjectForViewTransition(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> &temporaryObj)
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

    SWSS_LOG_DEBUG("processing: %s:%s", temporaryObj->str_object_type.c_str(), temporaryObj->str_object_id.c_str());

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
     * be different then we can't just create temporary object because of key
     * will match and we have conflict, so we need first in this case remove
     * previous object with key
     *
     * so there are 2 options here:
     *
     * - either our finding best match function will always include matching
     *   keys (if more keys, they all should (or any?) match should be right
     *   away taken) and this should be only object returned on list possible
     *   candidates as best match then below logic should figure out that we
     *   cant do "set" and we need to destroy object and destroy it now, and
     *   create temporary later this means that finding best logic can return
     *   objects that are not upgradable "cant do set"
     *
     * - or second solution, in this case, finding best match will not include
     *   this object on list, since other create only attribute will be
     *   different which also means we need to destroy and recreate, but in
     *   this case finding best match will figure this out and will destroy
     *   object on the way since it will know that set transition is not
     *   possible so here when we find best match (other object) in that case
     *   we always will be able to do "SET", or when no object will be returned
     *   then we can just right away create this object since all keys objects
     *   were removed by finding best match
     *
     * In both cases logic is the same, someone needs to figure out whether
     * updating object and set is possible or whether we leave object alone, or
     * we delete it before creating new one object.
     *
     * Preferably this logic could be in both but that duplicates compare
     * actions i would rather want this in finding best match, but that would
     * modify current view which i don't like much and here it seems like to
     * much hassle since after finding best match here we would have simple
     * operations.
     *
     * Currently KEY are in: vlan, queue, port, hostif_trap
     * - port - hw lane, we don't need to worry since we match them all
     * - queue - all other attributes can be set
     * - vlan - all other attributes can be set
     * - hostif_trap - all other attributes can be set
     *
     * We could add a check for that in sanity.
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
     * attribute will modify current object like SET operation, but second
     * attribute will not be able to do update because of CREATE_ONLY etc, then
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
     * NOTE: this function is called twice if first time will be successful
     * then logs will be doubled in syslog.
     */

    bool passed = performObjectSetTransition(currentView, temporaryView, currentBestMatch, temporaryObj, false);

    if (!passed)
    {
        if (temporaryObj->getObjectStatus() == SAI_OBJECT_STATUS_MATCHED)
        {
            /*
             * If object status is MATCHED, then we have the same object in
             * current view and temporary view, so it must be possible to make
             * transition to temporary view, hence for MATCHED objects
             * performObjectSetTransition must pass. There can be some corner
             * cases like existing objects after switch create and their attributes
             * that are OID set to existing objects (like QUEUES).
             */

            SWSS_LOG_THROW("performObjectSetTransition on MATCHED object (%s) FAILED! bug?",
                    temporaryObj->str_object_id.c_str());
        }

        /*
         * First pass was a failure, so we can't update existing object with
         * temporary one, probably because of CRATE_ONLY attributes.
         *
         * Our strategy here will be to leave untouched current object (second
         * strategy will be to remove it and it's children if link can't be
         * broken, Objects with non id like ROUTE FDB NEIGHBOR they need to be
         * removed first anyway, since we got best match, and for those best
         * match is via struct elements so we can't create next route with the
         * same elements in the struct.
         *
         * Then we create new object from temporary one and set it state to
         * final.
         */

        if (temporaryObj->getObjectType() == SAI_OBJECT_TYPE_SWITCH)
        {
            SWSS_LOG_THROW("we should not get switch object here, bug?");
        }

        if (!temporaryObj->isOidObject())
        {
            removeExistingObjectFromCurrentView(currentView, temporaryView, currentBestMatch);
        }
        else
        {
            /*
             * Later on if we decide we want to remove objects before
             * creating new one's we need to put here this action, or just
             * remove this entire switch and call remove.
             */
        }

        // No need to store VID since at this point we don't have RID yet, it will be
        // created on execute asic and RID will be saved to maps in both views.

        createNewObjectFromTemporaryObject(currentView, temporaryView, temporaryObj);

        return;
    }

    /*
     * First pass was successful, so we can do update on current object, lets do that now!
     */

    if (temporaryObj->isOidObject() && (temporaryObj->getObjectStatus() != SAI_OBJECT_STATUS_MATCHED))
    {
        /*
         * We track all oid object references, and since this object was
         * matched we need to insert it's vid to reference map since when we
         * will update attributes in current view from temporary view
         * attributes, then this object must be in reference map. This is
         * sanity check. Object can't be in matched state since matched objects
         * have the same RID and VID in both views, so VID already exists in
         * reference map.
         *
         * This object VID will be remapped inside UpdateObjectStatus to
         * rid/vid map since if we are here then we matched one of current
         * objects to this temporary object and update can be performed.
         */

         /*
          * TODO: This is temporary VID, it's not available in current view so
          * we need to remove this code here, since we will translate this VID
          * in temporary view to current view.
          */

        sai_object_id_t vid = temporaryObj->getVid();

        currentView.insertNewVidReference(vid);

        // save temporary vid to rid after match
        // can't be here since vim to rid map will not match
        // currentView.vidToRid[vid] = currentView.vidToRid.at(currentBestMatch->getVid());
    }

    performObjectSetTransition(currentView, temporaryView, currentBestMatch, temporaryObj, true);

    /*
     * Since we got here, that means matching or updating current best match
     * object was successful, and we can now set their state to final.
     */

    UpdateObjectStatus(currentView, temporaryView, currentBestMatch, temporaryObj);
}

void checkSwitch(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView)
{
    SWSS_LOG_ENTER();

    auto csws = currentView.getObjectsByObjectType(SAI_OBJECT_TYPE_SWITCH);
    auto tsws = temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_SWITCH);

    size_t csize = csws.size();
    size_t tsize = tsws.size();

    if (csize == 0 && tsize == 0)
    {
        return;
    }

    if (csize == 1 && tsize == 1)
    {
        /*
         * VID on both switches must match.
         */

        auto csw = csws.at(0);
        auto tsw = tsws.at(0);

        if (csw->getVid() != tsw->getVid())
        {
            SWSS_LOG_THROW("Current switch VID %s is different than temporary VID %s",
                    sai_serialize_object_id(csw->getVid()).c_str(),
                    sai_serialize_object_id(tsw->getVid()).c_str());
        }

        /*
         * TODO: We need to check create only attributes (hardware info), if
         * they differ then throw since update will not be possible, we can
         * separate this to different case to remove existing switch and
         * recreate new one check switches and VID matching if it's the same.
         */

        return;
    }

    /*
     * Current logic supports only 1 instance of switch so in both views we
     * should have only 1 instance of switch and they must match.
     */

    SWSS_LOG_THROW("unsupported number of switches: current %zu, temp %zu", csize, tsize);
}

// TODO this needs to be done in generic way for all
// default OID values set to attrvalue
void bringDefaultTrapGroupToFinalState(
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

    const auto &dtgObj = currentView.oOids.at(vid);

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

    bringNonRemovableObjectToDefaultState(currentView, dtgObj);
}

void applyViewTransition(
        _In_ AsicView &current,
        _In_ AsicView &temp)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("comparison logic");

    checkSwitch(current, temp);

    checkMatchedPorts(temp);

    /*
     * Process all objects
     */

    /*
     * During iteration no object from temp view are removed, so no need to
     * worry about any iterator issues here since removed objects are only from
     * current view.
     *
     * Order here can't be random, since it can mean that we will be adding
     * routes here, and on some ASICs there is limitation that default routes
     * must be put to ASIC first, so we need to compensate for that.
     *
     * TODO what about remove? can they be removed first ?
     *
     * There is another issue that when we are removing next hop group member
     * and it's the last next hop group member in group where group is still in
     * use by some group, then we can't remove it, we need to first remove
     * route that uses this group, this puts this task in conflict when
     * creating routes.
     *
     * XXX this is workaround. FIXME
     */

    for (auto &obj: temp.soAll)
    {
        if (obj.second->getObjectType() != SAI_OBJECT_TYPE_ROUTE_ENTRY)
        {
            processObjectForViewTransition(current, temp, obj.second);
        }
    }

    for (auto &obj: temp.soAll)
    {
        if (obj.second->getObjectType() == SAI_OBJECT_TYPE_ROUTE_ENTRY)
        {
            bool isDefault = obj.second->str_object_id.find("/0") != std::string::npos;

            if (isDefault)
            {
                processObjectForViewTransition(current, temp, obj.second);
            }
        }
    }

    for (auto &obj: temp.soAll)
    {
        if (obj.second->getObjectType() == SAI_OBJECT_TYPE_ROUTE_ENTRY)
        {
            bool isDefault = obj.second->str_object_id.find("/0") != std::string::npos;

            if (!isDefault)
            {
                processObjectForViewTransition(current, temp, obj.second);
            }
        }
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
     * if they don't exist. This apply to v0.9.4 and v1.0
     *
     * Similar thing can happen when we start using PROFILE_ID on
     * SCHEDULER_GROUP.
     *
     * TODO Revisit, since v1.0
     */

    bringDefaultTrapGroupToFinalState(current, temp);

    /*
     * Removing needs to be done from leaf with no references and it can be
     * multiple passes since if in first pass object had non zero references,
     * it can have zero in next pass so it is safe to remove.
     */

    /*
     * Another issue, since user may remove bridge port and vlan members and we
     * don't have references on them on the first place, then it may happen
     * that bridge port remove will be before vlan member remove and it will
     * fail. Similar problem is on hard reinit when removing existing objects.
     *
     * TODO we need dependency tree during sai discovery! but if we put those
     * to redis it will cause another problem, since some of those attributes
     * are create only, so object will be selected to "SET" after vid processing
     * but it wont be able to set create only attributes, we would need to skip
     * those.
     *
     * XXX this is workaround. FIXME
     */

    std::vector<sai_object_type_t> removeOrder = {
        SAI_OBJECT_TYPE_VLAN_MEMBER,
        SAI_OBJECT_TYPE_STP_PORT,
        SAI_OBJECT_TYPE_BRIDGE_PORT };

    for (const sai_object_type_t ot: removeOrder)
    {
        for (const auto &obj: current.getObjectsByObjectType(ot))
        {
            if (obj->getObjectStatus() == SAI_OBJECT_STATUS_NOT_PROCESSED)
            {
                if (current.getVidReferenceCount(obj->getVid()) == 0)
                {
                    removeExistingObjectFromCurrentView(current, temp, obj);
                }
            }
        }
    }

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

            if (obj->isOidObject())
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

        if (removed)
        {
            SWSS_LOG_NOTICE("loop removed %d objects", removed);
        }
    }

    /*
     * Check statuses will make sure that there are no objects with removed
     * status.
     */
}

void executeOperationsOnAsic(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView);

// TODO find better way to access this
extern std::set<sai_object_id_t> initViewRemovedVidSet;

void populateExistingObjects(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ const std::set<sai_object_id_t> rids)
{
    SWSS_LOG_ENTER();

    /*
     * We should transfer existing objects from current view to temporary view.
     * But not all objects, since user could remove some default removable
     * objects like vlan member, bridge port etc. We collected those removed
     * objects to initViewRemovedVidSet and we will use this as a reference to
     * transfer objects to temporary view.
     *
     * TODO: still sairedis metadata database have no idea about existing
     * objects so user can create object which already exists like vlan 1 if he
     * didn't queried it yet. We need to transfer all dependency tree to
     * sairedis after switch create.
     */

    if (temporaryView.getObjectsByObjectType(SAI_OBJECT_TYPE_SWITCH).size() == 0)
    {
        SWSS_LOG_NOTICE("no switch present in temporary view, skipping");
        return;
    }

    // XXX we have only 1 switch, so we can get away with this

    auto sw = switches.begin()->second;

    auto coldBootDiscoveredVids = sw->getColdBootDiscoveredVids();

    /*
     * If some objects that are existing objects on switch are not present in
     * temporary view, just populate them with empty values.  Since vid2rid
     * maps must match on both view after apply view.
     */

    for (const sai_object_id_t rid: rids)
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
            SWSS_LOG_THROW("unable to find existing object RID %s in current view",
                    sai_serialize_object_id(rid).c_str());
        }

        sai_object_id_t vid = it->second;

        if (initViewRemovedVidSet.find(vid) != initViewRemovedVidSet.end())
        {
            /*
             * If this vid was removed during init view mode, it still exits in
             * current view, but don't put it to existing objects, then
             * comparison logic will take care of that and, it will remove it
             * from current view as well.
             */

            continue;
        }

        /*
         * In case of warm boot, it may happen that user set some created
         * objects on default existing objects, like for example buffer profile
         * on ingress priority group.  In this case buffer profile should not
         * be considered as matched object and copied to temporary view, since
         * this object was not default existing object (on 1st cold boot) so in
         * this case it must be processed by comparison logic and matched with
         * possible new buffer profile created in temporary view. This may
         * happen if OA will not care what was set previously on ingress
         * priority group and just create new buffer profile and assign it.  In
         * this case we don't want any asic operations to happen.  Also if we
         * would pass this buffer profile as existing to temporary view, it
         * would not be matched by comparison logic, and in the result we will
         * end up with 2 buffer profiles, which 1st of them will be not
         * assigned anywhere and this will be memory leak.
         *
         * Also a bunch of new asic operations will be generated for setting
         * new user created buffer profile.  That's why we need default existing
         * vid list to distinguish between user created and default switch
         * created objects.
         *
         * For default existing objects, we don't need to copy attributes, since
         * if user didn't set them, we want them to be back to default values.
         *
         * NOTE: If we are here, then this RID exists only in current view, and
         * if this object contains any OID attributes, discovery logic queried
         * them so they are also existing in current view.
         */

        if (coldBootDiscoveredVids.find(vid) == coldBootDiscoveredVids.end())
        {
            SWSS_LOG_INFO("object is not on default existing list: %s RID %s VID %s",
                    sai_serialize_object_type(sai_object_type_query(rid)).c_str(),
                    sai_serialize_object_id(rid).c_str(),
                    sai_serialize_object_id(vid).c_str());

            continue;
        }

        temporaryView.createDummyExistingObject(rid, vid);

        SWSS_LOG_INFO("populate existing %s RID %s VID %s",
                sai_serialize_object_type(sai_object_type_query(rid)).c_str(),
                sai_serialize_object_id(rid).c_str(),
                sai_serialize_object_id(vid).c_str());

        /*
         * Move both objects to matched state since match oids was already
         * called, and here we created some new objects that should be matched.
         */

        currentView.oOids.at(vid)->setObjectStatus(SAI_OBJECT_STATUS_MATCHED);
        temporaryView.oOids.at(vid)->setObjectStatus(SAI_OBJECT_STATUS_MATCHED);
    }
}

void updateRedisDatabase(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView)
{
    SWSS_LOG_ENTER();

    /*
     * TODO: We can make LUA script for this which will be much faster.
     *
     * TODO: This needs to be updated if we want to support multiple switches.
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

        SWSS_LOG_DEBUG("setting key %s", key.c_str());

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
     *
     * TODO: This needs to be done per switch, we can't remove all maps.
     */

    redisClearVidToRidMap();
    redisClearRidToVidMap();

    for (auto &kv: temporaryView.ridToVid)
    {
        std::string strVid = sai_serialize_object_id(kv.second);
        std::string strRid = sai_serialize_object_id(kv.first);

        g_redisClient->hset(VIDTORID, strVid, strRid);
        g_redisClient->hset(RIDTOVID, strRid, strVid);
    }

    SWSS_LOG_NOTICE("updated redis database");
}

void logViewObjectCount(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView)
{
    SWSS_LOG_ENTER();

    bool asic_changes = false;

    for (int i = SAI_OBJECT_TYPE_NULL + 1; i < SAI_OBJECT_TYPE_MAX; i++)
    {
        sai_object_type_t ot = (sai_object_type_t)i;

        size_t c = currentView.getObjectsByObjectType(ot).size();
        size_t t = temporaryView.getObjectsByObjectType(ot).size();

        if (c == t)
            continue;

        asic_changes = true;

        SWSS_LOG_WARN("object count for %s on current view %zu is differnt than on temporary view: %zu",
                sai_serialize_object_type(ot).c_str(),
                c,
                t);
    }

    if (asic_changes)
    {
        SWSS_LOG_WARN("object count is differnt on both view, there will be ASIC OPERATIONS!");
    }
}

void checkAsicVsDatabaseConsistency(
        _In_ const AsicView cur,
        _In_ const AsicView &tmp)
{
    SWSS_LOG_ENTER();

    bool hasErrors = false;

    {
        SWSS_LOG_TIMER("consistency check");

        SWSS_LOG_WARN("performing consistency check");

        for (const auto &pair: tmp.soAll)
        {
            const auto &obj = pair.second;

            const auto &attrs = obj->getAllAttributes();

            // get object meta key for get (object id or *entry)

            sai_object_meta_key_t meta_key = obj->meta_key;

            // translate all VID's to RIDs in non object is's

            translate_vid_to_rid_non_object_id(meta_key);

            auto info = sai_metadata_get_object_type_info(obj->getObjectType());

            sai_attribute_t attr;

            memset(&attr, 0, sizeof(attr));

            if (attrs.size() == 0)
            {
                // get first attribute and do a get query to see if object exist's

                auto meta = info->attrmetadata[0];

                sai_status_t status = info->get(&meta_key, 1, &attr);

                switch (status)
                {
                    case SAI_STATUS_SUCCESS:
                    case SAI_STATUS_BUFFER_OVERFLOW:
                        continue;

                    case SAI_STATUS_NOT_IMPLEMENTED:
                    case SAI_STATUS_NOT_SUPPORTED:

                        SWSS_LOG_WARN("GET api for %s is not implemented on %s",
                                meta->attridname,
                                obj->str_object_id.c_str());
                    continue;
                }

                SWSS_LOG_ERROR("failed to get %s on %s: %s",
                        meta->attridname,
                        obj->str_object_id.c_str(),
                        sai_serialize_status(status).c_str());

                hasErrors = true;

                continue;
            }

            for (const auto &ap: attrs)
            {
                const auto &saiAttr = ap.second;

                auto meta = saiAttr->getAttrMetadata();

                // deserialize existing attribute so deserialize will allocate
                // memory for all list's

                attr.id = meta->attrid;

                sai_deserialize_attr_value(saiAttr->getStrAttrValue(), *meta, attr, false);

                // translate all VIDs from DB to RIDs for compare

                translate_vid_to_rid_list(obj->getObjectType(), 1, &attr);

                // get attr value with RIDs

                const std::string& dbValue = sai_serialize_attr_value(*meta, attr);

                sai_status_t status = info->get(&meta_key, 1, &attr);

                if (meta->attrid == SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST && meta->objecttype == SAI_OBJECT_TYPE_QOS_MAP && status == SAI_STATUS_SUCCESS)
                {
                    // order does not matter on this list

                    if (hasEqualQosMapList(attr.value.qosmap, saiAttr->getSaiAttr()->value.qosmap))
                    {
                        sai_deserialize_free_attribute_value(meta->attrvaluetype, attr);
                        continue;
                    }
                }

                // free possible allocated lists

                if (status != SAI_STATUS_SUCCESS)
                {
                    SWSS_LOG_ERROR("failed to get %s on %s %s",
                            meta->attridname,
                            obj->str_object_id.c_str(),
                            sai_serialize_status(status).c_str());

                    hasErrors = true;

                    sai_deserialize_free_attribute_value(meta->attrvaluetype, attr);
                    continue;
                }

                const std::string &asicValue = sai_serialize_attr_value(*meta, attr);

                sai_deserialize_free_attribute_value(meta->attrvaluetype, attr);

                // pointers will not be equal since those will be from
                // different process memory maps so just check if both pointers
                // are NULL or both are SET

                if (meta->attrvaluetype == SAI_ATTR_VALUE_TYPE_POINTER)
                {
                    if (attr.value.ptr == NULL && saiAttr->getSaiAttr()->value.ptr == NULL)
                        continue;

                    if (attr.value.ptr != NULL && saiAttr->getSaiAttr()->value.ptr != NULL)
                        continue;
                }

                if (asicValue == dbValue)
                    continue;

                SWSS_LOG_ERROR("value missmatch: %s on %s: ASIC: %s DB: %s, inconsistent state!",
                        meta->attridname,
                        obj->str_object_id.c_str(),
                        asicValue.c_str(),
                        dbValue.c_str());

                hasErrors = true;
            }
        }

        swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_INFO);
    }

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    if (hasErrors && enableUnittests())
    {
        SWSS_LOG_THROW("ASIC content is differnt than DB content!");
    }
}

sai_status_t syncdApplyView()
{
    SWSS_LOG_ENTER();

    if (enableRefernceCountLogs)
    {
        dump_object_reference();
    }

    SWSS_LOG_TIMER("apply");

    /*
     * We assume that there will be no case that we will move from 1 to 0, also
     * if at the beginning there is no switch, then when user will send create,
     * and it will be actually created (real call) so there should be no case
     * when we are moving from 0 -> 1.
     */

    /*
     * This method contains 2 stages.
     *
     * First stage is non destructive, when orchagent will build new view, and
     * there will be bug in comparison logic in first stage, then syncd will
     * send failure when doing apply view to orchagent but it will still be
     * running. No asic operations are performed during this stage.
     *
     * Second stage is destructive, so if there will be bug in comparison logic
     * or any asic operation will fail, then syncd will crash, since asic will
     * be in inconsistent state.
     */

    /*
     * Initialize rand for future candidate object selection if necessary.
     *
     * NOTE: Should this be deterministic? So we could repeat random choice
     * when something bad happen or we hit a bug, so in that case it will be
     * easier for reproduce, we could at least log value returned from time().
     *
     * TODO: To make it stable, we also need to make stable redisGetAsicView
     * since now order of items is random. Also redis result needs to be
     * sorted.
     */

    std::srand((unsigned int)std::time(0));

    /*
     * NOTE: Current view can contain multiple switches at once but in our
     * implementation we only will have 1 switch.
     */

    AsicView current;
    AsicView temp;

    /*
     * We are starting first stage here, it still can throw exceptions but it's
     * non destructive for ASIC, so just catch and return failure.
     */

    try
    {
        /*
         * NOTE: Need to be per switch. Asic view may contain multiple switches.
         *
         * In our current solution we will deal with only one switch, since
         * supporting multiple switches will have serious impact on redesign
         * current implementation.
         */

        if (switches.size() != 1)
        {
            /*
             * NOTE: In our solution multiple switches are not supported.
             */

            SWSS_LOG_THROW("only one switch is expected, got: %zu switches", switches.size());
        }

        // XXX we have only 1 switch, so we can get away with this

        auto sw = switches.begin()->second;

        ObjectIdMap vidToRidMap = sw->redisGetVidToRidMap();
        ObjectIdMap ridToVidMap = sw->redisGetRidToVidMap();

        current.ridToVid = ridToVidMap;
        current.vidToRid = vidToRidMap;

        /*
         * Those calls could be calls to SAI, but when this will be separate lib
         * then we would like to limit sai to minimum or reimplement getting those.
         *
         * TODO: This needs to be refactored and solved in other way since asic
         * view can contain multiple switches.
         *
         * TODO: This also can be optimized using metadata.
         * We need to add access to SaiSwitch in AsicView.
         */

        // TODO needs to be removed and done in generic
        current.defaultTrapGroupRid     = sw->getSwitchDefaultAttrOid(SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP);
        temp.defaultTrapGroupRid        = current.defaultTrapGroupRid;

        /*
         * Read current and temporary view from REDIS.
         */

        redisGetAsicView(ASIC_STATE_TABLE, current);
        redisGetAsicView(TEMP_PREFIX ASIC_STATE_TABLE, temp);

        /*
         * Match oids before calling populate existing objects since after
         * matching oids RID and VID maps will be populated.
         */

        matchOids(current, temp);

        /*
         * Populate existing objects to current and temp view if they don't
         * exist since we are populating them when syncd starts, and when we
         * switch view we don't want to loose any of those objects since during
         * syncd runtime is counting on that those objects exists.
         *
         * TODO: If some object's will be removed like VLAN members then this
         * existing objects needs to be updated in the switch!
         */

        const auto &existingObjects = sw->getDiscoveredRids();

        populateExistingObjects(current, temp, existingObjects);

        /*
         * Call main method!
         */

        if (enableRefernceCountLogs)
        {
            current.dumpRef("current START");
            temp.dumpRef("temp START");
        }

        logViewObjectCount(current, temp);

        applyViewTransition(current, temp);

        SWSS_LOG_NOTICE("ASIC operations to execute: %zu", current.asicGetOperationsCount());

        checkObjectsStatus(temp);

        SWSS_LOG_NOTICE("all temporary view objects were processed to FINAL state");

        checkObjectsStatus(current);

        SWSS_LOG_NOTICE("all current view objects were processed to FINAL state");

        /*
         * After all operations both views should look the same so number of
         * rid/vid should look the same.
         */

        if ((current.ridToVid.size() != temp.ridToVid.size()) ||
                (current.vidToRid.size() != temp.vidToRid.size()))
        {
            /*
             * TODO for debug we need to display differences
             */

            SWSS_LOG_THROW("wrong number of vid/rid items in map, forgot to translate? R2V: %zu:%zu, V2R: %zu:%zu, FIXME",
                    current.ridToVid.size(),
                    temp.ridToVid.size(),
                    current.vidToRid.size(),
                    temp.vidToRid.size());
        }

        /*
         * At the end number of soAll objects must be equal on both views. If
         * some on temporary views are missing, we need to transport empty
         * objects to temporary view, like queues, scheduler groups, virtual
         * router, trap groups etc.
         */

        if (current.soAll.size() != temp.soAll.size())
        {
            /*
             * If this will happen that means non object id values are
             * different since number of RID/VID maps is identical (previous
             * check).
             *
             * Unlikely to be routes/neighbors/fdbs, can be traps, switch,
             * vlan.
             *
             * TODO: For debug we will need to display differences
             */

            SWSS_LOG_THROW("wrong number of all objects current: %zu vs temp %zu, FIXME",
                    current.soAll.size(),
                    temp.soAll.size());
        }
    }
    catch (const std::exception &e)
    {
        /*
         * Exception was thrown in first stage, those were non destructive
         * actions so just log exception and let syncd running.
         */

        SWSS_LOG_ERROR("Exception: %s", e.what());

        return SAI_STATUS_FAILURE;
    }

    /*
     * This is second stage. Those operations are destructive, if any of them
     * fail, then we will have inconsistent state in ASIC.
     */

    executeOperationsOnAsic(current, temp);

    updateRedisDatabase(current, temp);

    if (g_enableConsistencyCheck)
    {
        checkAsicVsDatabaseConsistency(current, temp);
    }

    return SAI_STATUS_SUCCESS;
}

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
 * helpful when we will have to manage multiple switches at the same time.
 */

sai_object_id_t asic_translate_vid_to_rid(
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
            SWSS_LOG_THROW("unable to find VID %s in current view",
                    sai_serialize_object_id(vid).c_str());
        }
    }

    sai_object_id_t rid = currentIt->second;

    SWSS_LOG_INFO("translated VID 0x%lx to RID 0x%lx", vid, rid);

    return rid;
}

void asic_translate_list_vid_to_rid(
        _In_ const AsicView &current,
        _In_ const AsicView &temporary,
        _Inout_ sai_object_list_t &element)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < element.count; i++)
    {
        element.list[i] = asic_translate_vid_to_rid(current, temporary, element.list[i]);
    }
}

void asic_translate_vid_to_rid_list(
        _In_ const AsicView &current,
        _In_ const AsicView &temporary,
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * All id's received from view should be virtual, so lets translate them to
     * real id's.
     */

    for (uint32_t i = 0; i < attr_count; i++)
    {
        sai_attribute_t &attr = attr_list[i];

        auto meta = sai_metadata_get_attr_metadata(object_type, attr.id);

        // this should not happen we should get list right away from SaiAttr

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
                    sai_serialize_object_type(object_type).c_str(),
                    attr.id);

            /*
             * Asic will be in inconsistent state at this point
             */
        }

        switch (meta->attrvaluetype)
        {
            case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
                attr.value.oid = asic_translate_vid_to_rid(current, temporary, attr.value.oid);
                break;

            case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
                asic_translate_list_vid_to_rid(current, temporary, attr.value.objlist);
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:

                if (attr.value.aclfield.enable)
                {
                    attr.value.aclfield.data.oid = asic_translate_vid_to_rid(current, temporary, attr.value.aclfield.data.oid);
                }

                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:

                if (attr.value.aclfield.enable)
                {
                    asic_translate_list_vid_to_rid(current, temporary, attr.value.aclfield.data.objlist);
                }

                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:

                if (attr.value.aclaction.enable)
                {
                    attr.value.aclaction.parameter.oid = asic_translate_vid_to_rid(current, temporary, attr.value.aclaction.parameter.oid);
                }

                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:

                if (attr.value.aclaction.enable)
                {
                    asic_translate_list_vid_to_rid(current, temporary, attr.value.aclaction.parameter.objlist);
                }

                break;

            default:

                if (meta->isoidattribute)
                {
                    SWSS_LOG_THROW("attribute %s is oid attribute, but not handled, FIXME", meta->attridname);
                }

                break;
        }
    }
}

sai_status_t asic_handle_generic(
        _In_ AsicView &current,
        _In_ AsicView &temporary,
        _In_ sai_object_meta_key_t &meta_key,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("generic %s for %s",
            sai_serialize_common_api(api).c_str(),
            sai_serialize_object_type(meta_key.objecttype).c_str());

    /*
     * If we will use split for generics we can avoid meta key if we will
     * generate inside metadata generic pointers for all oid objects + switch
     * special method.
     *
     * And this needs to be executed in sai_meta_apis_query.
     */

    auto info = sai_metadata_get_object_type_info(meta_key.objecttype);

    sai_object_id_t object_id = meta_key.objectkey.key.object_id;

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            {
                /*
                 * We need to get switch ID, but we know we have one switch so
                 * we could use shortcut here, but let's do this proper way.
                 */

                sai_object_id_t switch_vid = redis_sai_switch_id_query(object_id);

                sai_object_id_t switch_rid = asic_translate_vid_to_rid(current, temporary, switch_vid);

                sai_status_t status = info->create(&meta_key, switch_rid, attr_count, attr_list);

                sai_object_id_t real_object_id = meta_key.objectkey.key.object_id;

                if (status == SAI_STATUS_SUCCESS)
                {
                    current.ridToVid[real_object_id] = object_id;
                    current.vidToRid[object_id] = real_object_id;

                    temporary.ridToVid[real_object_id] = object_id;
                    temporary.vidToRid[object_id] = real_object_id;

                    std::string str_vid = sai_serialize_object_id(object_id);
                    std::string str_rid = sai_serialize_object_id(real_object_id);

                    SWSS_LOG_INFO("saved VID %s to RID %s", str_vid.c_str(), str_rid.c_str());
                }
                else
                {
                    SWSS_LOG_ERROR("failed to create %s %s",
                            sai_serialize_object_type(meta_key.objecttype).c_str(),
                            sai_serialize_status(status).c_str());
                }

                return status;
            }

        case SAI_COMMON_API_REMOVE:
            {
                sai_object_id_t rid = asic_translate_vid_to_rid(current, temporary, object_id);

                meta_key.objectkey.key.object_id = rid;

                std::string str_vid = sai_serialize_object_id(object_id);
                std::string str_rid = sai_serialize_object_id(rid);

                /*
                 * Since object was removed, then we also need to remove it
                 * from removedVidToRid map just in case if there is some bug.
                 */

                current.removedVidToRid.erase(object_id);

                sai_status_t status = info->remove(&meta_key);

                if (status != SAI_STATUS_SUCCESS)
                {
                    // TODO get object attributes

                    SWSS_LOG_ERROR("remove %s RID: %s VID %s failed: %s",
                            sai_serialize_object_type(meta_key.objecttype).c_str(),
                            str_rid.c_str(),
                            str_vid.c_str(),
                            sai_serialize_status(status).c_str());
                }
                else
                {
                    /*
                     * When we remove object which was existing on the switch
                     * then we need to remove it from the SaiSwitch, because
                     * later on it will lead to creating this object again
                     * since isNonRemovable object will return true.
                     */

                    // XXX we have only 1 switch, so we can get away with this

                    auto sw = switches.begin()->second;

                    if (sw->isDiscoveredRid(rid))
                    {
                        sw->removeExistingObjectReference(rid);
                    }
                }

                return status;
            }

        case SAI_COMMON_API_SET:
            {
                sai_object_id_t rid = asic_translate_vid_to_rid(current, temporary, object_id);

                meta_key.objectkey.key.object_id = rid;

                sai_status_t status = info->set(&meta_key, attr_list);

                if (is_set_attribute_workaround(meta_key.objecttype, attr_list->id, status))
                {
                    return SAI_STATUS_SUCCESS;
                }

                return status;
            }

        default:
            SWSS_LOG_ERROR("other apis not implemented");
            return SAI_STATUS_FAILURE;
    }
}

void asic_translate_vid_to_rid_non_object_id(
        _In_ const AsicView &current,
        _In_ const AsicView &temporary,
        _In_ sai_object_meta_key_t &meta_key)
{
    SWSS_LOG_ENTER();

    auto info = sai_metadata_get_object_type_info(meta_key.objecttype);

    if (info->isobjectid)
    {
        meta_key.objectkey.key.object_id =
            asic_translate_vid_to_rid(current, temporary, meta_key.objectkey.key.object_id);

        return;
    }

    for (size_t idx = 0; idx < info->structmemberscount; ++idx)
    {
        const sai_struct_member_info_t *m = info->structmembers[idx];

        if (m->membervaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_ID)
        {
            sai_object_id_t vid = m->getoid(&meta_key);

            sai_object_id_t rid = asic_translate_vid_to_rid(current, temporary, vid);

            m->setoid(&meta_key, rid);
        }
    }
}

sai_status_t asic_handle_non_object_id(
        _In_ const AsicView &current,
        _In_ const AsicView &temporary,
        _In_ sai_object_meta_key_t &meta_key,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    asic_translate_vid_to_rid_non_object_id(current, temporary, meta_key);

    auto info = sai_metadata_get_object_type_info(meta_key.objecttype);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return info->create(&meta_key, SAI_NULL_OBJECT_ID, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return info->remove(&meta_key);

        case SAI_COMMON_API_SET:
            return info->set(&meta_key, attr_list);

        default:
            SWSS_LOG_ERROR("other apis not implemented");
            return SAI_STATUS_FAILURE;
    }
}

sai_status_t asic_process_event(
        _In_ AsicView &current,
        _In_ AsicView &temporary,
        _In_ const swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    /*
     * Since we have stored our keys in asic view, we can actually keep api
     * type right away so we don't have to serialize and deserialize everything
     * twice. We can take a look into that if update execution time will not be
     * sufficient.
     */

    const std::string &key = kfvKey(kco);
    const std::string &op = kfvOp(kco);

    sai_object_meta_key_t meta_key;
    sai_deserialize_object_meta_key(key, meta_key);

    SWSS_LOG_INFO("key: %s op: %s", key.c_str(), op.c_str());

    if (enableRefernceCountLogs)
    {
        SWSS_LOG_NOTICE("ASIC OP BEFORE : key: %s op: %s", key.c_str(), op.c_str());

        dump_object_reference();
        current.dumpRef("current before");
        temporary.dumpRef("temp before");
    }

    sai_common_api_t api = SAI_COMMON_API_MAX;

    // TODO convert those in common to use sai_common_api
    if (op == "set")
    {
        /*
         * Most common operation will probably be set so let put it first.
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
        SWSS_LOG_ERROR("api %s is not implemented on %s", op.c_str(), key.c_str());

        return SAI_STATUS_NOT_SUPPORTED;
    }

    std::stringstream ss;

    sai_object_type_t object_type = meta_key.objecttype;

    const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

    SaiAttributeList list(object_type, values, false);

    sai_attribute_t *attr_list = list.get_attr_list();
    uint32_t attr_count = list.get_attr_count();

    asic_translate_vid_to_rid_list(current, temporary, object_type, attr_count, attr_list);

    sai_status_t status;

    auto info = sai_metadata_get_object_type_info(object_type);

    if (object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        check_notifications_pointers(attr_count, attr_list);
    }

    if (info->isnonobjectid)
    {
        status = asic_handle_non_object_id(current, temporary, meta_key, api, attr_count, attr_list);
    }
    else
    {
        status = asic_handle_generic(current, temporary, meta_key, api, attr_count, attr_list);
    }

    if (enableRefernceCountLogs)
    {
        SWSS_LOG_NOTICE("ASIC OP AFTER : key: %s op: %s", key.c_str(), op.c_str());

        dump_object_reference();
        current.dumpRef("current after");
        temporary.dumpRef("temp after");
    }

    if (status == SAI_STATUS_SUCCESS)
    {
        return status;
    }

    for (const auto &v: values)
    {
        SWSS_LOG_ERROR("field: %s, value: %s", fvField(v).c_str(), fvValue(v).c_str());
    }

    /*
     * ASIC here will be in inconsistent state, we need to terminate.
     */

    SWSS_LOG_THROW("failed to execute api: %s, key: %s, status: %s",
            op.c_str(),
            key.c_str(),
            sai_serialize_status(status).c_str());
}

void dumpComparisonLogicOutput(
        _In_ const AsicView &currentView)
{
    SWSS_LOG_ENTER();

    std::stringstream ss;

    ss << "ASIC_OPERATIONS: " << currentView.asicGetOperationsCount() << std::endl;

    for (const auto &op: currentView.asicGetWithOptimizedRemoveOperations())
    {
        const std::string &key = kfvKey(*op.op);
        const std::string &opp = kfvOp(*op.op);

        ss << "o " << opp << ": " << key << std::endl;

        const auto &values = kfvFieldsValues(*op.op);

        for (auto v: values)
            ss << "a: " << fvField(v) << " " << fvValue(v) << std::endl;
    }

    std::ofstream log("applyview.log");

    if (log.is_open())
    {
        log << ss.str();

        log.close();

        SWSS_LOG_NOTICE("wrote apply_view asic operations to applyview.log");
    }
    else
    {
        SWSS_LOG_ERROR("failed to open applyview.log");
    }
}

void executeOperationsOnAsic(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("operations to execute on ASIC: %zu", currentView.asicGetOperationsCount());

    try
    {
        //swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_INFO);

        SWSS_LOG_TIMER("asic apply");

        if (enableUnittests())
            dumpComparisonLogicOutput(currentView);

        currentView.dumpVidToAsicOperatioId();

        SWSS_LOG_NOTICE("NOT optimized operations");

        for (const auto &op: currentView.asicGetOperations())
        {
            const std::string &key = kfvKey(*op.op);
            const std::string &opp = kfvOp(*op.op);

            SWSS_LOG_NOTICE("%s: %s", opp.c_str(), key.c_str());

            const auto &values = kfvFieldsValues(*op.op);

            for (auto v: values)
            {
                SWSS_LOG_NOTICE("- %s %s", fvField(v).c_str(), fvValue(v).c_str());
            }
        }

        SWSS_LOG_NOTICE("optimized operations!");

        std::map<std::string,int> opByObjectType;

        for (const auto &op: currentView.asicGetWithOptimizedRemoveOperations())
        {
            const std::string &key = kfvKey(*op.op);
            const std::string &opp = kfvOp(*op.op);

            SWSS_LOG_NOTICE("%s: %s", opp.c_str(), key.c_str());

            // count operations by object type
            opByObjectType[key.substr(0, key.find(":"))]++;
        }

        for (auto kvp: opByObjectType)
        {
            SWSS_LOG_NOTICE("operations on %s: %d", kvp.first.c_str(), kvp.second);
        }

        //for (const auto &op: currentView.asicGetOperations())
        for (const auto &op: currentView.asicGetWithOptimizedRemoveOperations())
        {
            /*
             * It is possible that this method will throw exception in that case we
             * also should exit syncd since we can be in the middle of executing
             * operations and if some problems will happen and we continue to stay
             * alive and next apply view, we will be in inconsistent state which
             * will lead to unexpected behaviour.
             */

            sai_status_t status = asic_process_event(currentView, temporaryView, *op.op);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_THROW("status of last operation was: %s, ASIC will be in inconsistent state, exiting",
                        sai_serialize_status(status).c_str());
            }
        }
    }
    catch (const std::exception &e)
    {
        SWSS_LOG_ERROR("Error while executing asic operations, ASIC is in inconsistent state: %s", e.what());

        /*
         * Re-throw exception to main loop.
         */

        throw;
    }

    SWSS_LOG_NOTICE("performed all operations on asic succesfully");
}
