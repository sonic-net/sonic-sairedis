#include "sai_vs.h"

/**
 * Routine Description:
 *   @brief Set port attribute value.
 *
 * Arguments:
 *    @param[in] port_id - port id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_set_port_attribute(
    _In_ sai_object_id_t port_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_PORT,
            port_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *   @brief Get port attribute value.
 *
 * Arguments:
 *    @param[in] port_id - port id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_get_port_attribute(
    _In_ sai_object_id_t port_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_PORT,
            port_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *   @brief Get port statistics counters.
 *
 * Arguments:
 *    @param[in] port_id - port id
 *    @param[in] counter_ids - specifies the array of counter ids
 *    @param[in] number_of_counters - number of counters in the array
 *    @param[out] counters - array of resulting counter values.
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_get_port_stats(
    _In_ sai_object_id_t port_id,
    _In_ const sai_port_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters,
    _Out_ uint64_t* counters)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * Routine Description:
 *   @brief Clear port statistics counters.
 *
 * Arguments:
 *    @param[in] port_id - port id
 *    @param[in] counter_ids - specifies the array of counter ids
 *    @param[in] number_of_counters - number of counters in the array
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_clear_port_stats(
    _In_ sai_object_id_t port_id,
    _In_ const sai_port_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * Routine Description:
 *   @brief Clear port's all statistics counters.
 *
 * Arguments:
 *    @param[in] port_id - port id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_clear_port_all_stats(
    _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * Routine Description:
 *   Port state change notification
 *   Passed as a parameter into sai_initialize_switch()
 *
 * Arguments:
 *   @param[in] count - number of notifications
 *   @param[in] data  - array of port operational status
 *
 * Return Values:
 *    None
 */
void  vs_port_state_change_notification(
    _In_ uint32_t count,
    _In_ sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();
}

/**
 * Routine Description:
 *   @brief Port event notification
 *
 * Arguments:
 *    @param[in] count - number of notifications
 *    @param[in] data  - array of port events

 * Return Values:
 *    None
 */
void  vs_port_event_notification(
    _In_ uint32_t count,
    _In_ sai_port_event_notification_t *data)
{
    SWSS_LOG_ENTER();
}

/**
 * @brief Port methods table retrieved with sai_api_query()
 */
const sai_port_api_t vs_port_api = {
    vs_set_port_attribute,
    vs_get_port_attribute,
    vs_get_port_stats,
    vs_clear_port_stats,
    vs_clear_port_all_stats,
};
