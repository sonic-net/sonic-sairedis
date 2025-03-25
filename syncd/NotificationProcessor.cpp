#include "NotificationProcessor.h"
#include "RedisClient.h"

#include "sairediscommon.h"

#include "meta/sai_serialize.h"
#include "meta/SaiAttributeList.h"

#include "swss/logger.h"
#include "swss/notificationproducer.h"

#include <inttypes.h>

using namespace syncd;
using namespace saimeta;

NotificationProcessor::NotificationProcessor(
        _In_ std::shared_ptr<NotificationProducerBase> producer,
        _In_ std::shared_ptr<RedisClient> client,
        _In_ std::function<void(const swss::KeyOpFieldsValuesTuple&)> synchronizer):
    m_synchronizer(synchronizer),
    m_client(client),
    m_notifications(producer)
{
    SWSS_LOG_ENTER();

    m_runThread = false;

    m_notificationQueue = std::make_shared<NotificationQueue>();
}

NotificationProcessor::~NotificationProcessor()
{
    SWSS_LOG_ENTER();

    stopNotificationsProcessingThread();
}

void NotificationProcessor::sendNotification(
        _In_ const std::string& op,
        _In_ const std::string& data,
        _In_ std::vector<swss::FieldValueTuple> entry)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("%s %s", op.c_str(), data.c_str());

    m_notifications->send(op, data, entry);

    SWSS_LOG_DEBUG("notification send successfully");
}

void NotificationProcessor::sendNotification(
        _In_ const std::string& op,
        _In_ const std::string& data)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry;

    sendNotification(op, data, entry);
}

void NotificationProcessor::process_on_switch_state_change(
        _In_ sai_object_id_t switch_rid,
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_vid = m_translator->translateRidToVid(switch_rid, SAI_NULL_OBJECT_ID);

    auto s = sai_serialize_switch_oper_status(switch_vid, switch_oper_status);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_SWITCH_STATE_CHANGE, s);
}

