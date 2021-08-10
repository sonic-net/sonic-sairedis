#include "VirtualSwitchSaiInterface.h"

#include "lib/inc/PerformanceIntervalTimer.h"

#include "swss/logger.h"
#include "swss/exec.h"
#include "swss/converter.h"

#include "meta/sai_serialize.h"
#include "meta/SaiAttributeList.h"

#include <inttypes.h>

#include "meta/sai_serialize.h"

#include "SwitchStateBase.h"
#include "SwitchBCM81724.h"
#include "SwitchBCM56850.h"
#include "SwitchMLNX2700.h"

/*
 * Max number of counters used in 1 api call
 */
#define VS_MAX_COUNTERS 128

#define MAX_HARDWARE_INFO_LENGTH 0x1000

using namespace saivs;
using namespace saimeta;
using namespace sairediscommon;

VirtualSwitchSaiInterface::VirtualSwitchSaiInterface(
        _In_ std::shared_ptr<ContextConfig> contextConfig):
    m_contextConfig(contextConfig)
{
    SWSS_LOG_ENTER();

    m_realObjectIdManager = std::make_shared<RealObjectIdManager>(
            m_contextConfig->m_guid,
            m_contextConfig->m_scc);
}

VirtualSwitchSaiInterface::~VirtualSwitchSaiInterface()
{
    SWSS_LOG_ENTER();

    // empty
}

sai_status_t VirtualSwitchSaiInterface::initialize(
        _In_ uint64_t flags,
        _In_ const sai_service_method_table_t *service_method_table)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t VirtualSwitchSaiInterface::uninitialize(void)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

void VirtualSwitchSaiInterface::setMeta(
        _In_ std::weak_ptr<saimeta::Meta> meta)
{
    SWSS_LOG_ENTER();

    m_meta = meta;
}

std::shared_ptr<WarmBootState> VirtualSwitchSaiInterface::extractWarmBootState(
        _In_ sai_object_id_t switchId)
{
    SWSS_LOG_ENTER();

    auto it = m_warmBootState.find(switchId);

    if (it == m_warmBootState.end())
    {
        SWSS_LOG_WARN("no warm boot state for switch %s",
                sai_serialize_object_id(switchId).c_str());

        return nullptr;
    }

    auto state = std::make_shared<WarmBootState>(it->second); // copy ctr

    // remove warm boot state for switch, each switch can only warm boot once

    m_warmBootState.erase(it);

    return state;
}

bool VirtualSwitchSaiInterface::validate_switch_warm_boot_atributes(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list) const
{
    SWSS_LOG_ENTER();

    /*
     * When in warm boot, as init attributes on switch we only allow
     * notifications and init attribute.  Actually we should check if
     * notifications we pass are the same as the one that we have in dumped db,
     * if not we should set missing one to NULL ptr.
     */

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr_list[i].id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("failed to find metadata for switch attribute %d", attr_list[i].id);
        }

        if (meta->attrid == SAI_SWITCH_ATTR_INIT_SWITCH)
            continue;

        if (meta->attrid == SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO)
            continue;

        if (meta->attrvaluetype == SAI_ATTR_VALUE_TYPE_POINTER)
            continue;

        SWSS_LOG_ERROR("attribute %s not supported in warm boot, expected INIT_SWITCH, HARDWARE_INFO or notification pointer", meta->attridname);

        return false;
    }

    return true;
}

