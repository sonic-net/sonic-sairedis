#include "ComparisonLogic.h"
#include "VidManager.h"
#include "BestCandidateFinder.h"
#include "NotificationHandler.h"
#include "VirtualOidTranslator.h"
#include "CommandLineOptions.h"
#include "Workaround.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"
#include "meta/SaiAttributeList.h"

#include <inttypes.h>

using namespace syncd;
using namespace saimeta;

/*
 * NOTE: All methods taking current and temporary view could be moved to
 * transition class etc to just use class members instead of passing those
 * parameters to every function.
 */

ComparisonLogic::ComparisonLogic(
        _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai,
        _In_ std::shared_ptr<SaiSwitchInterface> sw,
        _In_ std::shared_ptr<NotificationHandler> handler,
        _In_ std::set<sai_object_id_t> initViewRemovedVids,
        _In_ std::shared_ptr<AsicView> current,
        _In_ std::shared_ptr<AsicView> temp,
        _In_ std::shared_ptr<BreakConfig> breakConfig):
    m_vendorSai(vendorSai),
    m_switch(sw),
    m_initViewRemovedVids(initViewRemovedVids),
    m_current(current),
    m_temp(temp),
    m_handler(handler),
    m_breakConfig(breakConfig)
{
    SWSS_LOG_ENTER();

    m_enableRefernceCountLogs = false;

    // will inside filter only RID/VID to this particular switch

    // TODO move outside switch ? since later could be in different ASIC_DB
    m_current->m_ridToVid = m_switch->getRidToVidMap();
    m_current->m_vidToRid = m_switch->getVidToRidMap();

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

    m_current->m_defaultTrapGroupRid     = m_switch->getSwitchDefaultAttrOid(SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP);
    m_temp->m_defaultTrapGroupRid        = m_switch->getSwitchDefaultAttrOid(SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP);

    auto seed = (unsigned int)std::time(0);

    SWSS_LOG_NOTICE("srand seed for switch %s: %u", sai_serialize_object_id(m_switch->getVid()).c_str(), seed);

    std::srand(seed);
}

ComparisonLogic::~ComparisonLogic()
{
    SWSS_LOG_ENTER();

    // empty
}

void ComparisonLogic::compareViews()
{
    SWSS_LOG_ENTER();

    AsicView& current = *m_current;
    AsicView& temp = *m_temp;

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

    populateExistingObjects(current, temp);

    checkInternalObjects(current, temp);

    /*
     * Call main method!
     */

    if (m_enableRefernceCountLogs)
    {
        current.dumpRef("current START");
        temp.dumpRef("temp START");
    }

    createPreMatchMap(current, temp);

    logViewObjectCount(current, temp);

    applyViewTransition(current, temp);

    transferNotProcessed(current, temp);

    // TODO have a method to check for not processed objects
    // and maybe add them to list on processing attributes
    // and move note processed objects to temporary view as well
    // we need to check oid attributes as well

    SWSS_LOG_NOTICE("ASIC operations to execute: %zu", current.asicGetOperationsCount());

    temp.checkObjectsStatus();

    SWSS_LOG_NOTICE("all temporary view objects were processed to FINAL state");

    current.checkObjectsStatus();

    SWSS_LOG_NOTICE("all current view objects were processed to FINAL state");

    /*
     * After all operations both views should look the same so number of
     * rid/vid should look the same.
     */

    checkMap(current, temp);

    /*
     * At the end number of m_soAll objects must be equal on both views. If
     * some on temporary views are missing, we need to transport empty
     * objects to temporary view, like queues, scheduler groups, virtual
     * router, trap groups etc.
     */

    if (current.m_soAll.size() != temp.m_soAll.size())
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
                current.m_soAll.size(),
                temp.m_soAll.size());
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
void ComparisonLogic::matchOids(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView)
{
    SWSS_LOG_ENTER();

    auto coldBootDiscoveredVids = m_switch->getColdBootDiscoveredVids();

    for (const auto &temporaryIt: temporaryView.m_oOids)
    {
        sai_object_id_t temporaryVid = temporaryIt.first;

        const auto &currentIt = currentView.m_oOids.find(temporaryVid);

        if (currentIt == currentView.m_oOids.end())
        {
            continue;
        }

        sai_object_id_t vid = temporaryVid;
        sai_object_id_t rid = currentView.m_vidToRid.at(vid);

        // save VID and RID in temporary view

        temporaryView.m_ridToVid[rid] = vid;
        temporaryView.m_vidToRid[vid] = rid;

        // set both objects status as matched

        temporaryIt.second->setObjectStatus(SAI_OBJECT_STATUS_MATCHED);
        currentIt->second->setObjectStatus(SAI_OBJECT_STATUS_MATCHED);

        SWSS_LOG_INFO("matched %s RID %s VID %s",
                currentIt->second->m_str_object_type.c_str(),
                sai_serialize_object_id(rid).c_str(),
                sai_serialize_object_id(vid).c_str());

        if (coldBootDiscoveredVids.find(vid) == coldBootDiscoveredVids.end())
        {
            auto ot = currentIt->second->getObjectType();

            switch (ot)
            {
                case SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP:
                case SAI_OBJECT_TYPE_SCHEDULER_GROUP:
                case SAI_OBJECT_TYPE_QUEUE:
                case SAI_OBJECT_TYPE_PORT:
                    break;

                default:

                    // Should we also add check if also not in removed?
                    // This is for case of only GET:
                    // 1. cold boot:
                    //   - OA assigns buffer profile to Queue
                    // 2. warm boot:
                    //   - OA do GET on Queue and got previous buffer profile
                    //   - OA assigns new buffer profile on Queue
                    //
                    // Question: what should happen to old buffer profile? should it be removed?
                    // No, since OA still hold's the reference to that VID and may use it later.
                    SWSS_LOG_INFO("matched %s VID %s was not in cold boot, possible only GET?",
                            sai_serialize_object_id(vid).c_str(),
                            currentIt->second->m_str_object_type.c_str());
                    break;
            }
        }
    }

    SWSS_LOG_NOTICE("matched oids");
}

/**
 * @brief Check internal objects.
 *
 * During warm boot, when switch restarted we expect that switch will have the
 * same number of objects like before boot, and all vendor OIDs (RIDs) will be
 * exactly the same as before reboot.
 *
 * If OIDs are not the same, then this is a vendor bug.
 *
 * Exception of this rule is when orchagent will be restarted and it will add
 * some more objects or remove some objects.
 *
 * We take special care here about INGRESS_PRIORITY_GROUPS, SCHEDULER_GROUPS
 * and QUEUES, since those are internal objects created by vendor when switch
 * is instantiated.
 *
 * @param currentView Current view.
 * @param temporaryView Temporary view.
 */
void ComparisonLogic::checkInternalObjects(
        _In_ const AsicView &cv,
        _In_ const AsicView &tv)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("check internal objects");

    std::vector<sai_object_type_t> ots =
    {
        SAI_OBJECT_TYPE_QUEUE,
        SAI_OBJECT_TYPE_SCHEDULER_GROUP,
        SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP
    };

    for (auto ot: ots)
    {
        auto cot = cv.getObjectsByObjectType(ot);
        auto tot = tv.getObjectsByObjectType(ot);

        auto sot = sai_serialize_object_type(ot);

        if (cot.size() != tot.size())
        {
            SWSS_LOG_WARN("different number of objects %s, curr: %zu, tmp %zu (not expected if warm boot)",
                    sot.c_str(),
                    cot.size(),
                    tot.size());
        }

        for (auto o: cot)
            if (o->getObjectStatus() != SAI_OBJECT_STATUS_MATCHED)
                SWSS_LOG_ERROR("object status is not MATCHED on curr: %s:%s",
                        sot.c_str(), o->m_str_object_id.c_str());

        for (auto o: tot)
            if (o->getObjectStatus() != SAI_OBJECT_STATUS_MATCHED)
                SWSS_LOG_ERROR("object status is not MATCHED on temp: %s:%s",
                        sot.c_str(), o->m_str_object_id.c_str());
    }
}