sai_fdb_entry_type_t NotificationProcessor::getFdbEntryType(
        _In_ uint32_t count,
        _In_ const sai_attribute_t *list)
{
    SWSS_LOG_ENTER();

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

void NotificationProcessor::redisPutFdbEntryToAsicView(
        _In_ const sai_fdb_event_notification_data_t *fdb)
{
    SWSS_LOG_ENTER();

    // NOTE: this fdb entry already contains translated RID to VID

    std::vector<swss::FieldValueTuple> entry;

    entry = SaiAttributeList::serialize_attr_list(
            SAI_OBJECT_TYPE_FDB_ENTRY,
            fdb->attr_count,
            fdb->attr,
            false);

    sai_object_meta_key_t metaKey;

    metaKey.objecttype = SAI_OBJECT_TYPE_FDB_ENTRY;
    metaKey.objectkey.key.fdb_entry = fdb->fdb_entry;

    std::string strFdbEntry = sai_serialize_fdb_entry(fdb->fdb_entry);

    if ((fdb->fdb_entry.switch_id == SAI_NULL_OBJECT_ID ||
         fdb->fdb_entry.bv_id == SAI_NULL_OBJECT_ID) &&
        (fdb->event_type != SAI_FDB_EVENT_FLUSHED))
    {
        SWSS_LOG_WARN("skipped to put int db: %s", strFdbEntry.c_str());
        return;
    }

    if (fdb->event_type == SAI_FDB_EVENT_AGED)
    {
        SWSS_LOG_DEBUG("remove fdb entry %s for SAI_FDB_EVENT_AGED",
                sai_serialize_object_meta_key(metaKey).c_str());

        m_client->removeAsicObject(metaKey);
        return;
    }

    if (fdb->event_type == SAI_FDB_EVENT_FLUSHED)
    {
        sai_object_id_t bv_id = fdb->fdb_entry.bv_id;
        sai_object_id_t port_oid = 0;

        sai_fdb_flush_entry_type_t type = SAI_FDB_FLUSH_ENTRY_TYPE_DYNAMIC;

        for (uint32_t i = 0; i < fdb->attr_count; i++)
        {
            if (fdb->attr[i].id == SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID)
            {
                port_oid = fdb->attr[i].value.oid;
            }
            else if (fdb->attr[i].id == SAI_FDB_ENTRY_ATTR_TYPE)
            {
                type = (fdb->attr[i].value.s32 == SAI_FDB_ENTRY_TYPE_STATIC)
                    ? SAI_FDB_FLUSH_ENTRY_TYPE_STATIC
                    : SAI_FDB_FLUSH_ENTRY_TYPE_DYNAMIC;
            }
        }

        m_client->processFlushEvent(fdb->fdb_entry.switch_id, port_oid, bv_id, type);
        return;
    }

    if (fdb->event_type == SAI_FDB_EVENT_LEARNED || fdb->event_type == SAI_FDB_EVENT_MOVE)
    {
        if (fdb->event_type == SAI_FDB_EVENT_MOVE)
        {
            SWSS_LOG_DEBUG("remove fdb entry %s for SAI_FDB_EVENT_MOVE",
                    sai_serialize_object_meta_key(metaKey).c_str());

            m_client->removeAsicObject(metaKey);
        }
        // currently we need to add type manually since fdb event don't contain type
        sai_attribute_t attr;

        attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
        attr.value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;

        auto objectType = SAI_OBJECT_TYPE_FDB_ENTRY;

        auto meta = sai_metadata_get_attr_metadata(objectType, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %s, attribute %d",
                    sai_serialize_object_type(objectType).c_str(),
                    attr.id);
            /*
             * TODO We should notify orch agent here. And also this probably should
             * not be here, but on redis side, getting through metadata.
             */
        }

        std::string strAttrId = sai_serialize_attr_id(*meta);
        std::string strAttrValue = sai_serialize_attr_value(*meta, attr);

        entry.emplace_back(strAttrId, strAttrValue);

        m_client->createAsicObject(metaKey, entry);
        return;
    }

    SWSS_LOG_ERROR("event type %s not supported, FIXME",
            sai_serialize_fdb_event(fdb->event_type).c_str());
}

/**
 * @Brief Check FDB event notification data.
 *
 * Every OID field in notification data as well as all OID attributes are
 * checked if given OID (returned from ASIC) is already present in the syncd
 * local database. All bridge ports, vlans should be already discovered by
 * syncd discovery logic.  If vendor SAI will return unknown/invalid OID, this
 * function will return false.
 *
 * @param data FDB event notification data
 *
 * @return False if any of OID values is not present in local DB, otherwise
 * true.
 */
bool NotificationProcessor::check_fdb_event_notification_data(
        _In_ const sai_fdb_event_notification_data_t& data)
{
    SWSS_LOG_ENTER();

    /*
     * Any new RID value spotted in fdb notification can happen for 2 reasons:
     *
     * - a bug is present on the vendor SAI, all RID's are already in local or
     *   REDIS ASIC DB but vendor SAI returned new or invalid RID
     *
     * - orch agent didn't query yet bridge ID/vlan ID and already
     *   started to receive fdb notifications in which case warn message
     *   could be ignored.
     *
     * If vendor SAI will return invalid RID, then this will later on lead to
     * inconsistent DB state and possible failure on apply view after cold or
     * warm boot.
     *
     * On switch init we do discover phase, and we should discover all objects
     * so we should not get any of those messages if SAI is in consistent
     * state.
     */

    bool result = true;

    if (!m_translator->checkRidExists(data.fdb_entry.bv_id, true))
    {
        SWSS_LOG_ERROR("bv_id RID 0x%" PRIx64 " is not present on local ASIC DB: %s", data.fdb_entry.bv_id,
                sai_serialize_fdb_entry(data.fdb_entry).c_str());

        result = false;
    }

    if (!m_translator->checkRidExists(data.fdb_entry.switch_id) || data.fdb_entry.switch_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("switch_id RID 0x%" PRIx64 " is not present on local ASIC DB: %s", data.fdb_entry.switch_id,
                sai_serialize_fdb_entry(data.fdb_entry).c_str());

        result = false;
    }

    for (uint32_t i = 0; i < data.attr_count; i++)
    {
        const sai_attribute_t& attr = data.attr[i];

        auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_FDB_ENTRY, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_ERROR("unable to get metadata for fdb_entry attr.id = %d", attr.id);
            continue;
        }

        // skip non oid attributes
        if (meta->attrvaluetype != SAI_ATTR_VALUE_TYPE_OBJECT_ID)
            continue;

        if (!m_translator->checkRidExists(attr.value.oid, true))
        {
            SWSS_LOG_WARN("RID 0x%" PRIx64 " on %s is not present on local ASIC DB", attr.value.oid, meta->attridname);

            result = false;
        }
    }

    return result;
}