void VirtualSwitchSaiInterface::update_local_metadata(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    auto mmeta = m_meta.lock();

    if (!mmeta)
    {
        SWSS_LOG_THROW("meta pointer expired");
    }

    /*
     * After warm boot we recreated all ASIC state, but since we are using
     * meta_* to check all needed data, we need to use post_create/post_set
     * methods to recreate state in local metadata so when next APIs will be
     * called, we could check the actual state.
     */

    const auto &objectHash = m_switchStateMap.at(switch_id)->m_objectHash;//.at(object_type);

    // first create switch
    // first we need to create all "oid" objects to have reference base
    // then set all object attributes on those oids
    // then create all non oid like route etc.

    /*
     * First update switch, since all non switch objects will be using
     * sai_switch_id_query to check if oid is valid.
     */

    sai_object_meta_key_t mk;

    mk.objecttype = SAI_OBJECT_TYPE_SWITCH;
    mk.objectkey.key.object_id = switch_id;

    mmeta->meta_generic_validation_post_create(mk, switch_id, 0, NULL);

    /*
     * Create every non object id except switch. Switch object was already
     * created above, and non object ids like route may contain other object
     * id's inside *_entry struct, and since metadata is checking reference of
     * those objects, they must exists first.
     */

    for (auto& kvp: objectHash)
    {
        sai_object_type_t ot = kvp.first;

        if (ot == SAI_OBJECT_TYPE_NULL)
            continue;

        if (ot == SAI_OBJECT_TYPE_SWITCH)
            continue;

        auto info = sai_metadata_get_object_type_info(ot);

        if (info == NULL)
            SWSS_LOG_THROW("failed to get object type info for object type %d", ot);

        if (info->isnonobjectid)
            continue;

        mk.objecttype = ot;

        for (auto obj: kvp.second)
        {
            sai_deserialize_object_id(obj.first, mk.objectkey.key.object_id);

            mmeta->meta_generic_validation_post_create(mk, switch_id, 0, NULL);
        }
    }

    /*
     * Create all non object id's. All oids are created, so objects inside
     * *_entry structs can be referenced correctly.
     */

    for (auto& kvp: objectHash)
    {
        sai_object_type_t ot = kvp.first;

        if (ot == SAI_OBJECT_TYPE_NULL)
            continue;

        auto info = sai_metadata_get_object_type_info(ot);

        if (info == NULL)
            SWSS_LOG_THROW("failed to get object type info for object type %d", ot);

        if (info->isobjectid)
            continue;

        for (auto obj: kvp.second)
        {
            std::string key = std::string(info->objecttypename) + ":" + obj.first;

            sai_deserialize_object_meta_key(key, mk);

            mmeta->meta_generic_validation_post_create(mk, switch_id, 0, NULL);
        }
    }

    /*
     * Set all attributes on all objects. Since attributes maybe OID attributes
     * we need to set them too for correct reference count.
     */

    for (auto& kvp: objectHash)
    {
        sai_object_type_t ot = kvp.first;

        if (ot == SAI_OBJECT_TYPE_NULL)
            continue;

        auto info = sai_metadata_get_object_type_info(ot);

        if (info == NULL)
            SWSS_LOG_THROW("failed to get object type info for object type %d", ot);

        for (auto obj: kvp.second)
        {
            std::string key = std::string(info->objecttypename) + ":" + obj.first;

            sai_deserialize_object_meta_key(key, mk);

            for (auto a: obj.second)
            {
                auto meta = a.second->getAttrMetadata();

                if (meta->isreadonly)
                    continue;

                mmeta->meta_generic_validation_post_set(mk, a.second->getAttr());
            }
        }
    }

    /*
     * Since this method is called inside internal_vs_generic_create next
     * meta_generic_validation_post_create will be called after success return
     * of meta_sai_create_oid and it would fail since we already created switch
     * so we need to notify metadata that this is warm boot.
     */

    mmeta->meta_warm_boot_notify();
}

std::string VirtualSwitchSaiInterface::getHardwareInfo(
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList) const
{
    SWSS_LOG_ENTER();

     auto *attr = sai_metadata_get_attr_by_id(
             SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO,
             attrCount,
             attrList);

     if (attr == NULL)
         return "";

     auto& s8list = attr->value.s8list;

     if (s8list.count == 0)
         return "";

     if (s8list.list == NULL)
     {
         SWSS_LOG_WARN("SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO s8list.list is NULL! but count is %u", s8list.count);
         return "";
     }

     uint32_t count = s8list.count;

     if (count > SAI_MAX_HARDWARE_ID_LEN)
     {
         SWSS_LOG_WARN("SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO s8list.count (%u) > SAI_MAX_HARDWARE_ID_LEN (%d), LIMITING !!",
                 count,
                 SAI_MAX_HARDWARE_ID_LEN);

         count = SAI_MAX_HARDWARE_ID_LEN;
     }

     // check actual length, since buffer may contain nulls
     auto actualLength = strnlen((const char*)s8list.list, count);

     return std::string((const char*)s8list.list, actualLength);
}

sai_status_t VirtualSwitchSaiInterface::create(
        _In_ sai_object_type_t objectType,
        _Out_ sai_object_id_t* objectId,
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (!objectId)
    {
        SWSS_LOG_THROW("objectId pointer is NULL");
    }

    if (objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        // for given hardware info we always return same switch id,
        // this is required since we could be performing warm boot here

        auto hwinfo = getHardwareInfo(attr_count, attr_list);

        switchId = m_realObjectIdManager->allocateNewSwitchObjectId(hwinfo);

        *objectId = switchId;

        if (switchId == SAI_NULL_OBJECT_ID)
        {
            SWSS_LOG_ERROR("switch ID allocation failed");

            return SAI_STATUS_FAILURE;
        }

        if (m_switchStateMap.find(switchId) != m_switchStateMap.end())
        {
            if (m_warmBootData.find(switchId) == m_warmBootData.end())
            {
                SWSS_LOG_ERROR("switch %s with hwinfo '%s' already exists",
                        sai_serialize_object_id(switchId).c_str(),
                        hwinfo.c_str());

                return SAI_STATUS_FAILURE;
            }
        }
    }
    else
    {
        // create new real object ID
        *objectId = m_realObjectIdManager->allocateNewObjectId(objectType, switchId);
    }

    std::string str_object_id = sai_serialize_object_id(*objectId);

    return create(
            switchId,
            objectType,
            str_object_id,
            attr_count,
            attr_list);
}

sai_status_t VirtualSwitchSaiInterface::remove(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    return remove(
            switchIdQuery(objectId),
            objectType,
            sai_serialize_object_id(objectId));
}

sai_status_t VirtualSwitchSaiInterface::preSet(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    switch (objectType)
    {
        case SAI_OBJECT_TYPE_PORT:
            return preSetPort(objectId, attr);

        default:
            return SAI_STATUS_SUCCESS;
    }
}

sai_status_t VirtualSwitchSaiInterface::set(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    auto status = preSet(objectType, objectId, attr);

    if (status != SAI_STATUS_SUCCESS)
        return status;

    return set(
            switchIdQuery(objectId),
            objectType,
            sai_serialize_object_id(objectId),
            attr);
}

