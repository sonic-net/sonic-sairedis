#include "sai_redis.h"

/**
 * Routine Description:
 *    @brief Create virtual router
 *
 * Arguments:
 *    @param[out] vr_id - virtual router id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            SAI_STATUS_ADDR_NOT_FOUND if neither SAI_SWITCH_ATTR_SRC_MAC_ADDRESS nor
 *            SAI_VIRTUAL_ROUTER_ATTR_SRC_MAC_ADDRESS is set.
 */
sai_status_t redis_create_virtual_router(
        _Out_ sai_object_id_t *vr_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_VIRTUAL_ROUTER,
            vr_id,
            attr_count,
            attr_list,
            &redis_generic_create);
}

/**
 * Routine Description:
 *    @brief Remove virtual router
 *
 * Arguments:
 *    @param[in] vr_id - virtual router id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_virtual_router(
        _In_ sai_object_id_t vr_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_VIRTUAL_ROUTER,
            vr_id,
            &redis_generic_remove);
}

/**
 * Routine Description:
 *    @brief Set virtual router attribute Value
 *
 * Arguments:
 *    @param[in] vr_id - virtual router id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_virtual_router_attribute(
        _In_ sai_object_id_t vr_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_VIRTUAL_ROUTER,
            vr_id,
            attr,
            &redis_generic_set);
}

/**
 * Routine Description:
 *    @brief Get virtual router attribute Value
 *
 * Arguments:
 *    @param[in] vr_id - virtual router id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_virtual_router_attribute(
        _In_ sai_object_id_t vr_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_VIRTUAL_ROUTER,
            vr_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * @brief Virtual router methods table retrieved with sai_api_query()
 */
const sai_virtual_router_api_t redis_router_api = {
    redis_create_virtual_router,
    redis_remove_virtual_router,
    redis_set_virtual_router_attribute,
    redis_get_virtual_router_attribute,
};