bool NotificationProcessor::contains_fdb_flush_event(
        _In_ uint32_t count,
        _In_ const sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    sai_mac_t mac = { 0, 0, 0, 0, 0, 0 };

    for (uint32_t idx = 0; idx < count; idx++)
    {
        if (memcmp(mac, data[idx].fdb_entry.mac_address, sizeof(mac)) == 0)
            return true;
    }

    return false;
}

void NotificationProcessor::process_on_fdb_event(
        _In_ uint32_t count,
        _In_ sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("fdb event count: %u", count);

    bool sendntf = true;

    for (uint32_t i = 0; i < count; i++)
    {
        sai_fdb_event_notification_data_t *fdb = &data[i];

        sendntf &= check_fdb_event_notification_data(*fdb);

        if (!sendntf)
        {
            SWSS_LOG_ERROR("invalid OIDs in fdb notifications, NOT translating and NOT storing in ASIC DB");
            continue;
        }

        SWSS_LOG_DEBUG("fdb %u: type: %d", i, fdb->event_type);

        fdb->fdb_entry.switch_id = m_translator->translateRidToVid(fdb->fdb_entry.switch_id, SAI_NULL_OBJECT_ID);

        fdb->fdb_entry.bv_id = m_translator->translateRidToVid(fdb->fdb_entry.bv_id, fdb->fdb_entry.switch_id, true);

        m_translator->translateRidToVid(SAI_OBJECT_TYPE_FDB_ENTRY, fdb->fdb_entry.switch_id, fdb->attr_count, fdb->attr, true);

        /*
         * Currently because of brcm bug, we need to install fdb entries in
         * asic view and currently this event don't have fdb type which is
         * required on creation.
         */

        redisPutFdbEntryToAsicView(fdb);
    }

    if (sendntf)
    {
        std::string s = sai_serialize_fdb_event_ntf(count, data);

        sendNotification(SAI_SWITCH_NOTIFICATION_NAME_FDB_EVENT, s);
    }
    else
    {
        SWSS_LOG_ERROR("FDB notification was not sent since it contain invalid OIDs, bug?");
    }
}

/**
 * @Brief Check NAT event notification data.
 *
 * Every OID field in notification data as well as all OID attributes are
 * checked if given OID (returned from ASIC) is already present in the syncd
 * local database. If vendor SAI will return unknown/invalid OID, this
 * function will return false.
 *
 * @param data NAT event notification data
 *
 * @return False if any of OID values is not present in local DB, otherwise
 * true.
 */
bool NotificationProcessor::check_nat_event_notification_data(
        _In_ const sai_nat_event_notification_data_t& data)
{
    SWSS_LOG_ENTER();

    bool result = true;

    if (!m_translator->checkRidExists(data.nat_entry.vr_id, true))
    {
        SWSS_LOG_ERROR("vr_id RID 0x%" PRIx64 " is not present on local ASIC DB: %s", data.nat_entry.vr_id,
                sai_serialize_nat_entry(data.nat_entry).c_str());

        result = false;
    }

    if (!m_translator->checkRidExists(data.nat_entry.switch_id) || data.nat_entry.switch_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("switch_id RID 0x%" PRIx64 " is not present on local ASIC DB: %s", data.nat_entry.switch_id,
                sai_serialize_nat_entry(data.nat_entry).c_str());

        result = false;
    }

    return result;
}