sai_status_t VirtualSwitchSaiInterface::get(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return get(
            switchIdQuery(objectId),
            objectType,
            sai_serialize_object_id(objectId),
            attr_count,
            attr_list);
}

#define DECLARE_REMOVE_ENTRY(OT,ot)                             \
sai_status_t VirtualSwitchSaiInterface::remove(                 \
        _In_ const sai_ ## ot ## _t* entry)                     \
{                                                               \
    SWSS_LOG_ENTER();                                           \
    return remove(                                              \
            entry->switch_id,                                   \
            SAI_OBJECT_TYPE_ ## OT,                             \
            sai_serialize_ ## ot(*entry));                      \
}

DECLARE_REMOVE_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_REMOVE_ENTRY(INSEG_ENTRY,inseg_entry);
DECLARE_REMOVE_ENTRY(IPMC_ENTRY,ipmc_entry);
DECLARE_REMOVE_ENTRY(L2MC_ENTRY,l2mc_entry);
DECLARE_REMOVE_ENTRY(MCAST_FDB_ENTRY,mcast_fdb_entry);
DECLARE_REMOVE_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);
DECLARE_REMOVE_ENTRY(ROUTE_ENTRY,route_entry);
DECLARE_REMOVE_ENTRY(NAT_ENTRY,nat_entry);

#define DECLARE_CREATE_ENTRY(OT,ot)                             \
sai_status_t VirtualSwitchSaiInterface::create(                 \
        _In_ const sai_ ## ot ## _t* entry,                     \
        _In_ uint32_t attr_count,                               \
        _In_ const sai_attribute_t *attr_list)                  \
{                                                               \
    SWSS_LOG_ENTER();                                           \
    return create(                                              \
            entry->switch_id,                                   \
            SAI_OBJECT_TYPE_ ## OT,                             \
            sai_serialize_ ## ot(*entry),                       \
            attr_count,                                         \
            attr_list);                                         \
}

sai_status_t VirtualSwitchSaiInterface::create(
        _In_ const sai_route_entry_t* entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    static PerformanceIntervalTimer timer("VirtualSwitchSaiInterface::create(route_entry)");

    timer.start();

    auto status = create(
            entry->switch_id,
            SAI_OBJECT_TYPE_ROUTE_ENTRY,
            sai_serialize_route_entry(*entry),
            attr_count,
            attr_list);

    timer.stop();

    timer.inc();

    return status;
}

DECLARE_CREATE_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_CREATE_ENTRY(INSEG_ENTRY,inseg_entry);
DECLARE_CREATE_ENTRY(IPMC_ENTRY,ipmc_entry);
DECLARE_CREATE_ENTRY(L2MC_ENTRY,l2mc_entry);
DECLARE_CREATE_ENTRY(MCAST_FDB_ENTRY,mcast_fdb_entry);
DECLARE_CREATE_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);
//DECLARE_CREATE_ENTRY(ROUTE_ENTRY,route_entry);
DECLARE_CREATE_ENTRY(NAT_ENTRY,nat_entry);

#define DECLARE_SET_ENTRY(OT,ot)                                \
sai_status_t VirtualSwitchSaiInterface::set(                    \
        _In_ const sai_ ## ot ## _t* entry,                     \
        _In_ const sai_attribute_t *attr)                       \
{                                                               \
    SWSS_LOG_ENTER();                                           \
    return set(                                                 \
            entry->switch_id,                                   \
            SAI_OBJECT_TYPE_ ## OT,                             \
            sai_serialize_ ## ot(*entry),                       \
            attr);                                              \
}

DECLARE_SET_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_SET_ENTRY(INSEG_ENTRY,inseg_entry);
DECLARE_SET_ENTRY(IPMC_ENTRY,ipmc_entry);
DECLARE_SET_ENTRY(L2MC_ENTRY,l2mc_entry);
DECLARE_SET_ENTRY(MCAST_FDB_ENTRY,mcast_fdb_entry);
DECLARE_SET_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);
DECLARE_SET_ENTRY(ROUTE_ENTRY,route_entry);
DECLARE_SET_ENTRY(NAT_ENTRY,nat_entry);

std::shared_ptr<SwitchStateBase> VirtualSwitchSaiInterface::init_switch(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<SwitchConfig> config,
        _In_ std::shared_ptr<WarmBootState> warmBootState,
        _In_ std::weak_ptr<saimeta::Meta> meta,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("init");

    if (switch_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_THROW("init switch with NULL switch id is not allowed");
    }

    if (m_switchStateMap.find(switch_id) != m_switchStateMap.end())
    {
        SWSS_LOG_THROW("switch already exists %s", sai_serialize_object_id(switch_id).c_str());
    }

    switch (config->m_switchType)
    {
        case SAI_VS_SWITCH_TYPE_BCM56850:

            m_switchStateMap[switch_id] = std::make_shared<SwitchBCM56850>(switch_id, m_realObjectIdManager, config, warmBootState);
            break;

        case SAI_VS_SWITCH_TYPE_BCM81724:

            m_switchStateMap[switch_id] = std::make_shared<SwitchBCM81724>(switch_id, m_realObjectIdManager, config, warmBootState);
            break;

        case SAI_VS_SWITCH_TYPE_MLNX2700:

            m_switchStateMap[switch_id] = std::make_shared<SwitchMLNX2700>(switch_id, m_realObjectIdManager, config, warmBootState);

            break;

        default:

            SWSS_LOG_WARN("unknown switch type: %d", config->m_switchType);

            return nullptr;
    }

    auto ss = m_switchStateMap.at(switch_id);

    ss->setMeta(meta);

    if (warmBootState != nullptr)
    {
        ss->warm_boot_initialize_objects(); // TODO move to constructor

        SWSS_LOG_NOTICE("initialized switch %s in WARM boot mode", sai_serialize_object_id(switch_id).c_str());

        // XXX lane map may be different after warm boot if ports were added/removed
    }
    else
    {
        sai_status_t status = ss->initialize_default_objects(attr_count, attr_list); // TODO move to constructor

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_THROW("unable to init switch %s", sai_serialize_status(status).c_str());
        }

        SWSS_LOG_NOTICE("initialized switch %s", sai_serialize_object_id(switch_id).c_str());
    }

    return ss;
}

