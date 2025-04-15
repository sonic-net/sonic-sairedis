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
#include "ContextConfigContainer.h"

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

    m_globalContext = 0;
}

Sai::~Sai()
{
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        apiUninitialize();
    }
}

// INITIALIZE UNINITIALIZE

sai_status_t Sai::apiInitialize(
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

    auto *fabricLaneMapFile = service_method_table->profile_get_value(0, SAI_KEY_VS_INTERFACE_FABRIC_LANE_MAP_FILE);
    if (fabricLaneMapFile)
    {
        m_fabricLaneMapContainer = LaneMapFileParser::parseLaneMapFile(fabricLaneMapFile);
    }

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

    auto useTapDevice = SwitchConfig::parseBool(use_tap_dev);

    const char *bfd_offload_supported = service_method_table->profile_get_value(0, SAI_KEY_VS_BFD_OFFLOAD_SUPPORTED);
    auto bfdOffloadSupported = SwitchConfig::parseBfdOffloadSupported(bfd_offload_supported);

    SWSS_LOG_NOTICE("hostif use TAP device: %s", (useTapDevice ? "true" : "false"));

    const char *use_configured_speed_as_oper_speed = service_method_table->profile_get_value(0, SAI_KEY_VS_USE_CONFIGURED_SPEED_AS_OPER_SPEED);

    auto useConfiguredSpeedAsOperSpeed = SwitchConfig::parseBool(use_configured_speed_as_oper_speed);

    SWSS_LOG_NOTICE("use configured speed as oper speed: %s", (useConfiguredSpeedAsOperSpeed ? "true" : "false"));

    auto cstrGlobalContext = service_method_table->profile_get_value(0, SAI_KEY_VS_GLOBAL_CONTEXT);

    m_globalContext = 0;

    if (cstrGlobalContext != nullptr)
    {
        if (sscanf(cstrGlobalContext, "%u", &m_globalContext) != 1)
        {
            SWSS_LOG_WARN("failed to parse '%s' as uint32 using default globalContext", cstrGlobalContext);

            m_globalContext = 0;
        }
    }

    SWSS_LOG_NOTICE("using globalContext = %u", m_globalContext);

    auto cstrContextConfig = service_method_table->profile_get_value(0, SAI_KEY_VS_CONTEXT_CONFIG);

    auto ccc = ContextConfigContainer::loadFromFile(cstrContextConfig);

    for (auto& cc: ccc->getAllContextConfigs())
    {
        auto context = std::make_shared<Context>(cc);

        m_contextMap[cc->m_guid] = context;
    }

    auto context = getContext(m_globalContext);

    if (context == nullptr)
    {
        SWSS_LOG_ERROR("no context defined for global context %u", m_globalContext);

        return SAI_STATUS_FAILURE;
    }

    auto contextConfig = context->getContextConfig();

    auto scc = contextConfig->m_scc;

    if (scc->getSwitchConfigs().size() == 0)
    {
        SWSS_LOG_WARN("no switch configs defined, using default switch config");

        auto sc = std::make_shared<SwitchConfig>(0, "");

        scc->insert(sc);
    }

    // TODO currently switch configuration will share signal and event queue
    // but it should be moved to Context class

    m_signal = std::make_shared<Signal>();

    m_eventQueue = std::make_shared<EventQueue>(m_signal);

    for (auto& sc: scc->getSwitchConfigs())
    {
        // NOTE: switch index and hardware info is already populated

        sc->m_saiSwitchType = saiSwitchType;
        sc->m_switchType = switchType;
        sc->m_bootType = bootType;
        sc->m_useTapDevice = useTapDevice;
        sc->m_useConfiguredSpeedAsOperSpeed = useConfiguredSpeedAsOperSpeed;
        sc->m_laneMap = m_laneMapContainer->getLaneMap(sc->m_switchIndex);
        sc->m_bfdOffload = bfdOffloadSupported;

        if (sc->m_laneMap == nullptr)
        {
            SWSS_LOG_WARN("lane map for switch index %u is empty, loading default map (may have ifname conflict)", sc->m_switchIndex);

            sc->m_laneMap = LaneMap::getDefaultLaneMap(sc->m_switchIndex);
        }

        if (m_fabricLaneMapContainer)
        {
            sc->m_fabricLaneMap = m_fabricLaneMapContainer->getLaneMap(sc->m_switchIndex);
        }

        sc->m_eventQueue = m_eventQueue;
        sc->m_resourceLimiter = m_resourceLimiterContainer->getResourceLimiter(sc->m_switchIndex);
        sc->m_corePortIndexMap = m_corePortIndexMapContainer->getCorePortIndexMap(sc->m_switchIndex);
    }

    // most important

    // TODO move to Context class

    m_vsSai = std::make_shared<VirtualSwitchSaiInterface>(contextConfig);

    m_meta = std::make_shared<saimeta::Meta>(m_vsSai);

    m_vsSai->setMeta(m_meta);

    if (bootType == SAI_VS_BOOT_TYPE_WARM)
    {
        if (!m_vsSai->readWarmBootFile(m_warm_boot_read_file))
        {
            SWSS_LOG_WARN("failed to read warm boot read file, switching to COLD BOOT");

            for (auto& sc: scc->getSwitchConfigs())
            {
                sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
            }
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

sai_status_t Sai::apiUninitialize(void)
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

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_CREATE_ENTRY);

#define DECLARE_REMOVE_ENTRY(OT,ot)                         \
sai_status_t Sai::remove(                                   \
        _In_ const sai_ ## ot ## _t* entry)                 \
{                                                           \
    MUTEX();                                                \
    SWSS_LOG_ENTER();                                       \
    VS_CHECK_API_INITIALIZED();                             \
    return m_meta->remove(entry);                           \
}

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_REMOVE_ENTRY);

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

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_SET_ENTRY);

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

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_GET_ENTRY);

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

