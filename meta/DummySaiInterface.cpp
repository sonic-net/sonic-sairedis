#include "DummySaiInterface.h"

#include "swss/logger.h"

#include <memory>
#include <cstring>

#define MUTEX() std::lock_guard<std::mutex> _lock(m_mutex)

using namespace saimeta;

DummySaiInterface::DummySaiInterface():
    m_status(SAI_STATUS_SUCCESS),
    m_apiInitialized(false),
    m_runThread(false)
{
    SWSS_LOG_ENTER();

    memset(&m_sn, 0, sizeof(m_sn));
}

DummySaiInterface::~DummySaiInterface()
{
    SWSS_LOG_ENTER();

    if (m_apiInitialized)
    {
        apiUninitialize();
    }

    stop();
}

void DummySaiInterface::setStatus(
        _In_ sai_status_t status)
{
    SWSS_LOG_ENTER();

    m_status = status;
}

sai_status_t DummySaiInterface::apiInitialize(
        _In_ uint64_t flags,
        _In_ const sai_service_method_table_t *smt)
{
    SWSS_LOG_ENTER();

    memset(&m_sn, 0, sizeof(m_sn));

    if (smt && smt->profile_get_value)
    {
        SWSS_LOG_NOTICE("Dummy: profile_get_value(NULL): %s", smt->profile_get_value(0, NULL));
        SWSS_LOG_NOTICE("Dummy: profile_get_value(FOO): %s", smt->profile_get_value(0, "FOO"));
        SWSS_LOG_NOTICE("Dummy: profile_get_value(CAR): %s", smt->profile_get_value(0, "CAR"));
    }

    if (smt && smt->profile_get_next_value)
    {

        const char *var = NULL;
        const char *val = NULL;

        SWSS_LOG_NOTICE("Dummy: profile_get_next_value: %d", smt->profile_get_next_value(0, NULL, NULL));
        SWSS_LOG_NOTICE("Dummy: profile_get_next_value: %d", smt->profile_get_next_value(0, NULL, &val));
        SWSS_LOG_NOTICE("Dummy: profile_get_next_value: %d", smt->profile_get_next_value(0, &var, NULL));
        SWSS_LOG_NOTICE("Dummy: profile_get_next_value: %d", smt->profile_get_next_value(0, &var, &val));
        SWSS_LOG_NOTICE("Dummy: profile_get_next_value: %d", smt->profile_get_next_value(0, &var, &val));
    }

    m_apiInitialized = true;

    return SAI_STATUS_SUCCESS;
}

sai_status_t DummySaiInterface::apiUninitialize(void)
{
    SWSS_LOG_ENTER();

    m_apiInitialized = false;

    return SAI_STATUS_SUCCESS;
}

sai_status_t DummySaiInterface::create(
        _In_ sai_object_type_t objectType,
        _Out_ sai_object_id_t* objectId,
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // TODO implement some dummy OID handling

    if (objectId && m_status == SAI_STATUS_SUCCESS)
    {
        *objectId = (sai_object_id_t)1;
    }

    if (m_status == SAI_STATUS_SUCCESS && objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        updateNotificationPointers(attr_count, attr_list);
    }

    return m_status;
}

sai_status_t DummySaiInterface::remove(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::set(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    if (m_status == SAI_STATUS_SUCCESS && objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        updateNotificationPointers(1, attr);
    }

    return m_status;
}

sai_status_t DummySaiInterface::get(
        _In_ sai_object_type_t objectType,
        _In_ sai_object_id_t objectId,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return m_status;
}

#define DECLARE_REMOVE_ENTRY(OT,ot)                 \
sai_status_t DummySaiInterface::remove(             \
        _In_ const sai_ ## ot ## _t* ot)            \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return m_status;                                \
}

#define DECLARE_CREATE_ENTRY(OT,ot)                 \
sai_status_t DummySaiInterface::create(             \
        _In_ const sai_ ## ot ## _t* ot,            \
        _In_ uint32_t attr_count,                   \
        _In_ const sai_attribute_t *attr_list)      \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return m_status;                                \
}