void NotificationProcessor::process_on_nat_event(
        _In_ uint32_t count,
        _In_ sai_nat_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("nat event count: %u", count);

    bool sendntf = true;

    for (uint32_t i = 0; i < count; i++)
    {
        sai_nat_event_notification_data_t *nat = &data[i];

        sendntf &= check_nat_event_notification_data(*nat);

        if (!sendntf)
        {
            SWSS_LOG_ERROR("invalid OIDs in nat notifications, NOT translating and NOT storing in ASIC DB");
            continue;
        }

        SWSS_LOG_DEBUG("nat %u: type: %d", i, nat->event_type);

        nat->nat_entry.switch_id = m_translator->translateRidToVid(nat->nat_entry.switch_id, SAI_NULL_OBJECT_ID);

        nat->nat_entry.vr_id = m_translator->translateRidToVid(nat->nat_entry.vr_id, nat->nat_entry.switch_id, true);
    }

    if (sendntf)
    {
        std::string s = sai_serialize_nat_event_ntf(count, data);

        sendNotification(SAI_SWITCH_NOTIFICATION_NAME_NAT_EVENT, s);
    }
    else
    {
        SWSS_LOG_ERROR("NAT notification was not sent since it contain invalid OIDs, bug?");
    }
}

void NotificationProcessor::process_on_queue_deadlock_event(
        _In_ uint32_t count,
        _In_ sai_queue_deadlock_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("queue deadlock notification count: %u", count);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_queue_deadlock_notification_data_t *deadlock_data = &data[i];

        /*
         * We are using switch_rid as null, since queue should be already
         * defined inside local db after creation.
         *
         * If this will be faster than return from create queue then we can use
         * query switch id and extract rid of switch id and then convert it to
         * switch vid.
         */

        deadlock_data->queue_id = m_translator->translateRidToVid(deadlock_data->queue_id, SAI_NULL_OBJECT_ID);
    }

    std::string s = sai_serialize_queue_deadlock_ntf(count, data);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_QUEUE_PFC_DEADLOCK, s);
}


void NotificationProcessor::process_on_port_host_tx_ready_change(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_id_t port_id,
        _In_ sai_port_host_tx_ready_status_t *host_tx_ready_status)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("Port ID before translating from RID to VID is %s", sai_serialize_object_id(port_id).c_str());
    sai_object_id_t port_vid = m_translator->translateRidToVid(port_id, SAI_NULL_OBJECT_ID);
    SWSS_LOG_DEBUG("Port ID after translating from RID to VID is %s", sai_serialize_object_id(port_id).c_str());

    sai_object_id_t switch_vid = m_translator->translateRidToVid(switch_id, SAI_NULL_OBJECT_ID);

    std::string s = sai_serialize_port_host_tx_ready_ntf(switch_vid, port_vid, *host_tx_ready_status);

    SWSS_LOG_DEBUG("Host_tx_ready status after sai_serialize is %s", s.c_str());

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_PORT_HOST_TX_READY, s);
}


