#include "syncd.h"
#include "sairedis.h"

#include <queue>
#include <memory>
#include <condition_variable>

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

void process_on_switch_state_change(
        _In_ sai_object_id_t switch_rid,
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_vid = translate_rid_to_vid(switch_rid, SAI_NULL_OBJECT_ID);

    std::string s = sai_serialize_switch_oper_status(switch_vid, switch_oper_status);

    send_notification("switch_state_change", s);
}

sai_fdb_entry_type_t getFdbEntryType(
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

void redisPutFdbEntryToAsicView(
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

    sai_object_type_t objectType = SAI_OBJECT_TYPE_FDB_ENTRY;

    std::string strObjectType = sai_serialize_object_type(objectType);

    std::string strFdbEntry = sai_serialize_fdb_entry(fdb->fdb_entry);

    std::string key = ASIC_STATE_TABLE + (":" + strObjectType + ":" + strFdbEntry);

    if ((fdb->fdb_entry.switch_id == SAI_NULL_OBJECT_ID ||
         fdb->fdb_entry.bv_id == SAI_NULL_OBJECT_ID) && 
        (fdb->event_type != SAI_FDB_EVENT_FLUSHED))
    {
        SWSS_LOG_WARN("skipped to put int db: %s", strFdbEntry.c_str());
        return;
    }

    if (fdb->event_type == SAI_FDB_EVENT_AGED)
    {
        SWSS_LOG_DEBUG("remove fdb entry %s for SAI_FDB_EVENT_AGED",key.c_str());
        g_redisClient->del(key);
        return;
    }

    if (fdb->event_type == SAI_FDB_EVENT_FLUSHED)
    {
        sai_object_id_t bv_id = fdb->fdb_entry.bv_id;
        sai_object_id_t port_oid = 0;
        
        for (uint32_t i = 0; i < fdb->attr_count; i++)
        {
            if(fdb->attr[i].id == SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID)
            {
                port_oid = fdb->attr[i].value.oid;
            }
        }
        
                
        if (!port_oid && !bv_id)
        {
            /* we got a flush all fdb event here */
            /* example of a flush all fdb event   */
            /*
            [{
            "fdb_entry":"{
                \"bv_id\":\"oid:0x0\",
                \"mac\":\"00:00:00:00:00:00\",
                \"switch_id\":\"oid:0x21000000000000\"}",
            "fdb_event":"SAI_FDB_EVENT_FLUSHED",
                "list":[
                    {"id":"SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID","value":"oid:0x0"},
                    {"id":"SAI_FDB_ENTRY_ATTR_TYPE","value":"SAI_FDB_ENTRY_TYPE_DYNAMIC"},
                    {"id":"SAI_FDB_ENTRY_ATTR_PACKET_ACTION","value":"SAI_PACKET_ACTION_FORWARD"}
                ]
            }]
            */
            SWSS_LOG_NOTICE("received a flush all fdb event");
            std::string pattern = ASIC_STATE_TABLE + std::string(":SAI_OBJECT_TYPE_FDB_ENTRY:*");
            for (const auto &fdbkey: g_redisClient->keys(pattern))
            {
                /* we only remove dynamic fdb entries here, static fdb entries need to be deleted manually by user instead of flush */
                auto pEntryType = g_redisClient->hget(fdbkey, "SAI_FDB_ENTRY_ATTR_TYPE");
                if (pEntryType != NULL)
                {
                    std::string strEntryType = *pEntryType;
                    if (strEntryType == "SAI_FDB_ENTRY_TYPE_DYNAMIC")
                    {
                        SWSS_LOG_DEBUG("remove fdb entry %s for SAI_FDB_EVENT_FLUSHED",fdbkey.c_str());
                        g_redisClient->del(fdbkey);
                    }
                }
                else
                {
                    SWSS_LOG_ERROR("found unknown type fdb entry, key %s", fdbkey.c_str());
                }
            }
        }
        else if (port_oid && !bv_id)
        {
            /* we got a flush port fdb event here        */
            /* not supported yet, this is a place holder */
            /* example of a flush port fdb event         */
            /*
            [{
            "fdb_entry":"{
                \"bv_id\":\"oid:0x0\",
                \"mac\":\"00:00:00:00:00:00\",
                \"switch_id\":\"oid:0x21000000000000\"}",
            "fdb_event":"SAI_FDB_EVENT_FLUSHED",
                "list":[
                    {"id":"SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID","value":"oid:0x3a0000000009cf"},
                    {"id":"SAI_FDB_ENTRY_ATTR_TYPE","value":"SAI_FDB_ENTRY_TYPE_DYNAMIC"},
                    {"id":"SAI_FDB_ENTRY_ATTR_PACKET_ACTION","value":"SAI_PACKET_ACTION_FORWARD"}
                ]
            }]
            */
            SWSS_LOG_ERROR("received a flush port fdb event, port_oid = 0x%lx, bv_id = 0x%lx, unsupported", port_oid, bv_id);
        }
        else if (!port_oid && bv_id)
        {
            /* we got a flush vlan fdb event here        */
            /* not supported yet, this is a place holder */
            /* example of a flush vlan event             */
            /*
            [{
            "fdb_entry":"{
                \"bridge_id\":\"oid:0x23000000000000\",
                \"mac\":\"00:00:00:00:00:00\",
                \"switch_id\":\"oid:0x21000000000000\"}",
            "fdb_event":"SAI_FDB_EVENT_FLUSHED",
                "list":[
                    {"id":"SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID","value":"oid:0x0"},
                    {"id":"SAI_FDB_ENTRY_ATTR_TYPE","value":"SAI_FDB_ENTRY_TYPE_DYNAMIC"},
                    {"id":"SAI_FDB_ENTRY_ATTR_PACKET_ACTION","value":"SAI_PACKET_ACTION_FORWARD"}
                ]
            }]
            */
            SWSS_LOG_ERROR("received a flush vlan fdb event, port_oid = 0x%lx, bv_id = 0x%lx, unsupported", port_oid, bv_id);
            
        }
        else
        {
            SWSS_LOG_ERROR("received a flush fdb event, port_oid = 0x%lx, bv_id = 0x%lx, unsupported", port_oid, bv_id);
        }

        return;
    }

    for (const auto &e: entry)
    {
        const std::string &strField = fvField(e);
        const std::string &strValue = fvValue(e);

        g_redisClient->hset(key, strField, strValue);
    }

    // currently we need to add type manually since fdb event don't contain type
    sai_attribute_t attr;

    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr.value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;

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

    g_redisClient->hset(key, strAttrId, strAttrValue);
}

void check_fdb_event_notification_data(
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

    if (!check_rid_exists(data.fdb_entry.bv_id))
        SWSS_LOG_ERROR("bv_id RID 0x%lx is not present on local ASIC DB: %s", data.fdb_entry.bv_id,
                sai_serialize_fdb_entry(data.fdb_entry).c_str());

    if (!check_rid_exists(data.fdb_entry.switch_id) || data.fdb_entry.switch_id == SAI_NULL_OBJECT_ID)
        SWSS_LOG_ERROR("switch_id RID 0x%lx is not present on local ASIC DB: %s", data.fdb_entry.bv_id,
                sai_serialize_fdb_entry(data.fdb_entry).c_str());

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

        if (!check_rid_exists(attr.value.oid))
            SWSS_LOG_WARN("RID 0x%lx on %s is not present on local ASIC DB", attr.value.oid, meta->attridname);
    }
}

