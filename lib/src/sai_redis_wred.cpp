#include "sai_redis.h"

/**
 * @brief Create WRED Profile
 *
 * @param[out] wred_id - Wred profile Id.
 * @param[in] attr_count - number of attributes
 * @param[in] attr_list - array of attributes
 *
 *
 * @return SAI_STATUS_SUCCESS on success
 *         Failure status code on error
 */

sai_status_t redis_create_wred_profile(
        _Out_ sai_object_id_t *wred_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_WRED,
            wred_id,
            attr_count,
            attr_list,
            &redis_generic_create);
}

/**
 * @brief Remove WRED Profile
 *
 * @param[in] wred_id Wred profile Id.
 *
 * @return SAI_STATUS_SUCCESS on success
 *         Failure status code on error
 */
sai_status_t redis_remove_wred_profile(
        _In_ sai_object_id_t wred_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_WRED,
            wred_id,
            &redis_generic_remove);
}

/**
 * @brief Set attributes to Wred profile.
 *
 * @param[out] wred_id Wred profile Id.
 * @param[in] attr attribute
 *
 *
 * @return SAI_STATUS_SUCCESS on success
 *         Failure status code on error
 */

sai_status_t redis_set_wred_attribute(
        _In_ sai_object_id_t wred_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_WRED,
            wred_id,
            attr,
            &redis_generic_set);
}

/**
 * @brief Get Wred profile attribute
 *
 * @param[in] wred_id Wred Profile Id
 * @param[in] attr_count number of attributes
 * @param[inout] attr_list array of attributes
 *
 * @return SAI_STATUS_SUCCESS on success
 *        Failure status code on error
 */
sai_status_t redis_get_wred_attribute(
        _In_ sai_object_id_t wred_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_WRED,
            wred_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * @brief WRED methods table retrieved with sai_api_query()
 */
const sai_wred_api_t redis_wred_api = {
    redis_create_wred_profile,
    redis_remove_wred_profile,
    redis_set_wred_attribute,
    redis_get_wred_attribute,
};
