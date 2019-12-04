#include "sai_redis.h"
#include "meta/sai_serialize.h"
#include "meta/saiattributelist.h"

/**
 * @brief Get switch notifications structure.
 *
 * This function is executed in notifications thread, and it may happen that
 * during switch remove some notification arrived for that switch, so we need
 * to prevent race condition that switch will be removed before notification
 * will be processed.
 *
 * @return Copy of requested switch notifications struct or empty struct is
 * switch is not present in container.
 */
sai_switch_notifications_t getSwitchNotifications(
        _In_ sai_object_id_t switchId)
{
    MUTEX();

    SWSS_LOG_ENTER();

    auto sw = g_switchContainer->getSwitch(switchId);

    if (sw)
    {
        return sw->getSwitchNotifications(); // explicit copy
    }

    SWSS_LOG_WARN("switch %s not present in container, returning empty switch notifications",
            sai_serialize_object_id(switchId).c_str());

    return sai_switch_notifications_t { };
}

// We are assuming that notifications that has "count" member and don't have
// explicit switch_id defined in struct (like fdb event), they will always come
// from the same switch instance (same switch id). This is because we can define
// different notifications pointers per switch instance.

void handle_switch_state_change(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("data: %s", data.c_str());

    sai_switch_oper_status_t switch_oper_status;
    sai_object_id_t switch_id;

    sai_deserialize_switch_oper_status(data, switch_id, switch_oper_status);

    auto sn = getSwitchNotifications(switch_id);

    if (sn.on_switch_state_change != NULL)
    {
        sn.on_switch_state_change(switch_id, switch_oper_status);
    }
}

void process_metadata_on_fdb_event(
        _In_ uint32_t count,
        _In_ sai_fdb_event_notification_data_t* data)
{
    MUTEX();

    SWSS_LOG_ENTER();

    // NOTE: this meta api must be under mutex since
    // it will access meta DB and notification comes
    // from different thread
    //
    // TODO move this to future notification processor

    meta_sai_on_fdb_event(count, data);
}

void handle_fdb_event(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("data: %s", data.c_str());

    uint32_t count;
    sai_fdb_event_notification_data_t *fdbevent = NULL;

    sai_deserialize_fdb_event_ntf(data, count, &fdbevent);

    process_metadata_on_fdb_event(count, fdbevent);

    // since notification don't contain switch id, obtain it from fdb_entry
    sai_object_id_t switchId = SAI_NULL_OBJECT_ID;

    if (count)
        switchId = fdbevent[0].fdb_entry.switch_id;

    auto sn = getSwitchNotifications(switchId);

    if (sn.on_fdb_event != NULL)
    {
        sn.on_fdb_event(count, fdbevent);
    }

    sai_deserialize_free_fdb_event_ntf(count, fdbevent);
}

void handle_port_state_change(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("data: %s", data.c_str());

    uint32_t count;
    sai_port_oper_status_notification_t *portoperstatus = NULL;

    sai_deserialize_port_oper_status_ntf(data, count, &portoperstatus);

    sai_object_id_t switchId = SAI_NULL_OBJECT_ID;

    // since notification don't contain switch id, obtain it from port id
    if (count)
        switchId = sai_switch_id_query(portoperstatus[0].port_id);

    auto sn = getSwitchNotifications(switchId);

    if (sn.on_port_state_change != NULL)
    {
        sn.on_port_state_change(count, portoperstatus);
    }

    sai_deserialize_free_port_oper_status_ntf(count, portoperstatus);
}

void handle_switch_shutdown_request(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("switch shutdown request");

    sai_object_id_t switch_id;

    sai_deserialize_switch_shutdown_request(data, switch_id);

    auto sn = getSwitchNotifications(switch_id);

    if (sn.on_switch_shutdown_request != NULL)
    {
        sn.on_switch_shutdown_request(switch_id);
    }
}

void handle_packet_event(
        _In_ const std::string &data,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("data: %s, values: %lu", data.c_str(), values.size());

    SWSS_LOG_ERROR("not implemented");
}

void handle_queue_deadlock_event(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("data: %s", data.c_str());

    uint32_t count;
    sai_queue_deadlock_notification_data_t *ntfData = NULL;

    sai_deserialize_queue_deadlock_ntf(data, count, &ntfData);

    // TODO metadata validate under mutex, possible snoop queue_id

    // since notification don't contain switch id, obtain it from queue id
    sai_object_id_t switchId = SAI_NULL_OBJECT_ID;

    if (count)
        switchId = sai_switch_id_query(ntfData[0].queue_id);

    auto sn = getSwitchNotifications(switchId);

    if (sn.on_queue_pfc_deadlock != NULL)
    {
        sn.on_queue_pfc_deadlock(count, ntfData);
    }

    sai_deserialize_free_queue_deadlock_ntf(count, ntfData);
}

void handle_notification(
        _In_ const std::string &notification,
        _In_ const std::string &data,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    // TODO to pass switch_id for every notification we could add it to values at syncd side

    if (g_record)
    {
        recordLine("n|" + notification + "|" + data + "|" + joinFieldValues(values));
    }

    if (notification == "switch_state_change")
    {
        handle_switch_state_change(data);
    }
    else if (notification == "fdb_event")
    {
        handle_fdb_event(data);
    }
    else if (notification == "port_state_change")
    {
        handle_port_state_change(data);
    }
    else if (notification == "switch_shutdown_request")
    {
        handle_switch_shutdown_request(data);
    }
    else if (notification == "packet_event")
    {
        handle_packet_event(data, values);
    }
    else if (notification == "queue_deadlock")
    {
        handle_queue_deadlock_event(data);
    }
    else
    {
        SWSS_LOG_ERROR("unknow notification: %s", notification.c_str());
    }
}