void NotificationProcessor::process_on_port_state_change(
        _In_ uint32_t count,
        _In_ sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("port notification count: %u", count);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_port_oper_status_notification_t *oper_stat = &data[i];
        sai_object_id_t rid = oper_stat->port_id;

        /*
         * We are using switch_rid as null, since port should be already
         * defined inside local db after creation.
         *
         * If this will be faster than return from create port then we can use
         * query switch id and extract rid of switch id and then convert it to
         * switch vid.
         */

        SWSS_LOG_INFO("Port RID %s state change notification",
                sai_serialize_object_id(rid).c_str());

        if (false == m_translator->tryTranslateRidToVid(rid, oper_stat->port_id))
        {
            SWSS_LOG_WARN("Port RID %s transalted to null VID!!!", sai_serialize_object_id(rid).c_str());
        }

        /*
         * Port may be in process of removal. OA may receive notification for VID either
         * SAI_NULL_OBJECT_ID or non exist at time of processing
         */

        SWSS_LOG_INFO("Port VID %s state change notification",
                sai_serialize_object_id(oper_stat->port_id).c_str());
    }

    std::string s = sai_serialize_port_oper_status_ntf(count, data);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_PORT_STATE_CHANGE, s);
}

void NotificationProcessor::process_on_bfd_session_state_change(
        _In_ uint32_t count,
        _In_ sai_bfd_session_state_notification_t *data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("bfd session state notification count: %u", count);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_bfd_session_state_notification_t *bfd_session_state = &data[i];

        /*
         * We are using switch_rid as null, since BFD should be already
         * defined inside local db after creation.
         *
         * If this will be faster than return from create BFD then we can use
         * query switch id and extract rid of switch id and then convert it to
         * switch vid.
         */

        bfd_session_state->bfd_session_id = m_translator->translateRidToVid(bfd_session_state->bfd_session_id, SAI_NULL_OBJECT_ID, true);
    }

    std::string s = sai_serialize_bfd_session_state_ntf(count, data);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_BFD_SESSION_STATE_CHANGE, s);
}


void NotificationProcessor::process_on_switch_asic_sdk_health_event(
        _In_ sai_object_id_t switch_rid,
        _In_ sai_switch_asic_sdk_health_severity_t severity,
        _In_ sai_timespec_t timestamp,
        _In_ sai_switch_asic_sdk_health_category_t category,
        _In_ sai_switch_health_data_t data,
        _In_ const sai_u8_list_t description)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_vid = m_translator->translateRidToVid(switch_rid, SAI_NULL_OBJECT_ID);

    std::string s = sai_serialize_switch_asic_sdk_health_event(switch_vid, severity, timestamp, category, data, description);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_SWITCH_ASIC_SDK_HEALTH_EVENT, s);
}

void NotificationProcessor::process_on_switch_shutdown_request(
        _In_ sai_object_id_t switch_rid)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_vid = m_translator->translateRidToVid(switch_rid, SAI_NULL_OBJECT_ID);

    std::string s = sai_serialize_switch_shutdown_request(switch_vid);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_SWITCH_SHUTDOWN_REQUEST, s);
}

void NotificationProcessor::process_on_twamp_session_event(
        _In_ uint32_t count,
        _In_ sai_twamp_session_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("twamp session state notification count: %u", count);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_twamp_session_event_notification_data_t *twamp_session_state = &data[i];

        /*
         * We are using switch_rid as null, since TWAMP should be already
         * defined inside local db after creation.
         *
         * If this will be faster than return from create TWAMP then we can use
         * query switch id and extract rid of switch id and then convert it to
         * switch vid.
         */

        twamp_session_state->twamp_session_id = m_translator->translateRidToVid(twamp_session_state->twamp_session_id, SAI_NULL_OBJECT_ID);
    }

    /* send notification to syncd */
    std::string s = sai_serialize_twamp_session_event_ntf(count, data);

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_TWAMP_SESSION_EVENT, s);
}

void NotificationProcessor::handle_switch_state_change(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    sai_switch_oper_status_t switch_oper_status;
    sai_object_id_t switch_id;

    sai_deserialize_switch_oper_status(data, switch_id, switch_oper_status);

    process_on_switch_state_change(switch_id, switch_oper_status);
}

