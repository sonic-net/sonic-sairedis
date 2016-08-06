#include "sai_vs.h"

/**
 * Routine Description:
 *    @brief Create neighbor entry
 *
 * Arguments:
 *    @param[in] neighbor_entry - neighbor entry
 *    @param[in] attr_count - number of attributes
 *    @param[in] attrs - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 * Note: IP address expected in Network Byte Order.
 */
sai_status_t  vs_create_neighbor_entry(
    _In_ const sai_neighbor_entry_t* neighbor_entry,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create(
            SAI_OBJECT_TYPE_NEIGHBOR,
            neighbor_entry,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Remove neighbor entry
 *
 * Arguments:
 *    @param[in] neighbor_entry - neighbor entry
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 * Note: IP address expected in Network Byte Order.
 */
sai_status_t  vs_remove_neighbor_entry(
    _In_ const sai_neighbor_entry_t* neighbor_entry)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove(
            SAI_OBJECT_TYPE_NEIGHBOR,
            neighbor_entry);

    return status;
}

/**
 * Routine Description:
 *    @brief Set neighbor attribute value
 *
 * Arguments:
 *    @param[in] neighbor_entry - neighbor entry
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_set_neighbor_attribute(
    _In_ const sai_neighbor_entry_t* neighbor_entry,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_NEIGHBOR,
            neighbor_entry,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get neighbor attribute value
 *
 * Arguments:
 *    @param[in] neighbor_entry - neighbor entry
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attrs - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_get_neighbor_attribute(
    _In_ const sai_neighbor_entry_t* neighbor_entry,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_NEIGHBOR,
            neighbor_entry,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Remove all neighbor entries
 *
 * Arguments:
 *    None
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t vs_remove_all_neighbor_entries(void)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 *  @brief neighbor table methods, retrieved via sai_api_query()
 */
const sai_neighbor_api_t vs_neighbor_api = {
    vs_create_neighbor_entry,
    vs_remove_neighbor_entry,
    vs_set_neighbor_attribute,
    vs_get_neighbor_attribute,
    vs_remove_all_neighbor_entries,
};