#define DECLARE_SET_ENTRY(OT,ot)                    \
sai_status_t DummySaiInterface::set(                \
        _In_ const sai_ ## ot ## _t* ot,            \
        _In_ const sai_attribute_t *attr)           \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return m_status;                                \
}

#define DECLARE_GET_ENTRY(OT,ot)                    \
sai_status_t DummySaiInterface::get(                \
        _In_ const sai_ ## ot ## _t* ot,            \
        _In_ uint32_t attr_count,                   \
        _Inout_ sai_attribute_t *attr_list)         \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return m_status;                                \
}

#define DECLARE_BULK_CREATE_ENTRY(OT,ot)                \
sai_status_t DummySaiInterface::bulkCreate(             \
        _In_ uint32_t object_count,                     \
        _In_ const sai_ ## ot ## _t* ot,                \
        _In_ const uint32_t *attr_count,                \
        _In_ const sai_attribute_t **attr_list,         \
        _In_ sai_bulk_op_error_mode_t mode,             \
        _Out_ sai_status_t *object_statuses)            \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    for (uint32_t idx = 0; idx < object_count; idx++)   \
        object_statuses[idx] = m_status;                \
    return m_status;                                    \
}

#define DECLARE_BULK_REMOVE_ENTRY(OT,ot)                \
sai_status_t DummySaiInterface::bulkRemove(             \
        _In_ uint32_t object_count,                     \
        _In_ const sai_ ## ot ## _t* ot,                \
        _In_ sai_bulk_op_error_mode_t mode,             \
        _Out_ sai_status_t *object_statuses)            \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    for (uint32_t idx = 0; idx < object_count; idx++)   \
        object_statuses[idx] = m_status;                \
    return m_status;                                    \
}

#define DECLARE_BULK_SET_ENTRY(OT,ot)                   \
sai_status_t DummySaiInterface::bulkSet(                \
        _In_ uint32_t object_count,                     \
        _In_ const sai_ ## ot ## _t* ot,                \
        _In_ const sai_attribute_t *attr_list,          \
        _In_ sai_bulk_op_error_mode_t mode,             \
        _Out_ sai_status_t *object_statuses)            \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    for (uint32_t idx = 0; idx < object_count; idx++)   \
        object_statuses[idx] = m_status;                \
    return m_status;                                    \
}

#define DECLARE_BULK_GET_ENTRY(OT,ot)                       \
sai_status_t DummySaiInterface::bulkGet(                    \
        _In_ uint32_t object_count,                         \
        _In_ const sai_ ## ot ## _t *ot,                    \
        _In_ const uint32_t *attr_count,                    \
        _Inout_ sai_attribute_t **attr_list,                \
        _In_ sai_bulk_op_error_mode_t mode,                 \
        _Out_ sai_status_t *object_statuses)                \
{                                                           \
    SWSS_LOG_ENTER();                                       \
    SWSS_LOG_ERROR("FIXME not implemented");                \
    return SAI_STATUS_NOT_IMPLEMENTED;                      \
}

// NON QUAD API

SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_REMOVE_ENTRY);
SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_CREATE_ENTRY);
SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_SET_ENTRY);
SAIREDIS_DECLARE_EVERY_ENTRY(DECLARE_GET_ENTRY);
SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_CREATE_ENTRY);
SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_REMOVE_ENTRY);
SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_SET_ENTRY);
SAIREDIS_DECLARE_EVERY_BULK_ENTRY(DECLARE_BULK_GET_ENTRY);

