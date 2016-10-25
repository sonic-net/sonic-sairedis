#include "sai_redis.h"
#include "meta/saiserialize.h"
#include "meta/saiattributelist.h"

void handle_switch_state_change(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("data: %s", data.c_str());

    sai_switch_oper_status_t switch_oper_status;

    sai_deserialize_switch_oper_status(data, switch_oper_status);

    auto on_switch_state_change = redis_switch_notifications.on_switch_state_change;

    if (on_switch_state_change != NULL)
    {
        on_switch_state_change(switch_oper_status);
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

    auto on_fdb_event = redis_switch_notifications.on_fdb_event;

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

    auto on_port_state_change = redis_switch_notifications.on_port_state_change;

    if (on_port_state_change != NULL)
    {
        on_port_state_change(count, portoperstatus);
    }

    sai_deserialize_free_port_oper_status_ntf(count, portoperstatus);
}

void handle_port_event(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("data: %s", data.c_str());

    uint32_t count;
    sai_port_event_notification_t *portevent = NULL;

    sai_deserialize_port_event_ntf(data, count, &portevent);

    auto on_port_event = redis_switch_notifications.on_port_event;

    if (on_port_event != NULL)
    {
        on_port_event(count, portevent);
    }

    sai_deserialize_free_port_event_ntf(count, portevent);
}

void handle_switch_shutdown_request(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("switch shutdown request");

    auto on_switch_shutdown_request = redis_switch_notifications.on_switch_shutdown_request;

    if (on_switch_shutdown_request != NULL)
    {
        on_switch_shutdown_request();
    }
}

void handle_packet_event(
        _In_ const std::string &data,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("data: %s, values: %lu", data.c_str(), values.size());

    SWSS_LOG_ERROR("not implemented");

    /*
    auto on_packet_event = redis_switch_notifications.on_packet_event;

    if (on_packet_event != NULL)
    {
        on_packet_event(buffer.data(), buffer_size, list.get_attr_count(), list.get_attr_list());
    }*/
}

void handle_notification(
        _In_ const std::string &notification,
        _In_ const std::string &data,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    if (g_record)
    {
        recordLine("n," + notification + "," + data + "," + joinFieldValues(values));
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
    else if (notification == "port_event")
    {
        handle_port_event(data);
    }
    else if (notification == "switch_shutdown_request")
    {
        handle_switch_shutdown_request(data);
    }
    else if (notification == "packet_event")
    {
        handle_packet_event(data, values);
    }
    else
    {
        SWSS_LOG_ERROR("unknow notification: %s", notification.c_str());
    }
}
