#include "sai_vs.h"

/**
 * Routine Description:
 *    @brief Create Route
 *
 * Arguments:
 *    @param[in] unicast_route_entry - route entry
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 * Note: IP prefix/mask expected in Network Byte Order.
 *
 */
sai_status_t  vs_create_route(
    _In_ const sai_unicast_route_entry_t* unicast_route_entry,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create(
            SAI_OBJECT_TYPE_ROUTE,
            unicast_route_entry,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Remove Route
 *
 * Arguments:
 *    @param[in] unicast_route_entry - route entry
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 * Note: IP prefix/mask expected in Network Byte Order.
 */
sai_status_t  vs_remove_route(
    _In_ const sai_unicast_route_entry_t* unicast_route_entry)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove(
            SAI_OBJECT_TYPE_ROUTE,
            unicast_route_entry);

    return status;
}

/**
 * Routine Description:
 *    @brief Set route attribute value
 *
 * Arguments:
 *    @param[in] unicast_route_entry - route entry
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_set_route_attribute(
    _In_ const sai_unicast_route_entry_t* unicast_route_entry,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_ROUTE,
            unicast_route_entry,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get route attribute value
 *
 * Arguments:
 *    @param[in] unicast_route_entry - route entry
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_get_route_attribute(
    _In_ const sai_unicast_route_entry_t* unicast_route_entry,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_ROUTE,
            unicast_route_entry,
            attr_count,
            attr_list);

    return status;
}


/**
 *  @brief Router entry methods table retrieved with sai_api_query()
 */
const sai_route_api_t vs_route_api = {
    vs_create_route,
    vs_remove_route,
    vs_set_route_attribute,
    vs_get_route_attribute,
};
