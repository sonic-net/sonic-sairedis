#include "Sai.h"
#include "SaiInternal.h"
#include "RealObjectIdManager.h"
#include "VirtualSwitchSaiInterface.h"
#include "SwitchStateBase.h"
#include "LaneMapFileParser.h"
#include "HostInterfaceInfo.h"
#include "SwitchConfigContainer.h"
#include "ResourceLimiterParser.h"
#include "CorePortIndexMapFileParser.h"

#include "swss/logger.h"

#include "swss/notificationconsumer.h"
#include "swss/select.h"

#include "saivs.h"

#include <unistd.h>
#include <inttypes.h>

#include <algorithm>
#include <cstring>

using namespace saivs;

#define VS_CHECK_API_INITIALIZED()                                          \
    if (!m_apiInitialized) {                                                \
        SWSS_LOG_ERROR("%s: api not initialized", __PRETTY_FUNCTION__);     \
        return SAI_STATUS_FAILURE; }

Sai::Sai()
{
    SWSS_LOG_ENTER();

    m_unittestChannelRun = false;

    m_fdbAgingThreadRun = false;

    m_eventQueueThreadRun = false;

    m_apiInitialized = false;
}

Sai::~Sai()
{
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        uninitialize();
    }
}

// INITIALIZE UNINITIALIZE

