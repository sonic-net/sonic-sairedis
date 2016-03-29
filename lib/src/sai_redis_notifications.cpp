#include "sai_redis.h"

void handle_switch_state_change(
        _In_ const std::string &data)
{
    REDIS_LOG_ENTER();

    int index = 0;

    sai_switch_oper_status_t switch_oper_status;
    sai_deserialize_primitive(data, index, switch_oper_status);

    auto on_switch_state_change = redis_switch_notifications.on_switch_state_change;

    if (on_switch_state_change != NULL)
    {
        on_switch_state_change(switch_oper_status);
    }

    REDIS_LOG_EXIT();
}

void handle_fdb_event(
        _In_ const std::string &data)
{
    REDIS_LOG_ENTER();

    int index = 0;

    uint32_t count;
    sai_deserialize_primitive(data, index, count);

    std::vector<sai_fdb_event_notification_data_t> fdbdata;

    fdbdata.resize(count);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_fdb_event_notification_data_t *fdb = &fdbdata[i];

        sai_status_t status = sai_deserialize_fdb_event_notification_data(data, index, fdb);

        if (status != SAI_STATUS_SUCCESS)
        {
            REDIS_LOG_ERR("Unable to deserialize fdb event, status: %u", status);

            REDIS_LOG_EXIT();
            return;
        }
    }

    auto on_fdb_event = redis_switch_notifications.on_fdb_event;

    if (on_fdb_event != NULL)
    {
        on_fdb_event(count, fdbdata.data());
    }

    for (uint32_t i = 0; i < count; i++)
    {
        sai_fdb_event_notification_data_t *fdb = &fdbdata[i];

        sai_status_t status = sai_deserialize_free_fdb_event_notification_data(fdb);

        if (status != SAI_STATUS_SUCCESS)
        {
            REDIS_LOG_ERR("Unable to free fdb event, status: %u", status);
        }
    }

    REDIS_LOG_EXIT();
}

void handle_port_state_change(
        _In_ const std::string &data)
{
    REDIS_LOG_ENTER();

    int index = 0;

    uint32_t count;

    sai_deserialize_primitive(data, index, count);

    std::vector<sai_port_oper_status_notification_t> portdata;

    portdata.resize(count);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_port_oper_status_notification_t *oper_stat = &portdata[i];

        sai_deserialize_primitive(data, index, oper_stat);
    }

    auto on_port_state_change = redis_switch_notifications.on_port_state_change;

    if (on_port_state_change != NULL)
    {
        on_port_state_change(count, portdata.data());
    }

    REDIS_LOG_EXIT();
}

void handle_port_event(
        _In_ const std::string &data)
{
    REDIS_LOG_ENTER();

    int index = 0;

    uint32_t count;

    sai_deserialize_primitive(data, index, count);

    std::vector<sai_port_event_notification_t> portdata;

    portdata.resize(count);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_port_event_notification_t *oper_stat = &portdata[i];

        sai_deserialize_primitive(data, index, oper_stat);
    }

    auto on_port_event = redis_switch_notifications.on_port_event;

    if (on_port_event != NULL)
    {
        on_port_event(count, portdata.data());
    }

    REDIS_LOG_EXIT();
}

void handle_switch_shutdown_request(
        _In_ const std::string &data)
{
    REDIS_LOG_ENTER();

    auto on_switch_shutdown_request = redis_switch_notifications.on_switch_shutdown_request;

    if (on_switch_shutdown_request != NULL)
    {
        on_switch_shutdown_request();
    }

    REDIS_LOG_EXIT();
}

void handle_packet_event(
        _In_ const std::string &data,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    REDIS_LOG_ENTER();

    sai_size_t buffer_size;

    int index = 0;

    sai_deserialize_primitive(data, index, buffer_size);

    std::vector<unsigned char> buffer;

    buffer.resize(buffer_size);

    sai_deserialize_buffer(data, index, buffer_size, buffer.data());

    SaiAttributeList list(SAI_OBJECT_TYPE_PACKET, values, false);

    auto on_packet_event = redis_switch_notifications.on_packet_event;

    if (on_packet_event != NULL)
    {
        on_packet_event(buffer.data(), buffer_size, list.get_attr_count(), list.get_attr_list());
    }

    REDIS_LOG_EXIT();
}

void handle_notification(
        _In_ const std::string &notification,
        _In_ const std::string &data,
        _In_ const std::vector<swss::FieldValueTuple> &values)
{
    REDIS_LOG_ENTER();

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
        REDIS_LOG_ERR("unknow notification: %s", notification.c_str());
    }

    REDIS_LOG_EXIT();
}