sai_status_t VirtualSwitchSaiInterface::create(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t object_type,
        _In_ const std::string& serializedObjectId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        auto switchIndex = RealObjectIdManager::getSwitchIndex(switchId);

        auto config = m_contextConfig->m_scc->getConfig(switchIndex);

        if (config == nullptr)
        {
            SWSS_LOG_ERROR("failed to get switch config for switch %s, and index %u",
                    serializedObjectId.c_str(),
                    switchIndex);

            return SAI_STATUS_FAILURE;
        }

        std::shared_ptr<WarmBootState> warmBootState = nullptr;

        if (config->m_bootType == SAI_VS_BOOT_TYPE_WARM)
        {
            if (!validate_switch_warm_boot_atributes(attr_count, attr_list))
            {
                SWSS_LOG_ERROR("invalid attribute passed during warm boot");

                return SAI_STATUS_FAILURE;
            }

            warmBootState = extractWarmBootState(switchId);

            if (warmBootState == nullptr)
            {
                SWSS_LOG_ERROR("warm boot was requested on switch %s, but warm boot state is NULL",
                        sai_serialize_object_id(switchId).c_str());

                return SAI_STATUS_FAILURE;
            }
        }

        auto ss = init_switch(switchId, config, warmBootState, m_meta, attr_count, attr_list);

        if (!ss)
        {
            return SAI_STATUS_FAILURE;
        }

        if (warmBootState != nullptr)
        {
            update_local_metadata(switchId);

            if (config->m_useTapDevice)
            {
                ss->vs_recreate_hostif_tap_interfaces();
            }
        }
    }

    auto ss = m_switchStateMap.at(switchId);

    return ss->create(object_type, serializedObjectId, switchId, attr_count, attr_list);
}

void VirtualSwitchSaiInterface::removeSwitch(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    if (m_switchStateMap.find(switch_id) == m_switchStateMap.end())
    {
        SWSS_LOG_THROW("switch doesn't exist 0x%lx", switch_id);
    }

    SWSS_LOG_NOTICE("remove switch 0x%lx", switch_id);

    m_switchStateMap.erase(switch_id);
}

sai_status_t VirtualSwitchSaiInterface::remove(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ const std::string& serializedObjectId)
{
    SWSS_LOG_ENTER();

    auto ss = m_switchStateMap.at(switchId);

    // Perform db dump if warm restart was requested.

    if (objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        sai_attribute_t attr;

        attr.id = SAI_SWITCH_ATTR_RESTART_WARM;

        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);

        if (get(objectType, object_id, 1, &attr) == SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_NOTICE("switch %s SAI_SWITCH_ATTR_RESTART_WARM = %s",
                    sai_serialize_object_id(switchId).c_str(),
                    attr.value.booldata ? "true" : "false");

            if (attr.value.booldata)
            {
                m_warmBootData[switchId] = ss->dump_switch_database_for_warm_restart();
            }
        }
        else
        {
            SWSS_LOG_ERROR("failed to get SAI_SWITCH_ATTR_RESTART_WARM, no DB dump will be performed");
        }
    }

    auto status = ss->remove(objectType, serializedObjectId);

    if (objectType == SAI_OBJECT_TYPE_SWITCH &&
            status == SAI_STATUS_SUCCESS)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);

        SWSS_LOG_NOTICE("removed switch: %s", sai_serialize_object_id(object_id).c_str());

        m_realObjectIdManager->releaseObjectId(object_id);

        removeSwitch(object_id);
    }

    return status;
}

sai_status_t VirtualSwitchSaiInterface::set(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    auto ss = m_switchStateMap.at(switchId);

    return ss->set(objectType, serializedObjectId, attr);
}

sai_status_t VirtualSwitchSaiInterface::get(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ const std::string& serializedObjectId,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto ss = m_switchStateMap.at(switchId);

    return ss->get(objectType, serializedObjectId, attr_count, attr_list);
}

#define DECLARE_GET_ENTRY(OT,ot)                                \
sai_status_t VirtualSwitchSaiInterface::get(                    \
        _In_ const sai_ ## ot ## _t* entry,                     \
        _In_ uint32_t attr_count,                               \
        _Inout_ sai_attribute_t *attr_list)                     \
{                                                               \
    SWSS_LOG_ENTER();                                           \
    return get(                                                 \
            entry->switch_id,                                   \
            SAI_OBJECT_TYPE_ ## OT,                             \
            sai_serialize_ ## ot(*entry),                       \
            attr_count,                                         \
            attr_list);                                         \
}