sai_status_t Sai::initialize(
        _In_ uint64_t flags,
        _In_ const sai_service_method_table_t *service_method_table)
{
    MUTEX();

    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: api already initialized", __PRETTY_FUNCTION__);

        return SAI_STATUS_FAILURE;
    }

    if (flags != 0)
    {
        SWSS_LOG_ERROR("invalid flags passed to SAI API initialize");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if ((service_method_table == NULL) ||
            (service_method_table->profile_get_next_value == NULL) ||
            (service_method_table->profile_get_value == NULL))
    {
        SWSS_LOG_ERROR("invalid service_method_table handle passed to SAI API initialize");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    memcpy(&m_service_method_table, service_method_table, sizeof(m_service_method_table));

    auto switch_type = service_method_table->profile_get_value(0, SAI_KEY_VS_SWITCH_TYPE);

    if (switch_type == NULL)
    {
        SWSS_LOG_ERROR("failed to obtain service method table value: %s", SAI_KEY_VS_SWITCH_TYPE);

        return SAI_STATUS_FAILURE;
    }

    auto sai_switch_type = service_method_table->profile_get_value(0, SAI_KEY_VS_SAI_SWITCH_TYPE);
    sai_switch_type_t saiSwitchType;

    if (sai_switch_type == NULL)
    {
        SWSS_LOG_NOTICE("failed to obtain service method table value: %s", SAI_KEY_VS_SAI_SWITCH_TYPE);
        saiSwitchType = SAI_SWITCH_TYPE_NPU;
    } 
    else if (!SwitchConfig::parseSaiSwitchType(sai_switch_type, saiSwitchType))
    {
        return SAI_STATUS_FAILURE;
    }

    auto *laneMapFile = service_method_table->profile_get_value(0, SAI_KEY_VS_INTERFACE_LANE_MAP_FILE);

    m_laneMapContainer = LaneMapFileParser::parseLaneMapFile(laneMapFile);

    auto *corePortIndexMapFile = service_method_table->profile_get_value(0, SAI_KEY_VS_CORE_PORT_INDEX_MAP_FILE);

    m_corePortIndexMapContainer = CorePortIndexMapFileParser::parseCorePortIndexMapFile(corePortIndexMapFile);

    auto *resourceLimiterFile = service_method_table->profile_get_value(0, SAI_KEY_VS_RESOURCE_LIMITER_FILE);

    m_resourceLimiterContainer = ResourceLimiterParser::parseFromFile(resourceLimiterFile);

    auto boot_type          = service_method_table->profile_get_value(0, SAI_KEY_BOOT_TYPE);
    m_warm_boot_read_file   = service_method_table->profile_get_value(0, SAI_KEY_WARM_BOOT_READ_FILE);
    m_warm_boot_write_file  = service_method_table->profile_get_value(0, SAI_KEY_WARM_BOOT_WRITE_FILE);

    sai_vs_boot_type_t bootType;

    if (!SwitchConfig::parseBootType(boot_type, bootType))
    {
        return SAI_STATUS_FAILURE;
    }

    sai_vs_switch_type_t switchType;

    if (!SwitchConfig::parseSwitchType(switch_type, switchType))
    {
        return SAI_STATUS_FAILURE;
    }

    const char *use_tap_dev = service_method_table->profile_get_value(0, SAI_KEY_VS_HOSTIF_USE_TAP_DEVICE);

    auto useTapDevice = SwitchConfig::parseUseTapDevice(use_tap_dev);

    SWSS_LOG_NOTICE("hostif use TAP device: %s", (useTapDevice ? "true" : "false"));

    m_signal = std::make_shared<Signal>();

    m_eventQueue = std::make_shared<EventQueue>(m_signal);

    auto sc = std::make_shared<SwitchConfig>();

    sc->m_saiSwitchType = saiSwitchType;
    sc->m_switchType = switchType;
    sc->m_bootType = bootType;
    sc->m_switchIndex = 0;
    sc->m_useTapDevice = useTapDevice;
    sc->m_laneMap = m_laneMapContainer->getLaneMap(sc->m_switchIndex);
    sc->m_eventQueue = m_eventQueue;
    sc->m_resourceLimiter = m_resourceLimiterContainer->getResourceLimiter(sc->m_switchIndex);
    sc->m_corePortIndexMap = m_corePortIndexMapContainer->getCorePortIndexMap(sc->m_switchIndex);

    auto scc = std::make_shared<SwitchConfigContainer>();

    // TODO add support for multiple switches, (global context?) and config context will need
    // to be passed over SAI_KEY_ service method table, and here we need to load them
    // we also need global context value for those switches (VirtualSwitchSaiInterface/RealObjectIdManager)

    scc->insert(sc);

    // most important

    m_vsSai = std::make_shared<VirtualSwitchSaiInterface>(scc);

    m_meta = std::make_shared<saimeta::Meta>(m_vsSai);

    m_vsSai->setMeta(m_meta);

    if (bootType == SAI_VS_BOOT_TYPE_WARM)
    {
        if (!m_vsSai->readWarmBootFile(m_warm_boot_read_file))
        {
            SWSS_LOG_WARN("failed to read warm boot read file, switching to COLD BOOT");

            sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
        }
    }

    startEventQueueThread();

    startUnittestThread();

    if (saiSwitchType == SAI_SWITCH_TYPE_NPU) 
    {
        startFdbAgingThread();
    }

    m_apiInitialized = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t Sai::uninitialize(void)
{
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    SWSS_LOG_NOTICE("begin");

    // no mutex on uninitialized to prevent deadlock
    // if some thread would try to gram api mutex
    // so threads must be stopped first

    SWSS_LOG_NOTICE("stopping threads");

    stopUnittestThread();

    stopFdbAgingThread();

    stopEventQueueThread();

    // at this point packets may still arrive on hostif but event queue thread
    // ended so they will not be processed

    // NOTE: at this point we can have MUTEX();

    // clear state after ending all threads

    m_vsSai->writeWarmBootFile(m_warm_boot_write_file);

    m_vsSai = nullptr;
    m_meta = nullptr;

    m_apiInitialized = false;

    SWSS_LOG_NOTICE("end");

    return SAI_STATUS_SUCCESS;
}

// QUAD OID

sai_status_t Sai::create(
        _In_ sai_object_type_t objectType,
        _Out_ sai_object_id_t* objectId,
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->create(
            objectType,
            objectId,
            switchId,
            attr_count,
            attr_list);
}

sai_status_t Sai::remove(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->remove(objectType, objectId);
}

sai_status_t Sai::set(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t *attr)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    if (objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        if (attr)
        {
            if (attr->id == SAI_VS_SWITCH_ATTR_META_ENABLE_UNITTESTS)
            {
                m_meta->meta_unittests_enable(attr->value.booldata);
                return SAI_STATUS_SUCCESS;
            }

            if (attr->id == SAI_VS_SWITCH_ATTR_META_ALLOW_READ_ONLY_ONCE)
            {
                return m_meta->meta_unittests_allow_readonly_set_once(SAI_OBJECT_TYPE_SWITCH, attr->value.s32);
            }
        }
    }

    return m_meta->set(objectType, objectId, attr);
}

sai_status_t Sai::get(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->get(
            objectType,
            objectId,
            attr_count,
            attr_list);
}

// QUAD ENTRY

#define DECLARE_CREATE_ENTRY(OT,ot)                         \
sai_status_t Sai::create(                                   \
        _In_ const sai_ ## ot ## _t* entry,                 \
        _In_ uint32_t attr_count,                           \
        _In_ const sai_attribute_t *attr_list)              \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    VS_CHECK_API_INITIALIZED();                             \
    return m_meta->create(entry, attr_count, attr_list);    \
}

