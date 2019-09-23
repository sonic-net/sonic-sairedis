#include "sai_vs.h"
#include "sai_vs_state.h"
#include "sai_vs_internal.h"

bool doesFdbEntryNotMatchFlushAttr(
        _In_ const std::string &str_fdb_entry,
        _In_ AttrHash &fdb_attrs,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * Since there may be combination of flush attributes
     * that user requested to flush, so when one attribute
     * is different, then this fdb entry should NOT be flushed.
     */

    for (uint32_t idx = 0; idx < attr_count; ++idx)
    {
        const sai_attribute_t &attr = attr_list[idx];

        switch (attr.id)
        {
            case SAI_FDB_FLUSH_ATTR_BRIDGE_PORT_ID:

                // remove from list all entries not matching bridge port id
                if (fdb_attrs.at("SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID")->getAttr()->value.oid != attr.value.oid)
                {
                    return true;
                }

                break;

            case SAI_FDB_FLUSH_ATTR_BV_ID:

                {
                    sai_fdb_entry_t fdb_entry;

                    sai_deserialize_fdb_entry(str_fdb_entry, fdb_entry);

                    // remove from list all entries not matching vlan id
                    if (fdb_entry.bv_id != attr.value.oid)
                    {
                        return true;
                    }
                }

                break;

            case SAI_FDB_FLUSH_ATTR_ENTRY_TYPE:

                // remove from list all entries not matching type
                if (fdb_attrs.at("SAI_FDB_ENTRY_ATTR_TYPE")->getAttr()->value.s32 != attr.value.s32)
                {
                    return true;
                }

                break;

            default:
                SWSS_LOG_ERROR("flush attribute %d is not supported yet", attr.id);
                return true;
        }
    }

    // this fdb entry should be flushed
    return false;
}

void sendFdbDelNotification(
        _In_ const fdb_info_t &fi)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
    attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;

    attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = fi.bridge_port_id;

    sai_fdb_event_notification_data_t data;

    data.event_type = SAI_FDB_EVENT_AGED;

    data.fdb_entry = fi.fdb_entry;

    data.attr_count = 2;
    data.attr = attrs;

    // update metadata DB
    meta_sai_on_fdb_event(1, &data);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;

    sai_status_t status = vs_generic_get(SAI_OBJECT_TYPE_SWITCH, data.fdb_entry.switch_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get fdb event notify from switch %s",
                sai_serialize_object_id(data.fdb_entry.switch_id).c_str());
        return;
    }

    std::string s = sai_serialize_fdb_event_ntf(1, &data);

    SWSS_LOG_DEBUG("calling user fdb event callback: %s", s.c_str());

    sai_fdb_event_notification_fn ntf = (sai_fdb_event_notification_fn)attr.value.ptr;

    if (ntf != NULL)
    {
        ntf(1, &data);
    }
}

