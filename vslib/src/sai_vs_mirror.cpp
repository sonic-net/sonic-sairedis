#include "sai_vs.h"

/**
 * @brief Create mirror session.
 *
 * @param[out] session_id Port mirror session id
 * @param[in] attr_count Number of attributes
 * @param[in] attr_list Value of attributes
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *  error code is returned.
 */
sai_status_t  vs_create_mirror_session(
        _Out_ sai_object_id_t *session_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create(
            SAI_OBJECT_TYPE_MIRROR,
            session_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief Remove mirror session.
 *
 * @param[in] session_id Port mirror session id
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *  error code is returned.
 */
sai_status_t  vs_remove_mirror_session(
        _In_ sai_object_id_t session_id)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove(
            SAI_OBJECT_TYPE_MIRROR,
            session_id);

    return status;
}

/**
 * @brief Set mirror session attributes.
 *
 * @param[in] session_id Port mirror session id
 * @param[in] attr Value of attribute
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *  error code is returned.
 */
sai_status_t  vs_set_mirror_session_attribute(
        _In_ sai_object_id_t session_id,
        _In_ const  sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_MIRROR,
            session_id,
            attr);

    return status;
}

/**
 * @brief Get mirror session attributes.
 *
 * @param[in] session_id Port mirror session id
 * @param[in] attr_count Number of attributes
 * @param[inout] attr_list Value of attribute
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *  error code is returned.
 */
sai_status_t  vs_get_mirror_session_attribute(
        _In_ sai_object_id_t session_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_MIRROR,
            session_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief MIRROR method table retrieved with sai_api_query()
 */
const sai_mirror_api_t vs_mirror_api = {
    vs_create_mirror_session,
    vs_remove_mirror_session,
    vs_set_mirror_session_attribute,
    vs_get_mirror_session_attribute,
};
