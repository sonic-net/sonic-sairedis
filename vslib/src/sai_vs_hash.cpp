#include "sai_vs.h"

/**
 * Routine Description:
 *    @brief Create hash
 *
 * Arguments:
 *    @param[out] hash_id - hash id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t vs_create_hash(
    _Out_ sai_object_id_t* hash_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create(
            SAI_OBJECT_TYPE_HASH,
            hash_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Remove hash
 *
 * Arguments:
 *    @param[in] hash_id - hash id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t vs_remove_hash(
    _In_ sai_object_id_t hash_id)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove(
            SAI_OBJECT_TYPE_HASH,
            hash_id);

    return status;
}

/**
 * Routine Description:
 *    @brief Set hash attribute
 *
 * Arguments:
 *    @param[in] hash_id - hash id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_set_hash_attribute(
    _In_ sai_object_id_t hash_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_HASH,
            hash_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get hash attribute value
 *
 * Arguments:
 *    @param[in] hash_id - hash id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attrs - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_get_hash_attribute(
    _In_ sai_object_id_t hash_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_HASH,
            hash_id,
            attr_count,
            attr_list);

    return status;
}

/**
 *  @brief hash methods, retrieved via sai_api_query()
 */
const sai_hash_api_t vs_hash_api = {
    vs_create_hash,
    vs_remove_hash,
    vs_set_hash_attribute,
    vs_get_hash_attribute,
};