sai_status_t Sai::queryStatsCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _Inout_ sai_stat_capability_list_t *stats_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->queryStatsCapability(
            switchId,
            objectType,
            stats_capability);
}

sai_status_t Sai::queryStatsStCapability(
    _In_ sai_object_id_t switchId,
    _In_ sai_object_type_t objectType,
    _Inout_ sai_stat_st_capability_list_t *stats_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->queryStatsStCapability(
        switchId,
        objectType,
        stats_capability);
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

sai_status_t Sai::bulkGetStats(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_key_t *object_key,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Inout_ sai_status_t *object_statuses,
        _Out_ uint64_t *counters)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t Sai::bulkClearStats(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_key_t *object_key,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Inout_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return SAI_STATUS_NOT_IMPLEMENTED;
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

sai_status_t Sai::bulkGet(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ const uint32_t *attr_count,
        _Inout_ sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->bulkGet(
            object_type,
            object_count,
            object_id,
            attr_count,
            attr_list,
            mode,
            object_statuses);
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

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_CREATE_ENTRY);

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

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_REMOVE_ENTRY);

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

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_SET_ENTRY);

// BULK GET

#define DECLARE_BULK_GET_ENTRY(OT,ot)                       \
sai_status_t Sai::bulkGet(                                  \
        _In_ uint32_t object_count,                         \
        _In_ const sai_ ## ot ## _t *ot,                    \
        _In_ const uint32_t *attr_count,                    \
        _Inout_ sai_attribute_t **attr_list,                \
        _In_ sai_bulk_op_error_mode_t mode,                 \
        _Out_ sai_status_t *object_statuses)                \
{                                                           \
    SWSS_LOG_ENTER();                                       \
    MUTEX();                                                \
    VS_CHECK_API_INITIALIZED();                             \
    SWSS_LOG_ERROR("FIXME not implemented");                \
    return SAI_STATUS_NOT_IMPLEMENTED;                      \
}

SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_GET_ENTRY);

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

sai_status_t Sai::queryAttributeEnumValuesCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Inout_ sai_s32_list_t *enum_values_capability)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->queryAttributeEnumValuesCapability(
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

sai_status_t Sai::queryApiVersion(
        _Out_ sai_api_version_t *version)
{
    MUTEX();
    SWSS_LOG_ENTER();
    VS_CHECK_API_INITIALIZED();

    return m_meta->queryApiVersion(version);
}

std::shared_ptr<Context> Sai::getContext(
        _In_ uint32_t globalContext) const
{
    SWSS_LOG_ENTER();

    auto it = m_contextMap.find(globalContext);

    if (it == m_contextMap.end())
        return nullptr;

    return it->second;
}
