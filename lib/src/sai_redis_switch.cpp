#include "sai_redis.h"
#include <thread>

sai_switch_notification_t redis_switch_notifications;

bool g_initialised = false;
volatile bool g_run = false;

std::shared_ptr<std::thread> notification_thread;

void ntf_thread()
{
    SWSS_LOG_ENTER();

    swss::Select s;

    s.addSelectable(g_redisNotifications);

    while (g_run)
    {
        swss::Selectable *sel;

        int fd;

        int result = s.select(&sel, &fd, 500);

        if (result == swss::Select::OBJECT)
        {
            swss::KeyOpFieldsValuesTuple kco;

            g_redisNotifications->pop(kco);

            const std::string &op = kfvOp(kco);
            const std::string &key = kfvKey(kco);
            const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

            SWSS_LOG_DEBUG("notification: op = %s, key = %s", op.c_str(), key.c_str());

            if (op != "ntf")
                continue;

            const std::string &ntf = key.substr(0, key.find_first_of(":"));
            const std::string &data = key.substr(key.find_last_of(":") + 1);

            handle_notification(ntf, data, values);
        }
    }
}

/**
* Routine Description:
*   SDK initialization. After the call the capability attributes should be
*   ready for retrieval via sai_get_switch_attribute().
*
* Arguments:
*   @param[in] profile_id - Handle for the switch profile.
*   @param[in] switch_hardware_id - Switch hardware ID to open
*   @param[in] firmware_path_name - Vendor specific path name of the firmware
*                                   to load
*   @param[in] switch_notifications - switch notification table
* Return Values:
*   @return SAI_STATUS_SUCCESS on success
*           Failure status code on error
*/
sai_status_t redis_initialize_switch(
    _In_ sai_switch_profile_id_t profile_id,
    _In_reads_z_(SAI_MAX_HARDWARE_ID_LEN) char* switch_hardware_id,
    _In_reads_opt_z_(SAI_MAX_FIRMWARE_PATH_NAME_LEN) char* firmware_path_name,
    _In_ sai_switch_notification_t* switch_notifications)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    if (g_initialised)
    {
        SWSS_LOG_ERROR("switch is already initialized");

        return SAI_STATUS_FAILURE;
    }

    g_initialised = true;

    if (switch_notifications != NULL)
    {
        memcpy(&redis_switch_notifications,
               switch_notifications,
               sizeof(sai_switch_notification_t));
    }
    else
    {
        memset(&redis_switch_notifications, 0, sizeof(sai_switch_notification_t));
    }

    g_run = true;

    SWSS_LOG_DEBUG("creating notification thread");

    notification_thread = std::make_shared<std::thread>(std::thread(ntf_thread));

    return SAI_STATUS_SUCCESS;
}

/**
 * Routine Description:
 *   @brief Release all resources associated with currently opened switch
 *
 * Arguments:
 *   @param[in] warm_restart_hint - hint that indicates controlled warm restart.
 *                            Since warm restart can be caused by crash
 *                            (therefore there are no guarantees for this call),
 *                            this hint is really a performance optimization.
 *
 * Return Values:
 *   None
 */
void  redis_shutdown_switch(
    _In_ bool warm_restart_hint)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    if (!g_initialised)
    {
        SWSS_LOG_ERROR("not initialized");

        return;
    }

    g_run = false;

    notification_thread->join();

    g_initialised = false;

    memset(&redis_switch_notifications, 0, sizeof(sai_switch_notification_t));
}

/**
 * Routine Description:
 *   SDK connect. This API connects library to the initialized SDK.
 *   After the call the capability attributes should be ready for retrieval
 *   via sai_get_switch_attribute().
 *
 * Arguments:
 *   @param[in] profile_id - Handle for the switch profile.
 *   @param[in] switch_hardware_id - Switch hardware ID to open
 *   @param[in] switch_notifications - switch notification table
 * Return Values:
 *   @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t redis_connect_switch(
    _In_ sai_switch_profile_id_t profile_id,
    _In_reads_z_(SAI_MAX_HARDWARE_ID_LEN) char* switch_hardware_id,
    _In_ sai_switch_notification_t* switch_notifications)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * Routine Description:
 *   @brief Disconnect this SAI library from the SDK.
 *
 * Arguments:
 *   None
 * Return Values:
 *   None
 */
void redis_disconnect_switch(void)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");
}

/**
 * Routine Description:
 *    @brief Set switch attribute value
 *
 * Arguments:
 *    @param[in] attr - switch attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  redis_set_switch_attribute(
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_SWITCH,
            (sai_object_id_t)0, // dummy sai_object_id_t for switch
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get switch attribute value
 *
 * Arguments:
 *    @param[in] attr_count - number of switch attributes
 *    @param[inout] attr_list - array of switch attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  redis_get_switch_attribute(
    _In_ sai_uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_SWITCH,
            (sai_object_id_t)0,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief Switch method table retrieved with sai_api_query()
 */
const sai_switch_api_t redis_switch_api = {
    redis_initialize_switch,
    redis_shutdown_switch,
    redis_connect_switch,
    redis_disconnect_switch,
    redis_set_switch_attribute,
    redis_get_switch_attribute,
};
