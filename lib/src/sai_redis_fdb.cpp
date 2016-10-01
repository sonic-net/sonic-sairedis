#include "sai_redis.h"

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
sai_status_t redis_create_fdb_entry(
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_fdb_entry(
            fdb_entry,
            attr_count,
            attr_list,
            &redis_generic_create_fdb_entry);
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
sai_status_t redis_remove_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_fdb_entry(
            fdb_entry,
            &redis_generic_remove_fdb_entry);
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
sai_status_t redis_set_fdb_entry_attribute(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_fdb_entry(
            fdb_entry,
            attr,
            &redis_generic_set_fdb_entry);
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
sai_status_t redis_get_fdb_entry_attribute(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_fdb_entry(
            fdb_entry,
            attr_count,
            attr_list,
            &redis_generic_get_fdb_entry);
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
sai_status_t redis_flush_fdb_entries(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief FDB method table retrieved with sai_api_query()
 */
const sai_fdb_api_t redis_fdb_api = {
    redis_create_fdb_entry,
    redis_remove_fdb_entry,
    redis_set_fdb_entry_attribute,
    redis_get_fdb_entry_attribute,
    redis_flush_fdb_entries,
};
