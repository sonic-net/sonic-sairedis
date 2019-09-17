#include "sai_redis.h"
#include "sairedis.h"

#include "meta/sai_serialize.h"

#include <thread>

volatile bool g_asicInitViewMode = false; // default mode is apply mode
volatile bool g_useTempView = false;
volatile bool g_syncMode = false;

sai_status_t sai_redis_internal_notify_syncd(
        _In_ const std::string& key)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry;

    // ASIC INIT/APPLY view with small letter 'a'
    // and response is recorded as capital letter 'A'

    if (g_record)
    {
        recordLine("a|" + key);
    }

    g_asicState->set(key, entry, "notify");

    swss::Select s;

    s.addSelectable(g_redisGetConsumer.get());

    while (true)
    {
        SWSS_LOG_NOTICE("wait for notify response");

        swss::Selectable *sel;

        int result = s.select(&sel, GET_RESPONSE_TIMEOUT);

        if (result == swss::Select::OBJECT)
        {
            swss::KeyOpFieldsValuesTuple kco;

            g_redisGetConsumer->pop(kco);

            const std::string &op = kfvOp(kco);
            const std::string &opkey = kfvKey(kco);

            SWSS_LOG_NOTICE("notify response: %s", opkey.c_str());

            if (op != "notify")
            {
                continue;
            }

            if (g_record)
            {
                recordLine("A|" + opkey);
            }

            sai_status_t status;
            sai_deserialize_status(opkey, status);

            return status;
        }

        SWSS_LOG_ERROR("notify syncd failed to get response result from select: %d", result);
        break;
    }

    SWSS_LOG_ERROR("notify syncd failed to get response");

    if (g_record)
    {
        recordLine("A|SAI_STATUS_FAILURE");
    }

    return SAI_STATUS_FAILURE;
}