void process_on_fdb_event(
        _In_ uint32_t count,
        _In_ sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("fdb event count: %u", count);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_fdb_event_notification_data_t *fdb = &data[i];

        check_fdb_event_notification_data(*fdb);

        SWSS_LOG_DEBUG("fdb %u: type: %d", i, fdb->event_type);

        fdb->fdb_entry.switch_id = translate_rid_to_vid(fdb->fdb_entry.switch_id, SAI_NULL_OBJECT_ID);

        fdb->fdb_entry.bv_id = translate_rid_to_vid(fdb->fdb_entry.bv_id, fdb->fdb_entry.switch_id);

        translate_rid_to_vid_list(SAI_OBJECT_TYPE_FDB_ENTRY, fdb->fdb_entry.switch_id, fdb->attr_count, fdb->attr);

        /*
         * Currently because of brcm bug, we need to install fdb entries in
         * asic view and currently this event don't have fdb type which is
         * required on creation.
         */

        redisPutFdbEntryToAsicView(fdb);
    }

    std::string s = sai_serialize_fdb_event_ntf(count, data);

    send_notification("fdb_event", s);
}

void process_on_queue_deadlock_event(
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

        deadlock_data->queue_id = translate_rid_to_vid(deadlock_data->queue_id, SAI_NULL_OBJECT_ID);
    }

    std::string s = sai_serialize_queue_deadlock_ntf(count, data);

    send_notification("queue_deadlock", s);
}

