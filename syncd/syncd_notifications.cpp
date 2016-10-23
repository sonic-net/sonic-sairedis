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

    std::string s;
    sai_serialize_primitive(switch_oper_status, s);

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
            return (sai_fdb_entry_type_t)attr.value.s32;
    }

    SWSS_LOG_WARN("unknown fdb entry type");

    int ret = -1;
    return (sai_fdb_entry_type_t)ret;
}

void redisPutFdbEntryToAsicView(
    _In_ const sai_fdb_entry_t *fdbEntry,
    _In_ SaiAttributeList &list)
{
    // NOTE: this fdb entry already contains translated RID to VID

    std::vector<swss::FieldValueTuple> entry;

    entry = SaiAttributeList::serialize_attr_list(
            SAI_OBJECT_TYPE_FDB,
            list.get_attr_count(),
            list.get_attr_list(),
            false);

    sai_object_type_t objectType = SAI_OBJECT_TYPE_FDB;

    std::string strObjectType;
    sai_serialize_primitive(objectType, strObjectType);

    std::string strFdbEntry;
    sai_serialize_primitive(*fdbEntry, strFdbEntry);

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

    std::string strAttrValue;

    sai_status_t status = sai_serialize_attr_value(SAI_SERIALIZATION_TYPE_INT32, attr, strAttrValue, false);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Unable to serialize fdb entry attribute, status: %u", status);

        exit(EXIT_FAILURE);
    }

    std::string strAttrType;
    sai_serialize_primitive(attr.id, strAttrType);

    g_redisClient->hset(key, strAttrType, strAttrValue);
}

void on_fdb_event(
        _In_ uint32_t count,
        _In_ sai_fdb_event_notification_data_t *data)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    std::string s;
    sai_serialize_primitive(count, s);

    SWSS_LOG_DEBUG("fdb event count: %d", count);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_fdb_event_notification_data_t *fdb = &data[i];

        SWSS_LOG_DEBUG("fdb %u: type: %d", i, fdb->event_type);

        // NOTE: operate on copy to not modify sdk data

        sai_fdb_event_notification_data_t copy;

        memcpy(&copy, fdb, sizeof(sai_fdb_event_notification_data_t));

        std::vector<swss::FieldValueTuple> entry;

        entry = SaiAttributeList::serialize_attr_list(
                SAI_OBJECT_TYPE_FDB,
                copy.attr_count,
                copy.attr,
                false);

        SaiAttributeList list(SAI_OBJECT_TYPE_FDB, entry, false);

        translate_rid_to_vid_list(SAI_OBJECT_TYPE_FDB, list.get_attr_count(), list.get_attr_list());

        copy.attr_count = list.get_attr_count();
        copy.attr = list.get_attr_list();

        sai_status_t status = sai_serialize_fdb_event_notification_data(&copy, s);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Unable to serialize fdb event, status: %u", status);

            exit(EXIT_FAILURE);
        }

        // currently because of bcrm bug, we need to install fdb entries in asic view
        // and currently this event don't have fdb type which is required on creation

        redisPutFdbEntryToAsicView(&copy.fdb_entry, list);
    }

    send_notification("fdb_event", s);
}

void on_port_state_change(
            _In_ uint32_t count,
            _In_ sai_port_oper_status_notification_t *data)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("port notification count: %u", count);

    std::string s;
    sai_serialize_primitive(count, s);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_port_oper_status_notification_t *oper_stat = &data[i];

        // NOTE: make a copy to not modify sdk values
        sai_port_oper_status_notification_t copy;

        memcpy(&copy, oper_stat, sizeof(sai_port_oper_status_notification_t));

        copy.port_id = translate_rid_to_vid(copy.port_id);

        sai_serialize_primitive(copy, s);
    }

    send_notification("port_state_change", s);
}

void on_port_event(
        _In_ uint32_t count,
        _In_ sai_port_event_notification_t *data)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    std::string s;
    sai_serialize_primitive(count, s);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_port_event_notification_t *port_event = &data[i];

        // NOTE: make a copy to not modify sdk values
        sai_port_event_notification_t copy;

        memcpy(&copy, port_event, sizeof(sai_port_event_notification_t));

        copy.port_id = translate_rid_to_vid(copy.port_id);

        sai_serialize_primitive(copy, s);
    }

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

