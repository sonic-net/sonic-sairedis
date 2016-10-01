#include "sai_vs.h"
#include "sai_vs_state.h"

sai_switch_notification_t vs_switch_notifications;

bool g_switch_initialized = false;

sai_status_t vs_initialize_switch(
        _In_ sai_switch_profile_id_t profile_id,
        _In_reads_z_(SAI_MAX_HARDWARE_ID_LEN) char* switch_hardware_id,
        _In_reads_opt_z_(SAI_MAX_FIRMWARE_PATH_NAME_LEN) char* firmware_path_name,
        _In_ sai_switch_notification_t* switch_notifications)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    if (g_switch_initialized)
    {
        SWSS_LOG_ERROR("already initialized");

        return SAI_STATUS_FAILURE;
    }

    if (switch_notifications != NULL)
    {
        memcpy(&vs_switch_notifications,
                switch_notifications,
                sizeof(sai_switch_notification_t));
    }
    else
    {
        memset(&vs_switch_notifications, 0, sizeof(sai_switch_notification_t));
    }

    sai_status_t status = init_switch();

    if (status == SAI_STATUS_SUCCESS)
    {
        g_switch_initialized = true;
    }

    return status;
}

void vs_shutdown_switch(
        _In_ bool warm_restart_hint)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    if (!g_switch_initialized)
    {
        SWSS_LOG_ERROR("not initialized");

        return;
    }

    g_switch_initialized = false;

    memset(&vs_switch_notifications, 0, sizeof(sai_switch_notification_t));
}

sai_status_t vs_connect_switch(
        _In_ sai_switch_profile_id_t profile_id,
        _In_reads_z_(SAI_MAX_HARDWARE_ID_LEN) char* switch_hardware_id,
        _In_ sai_switch_notification_t* switch_notifications)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

void vs_disconnect_switch(void)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");
}

sai_status_t vs_set_switch_attribute(
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_switch(
            attr,
            &vs_generic_set_switch);
}

sai_status_t vs_get_switch_attribute(
        _In_ sai_uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_switch(
            attr_count,
            attr_list,
            &vs_generic_get_switch);
}

const sai_switch_api_t vs_switch_api = {
    vs_initialize_switch,
    vs_shutdown_switch,
    vs_connect_switch,
    vs_disconnect_switch,
    vs_set_switch_attribute,
    vs_get_switch_attribute,
};
