#include "sai_vs.h"
#include <thread>

sai_switch_notification_t vs_switch_notifications;

bool g_switchInitialized = false;

sai_object_id_t switch_object_id = (sai_object_id_t)SAI_OBJECT_TYPE_SWITCH << 48;

#define CHECK_STATUS(status) \
    if (status != SAI_STATUS_SUCCESS) \
        return status;

sai_status_t vs_initialize_default_objects()
{
    SWSS_LOG_ENTER();

    sai_status_t status;

    SWSS_LOG_INFO("create internal switch object");

    {
        sai_attribute_t attr;

        status = vs_generic_create(
                SAI_OBJECT_TYPE_SWITCH,
                &switch_object_id,
                0,
                &attr);

        CHECK_STATUS(status);
    }

    SWSS_LOG_INFO("create switch src mac address");

    {
        sai_attribute_t attr;

        attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;
        attr.value.mac[0] = 0x11;
        attr.value.mac[1] = 0x22;
        attr.value.mac[2] = 0x33;
        attr.value.mac[3] = 0x44;
        attr.value.mac[4] = 0x55;
        attr.value.mac[5] = 0x66;

        status = vs_switch_api.set_switch_attribute(&attr);

        CHECK_STATUS(status);
    }

    SWSS_LOG_INFO("create default vlan");

    {
        status = vs_vlan_api.create_vlan(1);

        CHECK_STATUS(status);
    }

    SWSS_LOG_INFO("create cpu port");

    {
        sai_object_id_t cpu_port_id;

        sai_attribute_t attr;

        // currently we don't have attributes on cpu port
        status = vs_generic_create(SAI_OBJECT_TYPE_PORT, &cpu_port_id, 0, &attr);

        CHECK_STATUS(status);


        // populate cpu port object on switch

        attr.id = SAI_SWITCH_ATTR_CPU_PORT;
        attr.value.oid = cpu_port_id;

        status = vs_switch_api.set_switch_attribute(&attr);

        CHECK_STATUS(status);

        //attr.id = SAI_PORT_ATTR_TYPE;
        //attr.value.s32 = SAI_PORT_TYPE_CPU;

        //status = vs_port_api.set_port_attribute(cpu_port_id, &attr);

        //CHECK_STATUS(status);
    }


    // TODO currently port information is hardcoded
    // but later on we can read this from config file

    const uint32_t port_count = 32;

    std::vector<sai_object_id_t> port_list;

    SWSS_LOG_INFO("create ports");

    // create ports
    {
        for (uint32_t i = 0; i < port_count; i++)
        {
            SWSS_LOG_INFO("create port index %u", i);

            sai_object_id_t port_id;

            sai_attribute_t attr;

            status = vs_generic_create(SAI_OBJECT_TYPE_PORT, &port_id, 0, &attr);

            CHECK_STATUS(status);

            port_list.push_back(port_id);

            // populate port object on switch

            attr.id = SAI_PORT_ATTR_SPEED;
            attr.value.u32 = 10 * 1000;

            status = vs_port_api.set_port_attribute(port_id, &attr);

            CHECK_STATUS(status);

            // TODO populate ports attributes
            // TODO populate vlan members
            // TODO populate port lanes
        }
    }

    SWSS_LOG_INFO("create port list");

    {
        sai_attribute_t attr;

        attr.id = SAI_SWITCH_ATTR_PORT_LIST;
        attr.value.objlist.count = port_count;
        attr.value.objlist.list = port_list.data();

        status = vs_switch_api.set_switch_attribute(&attr);

        CHECK_STATUS(status);

        attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
        attr.value.u32 = port_count;

        status = vs_switch_api.set_switch_attribute(&attr);

        CHECK_STATUS(status);
    }

    SWSS_LOG_INFO("create default virtual router");

    {
        sai_object_id_t virtual_router_id;

        sai_attribute_t attr;

        status = vs_router_api.create_virtual_router(&virtual_router_id, 0, &attr);

        CHECK_STATUS(status);

        attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;
        attr.value.oid = virtual_router_id;

        status = vs_switch_api.set_switch_attribute(&attr);

        CHECK_STATUS(status);
    }

    return status;
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
sai_status_t vs_initialize_switch(
    _In_ sai_switch_profile_id_t profile_id,
    _In_reads_z_(SAI_MAX_HARDWARE_ID_LEN) char* switch_hardware_id,
    _In_reads_opt_z_(SAI_MAX_FIRMWARE_PATH_NAME_LEN) char* firmware_path_name,
    _In_ sai_switch_notification_t* switch_notifications)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    // TODO could be used to pass config?
    if (firmware_path_name == NULL)
    {
        SWSS_LOG_ERROR("firmware path name is NULL");

        return SAI_STATUS_FAILURE;
    }

    if (g_switchInitialized)
    {
        SWSS_LOG_ERROR("switch is already initialized");

        return SAI_STATUS_FAILURE;
    }

    g_switchInitialized = true;

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

    // reset real id
    g_real_id = 0;

    // remove all objects

    g_objectHash.clear();

    sai_status_t status = vs_initialize_default_objects();

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to initialize switch, status: %d", status);
    }

    return status;
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
void  vs_shutdown_switch(
    _In_ bool warm_restart_hint)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    if (!g_switchInitialized)
    {
        SWSS_LOG_ERROR("not initialized");

        return;
    }

    g_switchInitialized = false;

    memset(&vs_switch_notifications, 0, sizeof(sai_switch_notification_t));
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

/**
 * Routine Description:
 *   @brief Disconnect this SAI library from the SDK.
 *
 * Arguments:
 *   None
 * Return Values:
 *   None
 */
void vs_disconnect_switch(void)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

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
sai_status_t vs_set_switch_attribute(
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_SWITCH,
            switch_object_id, // dummy object id for switch, different than NULL
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
sai_status_t vs_get_switch_attribute(
    _In_ sai_uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_SWITCH,
            switch_object_id, // dummy object id for switch, different than NULL
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief Switch method table retrieved with sai_api_query()
 */
const sai_switch_api_t vs_switch_api = {
    vs_initialize_switch,
    vs_shutdown_switch,
    vs_connect_switch,
    vs_disconnect_switch,
    vs_set_switch_attribute,
    vs_get_switch_attribute,
};
