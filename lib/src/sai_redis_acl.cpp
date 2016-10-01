#include "sai_redis.h"

/**
 * Routine Description:
 *   @brief Create an ACL table
 *
 * Arguments:
 *   @param[out] acl_table_id - the the acl table id
 *   @param[in] attr_count - number of attributes
 *   @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_create_acl_table(
        _Out_ sai_object_id_t* acl_table_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_ACL_TABLE,
            acl_table_id,
            attr_count,
            attr_list,
            &redis_generic_create);
}

/**
 * Routine Description:
 *    @brief Delete an ACL table
 *
 * Arguments:
 *    @param[in] acl_table_id - the acl table id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_remove_acl_table(
        _In_ sai_object_id_t acl_table_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_ACL_TABLE,
            acl_table_id,
            &redis_generic_remove);
}

/**
 * Routine Description:
 *   @brief Set ACL table attribute
 *
 * Arguments:
 *    @param[in] acl_table_id - the acl table id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_set_acl_table_attribute(
        _In_ sai_object_id_t acl_table_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_ACL_TABLE,
            acl_table_id,
            attr,
            &redis_generic_set);
}

/**
 * Routine Description:
 *   @brief Get ACL table attribute
 *
 * Arguments:
 *    @param[in] acl_table_id - acl table id
 *    @param[in] attr_count - number of attributes
 *    @param[out] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_get_acl_table_attribute(
        _In_ sai_object_id_t acl_table_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_ACL_TABLE,
            acl_table_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * Routine Description:
 *   @brief Create an ACL entry
 *
 * Arguments:
 *   @param[out] acl_entry_id - the acl entry id
 *   @param[in] attr_count - number of attributes
 *   @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_create_acl_entry(
        _Out_ sai_object_id_t *acl_entry_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_ACL_ENTRY,
            acl_entry_id,
            attr_count,
            attr_list,
            &redis_generic_create);
}

/**
 * Routine Description:
 *   @brief Delete an ACL entry
 *
 * Arguments:
 *   @param[in] acl_entry_id - the acl entry id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_remove_acl_entry(
        _In_ sai_object_id_t acl_entry_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_ACL_ENTRY,
            acl_entry_id,
            &redis_generic_remove);
}

/**
 * Routine Description:
 *   @brief Set ACL entry attribute
 *
 * Arguments:
 *    @param[in] acl_entry_id - the acl entry id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_set_acl_entry_attribute(
        _In_ sai_object_id_t acl_entry_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_ACL_ENTRY,
            acl_entry_id,
            attr,
            &redis_generic_set);
}

/**
 * Routine Description:
 *   @brief Get ACL entry attribute
 *
 * Arguments:
 *    @param[in] acl_entry_id - acl entry id
 *    @param[in] attr_count - number of attributes
 *    @param[out] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_get_acl_entry_attribute(
        _In_ sai_object_id_t acl_entry_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_ACL_ENTRY,
            acl_entry_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * Routine Description:
 *   @brief Create an ACL counter
 *
 * Arguments:
 *   @param[out] acl_counter_id - the acl counter id
 *   @param[in] attr_count - number of attributes
 *   @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_create_acl_counter(
        _Out_ sai_object_id_t *acl_counter_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_ACL_COUNTER,
            acl_counter_id,
            attr_count,
            attr_list,
            &redis_generic_create);
}

/**
 * Routine Description:
 *   @brief Delete an ACL counter
 *
 * Arguments:
 *   @param[in] acl_counter_id - the acl counter id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_remove_acl_counter(
        _In_ sai_object_id_t acl_counter_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_ACL_COUNTER,
            acl_counter_id,
            &redis_generic_remove);
}

/**
 * Routine Description:
 *   @brief Set ACL counter attribute
 *
 * Arguments:
 *    @param[in] acl_counter_id - the acl counter id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_acl_counter_attribute(
        _In_ sai_object_id_t acl_counter_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_ACL_COUNTER,
            acl_counter_id,
            attr,
            &redis_generic_set);
}

/**
 * Routine Description:
 *   @brief Get ACL counter attribute
 *
 * Arguments:
 *    @param[in] acl_counter_id - acl counter id
 *    @param[in] attr_count - number of attributes
 *    @param[out] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_acl_counter_attribute(
        _In_ sai_object_id_t acl_counter_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_ACL_COUNTER,
            acl_counter_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * Routine Description:
 *   @brief Create an ACL Range
 *
 * Arguments:
 *   @param[out] acl_range_id - the acl range id
 *   @param[in] attr_count - number of attributes
 *   @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_create_acl_range(
        _Out_ sai_object_id_t* acl_range_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_ACL_RANGE,
            acl_range_id,
            attr_count,
            attr_list,
            &redis_generic_create);
}

/**
 * Routine Description:
 *   @brief Remove an ACL Range
 *
 * Arguments:
 *   @param[in] acl_range_id - the acl range id
 *
 * Return Values:
 *   @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_remove_acl_range(
        _In_ sai_object_id_t acl_range_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_ACL_RANGE,
            acl_range_id,
            &redis_generic_remove);
}

/**
 * Routine Description:
 *   @brief Set ACL range attribute
 *
 * Arguments:
 *    @param[in] acl_range_id - the acl range id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_set_acl_range_attribute(
        _In_ sai_object_id_t acl_range_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_ACL_RANGE,
            acl_range_id,
            attr,
            &redis_generic_set);
}

/**
 * Routine Description:
 *   @brief Get ACL range attribute
 *
 * Arguments:
 *    @param[in] acl_range_id - acl range id
 *    @param[in] attr_count - number of attributes
 *    @param[out] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_get_acl_range_attribute(
        _In_ sai_object_id_t acl_range_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_ACL_RANGE,
            acl_range_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * @brief acl methods table retrieved with sai_api_query()
 */
const sai_acl_api_t redis_acl_api = {
    redis_create_acl_table,
    redis_remove_acl_table,
    redis_set_acl_table_attribute,
    redis_get_acl_table_attribute,

    redis_create_acl_entry,
    redis_remove_acl_entry,
    redis_set_acl_entry_attribute,
    redis_get_acl_entry_attribute,

    redis_create_acl_counter,
    redis_remove_acl_counter,
    redis_set_acl_counter_attribute,
    redis_get_acl_counter_attribute,

    redis_create_acl_range,
    redis_remove_acl_range,
    redis_set_acl_range_attribute,
    redis_get_acl_range_attribute,
};