void process_on_port_state_change(
        _In_ uint32_t count,
        _In_ sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("port notification count: %u", count);

    for (uint32_t i = 0; i < count; i++)
    {
        sai_port_oper_status_notification_t *oper_stat = &data[i];

        /*
         * We are using switch_rid as null, since port should be already
         * defined inside local db after creation.
         *
         * If this will be faster than return from create port then we can use
         * query switch id and extract rid of switch id and then convert it to
         * switch vid.
         */

        oper_stat->port_id = translate_rid_to_vid(oper_stat->port_id, SAI_NULL_OBJECT_ID);
    }

    std::string s = sai_serialize_port_oper_status_ntf(count, data);

    send_notification("port_state_change", s);
}

void process_on_switch_shutdown_request(
        _In_ sai_object_id_t switch_rid)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_vid = translate_rid_to_vid(switch_rid, SAI_NULL_OBJECT_ID);

    std::string s = sai_serialize_switch_shutdown_request(switch_vid);

    send_notification("switch_shutdown_request", s);
}

void handle_switch_state_change(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    sai_switch_oper_status_t switch_oper_status;
    sai_object_id_t switch_id;

    sai_deserialize_switch_oper_status(data, switch_id, switch_oper_status);

    process_on_switch_state_change(switch_id, switch_oper_status);
}

void handle_fdb_event(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    uint32_t count;
    sai_fdb_event_notification_data_t *fdbevent = NULL;

    sai_deserialize_fdb_event_ntf(data, count, &fdbevent);

    process_on_fdb_event(count, fdbevent);

    sai_deserialize_free_fdb_event_ntf(count, fdbevent);
}

void handle_queue_deadlock(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    uint32_t count;
    sai_queue_deadlock_notification_data_t *qdeadlockevent = NULL;

    sai_deserialize_queue_deadlock_ntf(data, count, &qdeadlockevent);

    process_on_queue_deadlock_event(count, qdeadlockevent);

    sai_deserialize_free_queue_deadlock_ntf(count, qdeadlockevent);
}

void handle_port_state_change(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    uint32_t count;
    sai_port_oper_status_notification_t *portoperstatus = NULL;

    sai_deserialize_port_oper_status_ntf(data, count, &portoperstatus);

    process_on_port_state_change(count, portoperstatus);

    sai_deserialize_free_port_oper_status_ntf(count, portoperstatus);
}

void handle_switch_shutdown_request(
        _In_ const std::string &data)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_id;

    sai_deserialize_switch_shutdown_request(data, switch_id);

    process_on_switch_shutdown_request(switch_id);
}

void processNotification(
        _In_ const swss::KeyOpFieldsValuesTuple &item)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    std::string notification = kfvKey(item);
    std::string data = kfvOp(item);

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
    else if (notification == "queue_deadlock")
    {
        handle_queue_deadlock(data);
    }
    else
    {
        SWSS_LOG_ERROR("unknow notification: %s", notification.c_str());
    }
}