DECLARE_GET_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_GET_ENTRY(INSEG_ENTRY,inseg_entry);
DECLARE_GET_ENTRY(IPMC_ENTRY,ipmc_entry);
DECLARE_GET_ENTRY(L2MC_ENTRY,l2mc_entry);
DECLARE_GET_ENTRY(MCAST_FDB_ENTRY,mcast_fdb_entry);
DECLARE_GET_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);
DECLARE_GET_ENTRY(ROUTE_ENTRY,route_entry);
DECLARE_GET_ENTRY(NAT_ENTRY,nat_entry);

sai_status_t VirtualSwitchSaiInterface::objectTypeGetAvailability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList,
        _Out_ uint64_t *count)
{
    SWSS_LOG_ENTER();

    // TODO: We should generate this metadata for the virtual switch rather
    // than hard-coding it here.

    if (objectType == SAI_OBJECT_TYPE_DEBUG_COUNTER)
    {
        *count = 3;
        return SAI_STATUS_SUCCESS;
    }
    // MPLS Inseg and MPLS NH CRM use sai_object_type_get_availability() API.
    else if ((objectType == SAI_OBJECT_TYPE_INSEG_ENTRY) ||
             ((objectType == SAI_OBJECT_TYPE_NEXT_HOP) &&
              (attrCount == 1) && attrList &&
              (attrList[0].id == SAI_NEXT_HOP_ATTR_TYPE) &&
              (attrList[0].value.s32 == SAI_NEXT_HOP_TYPE_MPLS)))
    {
        std::string cmd_str("sysctl net.mpls.platform_labels");
        std::string ret_str;
        *count = 1000;
        if (!swss::exec(cmd_str, ret_str))
        {
            std::string match("net.mpls.platform_labels = ");
            if (ret_str.find(match) != std::string::npos)
            {
                *count = std::stoul(ret_str.substr(match.length()).c_str());
            }
        }
        return SAI_STATUS_SUCCESS;
    }

    return SAI_STATUS_NOT_SUPPORTED;
}

sai_status_t VirtualSwitchSaiInterface::queryAttributeCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Out_ sai_attr_capability_t *capability)
{
    SWSS_LOG_ENTER();

    // TODO: We should generate this metadata for the virtual switch rather
    // than hard-coding it here.

    // in virtual switch by default all apis are implemented for all objects. SUCCESS for all attributes

    capability->create_implemented = true;
    capability->set_implemented    = true;
    capability->get_implemented    = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t VirtualSwitchSaiInterface::queryAattributeEnumValuesCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Inout_ sai_s32_list_t *enum_values_capability)
{
    SWSS_LOG_ENTER();

    // TODO: We should generate this metadata for the virtual switch rather
    // than hard-coding it here.

    if (object_type == SAI_OBJECT_TYPE_DEBUG_COUNTER && attr_id == SAI_DEBUG_COUNTER_ATTR_IN_DROP_REASON_LIST)
    {
        if (enum_values_capability->count < 3)
        {
            return SAI_STATUS_BUFFER_OVERFLOW;
        }

        enum_values_capability->count = 3;
        enum_values_capability->list[0] = SAI_IN_DROP_REASON_L2_ANY;
        enum_values_capability->list[1] = SAI_IN_DROP_REASON_L3_ANY;
        enum_values_capability->list[2] = SAI_IN_DROP_REASON_ACL_ANY;

        return SAI_STATUS_SUCCESS;
    }
    else if (object_type == SAI_OBJECT_TYPE_DEBUG_COUNTER && attr_id == SAI_DEBUG_COUNTER_ATTR_OUT_DROP_REASON_LIST)
    {
        if (enum_values_capability->count < 2)
        {
            return SAI_STATUS_BUFFER_OVERFLOW;
        }

        enum_values_capability->count = 2;
        enum_values_capability->list[0] = SAI_OUT_DROP_REASON_L2_ANY;
        enum_values_capability->list[1] = SAI_OUT_DROP_REASON_L3_ANY;

        return SAI_STATUS_SUCCESS;
    }
    else if (object_type == SAI_OBJECT_TYPE_DEBUG_COUNTER && attr_id == SAI_DEBUG_COUNTER_ATTR_TYPE)
    {
        if (enum_values_capability->count < 4)
        {
            return SAI_STATUS_BUFFER_OVERFLOW;
        }

        enum_values_capability->count = 4;
        enum_values_capability->list[0] = SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS;
        enum_values_capability->list[1] = SAI_DEBUG_COUNTER_TYPE_PORT_OUT_DROP_REASONS;
        enum_values_capability->list[2] = SAI_DEBUG_COUNTER_TYPE_SWITCH_IN_DROP_REASONS;
        enum_values_capability->list[3] = SAI_DEBUG_COUNTER_TYPE_SWITCH_OUT_DROP_REASONS;

        return SAI_STATUS_SUCCESS;
    }

    return SAI_STATUS_NOT_SUPPORTED;
}