void NotificationProcessor::handle_fdb_event(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    uint32_t count;
    sai_fdb_event_notification_data_t *fdbevent = NULL;

    sai_deserialize_fdb_event_ntf(data, count, &fdbevent);

    if (contains_fdb_flush_event(count, fdbevent))
    {
        SWSS_LOG_NOTICE("got fdb flush event: %s", data.c_str());
    }

    process_on_fdb_event(count, fdbevent);

    sai_deserialize_free_fdb_event_ntf(count, fdbevent);
}

void NotificationProcessor::handle_nat_event(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    uint32_t count;
    sai_nat_event_notification_data_t *natevent = NULL;

    sai_deserialize_nat_event_ntf(data, count, &natevent);

    process_on_nat_event(count, natevent);

    sai_deserialize_free_nat_event_ntf(count, natevent);
}

void NotificationProcessor::handle_queue_deadlock(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    uint32_t count;
    sai_queue_deadlock_notification_data_t *qdeadlockevent = NULL;

    sai_deserialize_queue_deadlock_ntf(data, count, &qdeadlockevent);

    process_on_queue_deadlock_event(count, qdeadlockevent);

    sai_deserialize_free_queue_deadlock_ntf(count, qdeadlockevent);
}

void NotificationProcessor::handle_port_state_change(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    uint32_t count;
    sai_port_oper_status_notification_t *portoperstatus = NULL;

    sai_deserialize_port_oper_status_ntf(data, count, &portoperstatus);

    process_on_port_state_change(count, portoperstatus);

    sai_deserialize_free_port_oper_status_ntf(count, portoperstatus);
}

void NotificationProcessor::handle_port_host_tx_ready_change(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    sai_object_id_t port_id;
    sai_object_id_t switch_id;
    sai_port_host_tx_ready_status_t host_tx_ready_status;

    sai_deserialize_port_host_tx_ready_ntf(data, switch_id, port_id, host_tx_ready_status);

    process_on_port_host_tx_ready_change(switch_id, port_id, &host_tx_ready_status);
}

void NotificationProcessor::handle_bfd_session_state_change(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    uint32_t count;
    sai_bfd_session_state_notification_t *bfdsessionstate = NULL;

    sai_deserialize_bfd_session_state_ntf(data, count, &bfdsessionstate);

    process_on_bfd_session_state_change(count, bfdsessionstate);

    sai_deserialize_free_bfd_session_state_ntf(count, bfdsessionstate);
}

void NotificationProcessor::handle_switch_asic_sdk_health_event(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_id;
    sai_switch_asic_sdk_health_severity_t severity;
    sai_timespec_t timestamp;
    sai_switch_asic_sdk_health_category_t category;
    sai_switch_health_data_t health_data;
    sai_u8_list_t description;

    sai_deserialize_switch_asic_sdk_health_event(data,
                                                 switch_id,
                                                 severity,
                                                 timestamp,
                                                 category,
                                                 health_data,
                                                 description);

    process_on_switch_asic_sdk_health_event(switch_id,
                                            severity,
                                            timestamp,
                                            category,
                                            health_data,
                                            description);

    sai_deserialize_free_switch_asic_sdk_health_event(description);
}
void NotificationProcessor::handle_switch_shutdown_request(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_id;

    sai_deserialize_switch_shutdown_request(data, switch_id);

    process_on_switch_shutdown_request(switch_id);
}

void NotificationProcessor::handle_twamp_session_event(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    uint32_t count;
    sai_twamp_session_event_notification_data_t *twampsessionevent = NULL;

    sai_deserialize_twamp_session_event_ntf(data, count, &twampsessionevent);

    process_on_twamp_session_event(count, twampsessionevent);

    sai_deserialize_free_twamp_session_event_ntf(count, twampsessionevent);
}

void NotificationProcessor::handle_tam_tel_type_config_change(
    _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("TAM telemesai_serialize_object_id(tam_type_id)try type config change on TAM id %s", data.c_str());

    sendNotification(SAI_SWITCH_NOTIFICATION_NAME_TAM_TEL_TYPE_CONFIG_CHANGE, data);
}