// condition variable will be used to notify processing thread
// that some notification arrived

std::condition_variable cv;

class ntf_queue_t
{
public:
    ntf_queue_t() { }
    bool enqueue(swss::KeyOpFieldsValuesTuple msg);
    bool tryDequeue(swss::KeyOpFieldsValuesTuple& msg);
    size_t queueStats()
    {
        SWSS_LOG_ENTER();

        std::lock_guard<std::mutex> lock(queue_mutex);

        return ntf_queue.size();
    }

private:
    std::mutex queue_mutex;
    std::queue<swss::KeyOpFieldsValuesTuple> ntf_queue;

    /*
     * Value based on typical L2 deployment with 256k MAC entries and
     * some extra buffer for other events like port-state, q-deadlock etc
     */
    const size_t limit = 300000;
};


/*
 * Make sure that notification queue pointer is populated before we start
 * thread, and before we create_switch, since at switch_create we can start
 * receiving fdb_notifications which will arrive on different thread and
 * will call queueStats() when queue pointer could be null (this=0x0).
 */

static std::unique_ptr<ntf_queue_t> ntf_queue_hdlr = std::unique_ptr<ntf_queue_t>(new ntf_queue_t);

bool ntf_queue_t::tryDequeue(
        _Out_ swss::KeyOpFieldsValuesTuple &item)
{
    std::lock_guard<std::mutex> lock(queue_mutex);

    SWSS_LOG_ENTER();

    if (ntf_queue.empty())
    {
        return false;
    }

    item = ntf_queue.front();

    ntf_queue.pop();

    return true;
}

bool ntf_queue_t::enqueue(
        _In_ swss::KeyOpFieldsValuesTuple item)
{
    SWSS_LOG_ENTER();

    std::string notification = kfvKey(item);

    /*
     * If the queue exceeds the limit, then drop all further FDB events This is
     * a temporary solution to handle high memory usage by syncd and the
     * notification queue keeps growing. The permanent solution would be to
     * make this stateful so that only the *latest* event is published.
     */

    if (queueStats() < limit || notification != "fdb_event")
    {
        std::lock_guard<std::mutex> lock(queue_mutex);

        ntf_queue.push(item);
        return true;
    }

    static uint32_t log_count = 0;

    if (!(log_count % 1000))
    {
        SWSS_LOG_NOTICE("Too many messages in queue (%zu), dropped %u FDB events!",
                         queueStats(), (log_count+1));
    }

    ++log_count;

    return false;
}

void enqueue_notification(
        _In_ std::string op,
        _In_ std::string data,
        _In_ std::vector<swss::FieldValueTuple> &entry)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("%s %s", op.c_str(), data.c_str());

    swss::KeyOpFieldsValuesTuple item(op, data, entry);

    if(ntf_queue_hdlr->enqueue(item))
    {
        cv.notify_all();
    }
}

void enqueue_notification(
        _In_ std::string op,
        _In_ std::string data)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry;

    enqueue_notification(op, data, entry);
}

void on_switch_state_change(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_switch_oper_status(switch_id, switch_oper_status);

    enqueue_notification("switch_state_change", s);
}

void on_fdb_event(
        _In_ uint32_t count,
        _In_ const sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_fdb_event_ntf(count, data);

    enqueue_notification("fdb_event", s);
}

void on_queue_deadlock(
        _In_ uint32_t count,
        _In_ const sai_queue_deadlock_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_queue_deadlock_ntf(count, data);

    enqueue_notification("queue_deadlock", s);
}

void on_port_state_change(
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_port_oper_status_ntf(count, data);

    enqueue_notification("port_state_change", s);
}

void on_switch_shutdown_request(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_switch_shutdown_request(switch_id);

    enqueue_notification("switch_shutdown_request", "");
}

void on_packet_event(
        _In_ sai_object_id_t switch_id,
        _In_ sai_size_t buffer_size,
        _In_ const void *buffer,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");
}

