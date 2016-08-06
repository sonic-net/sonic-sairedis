#include "sai_vs.h"

/**
 * Routine Description:
 *    @brief Create next hop group
 *
 * Arguments:
 *    @param[out] next_hop_group_id - next hop group id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_create_next_hop_group(
    _Out_ sai_object_id_t* next_hop_group_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
            next_hop_group_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Remove next hop group
 *
 * Arguments:
 *    @param[in] next_hop_group_id - next hop group id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_remove_next_hop_group(
    _In_ sai_object_id_t next_hop_group_id)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
            next_hop_group_id);

    return status;
}

/**
 * Routine Description:
 *    @brief Set Next Hop Group attribute
 *
 * Arguments:
 *    @param[in] sai_object_id_t - next_hop_group_id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_set_next_hop_group_attribute(
    _In_ sai_object_id_t next_hop_group_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
            next_hop_group_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get Next Hop Group attribute
 *
 * Arguments:
 *    @param[in] sai_object_id_t - next_hop_group_id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_get_next_hop_group_attribute(
    _In_ sai_object_id_t next_hop_group_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
            next_hop_group_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Add next hop to a group
 *
 * Arguments:
 *    @param[in] next_hop_group_id - next hop group id
 *    @param[in] next_hop_count - number of next hops
 *    @param[in] nexthops - array of next hops
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_add_next_hop_to_group(
    _In_ sai_object_id_t next_hop_group_id,
    _In_ uint32_t next_hop_count,
    _In_ const sai_object_id_t* nexthops)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * Routine Description:
 *    @brief Remove next hop from a group
 *
 * Arguments:
 *    @param[in] next_hop_group_id - next hop group id
 *    @param[in] next_hop_count - number of next hops
 *    @param[in] nexthops - array of next hops
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_remove_next_hop_from_group(
    _In_ sai_object_id_t next_hop_group_id,
    _In_ uint32_t next_hop_count,
    _In_ const sai_object_id_t* nexthops)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 *  @brief Next Hop methods table retrieved with sai_api_query()
 */
const sai_next_hop_group_api_t vs_next_hop_group_api = {
    vs_create_next_hop_group,
    vs_remove_next_hop_group,
    vs_set_next_hop_group_attribute,
    vs_get_next_hop_group_attribute,
    vs_add_next_hop_to_group,
    vs_remove_next_hop_from_group,
};