void ComparisonLogic::checkMatchedPorts(
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

        SWSS_LOG_THROW("port %s object status is not MATCHED (%d)", p->m_str_object_id.c_str(), p->getObjectStatus());
    }

    SWSS_LOG_NOTICE("all ports are matched");
}

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
void ComparisonLogic::procesObjectAttributesForViewTransition(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("%s %s", temporaryObj->m_str_object_type.c_str(), temporaryObj->m_str_object_id.c_str());

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

            auto tempParent = temporaryView.m_oOids.at(vid);

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

    for (size_t j = 0; j < temporaryObj->m_info->structmemberscount; ++j)
    {
        const sai_struct_member_info_t *m = temporaryObj->m_info->structmembers[j];

        if (m->membervaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID)
        {
            continue;
        }

        sai_object_id_t vid = m->getoid(&temporaryObj->m_meta_key);

        SWSS_LOG_INFO("- processing %s (%s) VID %s",
                temporaryObj->m_str_object_type.c_str(),
                m->membername,
                sai_serialize_object_id(vid).c_str());

        auto structObject = temporaryView.m_oOids.at(vid);

        processObjectForViewTransition(currentView, temporaryView, structObject); // recursion
    }
}

void ComparisonLogic::bringNonRemovableObjectToDefaultState(
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

        auto defaultValueAttr = BestCandidateFinder::getSaiAttrFromDefaultValue(currentView, m_switch, *meta);

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
bool ComparisonLogic::isNonRemovableObject(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<const SaiObj> &currentObj)
{
    SWSS_LOG_ENTER();

    if (currentObj->isOidObject())
    {
        sai_object_id_t rid = currentView.m_vidToRid.at(currentObj->getVid());

        return m_switch->isNonRemovableRid(rid);
    }

    /*
     * Object is non object id, like ROUTE_ENTRY etc, can be removed.
     */

    return false;
}

void ComparisonLogic::removeExistingObjectFromCurrentView(
        _In_ AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> &currentObj)
{
    SWSS_LOG_ENTER();

    if (currentObj->getObjectStatus() != SAI_OBJECT_STATUS_NOT_PROCESSED)
    {
        SWSS_LOG_THROW("FATAL: removing object with status: %d, logic error", currentObj->getObjectStatus());
    }

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
                    currentObj->m_str_object_type.c_str(),
                    currentObj->m_str_object_id.c_str(),
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

sai_object_id_t ComparisonLogic::translateTemporaryVidToCurrentVid(
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

    auto temporaryIt = temporaryView.m_vidToRid.find(tvid);

    if (temporaryIt == temporaryView.m_vidToRid.end())
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

        auto tempIt = temporaryView.m_oOids.find(tvid);

        if (tempIt == temporaryView.m_oOids.end())
        {
            SWSS_LOG_THROW("temporary VID %s not found in temporary view",
                    sai_serialize_object_id(tvid).c_str());
        }

        const auto &tempObj = tempIt->second;

        if (tempObj->m_createdObject)
        {
            SWSS_LOG_DEBUG("translated temp VID %s to current, since object was created",
                    sai_serialize_object_id(tvid).c_str());

            return tvid;
        }

        SWSS_LOG_THROW("VID %s was not found in temporary view, was object created? FIXME",
                sai_serialize_object_id(tvid).c_str());
    }

    sai_object_id_t rid = temporaryIt->second;

    auto currentIt = currentView.m_ridToVid.find(rid);

    if (currentIt == currentView.m_ridToVid.end())
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

std::shared_ptr<SaiAttr> ComparisonLogic::translateTemporaryVidsToCurrentVids(
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

            // TODO check is is oid object and throw as not translated

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

    attr->updateValue();

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
void ComparisonLogic::setAttributeOnCurrentObject(
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
                currentObj->m_str_object_type.c_str(),
                currentObj->m_str_object_id.c_str());
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
void ComparisonLogic::createNewObjectFromTemporaryObject(
        _In_ AsicView &currentView,
        _In_ const AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> &temporaryObj)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("creating object %s:%s",
                    temporaryObj->m_str_object_type.c_str(),
                    temporaryObj->m_str_object_id.c_str());

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

    currentObj->m_str_object_type  = temporaryObj->m_str_object_type;
    currentObj->m_str_object_id    = temporaryObj->m_str_object_id;      // temporary VID / non object id
    currentObj->m_meta_key         = temporaryObj->m_meta_key;           // temporary VID / non object id
    currentObj->m_info             = temporaryObj->m_info;

    if (!temporaryObj->isOidObject())
    {
        /*
         * Since object contains OID inside struct, this OID may already exist
         * in current view, so we need to do translation here.
         */

        for (size_t j = 0; j < currentObj->m_info->structmemberscount; ++j)
        {
            const sai_struct_member_info_t *m = currentObj->m_info->structmembers[j];

            if (m->membervaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID)
            {
                continue;
            }

            sai_object_id_t vid = m->getoid(&currentObj->m_meta_key);

            vid = translateTemporaryVidToCurrentVid(currentView, temporaryView, vid);

            m->setoid(&currentObj->m_meta_key, vid);

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

                currentObj->m_str_object_id = sai_serialize_route_entry(currentObj->m_meta_key.objectkey.key.route_entry);
                break;

            case SAI_OBJECT_TYPE_NEIGHBOR_ENTRY:

                currentObj->m_str_object_id = sai_serialize_neighbor_entry(currentObj->m_meta_key.objectkey.key.neighbor_entry);
                break;

            case SAI_OBJECT_TYPE_FDB_ENTRY:
                currentObj->m_str_object_id = sai_serialize_fdb_entry(currentObj->m_meta_key.objectkey.key.fdb_entry);
                break;

            case SAI_OBJECT_TYPE_NAT_ENTRY:
                currentObj->m_str_object_id = sai_serialize_nat_entry(currentObj->m_meta_key.objectkey.key.nat_entry);
                break;

            case SAI_OBJECT_TYPE_INSEG_ENTRY:
                currentObj->m_str_object_id = sai_serialize_inseg_entry(currentObj->m_meta_key.objectkey.key.inseg_entry);
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

    currentObj->m_createdObject = true;
    temporaryObj->m_createdObject = true;

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

void ComparisonLogic::updateObjectStatus(
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
            sai_object_id_t rid = currentView.m_vidToRid.at(cvid);

            SWSS_LOG_INFO("remapped current VID %s to temp VID %s using RID %s",
                    sai_serialize_object_id(cvid).c_str(),
                    sai_serialize_object_id(tvid).c_str(),
                    sai_serialize_object_id(rid).c_str());

            temporaryView.m_ridToVid[rid] = tvid;
            temporaryView.m_vidToRid[tvid] = rid;

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
                    currentBestMatch->m_str_object_id.c_str(),
                    temporaryObj->m_str_object_id.c_str());

            temporaryView.m_nonObjectIdMap[objectType][temporaryObj->m_str_object_id] = currentBestMatch->m_str_object_id;
            currentView.m_nonObjectIdMap[objectType][currentBestMatch->m_str_object_id] = temporaryObj->m_str_object_id;
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

bool ComparisonLogic::performObjectSetTransition(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj> currentBestMatch,
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

            SWSS_LOG_INFO("compare attr value curr %s vs temp %s",
                     currentBestMatch->getSaiAttr(attr.id)->getStrAttrValue().c_str(),
                     temporaryObj->getSaiAttr(attr.id)->getStrAttrValue().c_str());

            if (BestCandidateFinder::hasEqualAttribute(currentView, temporaryView, currentBestMatch, temporaryObj, attr.id))
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
                        temporaryObj->m_str_object_id.c_str());
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
             * There is another case here, if this attribute exists only in
             * temporary view and it has default value, and SET value is the
             * same as default, then there is no need for ASIC operation.
             *
             * NOTE: This can lead to not put attributes with default VALUE to
             * redis database and could be confusing when debugging.
             */

            const auto defaultValueAttr = BestCandidateFinder::getSaiAttrFromDefaultValue(currentView, m_switch, *meta);

            if (defaultValueAttr != nullptr)
            {
                std::string defStr = sai_serialize_attr_value(*meta, *defaultValueAttr->getSaiAttr());

                if (defStr == temporaryAttr->getStrAttrValue())
                {
                    SWSS_LOG_NOTICE("explicit %s:%s is the same as default, no need for ASIC SET action",
                            meta->attridname, defStr.c_str());

                    continue;
                }
            }

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

    const bool beginTempSizeZero = temporaryObj->getAllAttributes().size() == 0;

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
                sai_object_id_t rid = currentView.m_vidToRid.at(vid);

                if (m_switch->isDiscoveredRid(rid))
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

                    sai_object_id_t vid = currentBestMatch->getVid();

                    if (currentView.hasVid(vid))
                    {
                        // scheduler_group RID
                        sai_object_id_t rid = currentView.m_vidToRid.at(vid);

                        rid = m_switch->getDefaultValueForOidAttr(rid, SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID);

                        if (rid != SAI_NULL_OBJECT_ID && currentView.hasRid(rid))
                        {
                            /*
                             * We found default value
                             */

                            SWSS_LOG_DEBUG("found default rid %s, vid %s for %s",
                                    sai_serialize_object_id(rid).c_str(),
                                    sai_serialize_object_id(vid).c_str(),
                                    meta->attridname);

                            def = currentView.m_ridToVid.at(rid);
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

                // current best match is MATCHED

                //auto vid = currentBestMatch->getVid();

                // TODO don't transfer oid attributes, we don't know how to handle this yet
                // If OA did GET on any attributes, snoop in syncd should catch that and write
                // to database so we would have some attributes here.

                if (beginTempSizeZero && !meta->isoidattribute)
                {
                    SWSS_LOG_WARN("current attr is MoC|CaS and object is MATCHED: %s transferring %s:%s to temp object (was empty)",
                            currentBestMatch->m_str_object_id.c_str(),
                            meta->attridname,
                            currentAttr->getStrAttrValue().c_str());

                    std::shared_ptr<SaiAttr> transferedAttr = std::make_shared<SaiAttr>(
                            currentAttr->getStrAttrId(),
                            currentAttr->getStrAttrValue());

                    temporaryObj->setAttr(transferedAttr);

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
                        currentBestMatch->m_str_object_id.c_str(),
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

                    // don't produce too much noise for queues
                    if (currentAttr->getStrAttrId() != "SAI_QUEUE_ATTR_TYPE")
                    {
                        SWSS_LOG_WARN("current attr is CREATE_ONLY and object is MATCHED: %s transferring %s:%s to temp object",
                                currentBestMatch->m_str_object_id.c_str(),
                                meta->attridname,
                                currentAttr->getStrAttrValue().c_str());
                    }

                    std::shared_ptr<SaiAttr> transferedAttr = std::make_shared<SaiAttr>(
                            currentAttr->getStrAttrId(),
                            currentAttr->getStrAttrValue());

                    temporaryObj->setAttr(transferedAttr);

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

            const auto defaultValueAttr = BestCandidateFinder::getSaiAttrFromDefaultValue(currentView, m_switch, *meta);

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
void ComparisonLogic::processObjectForViewTransition(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _Inout_ std::shared_ptr<SaiObj> temporaryObj)
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

    SWSS_LOG_INFO("processing: %s:%s", temporaryObj->m_str_object_type.c_str(), temporaryObj->m_str_object_id.c_str());

    procesObjectAttributesForViewTransition(currentView, temporaryView, temporaryObj);

    /*
     * Now since all object ids (VIDs) were processed for temporary object we
     * can try to find current best match.
     */

    auto bcf = std::make_shared<BestCandidateFinder>(currentView, temporaryView, m_switch);

    std::shared_ptr<SaiObj> currentBestMatch = bcf->findCurrentBestMatch(temporaryObj);

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
                temporaryObj->m_str_object_type.c_str(),
                temporaryObj->m_str_object_id.c_str());

        /*
         * We can hit this scenario when, we will not have current best match,
         * and we still want to create new object, but resources are limited,
         * so it can happen that in this place we also would need to remove
         * some objects first, before we create new one.
         */
        breakBeforeMake(currentView, temporaryView, currentBestMatch, temporaryObj);

        createNewObjectFromTemporaryObject(currentView, temporaryView, temporaryObj);
        return;
    }

    SWSS_LOG_INFO("found best match %s: current: %s temporary: %s",
            currentBestMatch->m_str_object_type.c_str(),
            currentBestMatch->m_str_object_id.c_str(),
            temporaryObj->m_str_object_id.c_str());

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
                    temporaryObj->m_str_object_id.c_str());
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
             * Later on if we decide we want to remove objects before creating
             * new one's we need to put here this action, or just remove this
             * entire switch and call remove.
             */

            breakBeforeMake(currentView, temporaryView, currentBestMatch, temporaryObj);
        }

        // No need to store VID since at this point we don't have RID yet, it will be
        // created on execute asic and RID will be saved to maps in both views.

        SWSS_LOG_INFO("found best match, but set failed: %s: current: %s temporary: %s",
                currentBestMatch->m_str_object_type.c_str(),
                currentBestMatch->m_str_object_id.c_str(),
                temporaryObj->m_str_object_id.c_str());

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
         * This object VID will be remapped inside updateObjectStatus to
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
        // currentView.m_vidToRid[vid] = currentView.m_vidToRid.at(currentBestMatch->getVid());
    }

    performObjectSetTransition(currentView, temporaryView, currentBestMatch, temporaryObj, true);

    /*
     * Since we got here, that means matching or updating current best match
     * object was successful, and we can now set their state to final.
     */

    updateObjectStatus(currentView, temporaryView, currentBestMatch, temporaryObj);
}

void ComparisonLogic::removeCurrentObjectDependencyTree(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj>& currentObj)
{
    SWSS_LOG_ENTER();

    if (!currentObj->isOidObject())
    {
        // we should be able to remove non object right away since it's leaf

        removeExistingObjectFromCurrentView(currentView, temporaryView, currentObj);

        return;
    }

    // check reference count and remove other objects if necessary

    const int count = currentView.getVidReferenceCount(currentObj->getVid());

    if (count)
    {
        SWSS_LOG_INFO("similar best match has reference count: %d, will need to remove other objects", count);

        auto* info = sai_metadata_get_object_type_info(currentObj->getObjectType());

        // use reverse dependency graph to locate objects where current object can be used
        // need to lookout on loops on some objects

        for (size_t i = 0; i < info->revgraphmemberscount; i++)
        {
            auto *revgraph = info->revgraphmembers[i];

            if (revgraph->structmember)
            {
                SWSS_LOG_THROW("struct fields not supported yet, FIXME");
            }

            SWSS_LOG_INFO("used on %s:%s",
                    sai_serialize_object_type(revgraph->depobjecttype).c_str(),
                    revgraph->attrmetadata->attridname);

            // TODO it could be not processed, or matched, since on matched we
            // can still break the link

            auto objs = currentView.getObjectsByObjectType(revgraph->depobjecttype);

            for (auto& obj: objs)
            {
                // NOTE: current object can have multiple OID attributes that
                // they can have the same value, but they can have different
                // attributes (like set/create_only)

                auto status = obj->getObjectStatus();

                if (status != SAI_OBJECT_STATUS_NOT_PROCESSED &&
                         status != SAI_OBJECT_STATUS_MATCHED)
                {
                    continue;
                }

                auto attr = obj->tryGetSaiAttr(revgraph->attrmetadata->attrid);

                if (attr == nullptr)
                {
                    // no such attribute
                    continue;
                }

                if (revgraph->attrmetadata->attrvaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID &&
                        revgraph->attrmetadata->attrvaluetype != SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID)
                {
                    // currently we only support reference on OID, not list
                    SWSS_LOG_THROW("attr value type %d, not supported yet, FIXME",
                            revgraph->attrmetadata->attrvaluetype);
                }

                if (attr->getOid() != currentObj->getVid())
                {
                    // VID is not matching, skip this attribute
                    continue;
                }

                // we found object and attribute that is using current VID,
                // it's possible this VID is used on multiple attributes on the
                // same object

                SWSS_LOG_INFO("found reference object: %s", obj->m_str_object_id.c_str());

                if (revgraph->attrmetadata->iscreateonly && status == SAI_OBJECT_STATUS_NOT_PROCESSED)
                {
                    // attribute is create only, and object was not processed
                    // yet this means that we need to remove object, since we
                    // can't break the link

                    removeCurrentObjectDependencyTree(currentView, temporaryView, obj); // recursion
                }
                else if (revgraph->attrmetadata->iscreateandset && status == SAI_OBJECT_STATUS_NOT_PROCESSED)
                {
                    if (revgraph->attrmetadata->allownullobjectid)
                    {
                        // we can also remove entire object here too

                        SWSS_LOG_THROW("break the link is not implemented yet, FIXME");
                    }
                    else
                    {
                        // attribute is create and set, but not allow null object id
                        // so probably attribute is mandatory on create, we can't break
                        // the link, but we can remove entire object

                        removeCurrentObjectDependencyTree(currentView, temporaryView, obj); // recursion
                    }
                }
                else if (revgraph->attrmetadata->iscreateandset && status == SAI_OBJECT_STATUS_MATCHED)
                {
                    SWSS_LOG_THROW("matched break the link is not implemented yet, FIXME");
                }
                else
                {
                    SWSS_LOG_THROW("remove on %s, obj status: %d, not supported, FIXME", revgraph->attrmetadata->attridname, status);
                }
            }
        }
    }

    removeExistingObjectFromCurrentView(currentView, temporaryView, currentObj);
}

void ComparisonLogic::breakBeforeMake(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView,
        _In_ const std::shared_ptr<SaiObj>& currentBestMatch, // can be nullptr
        _In_ const std::shared_ptr<SaiObj>& temporaryObj)
{
    SWSS_LOG_ENTER();

    /*
     * Break Before Make rule.
     *
     * We can have 2 paths here:
     *
     * - current best match object was not found (nullptr), in case of limited
     *   resources, this mean that we want to remove some existing object
     *   before creating new, so number of existing objects will not exceed
     *   initial value, for example we remove acl table, then we create acl
     *   table.  Of course this will not hold true, if temporary number of
     *   objects of given type is greater than current existing objects.
     *
     * - current best match was found, but perform Object Set Transition
     *   operation failed, so we can't move current object attributes to
     *   temporary object attributes, because for example some are create only.
     */

    if (!m_breakConfig->shouldBreakBeforeMake(temporaryObj->getObjectType()))
    {
        // skip object if not in break config

        return;
    }

    if (currentBestMatch == nullptr)
    {
        // it can happen that current best match is not found for temporary
        // object then we need to find best object for removal from existing
        // ones the most suitable should be the one with most same attributes,
        // since maybe only one read only attribute has been changed, and this
        // will automatically result in null best match

        auto bcf = std::make_shared<BestCandidateFinder>(currentView, temporaryView, m_switch);

        std::shared_ptr<SaiObj> similarBestMatch = bcf->findSimilarBestMatch(temporaryObj);

        if (similarBestMatch == nullptr)
        {
            SWSS_LOG_WARN("similar best match is null, not removing");
            return;
        }

        removeCurrentObjectDependencyTree(currentView, temporaryView, similarBestMatch);

        return;
    }

    removeCurrentObjectDependencyTree(currentView, temporaryView, currentBestMatch);
}

void ComparisonLogic::checkSwitch(
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
void ComparisonLogic::bringDefaultTrapGroupToFinalState(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView)
{
    SWSS_LOG_ENTER();

    sai_object_id_t rid = currentView.m_defaultTrapGroupRid;

    if (temporaryView.hasRid(rid))
    {
        /*
         * Default trap group is defined inside temporary view
         * so it will be matched by RID, no need to process.
         */

        return;
    }

    sai_object_id_t vid = currentView.m_ridToVid.at(rid);

    const auto &dtgObj = currentView.m_oOids.at(vid);

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

void ComparisonLogic::populateExistingObjects(
        _In_ AsicView &currentView,
        _In_ AsicView &temporaryView)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("populate existing objects");

    auto rids = m_switch->getDiscoveredRids();

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

    auto coldBootDiscoveredVids = m_switch->getColdBootDiscoveredVids();
    auto warmBootDiscoveredVids = m_switch->getWarmBootDiscoveredVids();
    auto warmBootNewDiscoveredVids = m_switch->getWarmBootNewDiscoveredVids();

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
            SWSS_LOG_INFO("temporary view has existing %s RID %s",
                    sai_serialize_object_type(m_vendorSai->objectTypeQuery(rid)).c_str(),
                    sai_serialize_object_id(rid).c_str());

            continue;
        }

        auto it = currentView.m_ridToVid.find(rid);

        if (it == currentView.m_ridToVid.end())
        {
            SWSS_LOG_THROW("unable to find existing object RID %s in current view",
                    sai_serialize_object_id(rid).c_str());
        }

        sai_object_id_t vid = it->second;

        if (m_initViewRemovedVids.find(vid) != m_initViewRemovedVids.end())
        {
            /*
             * If this vid was removed during init view mode, it still exits in
             * current view, but don't put it to existing objects, then
             * comparison logic will take care of that and, it will remove it
             * from current view as well.
             */

            SWSS_LOG_INFO("object was removed in init view: %s RID %s VID %s",
                    sai_serialize_object_type(m_vendorSai->objectTypeQuery(rid)).c_str(),
                    sai_serialize_object_id(rid).c_str(),
                    sai_serialize_object_id(vid).c_str());

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
         *
         * Also in warm boot, when user removed port, and then created some new
         * ports, new QUEUEs, IPGs and SGs will be created automatically by
         * SAI.  Those new created objects mot likely will have different RID
         * values then previous instances for given port. Those values should
         * also be copied to temporary view, since they will not exist on cold
         * boot discovered VIDs. If not, then comparison logic will try to remove
         * them which is not what we want.
         *
         * This is tricky scenario, and there could be some issues also when
         * other object types would be created by user.
         */

        bool performColdCheck = true;

        if (warmBootDiscoveredVids.find(vid) != warmBootDiscoveredVids.end())
        {
            sai_object_type_t ot = VidManager::objectTypeQuery(vid);

            switch (ot)
            {
                case SAI_OBJECT_TYPE_QUEUE:
                case SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP:
                case SAI_OBJECT_TYPE_SCHEDULER_GROUP:

                    // TODO this case may require adjustment, if user will do a
                    // warm boot then remove/add some ports and make another
                    // warm boot, it may happen that current logic will be
                    // confused which of those objects are from previous warm
                    // boot or second one, need better way to mark changes to
                    // those objects in redis DB between warm boots

                    performColdCheck = false;

                    break;
                default:
                    break;
            }
        }

        if (warmBootNewDiscoveredVids.find(vid) != warmBootNewDiscoveredVids.end())
        {
            // We have some new discovered VIDs after warm boot, we need to
            // create temporary objects from them, so comparison logic will not
            // get confused and will not remove them.
            //
            // TODO: there could be potential issue here, when user will remove
            // one of the new discovered object in init view phase, then we
            // can't put that object to DB, and it should be removed.

            performColdCheck = false;

            sai_object_type_t ot = VidManager::objectTypeQuery(vid);

            SWSS_LOG_NOTICE("creating temporary object for new discovered VID %s:%s",
                    sai_serialize_object_type(ot).c_str(),
                    sai_serialize_object_id(vid).c_str());
        }

        if (performColdCheck && coldBootDiscoveredVids.find(vid) == coldBootDiscoveredVids.end())
        {
            SWSS_LOG_INFO("object is not on default existing list: %s RID %s VID %s",
                    sai_serialize_object_type(m_vendorSai->objectTypeQuery(rid)).c_str(),
                    sai_serialize_object_id(rid).c_str(),
                    sai_serialize_object_id(vid).c_str());

            continue;
        }

        temporaryView.createDummyExistingObject(rid, vid);

        SWSS_LOG_INFO("populate existing %s RID %s VID %s",
                sai_serialize_object_type(m_vendorSai->objectTypeQuery(rid)).c_str(),
                sai_serialize_object_id(rid).c_str(),
                sai_serialize_object_id(vid).c_str());

        /*
         * Move both objects to matched state since match oids was already
         * called, and here we created some new objects that should be matched.
         */

        currentView.m_oOids.at(vid)->setObjectStatus(SAI_OBJECT_STATUS_MATCHED);
        temporaryView.m_oOids.at(vid)->setObjectStatus(SAI_OBJECT_STATUS_MATCHED);
    }
}

bool ComparisonLogic::checkAsicVsDatabaseConsistency(
        _In_ std::shared_ptr<VirtualOidTranslator> translator)
{
    SWSS_LOG_ENTER();

    bool hasErrors = false;

    {
        SWSS_LOG_TIMER("consistency check");

        SWSS_LOG_WARN("performing consistency check");

        for (const auto &pair: m_temp->m_soAll)
        {
            const auto &obj = pair.second;

            const auto &attrs = obj->getAllAttributes();

            // get object meta key for get (object id or *entry)

            sai_object_meta_key_t meta_key = obj->m_meta_key;

            // translate all VID's to RIDs in non object is's

            translator->translateVidToRid(meta_key);

            auto info = sai_metadata_get_object_type_info(obj->getObjectType());

            sai_attribute_t attr;

            memset(&attr, 0, sizeof(attr));

            if (attrs.size() == 0)
            {
                // get first attribute and do a get query to see if object exist's

                auto meta = info->attrmetadata[0];

                sai_status_t status = m_vendorSai->get(meta_key, 1, &attr);

                switch (status)
                {
                    case SAI_STATUS_SUCCESS:
                    case SAI_STATUS_BUFFER_OVERFLOW:
                        continue;

                    case SAI_STATUS_NOT_IMPLEMENTED:
                    case SAI_STATUS_NOT_SUPPORTED:

                        SWSS_LOG_WARN("GET api for %s is not implemented on %s",
                                meta->attridname,
                                obj->m_str_object_id.c_str());
                    continue;
                }

                SWSS_LOG_ERROR("failed to get %s on %s: %s",
                        meta->attridname,
                        obj->m_str_object_id.c_str(),
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

                translator->translateVidToRid(obj->getObjectType(), 1, &attr);

                // get attr value with RIDs

                const std::string& dbValue = sai_serialize_attr_value(*meta, attr);

                sai_status_t status = m_vendorSai->get(meta_key, 1, &attr);

                if (meta->attrid == SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST && meta->objecttype == SAI_OBJECT_TYPE_QOS_MAP && status == SAI_STATUS_SUCCESS)
                {
                    // order does not matter on this list

                    if (BestCandidateFinder::hasEqualQosMapList(attr.value.qosmap, saiAttr->getSaiAttr()->value.qosmap))
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
                            obj->m_str_object_id.c_str(),
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
                        obj->m_str_object_id.c_str(),
                        asicValue.c_str(),
                        dbValue.c_str());

                hasErrors = true;
            }
        }

        swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_INFO);
    }

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    return hasErrors == false;
}

void ComparisonLogic::checkMap(
        _In_ const AsicView::ObjectIdMap& firstR2V,
        _In_ const char* firstR2Vname,
        _In_ const AsicView::ObjectIdMap& firstV2R,
        _In_ const char * firstV2Rname,
        _In_ const AsicView::ObjectIdMap& secondR2V,
        _In_ const char* secondR2Vname,
        _In_ const AsicView::ObjectIdMap& secondV2R,
        _In_ const char *secondV2Rname) const
{
    SWSS_LOG_ENTER();

    for (auto it: firstR2V)
    {
        sai_object_id_t r = it.first;
        sai_object_id_t v = it.second;

        if (firstV2R.find(v) == firstV2R.end())
            SWSS_LOG_ERROR("%s (0x%" PRIx64 ":0x%" PRIx64 ") is missing from %s", firstR2Vname, r, v, firstV2Rname);
        else if (firstV2R.at(v) != r)
            SWSS_LOG_ERROR("mismatch on %s (0x%" PRIx64 ":0x%" PRIx64 ") vs %s (0x%" PRIx64 ":0x%" PRIx64 ")", firstR2Vname, r, v, firstV2Rname, v, firstV2R.at(v));

        if (secondR2V.find(r) == secondR2V.end())
            SWSS_LOG_ERROR("%s (0x%" PRIx64 ":0x%" PRIx64 ") is missing from %s", firstR2Vname, r, v, secondR2Vname);
        else if (secondV2R.find(secondR2V.at(r)) == secondV2R.end())
            SWSS_LOG_ERROR("%s (0x%" PRIx64 ":0x%" PRIx64 ") is missing from %s", firstR2Vname, r, secondR2V.at(r), secondV2Rname);
    }
}

void ComparisonLogic::createPreMatchMapForObject(
        _In_ const AsicView& cur,
        _Inout_ AsicView& tmp,
        _In_ std::shared_ptr<const SaiObj> cObj,
        _In_ std::shared_ptr<const SaiObj> tObj,
        _Inout_ std::set<std::string>& processed)
{
    SWSS_LOG_ENTER();

    if (processed.find(tObj->m_str_object_id) != processed.end())
        return;

    processed.insert(tObj->m_str_object_id);

    if (cObj->getObjectType() != tObj->getObjectType())
        return;

    // this object is matched, so it have same vid/rid in both views
    // but it can have attributes with objects which are not matched
    // for those we want to create pre match map

    for (auto& ak: tObj->getAllAttributes())
    {
        auto id = ak.first;
        const auto& tAttr = ak.second;

        if (cObj->hasAttr(id) == false)
            continue;

        // both objects has the same attribute

        const auto& cAttr = cObj->getSaiAttr(id);

        const auto& tVids = tAttr->getOidListFromAttribute();
        const auto& cVids = cAttr->getOidListFromAttribute();

        if (tVids.size() != cVids.size())
            continue; // if number of attributes is different then skip

        if (tVids.size() != 1)
            continue; // for now skip list attributes

        for (size_t i = 0; i < tVids.size(); ++i)
        {
            sai_object_id_t tVid = tVids[i];
            sai_object_id_t cVid = cVids[i];

            if (tVid == SAI_NULL_OBJECT_ID || cVid == SAI_NULL_OBJECT_ID)
                continue;

            if (tmp.m_preMatchMap.find(tVid) != tmp.m_preMatchMap.end())
                continue;

            // since on one attribute sometimes different object types can be set
            // check if both object types are correct

            if (cur.m_oOids.at(cVid)->getObjectType() != tmp.m_oOids.at(tVid)->getObjectType())
                continue;

            SWSS_LOG_INFO("inserting pre match entry for %s:%s: 0x%" PRIx64 " (tmp) -> 0x%" PRIx64 " (cur)",
                    tObj->m_str_object_id.c_str(),
                    cAttr->getAttrMetadata()->attridname,
                    tVid,
                    cVid);

            tmp.m_preMatchMap[tVid] = cVid;

            // continue recursively through object dependency tree:

            createPreMatchMapForObject(cur, tmp, cur.m_oOids.at(cVid), tmp.m_oOids.at(tVid), processed);
        }
    }
}

void ComparisonLogic::cretePreMatchForLagMembers(
        _In_ const AsicView& cur,
        _Inout_ AsicView& tmp,
        _Inout_ std::set<std::string>& processed)
{
    SWSS_LOG_ENTER();

    // match lag members that have the same port and use that lag members
    // as createPreMatchMapForObject so it will create pre match entry for LAG
    // and then for acl table group

    auto cLagMembers = cur.getObjectsByObjectType(SAI_OBJECT_TYPE_LAG_MEMBER);
    auto tLagMembers = tmp.getObjectsByObjectType(SAI_OBJECT_TYPE_LAG_MEMBER);

    for (auto& cLagMember: cLagMembers)
    {
        if (processed.find(cLagMember->m_str_object_id) != processed.end())
            continue;

        for (auto& tLagMember: tLagMembers)
        {
            if (processed.find(tLagMember->m_str_object_id) != processed.end())
                continue;

            auto cPort = cLagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_PORT_ID);
            auto tPort = tLagMember->getSaiAttr(SAI_LAG_MEMBER_ATTR_PORT_ID);

            if (cPort->getOid() != tPort->getOid())
            {
                continue;
            }

            // at this point current and temporary lag member share the same
            // port VID, then we can assume that those objects are the same

            SWSS_LOG_INFO("pre match Lag Member: cur: %s, tmp: %s",
                    cLagMember->m_str_object_id.c_str(),
                    tLagMember->m_str_object_id.c_str());

            tmp.m_preMatchMap[tLagMember->getVid()] = cLagMember->getVid();

            createPreMatchMapForObject(cur, tmp, cLagMember, tLagMember, processed);

            break;
        }
    }
}

#define PFC_ACL_RULE_PRIORITY "999"

void ComparisonLogic::cretePreMatchForAclEntries(
        _In_ const AsicView& cur,
        _Inout_ AsicView& tmp,
        _Inout_ std::set<std::string>& processed)
{
    SWSS_LOG_ENTER();

    // match acl entries which have the same priority attribute

    auto cAclEntries = cur.getObjectsByObjectType(SAI_OBJECT_TYPE_ACL_ENTRY);
    auto tAclEntries = tmp.getObjectsByObjectType(SAI_OBJECT_TYPE_ACL_ENTRY);

    for (auto& cAclEntry: cAclEntries)
    {
        if (processed.find(cAclEntry->m_str_object_id) != processed.end())
            continue;

        for (auto& tAclEntry: tAclEntries)
        {
            if (processed.find(tAclEntry->m_str_object_id) != processed.end())
                continue;

            auto cPrio = cAclEntry->getSaiAttr(SAI_ACL_ENTRY_ATTR_PRIORITY);
            auto tPrio = tAclEntry->getSaiAttr(SAI_ACL_ENTRY_ATTR_PRIORITY);

            if (cPrio->getStrAttrValue() != tPrio->getStrAttrValue())
            {
                continue;
            }

            if (cPrio->getStrAttrValue() == PFC_ACL_RULE_PRIORITY)
            {
                // this is special case, all pfc rules to prevent packet storm
                // will have same priority, we will need to check SAI_ACL_ENTRY_ATTR_FIELD_TC

                auto cTc = cAclEntry->tryGetSaiAttr(SAI_ACL_ENTRY_ATTR_FIELD_TC);
                auto tTc = tAclEntry->tryGetSaiAttr(SAI_ACL_ENTRY_ATTR_FIELD_TC);

                if (!cTc || !tTc || cTc->getStrAttrValue() != tTc->getStrAttrValue())
                {
                    // TC attribute is different, not looking for this one
                    continue;
                }
            }

            // at this point current and temporary acl entry share the same
            // priority and in case of pfc priority, also the same TC field,
            // then we can assume that those objects are the same

            SWSS_LOG_NOTICE("pre match Acl Entry: cur: %s, tmp: %s, using prio: %s",
                    cAclEntry->m_str_object_id.c_str(),
                    tAclEntry->m_str_object_id.c_str(),
                    cPrio->getStrAttrValue().c_str());

            tmp.m_preMatchMap[tAclEntry->getVid()] = cAclEntry->getVid();

            createPreMatchMapForObject(cur, tmp, cAclEntry, tAclEntry, processed);

            break;
        }
    }
}

void ComparisonLogic::createPreMatchMap(
        _In_ const AsicView& cur,
        _Inout_ AsicView& tmp)
{
    SWSS_LOG_ENTER();

    /*
     * When comparison logic is running on object A it runs recursively on each
     * object that was fount in object A attributes, because we need to make
     * sure all objects are matched before actually processing object A.  For
     * example before processing ROUTE_ENTRY, we process first NEXT_HOP and
     * before that ROUTER_INTERFACE and before that PORT. Object processing
     * could be described going from down to top. But figuring out for top
     * object ex. WRED could be hard since we would need to check not matched
     * yet buffer profile and buffer pool before we use QUEUE or IPG as an
     * anchor object. We can actually leverage that and when processing graph
     * from top to bottom, we can create helper map which will contain
     * predictions which object will be suitable for current processing
     * objects. We can have N candidates objects and instead of choosing 1 at
     * random, to reduce number of ASIC operations we will use a pre match map
     * which is created in this method.
     *
     * This map should be almost exact match in warn boot case, but it will be
     * treated only as hint, not as actual mapping.
     *
     * We will create map for all matched objects and for route_entry,
     * fdb_entry and neighbor_entry.
     */

    std::set<std::string> processed;

    SWSS_LOG_TIMER("create preMatch map");

    for (auto& ok: tmp.m_soAll)
    {
        auto& tObj = ok.second;

        if (tObj->getObjectStatus() != SAI_OBJECT_STATUS_MATCHED)
            continue;

        sai_object_id_t cObjVid = cur.m_ridToVid.at(tmp.m_vidToRid.at(tObj->getVid()));

        auto& cObj = cur.m_oOids.at(cObjVid);

        createPreMatchMapForObject(cur, tmp, cObj, tObj, processed);
    }

    for (auto&pk: tmp.m_routesByPrefix)
    {
        auto& prefix = pk.first;

        // look only for unique prefixes

        if (pk.second.size() != 1)
            continue;

        auto it = cur.m_routesByPrefix.find(prefix);

        if (it == cur.m_routesByPrefix.end())
            continue;

        if (it->second.size() != 1)
            continue;

        auto& tObj = tmp.m_soAll.at(pk.second.at(0));
        auto& cObj = cur.m_soAll.at(it->second.at(0));

        createPreMatchMapForObject(cur, tmp, cObj, tObj, processed);
    }

    for (auto&pk: tmp.m_neighborsByIp)
    {
        auto& ip = pk.first;

        // look only for unique neighbors

        if (pk.second.size() != 1)
            continue;

        auto it = cur.m_neighborsByIp.find(ip);

        if (it == cur.m_neighborsByIp.end())
            continue;

        if (it->second.size() != 1)
            continue;

        auto& tObj = tmp.m_soAll.at(pk.second.at(0));
        auto& cObj = cur.m_soAll.at(it->second.at(0));

        createPreMatchMapForObject(cur, tmp, cObj, tObj, processed);

        sai_object_id_t tRifVid = tObj->m_meta_key.objectkey.key.neighbor_entry.rif_id;
        sai_object_id_t cRifVid = cObj->m_meta_key.objectkey.key.neighbor_entry.rif_id;

        if (processed.find(cObj->m_str_object_id) == processed.end())
        {
            SWSS_LOG_INFO("pre match Neighbor RIF: cur: %s, tmp: %s",
                    sai_serialize_object_id(cRifVid).c_str(),
                    sai_serialize_object_id(tRifVid).c_str());

            tmp.m_preMatchMap[tRifVid] = cRifVid;

            auto& cO = cur.m_oOids.at(cRifVid);
            auto& tO = tmp.m_oOids.at(tRifVid);

            createPreMatchMapForObject(cur, tmp, cO, tO, processed);
        }
    }

    cretePreMatchForLagMembers(cur, tmp, processed);

    size_t count = 0;

    for (auto& ok: tmp.m_soOids)
    {
        if (ok.second->getObjectStatus() != SAI_OBJECT_STATUS_MATCHED)
            count++;
    }

    cretePreMatchForAclEntries(cur, tmp, processed);

    SWSS_LOG_NOTICE("preMatch map size: %zu, tmp oid obj: %zu",
            tmp.m_preMatchMap.size(),
            count);
}

void ComparisonLogic::transferNotProcessed(
        _In_ AsicView& current,
        _In_ AsicView& temp)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("calling transferNotProcessed");

    /*
     * It may happen that after performing view transition, some objects will
     * not be processed. This may happen is scenario where buffer pool is
     * assigned to buffer profile, and then buffer profile is assigned to
     * queue.  If OA will query only for oid for that buffer profile, then
     * buffer pool will not be processed. Normally if it would be removed by
     * comparison logic. More on this issue can be found on github:
     * https://github.com/Azure/sonic-sairedis/issues/899
     *
     * So what we will do, we will transfer all not processed objects to
     * temporary view with the same RID and VID. If nothing will happen to them,
     * they will stay there until next warm boot where they will be removed.
     */

    /*
     * We need a loop (or recursion) since not processed objects may have oid
     * attributes as well.
     */

    while (current.getAllNotProcessedObjects().size())
    {
        SWSS_LOG_WARN("we have %zu not processed objects on current view, moving to temp view", current.getAllNotProcessedObjects().size());

        for (const auto& obj: current.getAllNotProcessedObjects())
        {
            auto vid = obj->getVid();

            auto rid =  current.m_vidToRid.at(vid);

            /*
             * We should have only oid objects here, since all non oid objects
             * are leafs in graph and has been removed.
             */

            auto tmp = temp.createDummyExistingObject(rid, vid);

            /*
             * Move both objects to matched state since match oids was already
             * called, and here we created some new objects that should be matched.
             */

            current.m_oOids.at(vid)->setObjectStatus(SAI_OBJECT_STATUS_FINAL);
            temp.m_oOids.at(vid)->setObjectStatus(SAI_OBJECT_STATUS_FINAL);

            SWSS_LOG_WARN("moved %s VID %s RID %s to temporary view, and marked FINAL",
                    obj->m_str_object_type.c_str(),
                    obj->m_str_object_id.c_str(),
                    sai_serialize_object_id(rid).c_str());

            for (auto& kvp: obj->getAllAttributes())
            {
                auto& sh = kvp.second;

                auto attr = std::make_shared<SaiAttr>(sh->getStrAttrId(), sh->getStrAttrValue());

                tmp->setAttr(attr);

                SWSS_LOG_WARN(" * with attr: %s: %s",
                        sh->getStrAttrId().c_str(),
                        sh->getStrAttrValue().c_str());
            }

        }
    }
}