sai_status_t VirtualSwitchSaiInterface::getStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    /*
     * Get stats is the same as get stats ext with mode == SAI_STATS_MODE_READ.
     */

    return getStatsExt(
            object_type,
            object_id,
            number_of_counters,
            counter_ids,
            SAI_STATS_MODE_READ,
            counters);
}

sai_status_t VirtualSwitchSaiInterface::getStatsExt(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_id = SAI_NULL_OBJECT_ID;

    if (m_switchStateMap.size() == 0)
    {
        SWSS_LOG_ERROR("no switch!, was removed but some function still call");
        return SAI_STATUS_FAILURE;
    }

    if (m_switchStateMap.size() == 1)
    {
        switch_id = m_switchStateMap.begin()->first;
    }
    else
    {
        SWSS_LOG_THROW("multiple switches not supported, FIXME");
    }

    if (m_switchStateMap.find(switch_id) == m_switchStateMap.end())
    {
        SWSS_LOG_ERROR("failed to find switch %s in switch state map", sai_serialize_object_id(switch_id).c_str());

        return SAI_STATUS_FAILURE;
    }

    auto ss = m_switchStateMap.at(switch_id);

    return ss->getStatsExt(
            object_type,
            object_id,
            number_of_counters,
            counter_ids,
            mode,
            counters);
}

sai_status_t VirtualSwitchSaiInterface::clearStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    /*
     * Clear stats is the same as get stats ext with mode ==
     * SAI_STATS_MODE_READ_AND_CLEAR and we just read counters locally and
     * discard them, in that way.
     */

    uint64_t counters[VS_MAX_COUNTERS];

    return getStatsExt(
            object_type,
            object_id,
            number_of_counters,
            counter_ids,
            SAI_STATS_MODE_READ_AND_CLEAR,
            counters);
}

sai_status_t VirtualSwitchSaiInterface::bulkRemove(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    auto ss = m_switchStateMap.at(switchId);

    return ss->bulkRemove(object_type, serialized_object_ids, mode, object_statuses);
}

sai_status_t VirtualSwitchSaiInterface::bulkRemove(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serializedObjectIds;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(sai_serialize_object_id(object_id[idx]));
    }

    auto switchId = switchIdQuery(*object_id);

    return bulkRemove(switchId, object_type, serializedObjectIds, mode, object_statuses);
}

sai_status_t VirtualSwitchSaiInterface::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t *route_entry,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serializedObjectIds;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(sai_serialize_route_entry(route_entry[idx]));
    }

    return bulkRemove(route_entry->switch_id, SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectIds, mode, object_statuses);
}

sai_status_t VirtualSwitchSaiInterface::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_nat_entry_t *nat_entry,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serializedObjectIds;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(sai_serialize_nat_entry(nat_entry[idx]));
    }

    return bulkRemove(nat_entry->switch_id, SAI_OBJECT_TYPE_NAT_ENTRY, serializedObjectIds, mode, object_statuses);
}

sai_status_t VirtualSwitchSaiInterface::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_inseg_entry_t *inseg_entry,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serializedObjectIds;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(sai_serialize_inseg_entry(inseg_entry[idx]));
    }

    return bulkRemove(inseg_entry->switch_id, SAI_OBJECT_TYPE_INSEG_ENTRY, serializedObjectIds, mode, object_statuses);
}

sai_status_t VirtualSwitchSaiInterface::bulkRemove(
        _In_ uint32_t object_count,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serializedObjectIds;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(sai_serialize_fdb_entry(fdb_entry[idx]));
    }

    return bulkRemove(fdb_entry->switch_id, SAI_OBJECT_TYPE_FDB_ENTRY, serializedObjectIds, mode, object_statuses);
}

sai_status_t VirtualSwitchSaiInterface::bulkSet(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serializedObjectIds;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(sai_serialize_object_id(object_id[idx]));
    }

    auto switchId = switchIdQuery(*object_id);

    return bulkSet(switchId, object_type, serializedObjectIds, attr_list, mode, object_statuses);
}

sai_status_t VirtualSwitchSaiInterface::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t *route_entry,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serializedObjectIds;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(sai_serialize_route_entry(route_entry[idx]));
    }

    return bulkSet(route_entry->switch_id, SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectIds, attr_list, mode, object_statuses);
}

sai_status_t VirtualSwitchSaiInterface::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_nat_entry_t *nat_entry,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serializedObjectIds;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(sai_serialize_nat_entry(nat_entry[idx]));
    }

    return bulkSet(nat_entry->switch_id, SAI_OBJECT_TYPE_NAT_ENTRY, serializedObjectIds, attr_list, mode, object_statuses);
}

sai_status_t VirtualSwitchSaiInterface::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_inseg_entry_t *inseg_entry,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serializedObjectIds;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(sai_serialize_inseg_entry(inseg_entry[idx]));
    }

    return bulkSet(inseg_entry->switch_id, SAI_OBJECT_TYPE_INSEG_ENTRY, serializedObjectIds, attr_list, mode, object_statuses);
}

sai_status_t VirtualSwitchSaiInterface::bulkSet(
        _In_ uint32_t object_count,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serializedObjectIds;

    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        serializedObjectIds.emplace_back(sai_serialize_fdb_entry(fdb_entry[idx]));
    }

    return bulkSet(fdb_entry->switch_id, SAI_OBJECT_TYPE_FDB_ENTRY, serializedObjectIds, attr_list, mode, object_statuses);
}

