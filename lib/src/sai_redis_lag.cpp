#include "sai_redis.h"

/**
 * Routine Description:
 *    @brief Create LAG
 *
 * Arguments:
 *    @param[out] lag_id - LAG id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_create_lag(
        _Out_ sai_object_id_t* lag_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_LAG,
            lag_id,
            attr_count,
            attr_list,
            &redis_generic_create);
}

/**
 * Routine Description:
 *    @brief Remove LAG
 *
 * Arguments:
 *    @param[in] lag_id - LAG id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_lag(
        _In_ sai_object_id_t lag_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_LAG,
            lag_id,
            &redis_generic_remove);
}

/**
 * Routine Description:
 *    @brief Set LAG attribute
 *
 * Arguments:
 *    @param[in] lag_id - LAG id
 *    @param[in] attr - Structure containing ID and value to be set
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_lag_attribute(
        _In_ sai_object_id_t lag_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_LAG,
            lag_id,
            attr,
            &redis_generic_set);
}

/**
 * Routine Description:
 *    @brief Get LAG attribute
 *
 * Arguments:
 *    @param[in] lag_id - LAG id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_lag_attribute(
        _In_ sai_object_id_t lag_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_LAG,
            lag_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * Routine Description:
 *    @brief Create LAG member
 *
 * Arguments:
 *    @param[out] lag_member_id - LAG member id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_create_lag_member(
        _Out_ sai_object_id_t* lag_member_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_LAG_MEMBER,
            lag_member_id,
            attr_count,
            attr_list,
            &redis_generic_create);
}

/**
 * Routine Description:
 *    @brief Remove LAG member
 *
 * Arguments:
 *    @param[in] lag_member_id - lag member id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_lag_member(
        _In_ sai_object_id_t lag_member_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_LAG_MEMBER,
            lag_member_id,
            &redis_generic_remove);
}

/**
 * Routine Description:
 *    @brief Set LAG member attribute
 *
 * Arguments:
 *    @param[in] lag_member_id - LAG member id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_lag_member_attribute(
        _In_ sai_object_id_t lag_member_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_LAG_MEMBER,
            lag_member_id,
            attr,
            &redis_generic_set);
}

/**
 * Routine Description:
 *    @brief Get LAG attribute
 *
 * Arguments:
 *    @param[in] lag_member_id - LAG member id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_lag_member_attribute(
        _In_ sai_object_id_t lag_member_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_LAG_MEMBER,
            lag_member_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * @brief LAG methods table retrieved with sai_api_query()
 */
const sai_lag_api_t redis_lag_api = {
    redis_create_lag,
    redis_remove_lag,
    redis_set_lag_attribute,
    redis_get_lag_attribute,
    redis_create_lag_member,
    redis_remove_lag_member,
    redis_set_lag_member_attribute,
    redis_get_lag_member_attribute,
};