void NotificationProcessor::processNotification(
        _In_ const swss::KeyOpFieldsValuesTuple& item)
{
    SWSS_LOG_ENTER();

    m_synchronizer(item);
}

void NotificationProcessor::syncProcessNotification(
        _In_ const swss::KeyOpFieldsValuesTuple& item)
{
    SWSS_LOG_ENTER();

    std::string notification = kfvKey(item);
    std::string data = kfvOp(item);

    if (notification == SAI_SWITCH_NOTIFICATION_NAME_SWITCH_STATE_CHANGE)
    {
        handle_switch_state_change(data);
    }
    else if (notification == SAI_SWITCH_NOTIFICATION_NAME_FDB_EVENT)
    {
        handle_fdb_event(data);
    }
    else if (notification == SAI_SWITCH_NOTIFICATION_NAME_NAT_EVENT)
    {
        handle_nat_event(data);
    }
    else if (notification == SAI_SWITCH_NOTIFICATION_NAME_PORT_STATE_CHANGE)
    {
        handle_port_state_change(data);
    }
    else if (notification == SAI_SWITCH_NOTIFICATION_NAME_PORT_HOST_TX_READY)
    {
        handle_port_host_tx_ready_change(data);
    }
    else if (notification == SAI_SWITCH_NOTIFICATION_NAME_SWITCH_SHUTDOWN_REQUEST)
    {
        handle_switch_shutdown_request(data);
    }
    else if (notification == SAI_SWITCH_NOTIFICATION_NAME_SWITCH_ASIC_SDK_HEALTH_EVENT)
    {
        handle_switch_asic_sdk_health_event(data);
    }
    else if (notification == SAI_SWITCH_NOTIFICATION_NAME_QUEUE_PFC_DEADLOCK)
    {
        handle_queue_deadlock(data);
    }
    else if (notification == SAI_SWITCH_NOTIFICATION_NAME_BFD_SESSION_STATE_CHANGE)
    {
        handle_bfd_session_state_change(data);
    }
    else if (notification == SAI_SWITCH_NOTIFICATION_NAME_TWAMP_SESSION_EVENT)
    {
        handle_twamp_session_event(data);
    }
    else if (notification == SAI_SWITCH_NOTIFICATION_NAME_TAM_TEL_TYPE_CONFIG_CHANGE)
    {
        handle_tam_tel_type_config_change(data);
    }
    else
    {
        SWSS_LOG_ERROR("unknown notification: %s", notification.c_str());
    }
}

void NotificationProcessor::ntf_process_function()
{
    SWSS_LOG_ENTER();

    std::mutex ntf_mutex;

    std::unique_lock<std::mutex> ulock(ntf_mutex);

    while (m_runThread)
    {
        m_cv.wait(ulock);

        // this is notifications processing thread context, which is different
        // from SAI notifications context, we can safe use syncd mutex here,
        // processing each notification is under same mutex as processing main
        // events, counters and reinit

        swss::KeyOpFieldsValuesTuple item;

        while (m_notificationQueue->tryDequeue(item))
        {
            processNotification(item);
        }
    }
}

void NotificationProcessor::startNotificationsProcessingThread()
{
    SWSS_LOG_ENTER();

    m_runThread = true;

    m_ntf_process_thread = std::make_shared<std::thread>(&NotificationProcessor::ntf_process_function, this);
}

void NotificationProcessor::stopNotificationsProcessingThread()
{
    SWSS_LOG_ENTER();

    m_runThread = false;

    m_cv.notify_all();

    if (m_ntf_process_thread != nullptr)
    {
        m_ntf_process_thread->join();
    }

    m_ntf_process_thread = nullptr;
}

void NotificationProcessor::signal()
{
    SWSS_LOG_ENTER();

    m_cv.notify_all();
}

std::shared_ptr<NotificationQueue> NotificationProcessor::getQueue() const
{
    SWSS_LOG_ENTER();

    return m_notificationQueue;
}