sai_status_t VirtualSwitchSaiInterface::bulkSet(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    auto ss = m_switchStateMap.at(switchId);

    return ss->bulkSet(object_type, serialized_object_ids, attr_list, mode, object_statuses);
}

sai_status_t VirtualSwitchSaiInterface::bulkCreate(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t object_count,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_object_id_t *object_id,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serialized_object_ids;

    // on create vid is put in db by syncd
    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        std::string str_object_id = sai_serialize_object_id(object_id[idx]);
        serialized_object_ids.push_back(str_object_id);
    }

    return bulkCreate(
            switch_id,
            object_type,
            serialized_object_ids,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VirtualSwitchSaiInterface::bulkCreate(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Inout_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    auto ss = m_switchStateMap.at(switchId);

    return ss->bulkCreate(switchId, object_type, serialized_object_ids, attr_count, attr_list, mode, object_statuses);;
}

sai_status_t VirtualSwitchSaiInterface::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t* route_entry,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serialized_object_ids;

    // on create vid is put in db by syncd
    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        std::string str_object_id = sai_serialize_route_entry(route_entry[idx]);
        serialized_object_ids.push_back(str_object_id);
    }

    return bulkCreate(
            route_entry->switch_id,
            SAI_OBJECT_TYPE_ROUTE_ENTRY,
            serialized_object_ids,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VirtualSwitchSaiInterface::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serialized_object_ids;

    // on create vid is put in db by syncd
    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        std::string str_object_id = sai_serialize_fdb_entry(fdb_entry[idx]);
        serialized_object_ids.push_back(str_object_id);
    }

    return bulkCreate(
            fdb_entry->switch_id,
            SAI_OBJECT_TYPE_FDB_ENTRY,
            serialized_object_ids,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VirtualSwitchSaiInterface::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_inseg_entry_t* inseg_entry,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serialized_object_ids;

    // on create vid is put in db by syncd
    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        std::string str_object_id = sai_serialize_inseg_entry(inseg_entry[idx]);
        serialized_object_ids.push_back(str_object_id);
    }

    return bulkCreate(
            inseg_entry->switch_id,
            SAI_OBJECT_TYPE_INSEG_ENTRY,
            serialized_object_ids,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_status_t VirtualSwitchSaiInterface::bulkCreate(
        _In_ uint32_t object_count,
        _In_ const sai_nat_entry_t* nat_entry,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> serialized_object_ids;

    // on create vid is put in db by syncd
    for (uint32_t idx = 0; idx < object_count; idx++)
    {
        std::string str_object_id = sai_serialize_nat_entry(nat_entry[idx]);
        serialized_object_ids.push_back(str_object_id);
    }

    return bulkCreate(
            nat_entry->switch_id,
            SAI_OBJECT_TYPE_NAT_ENTRY,
            serialized_object_ids,
            attr_count,
            attr_list,
            mode,
            object_statuses);
}

sai_object_type_t VirtualSwitchSaiInterface::objectTypeQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    return m_realObjectIdManager->saiObjectTypeQuery(objectId);
}

sai_object_id_t VirtualSwitchSaiInterface::switchIdQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    return m_realObjectIdManager->saiSwitchIdQuery(objectId);
}

sai_status_t VirtualSwitchSaiInterface::logSet(
        _In_ sai_api_t api,
        _In_ sai_log_level_t log_level)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

bool VirtualSwitchSaiInterface::writeWarmBootFile(
        _In_ const char* warmBootFile) const
{
    SWSS_LOG_ENTER();

    if (warmBootFile)
    {
        std::ofstream ofs;

        ofs.open(warmBootFile);

        if (!ofs.is_open())
        {
            SWSS_LOG_ERROR("failed to open: %s", warmBootFile);
            return false;
        }

        if (m_warmBootData.size() == 0)
        {
            SWSS_LOG_WARN("warm boot data is empty, is that what you want?");
        }

        for (auto& kvp: m_warmBootData)
        {
            ofs << kvp.second;
        }

        ofs.close();

        return true;
    }

    if (m_warmBootData.size())
    {
        SWSS_LOG_WARN("warm boot write file is not specified, but SAI_SWITCH_ATTR_RESTART_WARM was set to true!");
    }

    return false;
}