sai_status_t DummySaiInterface::flushFdbEntries(
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::switchMdioRead(
    _In_ sai_object_id_t switchId,
    _In_ uint32_t device_addr,
    _In_ uint32_t start_reg_addr,
    _In_ uint32_t number_of_registers,
    _Out_ uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::switchMdioWrite(
    _In_ sai_object_id_t switchId,
    _In_ uint32_t device_addr,
    _In_ uint32_t start_reg_addr,
    _In_ uint32_t number_of_registers,
    _In_ const uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::switchMdioCl22Read(
    _In_ sai_object_id_t switchId,
    _In_ uint32_t device_addr,
    _In_ uint32_t start_reg_addr,
    _In_ uint32_t number_of_registers,
    _Out_ uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::switchMdioCl22Write(
    _In_ sai_object_id_t switchId,
    _In_ uint32_t device_addr,
    _In_ uint32_t start_reg_addr,
    _In_ uint32_t number_of_registers,
    _In_ const uint32_t *reg_val)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::objectTypeGetAvailability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList,
        _Out_ uint64_t *count)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::queryAttributeCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _Out_ sai_attr_capability_t *capability)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::queryAttributeEnumValuesCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId,
        _Inout_ sai_s32_list_t *enumValuesCapability)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::getStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    // TODO add recording

    return m_status;
}

sai_status_t DummySaiInterface::queryStatsCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _Inout_ sai_stat_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::queryStatsStCapability(
    _In_ sai_object_id_t switchId,
    _In_ sai_object_type_t objectType,
    _Inout_ sai_stat_st_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::getStatsExt(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::clearStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::bulkGetStats(
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
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::bulkClearStats(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_key_t *object_key,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Inout_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    return m_status;
}

// bulk QUAD

sai_status_t DummySaiInterface::bulkRemove(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < object_count; idx++)
        object_statuses[idx] = m_status;

    return m_status;
}

sai_status_t DummySaiInterface::bulkSet(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < object_count; idx++)
        object_statuses[idx] = m_status;

    return m_status;
}

sai_status_t DummySaiInterface::bulkGet(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ const uint32_t *attr_count,
        _Inout_ sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented, FIXME");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t DummySaiInterface::bulkCreate(
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

    for (uint32_t idx = 0; idx < object_count; idx++)
        object_statuses[idx] = m_status;

    return m_status;
}

sai_object_type_t DummySaiInterface::objectTypeQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (m_status != SAI_STATUS_SUCCESS)
        SWSS_LOG_THROW("not implemented");

    return SAI_OBJECT_TYPE_NULL;
}

sai_object_id_t DummySaiInterface::switchIdQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (m_status != SAI_STATUS_SUCCESS)
        SWSS_LOG_THROW("not implemented");

    return SAI_NULL_OBJECT_ID;
}

sai_status_t DummySaiInterface::logSet(
        _In_ sai_api_t api,
        _In_ sai_log_level_t log_level)
{
    SWSS_LOG_ENTER();

    return m_status;
}

sai_status_t DummySaiInterface::queryApiVersion(
        _Out_ sai_api_version_t *version)
{
    SWSS_LOG_ENTER();

    if (version)
    {
        *version = SAI_API_VERSION;

        return m_status;
    }

    SWSS_LOG_ERROR("version parameter is NULL");

    return m_status;
}

void DummySaiInterface::updateNotificationPointers(
        _In_ uint32_t count,
        _In_ const sai_attribute_t* attrs)
{
    SWSS_LOG_ENTER();

    sai_metadata_update_switch_notification_pointers(&m_sn, count, attrs);
}

sai_status_t DummySaiInterface::start()
{
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_ERROR("api not initialized");

        return SAI_STATUS_FAILURE;
    }

    MUTEX();

    if (m_runThread)
    {
        SWSS_LOG_NOTICE("thread already is running");

        return SAI_STATUS_SUCCESS;
    }

    m_runThread = true;

    m_thread = std::make_shared<std::thread>(&DummySaiInterface::run, this);

    return SAI_STATUS_SUCCESS;
}

sai_status_t DummySaiInterface::stop()
{
    SWSS_LOG_ENTER();

    MUTEX();

    m_runThread = false;

    if (m_thread)
    {
        SWSS_LOG_NOTICE("joining thread");

        m_thread->join();

        m_thread = nullptr;
    }

    return SAI_STATUS_SUCCESS;
}