DECLARE_CREATE_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_CREATE_ENTRY(INSEG_ENTRY,inseg_entry);
DECLARE_CREATE_ENTRY(IPMC_ENTRY,ipmc_entry);
DECLARE_CREATE_ENTRY(L2MC_ENTRY,l2mc_entry);
DECLARE_CREATE_ENTRY(MCAST_FDB_ENTRY,mcast_fdb_entry);
DECLARE_CREATE_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);
DECLARE_CREATE_ENTRY(ROUTE_ENTRY,route_entry);
DECLARE_CREATE_ENTRY(NAT_ENTRY,nat_entry);


#define DECLARE_REMOVE_ENTRY(OT,ot)                         \
sai_status_t Sai::remove(                                   \
        _In_ const sai_ ## ot ## _t* entry)                 \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    VS_CHECK_API_INITIALIZED();                             \
    return m_meta->remove(entry);                           \
}

DECLARE_REMOVE_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_REMOVE_ENTRY(INSEG_ENTRY,inseg_entry);
DECLARE_REMOVE_ENTRY(IPMC_ENTRY,ipmc_entry);
DECLARE_REMOVE_ENTRY(L2MC_ENTRY,l2mc_entry);
DECLARE_REMOVE_ENTRY(MCAST_FDB_ENTRY,mcast_fdb_entry);
DECLARE_REMOVE_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);
DECLARE_REMOVE_ENTRY(ROUTE_ENTRY,route_entry);
DECLARE_REMOVE_ENTRY(NAT_ENTRY,nat_entry);

#define DECLARE_SET_ENTRY(OT,ot)                            \
sai_status_t Sai::set(                                      \
        _In_ const sai_ ## ot ## _t* entry,                 \
        _In_ const sai_attribute_t *attr)                   \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    VS_CHECK_API_INITIALIZED();                             \
    return m_meta->set(entry, attr);                        \
}

DECLARE_SET_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_SET_ENTRY(INSEG_ENTRY,inseg_entry);
DECLARE_SET_ENTRY(IPMC_ENTRY,ipmc_entry);
DECLARE_SET_ENTRY(L2MC_ENTRY,l2mc_entry);
DECLARE_SET_ENTRY(MCAST_FDB_ENTRY,mcast_fdb_entry);
DECLARE_SET_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);
DECLARE_SET_ENTRY(ROUTE_ENTRY,route_entry);
DECLARE_SET_ENTRY(NAT_ENTRY,nat_entry);

#define DECLARE_GET_ENTRY(OT,ot)                            \
sai_status_t Sai::get(                                      \
        _In_ const sai_ ## ot ## _t* entry,                 \
        _In_ uint32_t attr_count,                           \
        _Inout_ sai_attribute_t *attr_list)                 \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    VS_CHECK_API_INITIALIZED();                             \
    return m_meta->get(entry, attr_count, attr_list);       \
}

DECLARE_GET_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_GET_ENTRY(INSEG_ENTRY,inseg_entry);
DECLARE_GET_ENTRY(IPMC_ENTRY,ipmc_entry);
DECLARE_GET_ENTRY(L2MC_ENTRY,l2mc_entry);
DECLARE_GET_ENTRY(MCAST_FDB_ENTRY,mcast_fdb_entry);
DECLARE_GET_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);
DECLARE_GET_ENTRY(ROUTE_ENTRY,route_entry);
DECLARE_GET_ENTRY(NAT_ENTRY,nat_entry);

// QUAD SERIALIZED

