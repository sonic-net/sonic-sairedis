#include "sai_vs.h"

/**
 * Routine Description:
 *    @brief Create tunnel map
 *
 * Arguments:
 *    @param[out] tunnel_map_id - tunnel map id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t  vs_create_tunnel_map(
    _Out_ sai_object_id_t* tunnel_map_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create(
            SAI_OBJECT_TYPE_TUNNEL_MAP,
            tunnel_map_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Remove tunnel map
 *
 * Arguments:
 *    @param[out] tunnel_map_id - tunnel map id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t  vs_remove_tunnel_map(
    _In_ sai_object_id_t tunnel_map_id)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove(
            SAI_OBJECT_TYPE_TUNNEL_MAP,
            tunnel_map_id);

    return status;
}

/**
 * Routine Description:
 *    @brief Set tunnel map attribute Value
 *
 * Arguments:
 *    @param[in] tunnel_map_id - tunnel map id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_set_tunnel_map_attribute(
    _In_ sai_object_id_t  tunnel_map_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_TUNNEL_MAP,
            tunnel_map_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get tunnel map attribute Value
 *
 * Arguments:
 *    @param[in] tunnel_map_id - tunnel map id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_get_tunnel_map_attribute(
    _In_ sai_object_id_t   tunnel_map_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_TUNNEL_MAP,
            tunnel_map_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Create tunnel
 *
 * Arguments:
 *    @param[out] tunnel_id - tunnel id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t  vs_create_tunnel(
    _Out_ sai_object_id_t* tunnel_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create(
            SAI_OBJECT_TYPE_TUNNEL,
            tunnel_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Remove tunnel
 *
 * Arguments:
 *    @param[out] tunnel_id - tunnel map
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t  vs_remove_tunnel(
    _In_ sai_object_id_t tunnel_id)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove(
            SAI_OBJECT_TYPE_TUNNEL,
            tunnel_id);

    return status;
}

/**
 * Routine Description:
 *    @brief Set tunnel attribute Value
 *
 * Arguments:
 *    @param[in] tunnel_id - tunnel id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_set_tunnel_attribute(
    _In_ sai_object_id_t tunnel_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_TUNNEL,
            tunnel_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get tunnel attribute Value
 *
 * Arguments:
 *    @param[in] tunnel_id - tunnel id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_get_tunnel_attribute(
    _In_ sai_object_id_t tunnel_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_TUNNEL,
            tunnel_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Create tunnel term table
 *
 * Arguments:
 *    @param[out] tunnel_term_table_entry_id - tunnel term table entry id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t  vs_create_tunnel_term_table_entry (
    _Out_ sai_object_id_t* tunnel_term_table_entry_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create(
            SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
            tunnel_term_table_entry_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Remove tunnel term table
 *
 * Arguments:
 *    @param[out] tunnel_term_table_entry_id - tunnel term table entry id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t  vs_remove_tunnel_term_table_entry (
    _In_ sai_object_id_t tunnel_term_table_entry_id)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove(
            SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
            tunnel_term_table_entry_id);

    return status;
}

/**
 * Routine Description:
 *    @brief Set tunnel term table attribute Value
 *
 * Arguments:
 *    @param[in] tunnel_term_table_entry_id, - tunnel term table id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_set_tunnel_term_table_entry_attribute(
    _In_ sai_object_id_t tunnel_term_table_entry_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
            tunnel_term_table_entry_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get tunnel term table attribute Value
 *
 * Arguments:
 *    @param[in] tunnel_term_table_entry_id, - tunnel term table id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_get_tunnel_term_table_entry_attribute(
    _In_ sai_object_id_t tunnel_term_table_entry_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
            tunnel_term_table_entry_id,
            attr_count,
            attr_list);

    return status;
}

/**
 *  @brief tunnel table methods, retrieved via sai_api_query()
 */
const sai_tunnel_api_t vs_tunnel_api = {
    vs_create_tunnel_map,
    vs_remove_tunnel_map,
    vs_set_tunnel_map_attribute,
    vs_get_tunnel_map_attribute,
    vs_create_tunnel,
    vs_remove_tunnel,
    vs_set_tunnel_attribute,
    vs_get_tunnel_attribute,
    vs_create_tunnel_term_table_entry,
    vs_remove_tunnel_term_table_entry,
    vs_set_tunnel_term_table_entry_attribute,
    vs_get_tunnel_term_table_entry_attribute,
};