sai_status_t internal_vs_flush_fdb_entries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * There are 3 databases for fdb entries, one is in metadata and second is
     * in g_fdb_info_set and third is local virtual switch. Second one holds
     * all dynamic entries learned from interfaces. All learned entries are
     * processed by metadata and propagated from info set to metadata. But
     * there may be short period of time that those 3 sets will be out of sync
     * until notifications learned/aged will be sent.
     *
     * After learning fdb event, fdb entry will be put into 3 places:
     * - local db g_switch_state_map
     * - metadata db, and
     * - g_fdb_info_set (contains only learned entries)
     *
     * This call should clear g_switch_state_map and g_fdb_info_set, and
     * metadata db should be cleared by flush notification handler.
     */

    auto &fdbs = g_switch_state_map.at(switch_id)->objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY);

    std::map<std::string, AttrHash> static_fdbs;
    std::map<std::string, AttrHash> dynamic_fdbs;

    for (auto it = fdbs.begin(); it != fdbs.end();)
    {
        if (doesFdbEntryNotMatchFlushAttr(it->first, it->second, attr_count, attr_list))
        {
            /*
             * If fdb entry does not match flush attributes, we will skip this entry.
             */

            ++it;
        }
        else
        {
            /*
             * Entries to be flushed we need to split for 2 groups static and
             * dynamic entries.
             */

            sai_fdb_entry_type_t type = (sai_fdb_entry_type_t)it->second.at("SAI_FDB_ENTRY_ATTR_TYPE")->getAttr()->value.s32;

            switch (type)
            {
                case SAI_FDB_ENTRY_TYPE_DYNAMIC:
                    dynamic_fdbs.insert(*it);
                    break;

                case SAI_FDB_ENTRY_TYPE_STATIC:
                    static_fdbs.insert(*it);
                    break;

                default:
                    SWSS_LOG_ERROR("unsupported fdb_entry type %d on %s", type, it->first.c_str());
                    break;
            }

            // update fdb info set

            fdb_info_t fi;

            memset(&fi, 0, sizeof(fi));

            sai_deserialize_fdb_entry(it->first, fi.fdb_entry);

            auto fit = g_fdb_info_set.find(fi);

            if (fit == g_fdb_info_set.end())
            {
                // this may happen if vlan is invalid
                SWSS_LOG_ERROR("failed to find fdb entry in info set: %s", it->first.c_str());
            }
            else
            {
                g_fdb_info_set.erase(fit);
            }

            /*
             * Since we are using &on fdbs then this will also clear local
             * data base.
             */

            fi.bridge_port_id = it->second.at("SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID")->getAttr()->value.oid;

            SWSS_LOG_INFO("FDBDelNotification: fdb_entry:%s bpid:%s", sai_serialize_fdb_entry(fi.fdb_entry).c_str(), sai_serialize_object_id(fi.bridge_port_id).c_str());

            it = fdbs.erase(it);

            sendFdbDelNotification(fi);
        }
    }

    /* Sending notification for each fdb entry individually above, as a single
     * response causes syncd to delete ASIC_DB entries in a loop  and
     * stuck for long time when a large number of FDB entries are present.
     */

    /*
     * We can have 3 attributes (so far) to flush by:
     * - entry type
     * - bridge port id
     * - vlan id
     *
     * We are sending consolidated FLUSH event to not send each fdb entry 1 by
     * 1 in that case we need to mark data in special way reusing current
     * attributes.
     *
     * Since flush event is considered like regular fdb event, data inside will
     * be special. Indication that this is flush event will be by actual event
     * type and consolidation will marked as MAC address 00:00:00:00:00:00.
     * In that case user receiving notification will know that flush event is
     * consolidated event and not actual fdb entry.
     *
     * To indicate what entry type was flushed, entry_type field in fdb_entry
     * will be populated. If no entry will be specified 2 notifications will be
     * sent 1 for static entries and 1 for dynamic entries, or 1 notification
     * with 2 data entries will be send.
     *
     * To indicate what vlan if was flushed, vlan_id field in fdb_entry will be
     * populated with flushed vlan_id. If no vlan has been set, vlan_id will be
     * zero.
     *
     * To indicate what bridge port id was flushed,
     * SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID attribute will be added with oid value
     * set to that bridge port id, or if no bridge port id was specified then
     * this attribute will be missing or oid will be set to SAI_NULL_OBJECT_ID.
     *
     * By those rules user receiving notification can figure out what flush
     * event is handling.
     *
     * All other attributes in consolidated fdb event notification are
     * irrelevant.
     */

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;

    sai_status_t status = vs_generic_get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("unable to get SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY attribute for %s",
                sai_serialize_object_id(switch_id).c_str());

        attr.value.ptr = NULL;
    }

    SWSS_LOG_NOTICE("generating fdb flush notifications");

    /* notification has been sent for individual mac delete, so 
     * not sending the bulk flush notification
     */
#if 0
    sai_fdb_event_notification_fn ntf = (sai_fdb_event_notification_fn)attr.value.ptr;
#endif

    sai_fdb_event_notification_data_t data;
    sai_attribute_t attrs[2];

    memset(&data, 0, sizeof(data));

    memset(attrs, 0, sizeof(attrs));

    data.event_type             = SAI_FDB_EVENT_FLUSHED;
    data.fdb_entry.switch_id    = switch_id;
    data.attr_count             = 1;
    data.attr                   = attrs;

    auto bpid = sai_metadata_get_attr_by_id(SAI_FDB_FLUSH_ATTR_BRIDGE_PORT_ID, attr_count, attr_list);

    if (bpid != NULL)
    {
        data.attr_count = 2;

        attrs[1].id        = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
        attrs[1].value.oid = bpid->value.oid;
    }

    auto vlanid = sai_metadata_get_attr_by_id(SAI_FDB_FLUSH_ATTR_BV_ID, attr_count, attr_list);

    if (vlanid != NULL)
    {
        data.fdb_entry.bv_id = vlanid->value.oid;
    }

    if (static_fdbs.size() > 0)
    {
        SWSS_LOG_NOTICE("flushing %zu static entries", static_fdbs.size());

        attrs[0].id        = SAI_FDB_ENTRY_ATTR_TYPE;
        attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;

        meta_sai_on_fdb_event(1, &data); // update metadata
        
        /* notification has been sent for individual mac delete, so 
         * not sending the bulk flush notification
         */
#if 0
        if (ntf != NULL)
        {
            ntf(1, &data);
        }
#endif
    }

    if (dynamic_fdbs.size() > 0)
    {
        SWSS_LOG_NOTICE("flushing %zu dynamic entries", dynamic_fdbs.size());

        attrs[0].id        = SAI_FDB_ENTRY_ATTR_TYPE;
        attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;

        meta_sai_on_fdb_event(1, &data); // update metadata
        
        /* notification has been sent for individual mac delete, so 
         * not sending the bulk flush notification
         */
#if 0    
        if (ntf != NULL)
        {
            ntf(1, &data);
        }
#endif
        SWSS_LOG_NOTICE("1");
    }

    // TODO: we can add config entry to send notifications 1 by 1 as option to consolidated

    return SAI_STATUS_SUCCESS;
}

sai_status_t vs_flush_fdb_entries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return meta_sai_flush_fdb_entries(
            switch_id,
            attr_count,
            attr_list,
            internal_vs_flush_fdb_entries);
}

VS_GENERIC_QUAD_ENTRY(FDB_ENTRY,fdb_entry);

const sai_fdb_api_t vs_fdb_api = {

    VS_GENERIC_QUAD_API(fdb_entry)

    vs_flush_fdb_entries,
};
