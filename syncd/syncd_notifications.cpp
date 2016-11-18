#include "syncd.h"

void send_notification(
        _In_ std::string op,
        _In_ std::string data,
        _In_ std::vector<swss::FieldValueTuple> &entry)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("%s %s", op.c_str(), data.c_str());

    notifications->send(op, data, entry);

    SWSS_LOG_DEBUG("notification send successfull");
}

void send_notification(
        _In_ std::string op,
        _In_ std::string data)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry;

    send_notification(op, data, entry);
}

void on_switch_state_change(
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    std::string s = sai_serialize_switch_oper_status(switch_oper_status);

    send_notification("switch_state_change", s);
}

sai_fdb_entry_type_t getFdbEntryType(
        _In_ uint32_t count,
        _In_ const sai_attribute_t *list)
{
    for (uint32_t idx = 0; idx < count; idx++)
    {
        const sai_attribute_t &attr = list[idx];

        if (attr.id == SAI_FDB_ENTRY_ATTR_TYPE)
        {
            return (sai_fdb_entry_type_t)attr.value.s32;
        }
    }

    SWSS_LOG_WARN("unknown fdb entry type");

    int ret = -1;
    return (sai_fdb_entry_type_t)ret;
}

void redisPutFdbEntryToAsicView(
        _In_ const sai_fdb_event_notification_data_t *fdb)
{
    // NOTE: this fdb entry already contains translated RID to VID

    std::vector<swss::FieldValueTuple> entry;

    entry = SaiAttributeList::serialize_attr_list(
            SAI_OBJECT_TYPE_FDB,
            fdb->attr_count,
            fdb->attr,
            false);

    sai_object_type_t objectType = SAI_OBJECT_TYPE_FDB;

    std::string strObjectType = sai_serialize_object_type(objectType);

    std::string strFdbEntry = sai_serialize_fdb_entry(fdb->fdb_entry);

    std::string key = "ASIC_STATE:" + strObjectType + ":" + strFdbEntry;

    for (const auto &e: entry)
    {
        const std::string &strField = fvField(e);
        const std::string &strValue = fvValue(e);

        g_redisClient->hset(key, strField, strValue);
    }

    // currently we need to add type manually since fdb event don't contain type
    sai_attribute_t attr;

    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr.value.s32 = SAI_FDB_ENTRY_DYNAMIC;

    auto meta = get_attribute_metadata(objectType, attr.id);

    if (meta == NULL)
    {
        SWSS_LOG_ERROR("unable to get metadata for object type %x, attribute %x", objectType, attr.id);
        exit_and_notify(EXIT_FAILURE);
    }

    std::string strAttrId = sai_serialize_attr_id(*meta);
    std::string strAttrValue = sai_serialize_attr_value(*meta, attr);

    g_redisClient->hset(key, strAttrId, strAttrValue);
}

void on_fdb_event(
        _In_ uint32_t count,
        _In_ sai_fdb_event_notification_data_t *data)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("fdb event count: %d", count);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_fdb_event_notification_data_t *fdb = &data[i];

        SWSS_LOG_DEBUG("fdb %u: type: %d", i, fdb->event_type);

        translate_rid_to_vid_list(SAI_OBJECT_TYPE_FDB, fdb->attr_count, fdb->attr);

        // currently because of bcrm bug, we need to install fdb entries in asic view
        // and currently this event don't have fdb type which is required on creation

        redisPutFdbEntryToAsicView(fdb);
    }

    std::string s = sai_serialize_fdb_event_ntf(count, data);

    send_notification("fdb_event", s);
}

void on_port_state_change(
        _In_ uint32_t count,
        _In_ sai_port_oper_status_notification_t *data)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("port notification count: %u", count);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_port_oper_status_notification_t *oper_stat = &data[i];

        oper_stat->port_id = translate_rid_to_vid(oper_stat->port_id);
    }

    std::string s = sai_serialize_port_oper_status_ntf(count, data);

    send_notification("port_state_change", s);
}

void on_port_event(
        _In_ uint32_t count,
        _In_ sai_port_event_notification_t *data)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < count; i++)
    {
        sai_port_event_notification_t *port_event = &data[i];

        port_event->port_id = translate_rid_to_vid(port_event->port_id);
    }

    std::string s = sai_serialize_port_event_ntf(count, data);

    send_notification("port_event", s);
}

void on_switch_shutdown_request()
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    send_notification("switch_shutdown_request", "");
}

void on_packet_event(
        _In_ const void *buffer,
        _In_ sai_size_t buffer_size,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    /*
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

    translate_rid_to_vid_list(SAI_OBJECT_TYPE_PACKET, copy.get_attr_count(), copy.get_attr_list());

    entry = SaiAttributeList::serialize_attr_list(
    SAI_OBJECT_TYPE_PACKET,
    copy.get_attr_count(),
    copy.get_attr_list(),
    false);

    send_notification("packet_event", s, entry);
    */
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