// determine whether notification thread is running

volatile bool runThread;

std::mutex ntf_mutex;
std::unique_lock<std::mutex> ulock(ntf_mutex);

void ntf_process_function()
{
    SWSS_LOG_ENTER();

    while (runThread)
    {
        cv.wait(ulock);

        // this is notifications processing thread context, which is different
        // from SAI notifications context, we can safe use g_mutex here,
        // processing each notification is under same mutex as processing main
        // events, counters and reinit

        swss::KeyOpFieldsValuesTuple item;

        while (ntf_queue_hdlr->tryDequeue(item))
        {
            processNotification(item);
        }
    }
}

std::shared_ptr<std::thread> ntf_process_thread;

void startNotificationsProcessingThread()
{
    SWSS_LOG_ENTER();

    runThread = true;

    ntf_process_thread = std::make_shared<std::thread>(ntf_process_function);
}

void stopNotificationsProcessingThread()
{
    SWSS_LOG_ENTER();

    runThread = false;

    cv.notify_all();

    if (ntf_process_thread != nullptr)
    {
        ntf_process_thread->join();
    }

    ntf_process_thread = nullptr;
}

sai_switch_state_change_notification_fn     on_switch_state_change_ntf = on_switch_state_change;
sai_switch_shutdown_request_notification_fn on_switch_shutdown_request_ntf = on_switch_shutdown_request;
sai_fdb_event_notification_fn               on_fdb_event_ntf = on_fdb_event;
sai_port_state_change_notification_fn       on_port_state_change_ntf = on_port_state_change;
sai_packet_event_notification_fn            on_packet_event_ntf = on_packet_event;
sai_queue_pfc_deadlock_notification_fn      on_queue_deadlock_ntf = on_queue_deadlock;

void check_notifications_pointers(
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * This function should only be called on CREATE/SET api when object is
     * SWITCH.
     *
     * Notifications pointers needs to be corrected since those we receive from
     * sairedis are in sairedis memory space and here we are using those ones
     * we declared in syncd memory space.
     *
     * Also notice that we are using the same pointers for ALL switches.
     */

    for (uint32_t index = 0; index < attr_count; ++index)
    {
        sai_attribute_t &attr = attr_list[index];

        auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr.id);

        if (meta->attrvaluetype != SAI_ATTR_VALUE_TYPE_POINTER)
        {
            continue;
        }

        /*
         * Does not matter if pointer is valid or not, we just want the
         * previous value.
         */

        sai_pointer_t prev = attr.value.ptr;

        if (prev == NULL)
        {
            /*
             * If pointer is NULL, then fine, let it be.
             */

            continue;
        }

        switch (attr.id)
        {
            case SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY:
                attr.value.ptr = (void*)on_switch_state_change_ntf;
                break;

            case SAI_SWITCH_ATTR_SHUTDOWN_REQUEST_NOTIFY:
                attr.value.ptr = (void*)on_switch_shutdown_request_ntf;
                break;

            case SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY:
                attr.value.ptr = (void*)on_fdb_event_ntf;
                break;

            case SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY:
                attr.value.ptr = (void*)on_port_state_change_ntf;
                break;

            case SAI_SWITCH_ATTR_PACKET_EVENT_NOTIFY:
                attr.value.ptr = (void*)on_packet_event_ntf;
                break;

            case SAI_SWITCH_ATTR_QUEUE_PFC_DEADLOCK_NOTIFY:
                attr.value.ptr = (void*)on_queue_deadlock_ntf;
                break;

            default:

                SWSS_LOG_ERROR("pointer for %s is not handled, FIXME!", meta->attridname);
                continue;
        }

        /*
         * Here we translated pointer, just log it.
         */

        SWSS_LOG_INFO("%s: 0x%lX (orch) => 0x%lX (syncd)", meta->attridname, (uint64_t)prev, (uint64_t)attr.value.ptr);
    }
}
