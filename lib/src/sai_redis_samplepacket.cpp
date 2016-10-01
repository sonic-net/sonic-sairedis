#include "sai_redis.h"

/**
 * @brief Create samplepacket session.
 *
 * @param[out] session_id samplepacket session id
 * @param[in] attr_count Number of attributes
 * @param[in] attr_list Value of attributes
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *  error code is returned.
 */
sai_status_t redis_create_samplepacket_session(
        _Out_ sai_object_id_t *session_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_SAMPLEPACKET,
            session_id,
            attr_count,
            attr_list,
            &redis_generic_create);
}

/**
 * @brief Remove samplepacket session.
 *
 * @param[in] session_id samplepacket session id
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *  error code is returned.
 */
sai_status_t  redis_remove_samplepacket_session(
        _In_ sai_object_id_t session_id)
{
    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_SAMPLEPACKET,
            session_id,
            &redis_generic_remove);
}

/**
 * @brief Set samplepacket session attributes.
 *
 * @param[in] session_id samplepacket session id
 * @param[in] attr Value of attribute
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *  error code is returned.
 */
sai_status_t  redis_set_samplepacket_attribute(
        _In_ sai_object_id_t session_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_SAMPLEPACKET,
            session_id,
            attr,
            &redis_generic_set);
}

/**
 * @brief Get samplepacket session attributes.
 *
 * @param[in] session_id samplepacket session id
 * @param[in] attr_count Number of attributes
 * @param[inout] attr_list Value of attribute
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *  error code is returned.
 */
sai_status_t  redis_get_samplepacket_attribute(
        _In_ sai_object_id_t session_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_SAMPLEPACKET,
            session_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * @brief samplepacket method table retrieved with sai_api_query()
 */
const sai_samplepacket_api_t redis_samplepacket_api = {
    redis_create_samplepacket_session,
    redis_remove_samplepacket_session,
    redis_set_samplepacket_attribute,
    redis_get_samplepacket_attribute,
};