void ComparisonLogic::applyViewTransition(
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

    for (auto &obj: temp.m_soAll)
    {
        // TODO make generic list of root objects (or meta SAI for this)

        if (obj.second->getObjectType() == SAI_OBJECT_TYPE_ACL_TABLE_GROUP ||
            obj.second->getObjectType() == SAI_OBJECT_TYPE_ACL_ENTRY)
        {
            if (temp.m_preMatchMap.find(obj.second->getVid()) == temp.m_preMatchMap.end())
            {
                // object not in pre match
                continue;
            }

            SWSS_LOG_INFO("processing explicit pre match: %s:%s",
                    obj.second->m_str_object_type.c_str(),
                    obj.second->m_str_object_id.c_str());

            processObjectForViewTransition(current, temp, obj.second);
        }
    }

    for (auto &obj: temp.m_soAll)
    {
        /*
         * Make sure we will create all neighbor entries before next hop that
         * have the same IP address as neighbor entry. In Broadcom platform
         * neighbor entry needs to be created before next hop with the same ip
         * address otherwise create next hop will fail (hardware limitation?).
         */

        if (obj.second->getObjectType() == SAI_OBJECT_TYPE_NEIGHBOR_ENTRY)
        {
            processObjectForViewTransition(current, temp, obj.second);
        }
    }

    for (auto &obj: temp.m_soAll)
    {
        if (obj.second->getObjectType() != SAI_OBJECT_TYPE_ROUTE_ENTRY)
        {
            processObjectForViewTransition(current, temp, obj.second);
        }
    }

    for (auto &obj: temp.m_soAll)
    {
        if (obj.second->getObjectType() == SAI_OBJECT_TYPE_ROUTE_ENTRY)
        {
            bool isDefault = obj.second->m_str_object_id.find("/0") != std::string::npos;

            if (isDefault)
            {
                processObjectForViewTransition(current, temp, obj.second);
            }
        }
    }

    for (auto &obj: temp.m_soAll)
    {
        if (obj.second->getObjectType() == SAI_OBJECT_TYPE_ROUTE_ENTRY)
        {
            bool isDefault = obj.second->m_str_object_id.find("/0") != std::string::npos;

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

void ComparisonLogic::logViewObjectCount(
        _In_ const AsicView &currentView,
        _In_ const AsicView &temporaryView)
{
    SWSS_LOG_ENTER();

    bool asic_changes = false;

    // skip null object type

    for (size_t i = 1; i < sai_metadata_enum_sai_object_type_t.valuescount; ++i)
    {
        sai_object_type_t ot = (sai_object_type_t)sai_metadata_enum_sai_object_type_t.values[i];

        size_t c = currentView.getObjectsByObjectType(ot).size();
        size_t t = temporaryView.getObjectsByObjectType(ot).size();

        if (c == t)
            continue;

        asic_changes = true;

        SWSS_LOG_WARN("object count for %s on current view %zu is different than on temporary view: %zu",
                sai_serialize_object_type(ot).c_str(),
                c,
                t);
    }

    if (asic_changes)
    {
        SWSS_LOG_WARN("object count is different on both view, there will be ASIC OPERATIONS!");
    }
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

sai_object_id_t ComparisonLogic::asic_translate_vid_to_rid(
        _In_ const AsicView &current,
        _In_ const AsicView &temporary,
        _In_ sai_object_id_t vid)
{
    SWSS_LOG_ENTER();

    if (vid == SAI_NULL_OBJECT_ID)
    {
        return SAI_NULL_OBJECT_ID;
    }

    auto currentIt = current.m_vidToRid.find(vid);

    if (currentIt == current.m_vidToRid.end())
    {
        /*
         * If object was removed it RID was moved to removed map.  This is
         * required since at the end we need to check whether rid/vid maps are
         * the same for both views.
         */

        currentIt = current.m_removedVidToRid.find(vid);

        if (currentIt == current.m_removedVidToRid.end())
        {
            SWSS_LOG_THROW("unable to find VID %s in current view",
                    sai_serialize_object_id(vid).c_str());
        }
    }

    sai_object_id_t rid = currentIt->second;

    SWSS_LOG_INFO("translated VID 0x%" PRIx64 " to RID 0x%" PRIx64, vid, rid);

    return rid;
}

void ComparisonLogic::asic_translate_list_vid_to_rid(
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

void ComparisonLogic::asic_translate_vid_to_rid_list(
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

sai_status_t ComparisonLogic::asic_handle_generic(
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

    sai_object_id_t object_id = meta_key.objectkey.key.object_id;

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            {
                /*
                 * We need to get switch ID, but we know we have one switch so
                 * we could use shortcut here, but let's do this proper way.
                 */

                sai_object_id_t switch_vid = VidManager::switchIdQuery(object_id);

                sai_object_id_t switch_rid = asic_translate_vid_to_rid(current, temporary, switch_vid);

                sai_status_t status = m_vendorSai->create(meta_key, switch_rid, attr_count, attr_list);

                sai_object_id_t real_object_id = meta_key.objectkey.key.object_id;

                if (status == SAI_STATUS_SUCCESS)
                {
                    current.m_ridToVid[real_object_id] = object_id;
                    current.m_vidToRid[object_id] = real_object_id;

                    temporary.m_ridToVid[real_object_id] = object_id;
                    temporary.m_vidToRid[object_id] = real_object_id;

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
                 * from m_removedVidToRid map just in case if there is some bug.
                 */

                current.m_removedVidToRid.erase(object_id);

                sai_status_t status = m_vendorSai->remove(meta_key);

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

                    if (m_switch->isDiscoveredRid(rid))
                    {
                        m_switch->removeExistingObjectReference(rid);
                    }
                }

                return status;
            }

        case SAI_COMMON_API_SET:
            {
                sai_object_id_t rid = asic_translate_vid_to_rid(current, temporary, object_id);

                meta_key.objectkey.key.object_id = rid;

                sai_status_t status = m_vendorSai->set(meta_key, attr_list);

                if (Workaround::isSetAttributeWorkaround(meta_key.objecttype, attr_list->id, status))
                {
                    return SAI_STATUS_SUCCESS;
                }

                if (Workaround::isSetAttributeWorkaroundDuringApplyView(current, object_id, attr_list->id, status)) {
                    return SAI_STATUS_SUCCESS;
                }

                return status;
            }

        default:
            SWSS_LOG_ERROR("other apis not implemented");
            return SAI_STATUS_FAILURE;
    }
}

void ComparisonLogic::asic_translate_vid_to_rid_non_object_id(
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

sai_status_t ComparisonLogic::asic_handle_non_object_id(
        _In_ const AsicView &current,
        _In_ const AsicView &temporary,
        _In_ sai_object_meta_key_t &meta_key,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    asic_translate_vid_to_rid_non_object_id(current, temporary, meta_key);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return m_vendorSai->create(meta_key, SAI_NULL_OBJECT_ID, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return m_vendorSai->remove(meta_key);

        case SAI_COMMON_API_SET:
            return m_vendorSai->set(meta_key, attr_list);

        default:
            SWSS_LOG_ERROR("other apis not implemented");
            return SAI_STATUS_FAILURE;
    }
}

sai_status_t ComparisonLogic::asic_process_event(
        _In_ AsicView& current,
        _In_ AsicView& temporary,
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

    if (m_enableRefernceCountLogs)
    {
        SWSS_LOG_NOTICE("ASIC OP BEFORE : key: %s op: %s", key.c_str(), op.c_str());

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
        // only because user could change notifications he wanted to subscribe
        m_handler->updateNotificationsPointers(attr_count, attr_list);
    }

    if (info->isnonobjectid)
    {
        status = asic_handle_non_object_id(current, temporary, meta_key, api, attr_count, attr_list);
    }
    else
    {
        status = asic_handle_generic(current, temporary, meta_key, api, attr_count, attr_list);
    }

    if (m_enableRefernceCountLogs)
    {
        SWSS_LOG_NOTICE("ASIC OP AFTER : key: %s op: %s", key.c_str(), op.c_str());

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

void ComparisonLogic::executeOperationsOnAsic()
{
    SWSS_LOG_ENTER();

    AsicView& currentView = *m_current;
    AsicView& temporaryView = *m_temp;

    SWSS_LOG_NOTICE("operations to execute on ASIC: %zu", currentView.asicGetOperationsCount());

    try
    {
        //swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_INFO);

        SWSS_LOG_TIMER("asic apply");

        currentView.dumpVidToAsicOperatioId();

        SWSS_LOG_NOTICE("NOT optimized operations");

        for (const auto &op: currentView.asicGetOperations())
        {
            const std::string &key = kfvKey(*op.m_op);
            const std::string &opp = kfvOp(*op.m_op);

            SWSS_LOG_NOTICE("%s: %s", opp.c_str(), key.c_str());

            const auto &values = kfvFieldsValues(*op.m_op);

            if (op.m_currentValue.size() && opp == "set")
            {
                SWSS_LOG_NOTICE("- %s %s (current: %s)",
                        fvField(values.at(0)).c_str(),
                        fvValue(values.at(0)).c_str(),
                        op.m_currentValue.c_str());
            }
            else
            {
                for (auto v: values)
                {
                    SWSS_LOG_NOTICE("- %s %s", fvField(v).c_str(), fvValue(v).c_str());
                }
            }
        }

        SWSS_LOG_NOTICE("optimized operations!");

        std::map<std::string, int> opByObjectType;

        for (const auto &op: currentView.asicGetWithOptimizedRemoveOperations())
        {
            const std::string &key = kfvKey(*op.m_op);
            const std::string &opp = kfvOp(*op.m_op);

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

            sai_status_t status = asic_process_event(currentView, temporaryView, *op.m_op);

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

    SWSS_LOG_NOTICE("performed all operations on asic successfully");
}

void ComparisonLogic::checkMap(
        _In_ const AsicView& current,
        _In_ const AsicView& temp) const
{
    SWSS_LOG_ENTER();

    if ((current.m_ridToVid.size() != temp.m_ridToVid.size()) ||
            (current.m_vidToRid.size() != temp.m_vidToRid.size()))
    {
        // Check all possible differences.

        checkMap(current.m_ridToVid, "current R2V", current.m_vidToRid, "current V2R", temp.m_ridToVid, "temp R2V", temp.m_vidToRid, "temp V2R");
        checkMap(temp.m_ridToVid, "temp R2V", temp.m_vidToRid, "temp V2R", current.m_ridToVid, "current R2V", current.m_vidToRid, "current V2R");

        current.dumpVidToAsicOperatioId();

        SWSS_LOG_THROW("wrong number of vid/rid items in map, forgot to translate? R2V: %zu:%zu, V2R: %zu:%zu, FIXME",
                current.m_ridToVid.size(),
                temp.m_ridToVid.size(),
                current.m_vidToRid.size(),
                temp.m_vidToRid.size());
    }
}