bool VirtualSwitchSaiInterface::readWarmBootFile(
        _In_ const char* warmBootFile)
{
    SWSS_LOG_ENTER();

    if (warmBootFile == NULL)
    {
        SWSS_LOG_ERROR("warm boot read file is NULL");

        return false;
    }

    {
        std::ifstream in(warmBootFile, std::ifstream::ate | std::ifstream::binary);
        SWSS_LOG_NOTICE("%s file size: %zu", warmBootFile, (size_t)in.tellg());
    }

    std::ifstream ifs;

    ifs.open(warmBootFile);

    if (!ifs.is_open())
    {
        SWSS_LOG_ERROR("failed to open: %s", warmBootFile);

        return false;
    }

    std::string line;

    while (std::getline(ifs, line))
    {
        SWSS_LOG_DEBUG("line: %s", line.c_str());

        // line format: OBJECT_TYPE OBJECT_ID ATTR_ID ATTR_VALUE
        std::istringstream iss(line);

        std::string strObjectType;
        std::string strObjectId;
        std::string strAttrId;
        std::string strAttrValue;

        iss >> strObjectType >> strObjectId;

        if (strObjectType == SAI_VS_FDB_INFO)
        {
            /*
             * If we read line from fdb info set and use tap device is enabled
             * just parse line and repopulate fdb info set.
             */

            FdbInfo fi = FdbInfo::deserialize(strObjectId);

            auto switchId = switchIdQuery(fi.m_portId);

            if (switchId == SAI_NULL_OBJECT_ID)
            {
                SWSS_LOG_ERROR("switchIdQuery returned NULL on fi.m_port = %s",
                        sai_serialize_object_id(fi.m_portId).c_str());

                m_warmBootState.clear();
                return false;
            }

            m_warmBootState[switchId].m_switchId = switchId;

            m_warmBootState[switchId].m_fdbInfoSet.insert(fi);

            continue;
        }

        iss >> strAttrId >> strAttrValue;

        sai_object_meta_key_t metaKey;
        sai_deserialize_object_meta_key(strObjectType + ":" + strObjectId, metaKey);

        /*
         * Since all objects we are creating, then during warm boot we need to
         * get the biggest object index, so after warm boot we can start
         * generating new objects with index value not colliding with objects
         * loaded from warm boot scenario. We only need to consider OID
         * objects.
         */

        m_realObjectIdManager->updateWarmBootObjectIndex(metaKey.objectkey.key.object_id);

        // query each object for switch id

        auto switchId = switchIdQuery(metaKey.objectkey.key.object_id);

        if (switchId == SAI_NULL_OBJECT_ID)
        {
            SWSS_LOG_ERROR("switchIdQuery returned NULL on oid = %s",
                    sai_serialize_object_id(metaKey.objectkey.key.object_id).c_str());

            m_warmBootState.clear();
            return false;
        }

        m_warmBootState[switchId].m_switchId = switchId;

        auto &objectHash = m_warmBootState[switchId].m_objectHash[metaKey.objecttype]; // will create if not exist

        if (objectHash.find(strObjectId) == objectHash.end())
        {
            objectHash[strObjectId] = {};
        }

        if (strAttrId == "NULL")
        {
            // skip empty attributes
            continue;
        }

        objectHash[strObjectId][strAttrId] =
            std::make_shared<SaiAttrWrap>(strAttrId, strAttrValue);
    }

    // NOTE notification pointers should be restored by attr_list when creating switch

    ifs.close();

    SWSS_LOG_NOTICE("warm boot file %s stats, loaded switches: %zu", warmBootFile, m_warmBootState.size());

    for (auto& kvp: m_warmBootState)
    {
        size_t count = 0;

        for (auto& o: kvp.second.m_objectHash)
        {
            count += o.second.size();
        }

        SWSS_LOG_NOTICE("switch %s loaded %zu objects",
                sai_serialize_object_id(kvp.first).c_str(),
                count);

        SWSS_LOG_NOTICE("switch %s loaded %zu fdb infos",
                sai_serialize_object_id(kvp.first).c_str(),
                kvp.second.m_fdbInfoSet.size());
    }

    return true;
}

void VirtualSwitchSaiInterface::debugSetStats(
        _In_ sai_object_id_t oid,
        _In_ const std::map<sai_stat_id_t, uint64_t>& stats)
{
    SWSS_LOG_ENTER();

    auto switchId = switchIdQuery(oid);

    auto it = m_switchStateMap.find(switchId);

    if (it == m_switchStateMap.end())
    {
        SWSS_LOG_ERROR("oid %s and it switch %s don't exists in switch state map",
                sai_serialize_object_id(oid).c_str(),
                sai_serialize_object_id(switchId).c_str());

        return;
    }

    it->second->debugSetStats(oid, stats);
}

// those are asynchronous events that are executed under api mutex, we should
// take care and double check if objects received still exists, and we should
// use meta to check that

void VirtualSwitchSaiInterface::syncProcessEventPacket(
        _In_ std::shared_ptr<EventPayloadPacket> payload)
{
    SWSS_LOG_ENTER();

    // this method is executed under mutex, but from other thread so actual
    // switch may not exists

    auto port = payload->getPort();

    auto switchId = switchIdQuery(port);

    auto it = m_switchStateMap.find(switchId);

    if (it == m_switchStateMap.end())
    {
        SWSS_LOG_WARN("oid %s and it switch %s don't exists in switch state map",
                sai_serialize_object_id(port).c_str(),
                sai_serialize_object_id(switchId).c_str());

        return;
    }

    auto& buffer = payload->getBuffer();

    it->second->process_packet_for_fdb_event(
            port,
            payload->getIfName(),
            buffer.getData(),
            buffer.getSize());
}

void VirtualSwitchSaiInterface::syncProcessEventNetLinkMsg(
        _In_ std::shared_ptr<EventPayloadNetLinkMsg> payload)
{
    SWSS_LOG_ENTER();

    auto switchId = payload->getSwitchId();

    auto it = m_switchStateMap.find(switchId);

    if (it == m_switchStateMap.end())
    {
        SWSS_LOG_NOTICE("switch %s don't exists in switch state map",
                sai_serialize_object_id(switchId).c_str());

        return;
    }

    it->second->syncOnLinkMsg(payload);
}
