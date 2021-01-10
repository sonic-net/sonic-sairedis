#include "AsicView.h"
#include "VidManager.h"

#include "meta/sai_serialize.h"
#include "meta/SaiAttributeList.h"

#include "swss/logger.h"

#include <inttypes.h>

#include <algorithm>

using namespace syncd;
using namespace saimeta;

AsicView::AsicView():
    m_asicOperationId(0)
{
    SWSS_LOG_ENTER();

    m_enableRefernceCountLogs = false;
}

AsicView::AsicView(
        _In_ const swss::TableDump &dump):
    m_asicOperationId(0)
{
    SWSS_LOG_ENTER();

    m_enableRefernceCountLogs = false;

    fromDump(dump);
}

AsicView::~AsicView()
{
    SWSS_LOG_ENTER();

    // empty
}

/**
 * @brief Populates ASIC view from REDIS table dump
 *
 * @param[in] dump Redis table dump
 *
 * NOTE: Could be static method that returns AsicView object.
 */
void AsicView::fromDump(
        _In_ const swss::TableDump &dump)
{
    SWSS_LOG_ENTER();

    /*
     * Input should be also existing objects, so they could be created
     * here right away but we would need VIDs as well.
     */

    int switchesCount = 0;

    for (const auto &key: dump)
    {
        auto start = key.first.find_first_of(":");

        if (start == std::string::npos)
        {
            SWSS_LOG_THROW("failed to find colon in %s", key.first.c_str());
        }

        std::shared_ptr<SaiObj> o = std::make_shared<SaiObj>();

        // TODO we could use sai deserialize object meta key

        o->m_str_object_type  = key.first.substr(0, start);
        o->m_str_object_id    = key.first.substr(start + 1);

        sai_deserialize_object_type(o->m_str_object_type, o->m_meta_key.objecttype);

        o->m_info = sai_metadata_get_object_type_info(o->m_meta_key.objecttype);

        /*
         * Since neighbor/route/fdb structs objects contains OIDs, we
         * need to increase vid reference. With new metadata for SAI
         * 1.0 this can be done in generic way for all non object ids.
         */

        switch (o->m_meta_key.objecttype)
        {
            case SAI_OBJECT_TYPE_FDB_ENTRY:
                sai_deserialize_fdb_entry(o->m_str_object_id, o->m_meta_key.objectkey.key.fdb_entry);
                m_soFdbs[o->m_str_object_id] = o;
                break;

            case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
                sai_deserialize_neighbor_entry(o->m_str_object_id, o->m_meta_key.objectkey.key.neighbor_entry);
                m_soNeighbors[o->m_str_object_id] = o;
                break;

            case SAI_OBJECT_TYPE_ROUTE_ENTRY:
                sai_deserialize_route_entry(o->m_str_object_id, o->m_meta_key.objectkey.key.route_entry);
                m_soRoutes[o->m_str_object_id] = o;

                m_routesByPrefix[sai_serialize_ip_prefix(o->m_meta_key.objectkey.key.route_entry.destination)].push_back(o->m_str_object_id);

                break;

            case SAI_OBJECT_TYPE_NAT_ENTRY:
                sai_deserialize_nat_entry(o->m_str_object_id, o->m_meta_key.objectkey.key.nat_entry);
                m_soNatEntries[o->m_str_object_id] = o;
                break;

            default:

                if (o->m_info->isnonobjectid)
                {
                    SWSS_LOG_THROW("object %s is non object id, not handled, FIXME", key.first.c_str());
                }

                sai_deserialize_object_id(o->m_str_object_id, o->m_meta_key.objectkey.key.object_id);

                m_soOids[o->m_str_object_id] = o;
                m_oOids[o->m_meta_key.objectkey.key.object_id] = o;

                if (o->m_meta_key.objecttype == SAI_OBJECT_TYPE_SWITCH)
                {
                    switchesCount++;
                }

                break;
        }

        m_soAll[o->m_str_object_id] = o;
        m_sotAll[o->m_meta_key.objecttype][o->m_str_object_id] = o;

        if (o->m_info->isnonobjectid)
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

            m_vidReference[o->m_meta_key.objectkey.key.object_id] += 0;
        }

        populateAttributes(o, key.second);
    }

    if (switchesCount != 1)
    {
        // NOTE: In our solution multiple switches are not supported in single AsicView

        SWSS_LOG_THROW("only one switch is expected in ASIC view, got: %d switches", switchesCount);
    }
}

