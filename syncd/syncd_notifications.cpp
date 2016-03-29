#include "syncd.h"

void send_notification(
        _In_ std::string key,
        _In_ std::string data,
        _In_ std::vector<swss::FieldValueTuple> &entry)
{
    key = key + ":" + data;

    notifications->set(key, entry, "ntf");
    notifications->del(key, "delntf");
}

void send_notification(
        _In_ std::string key,
        _In_ std::string data)
{
    std::vector<swss::FieldValueTuple> entry;

    send_notification(key, data, entry);
}

void on_switch_state_change(
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    SYNCD_LOG_ENTER();

    std::string s;
    sai_serialize_primitive(switch_oper_status, s);

    send_notification("switch_state_change", s);

    SYNCD_LOG_EXIT();
}

void on_fdb_event(
        _In_ uint32_t count,
        _In_ sai_fdb_event_notification_data_t *data)
{
    SYNCD_LOG_ENTER();

    std::string s;
    sai_serialize_primitive(count, s);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_fdb_event_notification_data_t *fdb = &data[i];

        sai_status_t status = sai_serialize_fdb_event_notification_data(fdb, s);

        if (status != SAI_STATUS_SUCCESS)
        {
            SYNCD_LOG_ERR("Unable to serialize fdb event, status: %u", status);

            // we need to break here since serialized data may be inconsistent

            SYNCD_LOG_EXIT();
            return;
        }
    }

    send_notification("fdb_event", s);

    SYNCD_LOG_EXIT();
}

void on_port_state_change(
            _In_ uint32_t count,
            _In_ sai_port_oper_status_notification_t *data)
{
    SYNCD_LOG_ENTER();

    std::string s;
    sai_serialize_primitive(count, s);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_port_oper_status_notification_t *oper_stat = &data[i];
        sai_serialize_primitive(oper_stat, s);
    }

    send_notification("port_state_change", s);

    SYNCD_LOG_EXIT();
}

void on_port_event(
        _In_ uint32_t count,
        _In_ sai_port_event_notification_t *data)
{
    SYNCD_LOG_ENTER();

    std::string s;
    sai_serialize_primitive(count, s);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_port_event_notification_t *port_event = &data[i];
        sai_serialize_primitive(port_event, s);
    }

    send_notification("port_event", s);

    SYNCD_LOG_EXIT();
}

void on_switch_shutdown_request()
{
    SYNCD_LOG_ENTER();

    send_notification("switch_shutdown_request", "");

    SYNCD_LOG_EXIT();
}

void on_packet_event(
        _In_ const void *buffer,
        _In_ sai_size_t buffer_size,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SYNCD_LOG_ENTER();

    std::string s;
    sai_serialize_primitive(buffer_size, s);

    sai_serialize_buffer(buffer, buffer_size, s);

    std::vector<swss::FieldValueTuple> entry;

    entry = SaiAttributeList::serialize_attr_list(
            SAI_OBJECT_TYPE_PACKET,
            attr_count,
            attr_list,
            false);

    // since attr_list is const, we can't replace rid's
    // we need to create copy of that list

    SaiAttributeList copy(SAI_OBJECT_TYPE_PACKET, entry, false);

    translate_rid_to_vid(SAI_OBJECT_TYPE_PACKET, copy.get_attr_count(), copy.get_attr_list());

    entry = SaiAttributeList::serialize_attr_list(
            SAI_OBJECT_TYPE_PACKET,
            copy.get_attr_count(),
            copy.get_attr_list(),
            false);

    send_notification("packet_event", s, entry);

    SYNCD_LOG_EXIT();
}

sai_switch_notification_t switch_notifications
{
    on_switch_state_change,
    on_fdb_event,
    on_port_state_change,
    on_port_event,
    on_switch_shutdown_request,
    on_packet_event
};