void DummySaiInterface::run()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("starting dummy notification thread");

    while(m_runThread)
    {
        sai_attr_id_t id;

        if (tryGetNotificationToSend(id))
        {
            sendNotification(id);
            continue;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(64));
    }

    SWSS_LOG_NOTICE("ending dummy notification thread");
}

sai_status_t DummySaiInterface::enqueueNotificationToSend(
        _In_ sai_attr_id_t id)
{
    SWSS_LOG_ENTER();

    if (!m_apiInitialized)
    {
        SWSS_LOG_NOTICE("api not initialized");

        return SAI_STATUS_FAILURE;
    }

    MUTEX();

    m_queue.push(id);

    return SAI_STATUS_SUCCESS;
}

bool DummySaiInterface::tryGetNotificationToSend(
        _Out_ sai_attr_id_t& id)
{
    SWSS_LOG_ENTER();

    MUTEX();

    if (m_queue.empty())
        return false;

    id = m_queue.front();

    m_queue.pop();

    return true;
}

void DummySaiInterface::sendNotification(
        _In_ sai_attr_id_t id)
{
    SWSS_LOG_ENTER();

    // get local copy, in case m_sn will change
    // this probably should be under separate mutex (m_sn)

    sai_switch_notifications_t sn = m_sn;

    auto* m = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, id);

    switch (id)
    {
        case SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY:

            if (sn.on_switch_state_change)
            {
                SWSS_LOG_NOTICE("sending sn.on_switch_state_change");

                sn.on_switch_state_change(0x1, SAI_SWITCH_OPER_STATUS_UP);
            }
            else
            {
                SWSS_LOG_WARN("pointer sn.on_switch_state_change is NULL");
            }
            break;

        case SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY:

            if (sn.on_fdb_event)
            {
                SWSS_LOG_NOTICE("sending sn.on_fdb_event");

                sai_fdb_event_notification_data_t data;

                data.event_type = SAI_FDB_EVENT_LEARNED;
                data.fdb_entry.switch_id = 0x1;
                data.fdb_entry.mac_address[0] = 0x11;
                data.fdb_entry.mac_address[1] = 0x22;
                data.fdb_entry.mac_address[2] = 0x33;
                data.fdb_entry.mac_address[3] = 0x44;
                data.fdb_entry.mac_address[4] = 0x55;
                data.fdb_entry.mac_address[5] = 0x66;
                data.fdb_entry.bv_id = 0x2;
                data.attr_count = 0;
                data.attr = nullptr;

                sn.on_fdb_event(1, &data);
            }
            else
            {
                SWSS_LOG_WARN("pointer sn.on_fdb_event is NULL");
            }
            break;


        case SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY:

            if (sn.on_port_state_change)
            {
                SWSS_LOG_NOTICE("sending sn.on_port_state_change");

                sai_port_oper_status_notification_t data;

                data.port_id = 0x2;
                data.port_state = SAI_PORT_OPER_STATUS_UP;

                sn.on_port_state_change(1, &data);
            }
            else
            {
                SWSS_LOG_WARN("pointer sn.on_port_state_change is NULL");
            }
            break;

        case SAI_SWITCH_ATTR_SHUTDOWN_REQUEST_NOTIFY:

            if (sn.on_switch_shutdown_request)
            {
                SWSS_LOG_NOTICE("sending sn.on_switch_shutdown_request");

                sn.on_switch_shutdown_request(0x1);
            }
            else
            {
                SWSS_LOG_WARN("pointer sn.on_switch_shutdown_request is NULL");
            }
            break;

        case SAI_SWITCH_ATTR_NAT_EVENT_NOTIFY:

            if (sn.on_nat_event)
            {
                SWSS_LOG_NOTICE("sending sn.on_nat_event");

                sai_nat_event_notification_data_t data;

                data.event_type = SAI_NAT_EVENT_NONE;
                data.nat_entry.switch_id = 0x1;
                data.nat_entry.vr_id = 0x2;
                data.nat_entry.nat_type = SAI_NAT_TYPE_NONE;

                memset(&data.nat_entry.data, 0, sizeof(data.nat_entry.data));

                sn.on_nat_event(1, &data);
            }
            else
            {
                SWSS_LOG_WARN("pointer sn.on_nat_event is NULL");
            }
            break;


        case SAI_SWITCH_ATTR_PORT_HOST_TX_READY_NOTIFY:

            if (sn.on_nat_event)
            {
                SWSS_LOG_NOTICE("sending sn.on_port_host_tx_ready");

                sn.on_port_host_tx_ready(0x1, 0x2, SAI_PORT_HOST_TX_READY_STATUS_NOT_READY);
            }
            else
            {
                SWSS_LOG_WARN("pointer sn.on_port_host_tx_readyis NULL");
            }
            break;

        case SAI_SWITCH_ATTR_SWITCH_ASIC_SDK_HEALTH_EVENT_NOTIFY:

            if (sn.on_nat_event)
            {
                SWSS_LOG_NOTICE("sending sn.on_switch_asic_sdk_health_event");

                sai_timespec_t timespec;

                timespec.tv_sec = 0;
                timespec.tv_nsec = 0;

                sai_switch_health_data_t hd;

                hd.data_type = SAI_HEALTH_DATA_TYPE_GENERAL;

                sai_u8_list_t desc;
                desc.count = 0;
                desc.list = NULL;

                sn.on_switch_asic_sdk_health_event(
                        0x1,
                        SAI_SWITCH_ASIC_SDK_HEALTH_SEVERITY_NOTICE,
                        timespec,
                        SAI_SWITCH_ASIC_SDK_HEALTH_CATEGORY_SW,
                        hd,
                        desc);
            }
            else
            {
                SWSS_LOG_WARN("pointer sn.sn.on_switch_asic_sdk_health_event");
            }
            break;

        case SAI_SWITCH_ATTR_QUEUE_PFC_DEADLOCK_NOTIFY:

            if (sn.on_queue_pfc_deadlock)
            {
                SWSS_LOG_NOTICE("sending sn.on_queue_pfc_deadlock");

                sai_queue_deadlock_notification_data_t data;

                data.queue_id = 0x2;
                data.event = SAI_QUEUE_PFC_DEADLOCK_EVENT_TYPE_DETECTED;
                data.app_managed_recovery = true;

                sn.on_queue_pfc_deadlock(1, &data);
            }
            else
            {
                SWSS_LOG_WARN("pointer sn.on_port_host_tx_readyis NULL");
            }
            break;

        case SAI_SWITCH_ATTR_BFD_SESSION_STATE_CHANGE_NOTIFY:

            if (sn.on_bfd_session_state_change)
            {
                SWSS_LOG_NOTICE("sending sn.on_bfd_session_state_change");

                sai_bfd_session_state_notification_t data;

                data.bfd_session_id = 0x2;
                data.session_state = SAI_BFD_SESSION_STATE_ADMIN_DOWN;

                sn.on_bfd_session_state_change(1, &data);
            }
            else
            {
                SWSS_LOG_WARN("pointer sn.on_bfd_session_state_change");
            }
            break;

        case SAI_SWITCH_ATTR_TWAMP_SESSION_EVENT_NOTIFY:

            if (sn.on_twamp_session_event)
            {
                SWSS_LOG_NOTICE("sending sn.on_twamp_session_event");

                sai_twamp_session_event_notification_data_t data;

                data.twamp_session_id = 0x1;
                data.session_state = SAI_TWAMP_SESSION_STATE_INACTIVE;

                data.session_stats.index = 0;
                data.session_stats.number_of_counters = 0;
                data.session_stats.counters_ids = nullptr;
                data.session_stats.counters = nullptr;

                sn.on_twamp_session_event(1, &data);
            }
            else
            {
                SWSS_LOG_WARN("pointer sn.on_twamp_session_event");
            }
            break;

        default:

            SWSS_LOG_WARN("notification for SWITCH attr id: %d (%s) is not supported, FIXME", id, (m ? m->attridname : "UNKNOWN"));
            break;
    }
}