/**
 * @brief Release existing VID links (references) based on given attribute.
 *
 * @param[in] attr Attribute which will be used to obtain oids.
 */
void AsicView::releaseExisgingLinks(
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
void AsicView::releaseExisgingLinks(
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
void AsicView::releaseVidReference(
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
void AsicView::bindNewLinks(
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
void AsicView::bindNewLinks(
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
void AsicView::bindNewVidReference(
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

/**
 * @brief Gets VID reference count.
 *
 * @param vid Virtual ID to obtain reference count.
 *
 * @return Reference count or -1 if VID was not found.
 */
int AsicView::getVidReferenceCount(
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
void AsicView::insertNewVidReference(
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

/**
 * @brief Gets objects by object type.
 *
 * @param object_type Object type to be used as filter.
 *
 * @return List of objects with requested object type.
 * Order on list is random.
 */
std::vector<std::shared_ptr<SaiObj>> AsicView::getObjectsByObjectType(
        _In_ sai_object_type_t object_type) const
{
    SWSS_LOG_ENTER();

    std::vector<std::shared_ptr<SaiObj>> list;

    /*
     * We need to use find, since object type may not exist.
     */

    auto it = m_sotAll.find(object_type);

    if (it == m_sotAll.end())
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
std::vector<std::shared_ptr<SaiObj>> AsicView::getNotProcessedObjectsByObjectType(
        _In_ sai_object_type_t object_type) const
{
    SWSS_LOG_ENTER();

    std::vector<std::shared_ptr<SaiObj>> list;

    /*
     * We need to use find, since object type may not exist.
     */

    auto it = m_sotAll.find(object_type);

    if (it == m_sotAll.end())
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
std::vector<std::shared_ptr<SaiObj>> AsicView::getAllNotProcessedObjects() const
{
    SWSS_LOG_ENTER();

    std::vector<std::shared_ptr<SaiObj>> list;

    for (const auto &p: m_soAll)
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
void AsicView::createDummyExistingObject(
        _In_ sai_object_id_t rid,
        _In_ sai_object_id_t vid)
{
    SWSS_LOG_ENTER();

    sai_object_type_t object_type = VidManager::objectTypeQuery(vid);

    if (object_type == SAI_OBJECT_TYPE_NULL)
    {
        SWSS_LOG_THROW("got null object type from VID %s",
                sai_serialize_object_id(vid).c_str());
    }

    std::shared_ptr<SaiObj> o = std::make_shared<SaiObj>();

    o->m_str_object_type = sai_serialize_object_type(object_type);
    o->m_str_object_id   = sai_serialize_object_id(vid);

    o->m_meta_key.objecttype = object_type;
    o->m_meta_key.objectkey.key.object_id = vid;

    o->m_info = sai_metadata_get_object_type_info(object_type);

    m_soOids[o->m_str_object_id] = o;
    m_oOids[vid] = o;

    m_vidReference[vid] += 0;

    m_soAll[o->m_str_object_id] = o;
    m_sotAll[o->m_meta_key.objecttype][o->m_str_object_id] = o;

    m_ridToVid[rid] = vid;
    m_vidToRid[vid] = rid;
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
void AsicView::asicSetAttribute(
        _In_ const std::shared_ptr<SaiObj> &currentObj,
        _In_ const std::shared_ptr<SaiAttr> &attr)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("%s: %s -> %s:%s", currentObj->m_str_object_type.c_str(), currentObj->m_str_object_id.c_str(),
            attr->getStrAttrId().c_str(), attr->getStrAttrValue().c_str());

    m_asicOperationId++;

    /*
     * Release previous references if attribute is object id and bind
     * new reference in that place.
     */

    auto meta = attr->getAttrMetadata();

    auto currentAttr = currentObj->tryGetSaiAttr(meta->attrid);

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

    auto entry = SaiAttributeList::serialize_attr_list(
            currentObj->getObjectType(),
            1,
            attr->getSaiAttr(),
            false);

    std::string key = currentObj->m_str_object_type + ":" + currentObj->m_str_object_id;

    auto kco = std::make_shared<swss::KeyOpFieldsValuesTuple>(key, "set", entry);

    sai_object_id_t vid = (currentObj->isOidObject()) ? currentObj->getVid() : SAI_NULL_OBJECT_ID;

    m_asicOperations.push_back(AsicOperation(m_asicOperationId, vid, false, kco));

    if (currentAttr)
    {
        // if current attribute exists, save it value for log purpose
        m_asicOperations.rbegin()->m_currentValue = currentAttr->getStrAttrValue();
    }

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
void AsicView::asicCreateObject(
        _In_ const std::shared_ptr<SaiObj> &currentObj)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("%s: %s", currentObj->m_str_object_type.c_str(), currentObj->m_str_object_id.c_str());

    m_asicOperationId++;

    if (currentObj->isOidObject())
    {
        m_soOids[currentObj->m_str_object_id] = currentObj;
        m_oOids[currentObj->m_meta_key.objectkey.key.object_id] = currentObj;

        m_soAll[currentObj->m_str_object_id] = currentObj;
        m_sotAll[currentObj->m_meta_key.objecttype][currentObj->m_str_object_id] = currentObj;

        /*
         * Since we are creating object, we just need to mark that
         * reference is there.  But at this point object is not used
         * anywhere.
         */

        m_vidReference[currentObj->m_meta_key.objectkey.key.object_id] += 0;
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
                //sai_deserialize_fdb_entry(currentObj->m_str_object_id, currentObj->m_meta_key.objectkey.key.fdb_entry);
                m_soFdbs[currentObj->m_str_object_id] = currentObj;
                break;

            case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
                //sai_deserialize_neighbor_entry(currentObj->m_str_object_id, currentObj->m_meta_key.objectkey.key.neighbor_entry);
                m_soNeighbors[currentObj->m_str_object_id] = currentObj;
                break;

            case SAI_OBJECT_TYPE_ROUTE_ENTRY:
                //sai_deserialize_route_entry(currentObj->m_str_object_id, currentObj->m_meta_key.objectkey.key.route_entry);
                m_soRoutes[currentObj->m_str_object_id] = currentObj;
                break;

            case SAI_OBJECT_TYPE_NAT_ENTRY:
                m_soNatEntries[currentObj->m_str_object_id] = currentObj;
                break;

            default:

                SWSS_LOG_THROW("unsupported object type: %s",
                        sai_serialize_object_type(currentObj->getObjectType()).c_str());
        }

        m_soAll[currentObj->m_str_object_id] = currentObj;
        m_sotAll[currentObj->m_meta_key.objecttype][currentObj->m_str_object_id] = currentObj;

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

    std::string key = currentObj->m_str_object_type + ":" + currentObj->m_str_object_id;

    auto kco = std::make_shared<swss::KeyOpFieldsValuesTuple>(key, "create", entry);

    sai_object_id_t vid = (currentObj->isOidObject()) ? currentObj->getVid() : SAI_NULL_OBJECT_ID;

    m_asicOperations.push_back(AsicOperation(m_asicOperationId, vid, false, kco));

    dumpRef("create");
}

/**
 * @brief Generate ASIC remove operation for current existing object.
 *
 * @param currentObj Current existing object to be removed.
 */
void AsicView::asicRemoveObject(
        _In_ const std::shared_ptr<SaiObj> &currentObj)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("%s: %s", currentObj->m_str_object_type.c_str(), currentObj->m_str_object_id.c_str());

    if (currentObj->getObjectStatus() != SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        SWSS_LOG_THROW("FATAL: removing object with status: %d, logic error", currentObj->getObjectStatus());
    }

    m_asicOperationId++;

    if (currentObj->isOidObject())
    {
        /*
         * Reference count is already check externally, but we can move
         * that check here also as sanity check.
         */

        int count = getVidReferenceCount(currentObj->getVid());

        if (count != 0)
        {
            SWSS_LOG_THROW("can't remove existing object %s:%s since reference count is %d, FIXME",
                    currentObj->m_str_object_type.c_str(),
                    currentObj->m_str_object_id.c_str(),
                    count);
        }

        m_soOids.erase(currentObj->m_str_object_id);
        m_oOids.erase(currentObj->m_meta_key.objectkey.key.object_id);

        m_soAll.erase(currentObj->m_str_object_id);
        m_sotAll.at(currentObj->m_meta_key.objecttype).erase(currentObj->m_str_object_id);

        m_vidReference[currentObj->m_meta_key.objectkey.key.object_id] -= 1;

        /*
         * Clear object also from rid/vid maps.
         */

        sai_object_id_t vid = currentObj->getVid();
        sai_object_id_t rid = m_vidToRid.at(vid);

        /*
         * This will have impact on translate_vid_to_rid, we need to put
         * this in other view.
         */

        m_ridToVid.erase(rid);
        m_vidToRid.erase(vid);

        /*
         * We could remove this VID also from m_vidReference, but it's not
         * required.
         */

        m_removedVidToRid[vid] = rid;
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
                m_soFdbs.erase(currentObj->m_str_object_id);
                break;

            case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:
                m_soNeighbors.erase(currentObj->m_str_object_id);
                break;

            case SAI_OBJECT_TYPE_ROUTE_ENTRY:
                m_soRoutes.erase(currentObj->m_str_object_id);
                break;

            case SAI_OBJECT_TYPE_NAT_ENTRY:
                m_soNatEntries.erase(currentObj->m_str_object_id);
                break;

            default:

                SWSS_LOG_THROW("unsupported object type: %s",
                        sai_serialize_object_type(currentObj->getObjectType()).c_str());
        }

        m_soAll.erase(currentObj->m_str_object_id);
        m_sotAll.at(currentObj->m_meta_key.objecttype).erase(currentObj->m_str_object_id);

        updateNonObjectIdVidReferenceCountByValue(currentObj, -1);
    }

    releaseExisgingLinks(currentObj); // handle attribute references

    /*
     * Generate asic commands.
     */

    std::vector<swss::FieldValueTuple> entry;

    std::string key = currentObj->m_str_object_type + ":" + currentObj->m_str_object_id;

    auto kco = std::make_shared<swss::KeyOpFieldsValuesTuple>(key, "remove", entry);

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

std::vector<AsicOperation> AsicView::asicGetWithOptimizedRemoveOperations() const
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

        if (!op.m_isRemove)
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

        if (op.m_vid == SAI_NULL_OBJECT_ID)
        {
            SWSS_LOG_THROW("non object id remove not exected here");
        }

        auto mit = m_vidToAsicOperationId.find(op.m_vid);

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

            auto ot = VidManager::objectTypeQuery(op.m_vid);

            SWSS_LOG_INFO("move %s all way up (not in map): %s to index: %zu",
                    sai_serialize_object_id(op.m_vid).c_str(),
                    sai_serialize_object_type(ot).c_str(),
                    index);

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

        auto itr = find_if(v.begin(), v.end(), [lastOpIdDecRef] (const AsicOperation& ao) { return ao.m_opId == lastOpIdDecRef; } );

        if (itr == v.end())
        {
            SWSS_LOG_THROW("something wrong, vid %s in map, but not found on list!",
                    sai_serialize_object_id(op.m_vid).c_str());
        }

        if (itr->m_isRemove)
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

        auto ot = VidManager::objectTypeQuery(op.m_vid);

        SWSS_LOG_INFO("move 0x%" PRIx64 " in the middle up: %s (last: %zu curr: %zu)",
                op.m_vid,
                sai_serialize_object_type(ot).c_str(),
                lastOpIdDecRefIndex,
                index);

        index++;

        moved++;
    }

    SWSS_LOG_NOTICE("moved %zu REMOVE operations upper in stack from total %zu operations", moved, v.size());

    return v;
}

std::vector<AsicOperation> AsicView::asicGetOperations() const
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

size_t AsicView::asicGetOperationsCount() const
{
    SWSS_LOG_ENTER();

    return m_asicOperations.size() + m_asicRemoveOperationsNonObjectId.size();
}

bool AsicView::hasRid(
        _In_ sai_object_id_t rid) const
{
    SWSS_LOG_ENTER();

    return m_ridToVid.find(rid) != m_ridToVid.end();
}

bool AsicView::hasVid(
        _In_ sai_object_id_t vid) const
{
    SWSS_LOG_ENTER();

    return m_vidToRid.find(vid) != m_vidToRid.end();
}

void AsicView::dumpRef(const std::string & asicName)
{
    SWSS_LOG_ENTER();

    if (m_enableRefernceCountLogs == false)
        return;

    SWSS_LOG_NOTICE("dump references in ASIC VIEW: %s", asicName.c_str());

    for (auto& kvp: m_vidReference)
    {
        sai_object_id_t oid = kvp.first;

        auto ot = VidManager::objectTypeQuery(oid);

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

void AsicView::dumpVidToAsicOperatioId() const
{
    SWSS_LOG_ENTER();

    for (auto& a: m_vidToAsicOperationId)
    {
        auto ot = VidManager::objectTypeQuery(a.first);

        SWSS_LOG_WARN("%d: %s:%s", a.second,
                sai_serialize_object_type(ot).c_str(),
                sai_serialize_object_id(a.first).c_str());
    }
}

void AsicView::populateAttributes(
        _In_ std::shared_ptr<SaiObj> &obj,
        _In_ const swss::TableMap &map)
{
    SWSS_LOG_ENTER();

    for (const auto& field: map)
    {
        std::shared_ptr<SaiAttr> attr = std::make_shared<SaiAttr>(field.first, field.second);

        if (obj->getObjectType() == SAI_OBJECT_TYPE_ACL_COUNTER)
        {
            auto* meta = attr->getAttrMetadata();

            switch (meta->attrid)
            {
                case SAI_ACL_COUNTER_ATTR_PACKETS:
                case SAI_ACL_COUNTER_ATTR_BYTES:

                    // when reading asic view, ignore acl counter packets and bytes
                    // this will result to not compare them during comparison logic

                    SWSS_LOG_INFO("ignoring %s for %s", meta->attridname, obj->m_str_object_id.c_str());

                    continue;

                default:
                    break;
            }
        }

        if (obj->getObjectType() == SAI_OBJECT_TYPE_NAT_ENTRY)
        {
            auto* meta = attr->getAttrMetadata();

            switch (meta->attrid)
            {
                case SAI_NAT_ENTRY_ATTR_HIT_BIT_COR:
                case SAI_NAT_ENTRY_ATTR_HIT_BIT:

                    // when reading asic view, ignore Nat entry hit-bit attribute
                    // this will result to not compare them during comparison logic

                    SWSS_LOG_INFO("ignoring %s for %s", meta->attridname, obj->m_str_object_id.c_str());

                    continue;

                default:
                    break;
            }
        }

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
void AsicView::updateNonObjectIdVidReferenceCountByValue(
        _In_ const std::shared_ptr<SaiObj> &currentObj,
        _In_ int value)
{
    SWSS_LOG_ENTER();

    for (size_t j = 0; j < currentObj->m_info->structmemberscount; ++j)
    {
        const sai_struct_member_info_t *m = currentObj->m_info->structmembers[j];

        if (m->membervaluetype == SAI_ATTR_VALUE_TYPE_OBJECT_ID)
        {
            sai_object_id_t vid = m->getoid(&currentObj->m_meta_key);

            m_vidReference[vid] += value;

            if (m_enableRefernceCountLogs)
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

void AsicView::checkObjectsStatus() const
{
    SWSS_LOG_ENTER();

    int count = 0;

    for (const auto &p: m_soAll)
    {
        if (p.second->getObjectStatus() != SAI_OBJECT_STATUS_FINAL)
        {
            const auto &o = *p.second;

            SWSS_LOG_ERROR("object was not processed: %s %s, status: %d (ref: %d)",
                    o.m_str_object_type.c_str(),
                    o.m_str_object_id.c_str(),
                    o.getObjectStatus(),
                    o.isOidObject() ? getVidReferenceCount(o.getVid()): -1);

            count++;
        }
    }

    if (count > 0)
    {
        SWSS_LOG_THROW("%d objects were not processed", count);
    }
}

sai_object_id_t AsicView::getSwitchVid() const
{
    SWSS_LOG_ENTER();

    for (auto& kvp: m_vidToRid)
    {
        auto vid = kvp.first;

        if (VidManager::objectTypeQuery(vid) == SAI_OBJECT_TYPE_SWITCH)
        {
            return vid;
        }
    }

    SWSS_LOG_THROW("no SWITCH present in ASIC view, FATAL");
}