sai_status_t Sai::create(
        _In_ sai_object_type_t object_type,
        _In_ const std::string& serializedObjectId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t Sai::remove(
        _In_ sai_object_type_t objectType,
        _In_ const std::string& serializedObjectId)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t Sai::set(
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t Sai::get(
        _In_ sai_object_type_t objectType,
        _In_ const std::string& serializedObjectId,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

// STATS

sai_status_t Sai::getStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->getStats(
            object_type,
            object_id,
            number_of_counters,
            counter_ids,
            counters);
}

sai_status_t Sai::getStatsExt(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->getStatsExt(
            object_type,
            object_id,
            number_of_counters,
            counter_ids,
            mode,
            counters);
}

sai_status_t Sai::clearStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->clearStats(
            object_type,
            object_id,
            number_of_counters,
            counter_ids);
}

// BULK QUAD OID

sai_status_t Sai::bulkCreate(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t object_count,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_object_id_t *object_id,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->bulkCreate(
            object_type,
            switch_id,
            object_count,
            attr_count,
            attr_list,
            mode,
            object_id,
            object_statuses);
}

sai_status_t Sai::bulkRemove(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->bulkRemove(
            object_type,
            object_count,
            object_id,
            mode,
            object_statuses);
}

sai_status_t Sai::bulkSet(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->bulkSet(
            object_type,
            object_count,
            object_id,
            attr_list,
            mode,
            object_statuses);
}

// BULK QUAD SERIALIZED

sai_status_t Sai::bulkCreate(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Inout_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t Sai::bulkRemove(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t Sai::bulkSet(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

// BULK QUAD ENTRY

#define DECLARE_BULK_CREATE_ENTRY(OT,ot)                    \
sai_status_t Sai::bulkCreate(                               \
        _In_ uint32_t object_count,                         \
        _In_ const sai_ ## ot ## _t* entries,               \
        _In_ const uint32_t *attr_count,                    \
        _In_ const sai_attribute_t **attr_list,             \
        _In_ sai_bulk_op_error_mode_t mode,                 \
        _Out_ sai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    VS_CHECK_API_INITIALIZED();                             \
    return m_meta->bulkCreate(                              \
            object_count,                                   \
            entries,                                        \
            attr_count,                                     \
            attr_list,                                      \
            mode,                                           \
            object_statuses);                               \
}

DECLARE_BULK_CREATE_ENTRY(ROUTE_ENTRY,route_entry)
DECLARE_BULK_CREATE_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_BULK_CREATE_ENTRY(NAT_ENTRY,nat_entry)


// BULK REMOVE

#define DECLARE_BULK_REMOVE_ENTRY(OT,ot)                    \
sai_status_t Sai::bulkRemove(                               \
        _In_ uint32_t object_count,                         \
        _In_ const sai_ ## ot ## _t *entries,               \
        _In_ sai_bulk_op_error_mode_t mode,                 \
        _Out_ sai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    VS_CHECK_API_INITIALIZED();                             \
    return m_meta->bulkRemove(                              \
            object_count,                                   \
            entries,                                        \
            mode,                                           \
            object_statuses);                               \
}

DECLARE_BULK_REMOVE_ENTRY(ROUTE_ENTRY,route_entry)
DECLARE_BULK_REMOVE_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_BULK_REMOVE_ENTRY(NAT_ENTRY,nat_entry)

// BULK SET

#define DECLARE_BULK_SET_ENTRY(OT,ot)                       \
sai_status_t Sai::bulkSet(                                  \
        _In_ uint32_t object_count,                         \
        _In_ const sai_ ## ot ## _t *entries,               \
        _In_ const sai_attribute_t *attr_list,              \
        _In_ sai_bulk_op_error_mode_t mode,                 \
        _Out_ sai_status_t *object_statuses)                \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    VS_CHECK_API_INITIALIZED();                             \
    return m_meta->bulkSet(                                 \
            object_count,                                   \
            entries,                                        \
            attr_list,                                      \
            mode,                                           \
            object_statuses);                               \
}

DECLARE_BULK_SET_ENTRY(ROUTE_ENTRY,route_entry);
DECLARE_BULK_SET_ENTRY(FDB_ENTRY,fdb_entry);
DECLARE_BULK_SET_ENTRY(NAT_ENTRY,nat_entry);

// NON QUAD API

sai_status_t Sai::flushFdbEntries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->flushFdbEntries(
            switch_id,
            attr_count,
            attr_list);
}

// SAI API

sai_status_t Sai::objectTypeGetAvailability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList,
        _Out_ uint64_t *count)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->objectTypeGetAvailability(
            switchId,
            objectType,
            attrCount,
            attrList,
            count);
}

sai_status_t Sai::queryAttributeCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _Out_ sai_attr_capability_t *capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->queryAttributeCapability(
            switchId,
            objectType,
            attrId,
            capability);
}

sai_status_t Sai::queryAattributeEnumValuesCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Inout_ sai_s32_list_t *enum_values_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->queryAattributeEnumValuesCapability(
            switch_id,
            object_type,
            attr_id,
            enum_values_capability);
}

sai_object_type_t Sai::objectTypeQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: SAI API not initialized", __PRETTY_FUNCTION__);

        return SAI_OBJECT_TYPE_NULL;
    }

    // not need for metadata check or mutex since this method is static

    return RealObjectIdManager::objectTypeQuery(objectId);
}

sai_object_id_t Sai::switchIdQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("%s: SAI API not initialized", __PRETTY_FUNCTION__);

        return SAI_NULL_OBJECT_ID;
    }

    // not need for metadata check or mutex since this method is static

    return RealObjectIdManager::switchIdQuery(objectId);
}

sai_status_t Sai::logSet(
        _In_ sai_api_t api,
        _In_ sai_log_level_t log_level)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->logSet(api, log_level);
}
