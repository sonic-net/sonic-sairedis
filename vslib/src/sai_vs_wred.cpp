#include "sai_vs.h"

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

sai_status_t  vs_create_wred_profile(
    _Out_ sai_object_id_t *wred_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create(
            SAI_OBJECT_TYPE_WRED,
            wred_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief Remove WRED Profile
 *
 * @param[in] wred_id Wred profile Id.
 *
 * @return SAI_STATUS_SUCCESS on success
 *         Failure status code on error
 */
sai_status_t  vs_remove_wred_profile(
    _In_ sai_object_id_t wred_id)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove(
            SAI_OBJECT_TYPE_WRED,
            wred_id);

    return status;
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

sai_status_t  vs_set_wred_attribute(
    _In_ sai_object_id_t wred_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_WRED,
            wred_id,
            attr);

    return status;
}

/**
 * @brief  Get Wred profile attribute
 *
 * @param[in] wred_id Wred Profile Id
 * @param[in] attr_count number of attributes
 * @param[inout] attr_list  array of attributes
 *
 * @return SAI_STATUS_SUCCESS on success
 *        Failure status code on error
 */
sai_status_t  vs_get_wred_attribute(
    _In_ sai_object_id_t wred_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_WRED,
            wred_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief WRED methods table retrieved with sai_api_query()
 */
const sai_wred_api_t vs_wred_api = {
    vs_create_wred_profile,
    vs_remove_wred_profile,
    vs_set_wred_attribute,
    vs_get_wred_attribute,
};