sai_status_t sai_redis_notify_syncd(
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    // we need to use "GET" channel to be sure that
    // all previous operations were applied, if we don't
    // use GET channel then we may hit race condition
    // on syncd side where syncd will start compare view
    // when there are still objects in op queue
    //
    // other solution can be to use notify event
    // and then on syncd side read all the asic state queue
    // and apply changes before switching to init/apply mode

    std::string op;

    switch (attr->value.s32)
    {
        case SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW:
            SWSS_LOG_NOTICE("sending syncd INIT view");
            op = SYNCD_INIT_VIEW;
            g_asicInitViewMode = true;
            break;

        case SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW:
            SWSS_LOG_NOTICE("sending syncd APPLY view");
            op = SYNCD_APPLY_VIEW;
            g_asicInitViewMode = false;
            break;

        case SAI_REDIS_NOTIFY_SYNCD_INSPECT_ASIC:
            SWSS_LOG_NOTICE("sending syncd INSPECT ASIC");
            op = SYNCD_INSPECT_ASIC;
            break;

        default:
            SWSS_LOG_ERROR("invalid notify syncd attr value %d", attr->value.s32);
            return SAI_STATUS_FAILURE;
    }

    sai_status_t status = sai_redis_internal_notify_syncd(op);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("notify syncd failed: %s", sai_serialize_status(status).c_str());
        return status;
    }

    SWSS_LOG_NOTICE("notify syncd succeeded");

    if (attr->value.s32 == SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW)
    {
        SWSS_LOG_NOTICE("clearing current local state since init view is called on initialized switch");

        clear_local_state();
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t redis_create_switch(
        _Out_ sai_object_id_t* switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    /*
     * Creating switch can't have any object attributes set on it, need to be
     * set on separate api.
     */

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr_list[i].id);

        if (meta == NULL)
        {
            SWSS_LOG_ERROR("attribute %d not found", attr_list[i].id);

            return SAI_STATUS_INVALID_PARAMETER;
        }

        if (meta->allowedobjecttypeslength > 0)
        {
            SWSS_LOG_ERROR("attribute %s is object id attribute, not allowed on create", meta->attridname);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = meta_sai_create_oid(
            SAI_OBJECT_TYPE_SWITCH,
            switch_id,
            SAI_NULL_OBJECT_ID, // no switch id since we create switch
            attr_count,
            attr_list,
            &redis_generic_create);

    if (status == SAI_STATUS_SUCCESS)
    {
        /*
         * When doing CREATE operation user may want to update notification
         * pointers. TODO this be per switch.
         *
         * Currently we will have same notifications for all switches.
         */

        check_notifications_pointers(attr_count, attr_list);
    }

    return status;
}

sai_status_t redis_remove_switch(
        _In_ sai_object_id_t switch_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    sai_status_t status = meta_sai_remove_oid(
            SAI_OBJECT_TYPE_SWITCH,
            switch_id,
            &redis_generic_remove);

    if (status == SAI_STATUS_SUCCESS)
    {
        /*
         * NOTE: Should we remove notifications here? Probably not since we can
         * have multiple switches and then other switches also may send
         * notifications and they still can be used since we have global
         * notifications and not per switch.
         */
    }

    return status;
}

sai_status_t redis_set_switch_attribute(
        _In_ sai_object_id_t switch_id,
        _In_ const sai_attribute_t *attr)
{
    // SWSS_LOG_ENTER() omitted here, defined below after mutex

    if (attr != NULL && attr->id == SAI_REDIS_SWITCH_ATTR_PERFORM_LOG_ROTATE)
    {
        /*
         * Let's avoid using mutexes, since this attribute could be used in
         * signal handler, so check it's value here. If set this attribute will
         * be performed from multiple threads there is possibility for race
         * condition here, but this doesn't matter since we only set logrotate
         * flag, and if that happens we will just reopen file less times then
         * actual set operation was called.
         */

        g_logrotate = true;

        return SAI_STATUS_SUCCESS;
    }

    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr != NULL)
    {
        /*
         * NOTE: that this will work without
         * switch being created.
         */

        switch (attr->id)
        {
            case SAI_REDIS_SWITCH_ATTR_RECORD:
                setRecording(attr->value.booldata);
                return SAI_STATUS_SUCCESS;

            case SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD:
                return sai_redis_notify_syncd(attr);

            case SAI_REDIS_SWITCH_ATTR_USE_TEMP_VIEW:
                g_useTempView = attr->value.booldata;
                return SAI_STATUS_SUCCESS;

            case SAI_REDIS_SWITCH_ATTR_SYNC_MODE:

                g_syncMode = attr->value.booldata;

                if (g_syncMode)
                {
                    SWSS_LOG_NOTICE("disabling buffered pipeline in sync mode");
                    g_asicState->setBuffered(false);
                }

                return SAI_STATUS_SUCCESS;

            case SAI_REDIS_SWITCH_ATTR_USE_PIPELINE:

                if (g_syncMode)
                {
                    SWSS_LOG_WARN("use pipeline is not supported in sync mode");
                    return SAI_STATUS_NOT_SUPPORTED;
                }

                g_asicState->setBuffered(attr->value.booldata);
                return SAI_STATUS_SUCCESS;

            case SAI_REDIS_SWITCH_ATTR_FLUSH:
                g_asicState->flush();
                return SAI_STATUS_SUCCESS;

            case SAI_REDIS_SWITCH_ATTR_RECORDING_OUTPUT_DIR:
                return setRecordingOutputDir(*attr);

            default:
                break;
        }
    }

    sai_status_t status = meta_sai_set_oid(
            SAI_OBJECT_TYPE_SWITCH,
            switch_id,
            attr,
            &redis_generic_set);

    if (status == SAI_STATUS_SUCCESS)
    {
        /*
         * When doing SET operation user may want to update notification
         * pointers.
         *
         * TODO this should be per switch.
         *
         * Currently we will have same notifications for all switches.
         */

        check_notifications_pointers(1, attr);
    }

    return status;
}

sai_status_t redis_get_switch_attribute(
        _In_ sai_object_id_t switch_id,
        _In_ sai_uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_SWITCH,
            switch_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * @brief Switch method table retrieved with sai_api_query()
 */
REDIS_GENERIC_STATS(SWITCH, switch);

const sai_switch_api_t redis_switch_api = {
    redis_create_switch,
    redis_remove_switch,
    redis_set_switch_attribute,
    redis_get_switch_attribute,
    REDIS_GENERIC_STATS_API(switch)
};
