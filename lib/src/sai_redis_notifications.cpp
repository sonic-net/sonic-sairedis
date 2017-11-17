#include "sai_redis.h"
#include "meta/saiserialize.h"
#include "meta/saiattributelist.h"

/*
 * NOTE: currently we only support 1 set of notifications per all switches,
 * this will need to be corrected later.
 */

sai_switch_state_change_notification_fn     on_switch_state_change = NULL;
sai_switch_shutdown_request_notification_fn on_switch_shutdown_request_notification = NULL;
sai_fdb_event_notification_fn               on_fdb_event = NULL;
sai_port_state_change_notification_fn       on_port_state_change = NULL;
sai_packet_event_notification_fn            on_packet_event = NULL;
sai_queue_pfc_deadlock_notification_fn      on_queue_deadlock = NULL;

void clear_notifications()
{
    SWSS_LOG_ENTER();

    on_switch_state_change = NULL;
    on_switch_shutdown_request_notification = NULL;
    on_fdb_event = NULL;
    on_port_state_change = NULL;
    on_packet_event = NULL;
    on_queue_deadlock = NULL;
}

void check_notifications_pointers(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * This function should only be called on CREATE/SET
     * api when object is SWITCH.
     */

    for (uint32_t index = 0; index < attr_count; ++index)
    {
        const sai_attribute_t &attr = attr_list[index];

        auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_ERROR("failed to find metadata for switch attr %d", attr.id);
            continue;
        }

        if (meta->attrvaluetype != SAI_ATTR_VALUE_TYPE_POINTER)
        {
            continue;
        }

        switch (attr.id)
        {
            case SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY:
                on_switch_state_change = (sai_switch_state_change_notification_fn)attr.value.ptr;
                break;

            case SAI_SWITCH_ATTR_SHUTDOWN_REQUEST_NOTIFY:
                on_switch_shutdown_request_notification = (sai_switch_shutdown_request_notification_fn)attr.value.ptr;
                break;

            case SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY:
                on_fdb_event = (sai_fdb_event_notification_fn)attr.value.ptr;
                break;

            case SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY:
                on_port_state_change = (sai_port_state_change_notification_fn)attr.value.ptr;
                break;

            case SAI_SWITCH_ATTR_PACKET_EVENT_NOTIFY:
                on_packet_event = (sai_packet_event_notification_fn)attr.value.ptr;
                break;

            case SAI_SWITCH_ATTR_QUEUE_PFC_DEADLOCK_NOTIFY:
                on_queue_deadlock = (sai_queue_pfc_deadlock_notification_fn)attr.value.ptr;
                break;

            default:
                SWSS_LOG_ERROR("pointer for %s is not handled, FIXME!", meta->attridname);
                break;
        }
    }
}

void handle_switch_state_change(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("data: %s", data.c_str());

    sai_switch_oper_status_t switch_oper_status;
    sai_object_id_t switch_id;

    sai_deserialize_switch_oper_status(data, switch_id, switch_oper_status);

    if (on_switch_state_change != NULL)
    {
        on_switch_state_change(switch_id, switch_oper_status);
    }
}

void handle_fdb_event(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("data: %s", data.c_str());

    uint32_t count;
    sai_fdb_event_notification_data_t *fdbevent = NULL;

    sai_deserialize_fdb_event_ntf(data, count, &fdbevent);

    {
        std::lock_guard<std::mutex> lock(g_apimutex);

        // NOTE: this meta api must be under mutex since
        // it will access meta DB and notification comes
        // from different thread

        meta_sai_on_fdb_event(count, fdbevent);
    }

    if (on_fdb_event != NULL)
    {
        on_fdb_event(count, fdbevent);
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

    if (on_port_state_change != NULL)
    {
        on_port_state_change(count, portoperstatus);
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

    if (on_switch_shutdown_request_notification != NULL)
    {
        on_switch_shutdown_request_notification(switch_id);
    }
}

void handle_packet_event(
        _In_ const std::string &data,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("data: %s, values: %lu", data.c_str(), values.size());

    SWSS_LOG_ERROR("not implemented");

    if (on_packet_event != NULL)
    {
        //on_packet_event(switch_id, buffer.data(), buffer_size, list.get_attr_count(), list.get_attr_list());
    }
}

void handle_queue_deadlock_event(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("data: %s", data.c_str());

    uint32_t count;
    sai_queue_deadlock_notification_data_t *ntfData = NULL;

    sai_deserialize_queue_deadlock_ntf(data, count, &ntfData);

    if (on_queue_deadlock != NULL)
    {
        on_queue_deadlock(count, ntfData);
    }

    sai_deserialize_free_queue_deadlock_ntf(count, ntfData);
}

void handle_notification(
        _In_ const std::string &notification,
        _In_ const std::string &data,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

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
