#include "sai_vs.h"

/**
 * Routine Description:
 *    @brief Create FDB entry
 *
 * Arguments:
 *    @param[in] fdb_entry - fdb entry
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_create_fdb_entry(
    _In_ const sai_fdb_entry_t *fdb_entry,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create(
            SAI_OBJECT_TYPE_FDB,
            fdb_entry,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Remove FDB entry
 *
 * Arguments:
 *    @param[in] fdb_entry - fdb entry
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_remove_fdb_entry(
    _In_ const sai_fdb_entry_t* fdb_entry)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove(
            SAI_OBJECT_TYPE_FDB,
            fdb_entry);

    return status;
}

/**
 * Routine Description:
 *    @brief Set fdb entry attribute value
 *
 * Arguments:
 *    @param[in] fdb_entry - fdb entry
 *    @param[in] attr - attribute
 * * Return Values: *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_set_fdb_entry_attribute(
    _In_ const sai_fdb_entry_t* fdb_entry,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_FDB,
            fdb_entry,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get fdb entry attribute value
 *
 * Arguments:
 *    @param[in] fdb_entry - fdb entry
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_get_fdb_entry_attribute(
    _In_ const sai_fdb_entry_t* fdb_entry,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_FDB,
            fdb_entry,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Remove all FDB entries by attribute set in sai_fdb_flush_attr
 *
 * Arguments:
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_flush_fdb_entries(
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * Routine Description:
 *     @brief FDB notifications
 *
 * Arguments:
 *    @param[in] count - number of notifications
 *    @param[in] data  - pointer to fdb event notification data array
 *
 * Return Values:
 *    None
 */
void  vs_fdb_event_notification(
    _In_ uint32_t count,
    _In_ sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();
}

/**
 * @brief FDB method table retrieved with sai_api_query()
 */
const sai_fdb_api_t vs_fdb_api = {
    vs_create_fdb_entry,
    vs_remove_fdb_entry,
    vs_set_fdb_entry_attribute,
    vs_get_fdb_entry_attribute,
    vs_flush_fdb_entries,
};
