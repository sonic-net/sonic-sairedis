#include "sai_vs.h"

/**
 * Routine Description:
 *    @brief Create a VLAN
 *
 * Arguments:
 *    @param[in] vlan_id - VLAN id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_create_vlan(
    _In_ sai_vlan_id_t vlan_id)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create_vlan(
            SAI_OBJECT_TYPE_VLAN,
            vlan_id);

    return status;
}

/**
 * Routine Description:
 *    @brief Remove a VLAN
 *
 * Arguments:
 *    @param[in] vlan_id - VLAN id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t vs_remove_vlan(
    _In_ sai_vlan_id_t vlan_id)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove_vlan(
            SAI_OBJECT_TYPE_VLAN,
            vlan_id);

    return status;
}

/**
 * Routine Description:
 *    @brief Set VLAN attribute Value
 *
 * Arguments:
 *    @param[in] vlan_id - VLAN id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_set_vlan_attribute(
    _In_ sai_vlan_id_t vlan_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set_vlan(
            SAI_OBJECT_TYPE_VLAN,
            vlan_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get VLAN attribute Value
 *
 * Arguments:
 *    @param[in] vlan_id - VLAN id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_get_vlan_attribute(
    _In_ sai_vlan_id_t vlan_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get_vlan(
            SAI_OBJECT_TYPE_VLAN,
            vlan_id,
            attr_count,
            attr_list);

    return status;
}

/*
    \brief Create VLAN Member
    \param[out] vlan_member_id VLAN Member id
    \param[in] attr_count number of attributes
    \param[in] attr_list array of attributes
    \return Success: SAI_STATUS_SUCCESS
            Failure: Failure status code on error
*/
sai_status_t vs_create_vlan_member(
    _Out_ sai_object_id_t* vlan_member_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            attr_count,
            attr_list);

    return status;
}

/*
    \brief Remove VLAN Member
    \param[in] vlan_member_id VLAN Member id
    \return Success: SAI_STATUS_SUCCESS
            Failure: Failure status code on error
*/
sai_status_t vs_remove_vlan_member(
    _In_ sai_object_id_t vlan_member_id)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id);

    return status;
}

/*
    \brief Set VLAN Member Attribute
    \param[in] vlan_member_id VLAN Member id
    \param[in] attr Structure containing ID and value to be set
    \return Success: SAI_STATUS_SUCCESS
            Failure: Failure status code on error
*/
sai_status_t  vs_set_vlan_member_attribute(
    _In_ sai_object_id_t  vlan_member_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            attr);

    return status;
}

/*
    \brief Get VLAN Member Attribute
    \param[in] vlan_member_id VLAN Member id
    \param[in] attr_count Number of attributes to be get
    \param[in,out] attr_list List of structures containing ID and value to be get
    \return Success: SAI_STATUS_SUCCESS
            Failure: Failure status code on error
*/

sai_status_t  vs_get_vlan_member_attribute(
    _In_ sai_object_id_t vlan_member_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            attr_count,
            attr_list);

    return status;
}


/**
 * Routine Description:
 *   @brief Get vlan statistics counters.
 *
 * Arguments:
 *    @param[in] vlan_id - VLAN id
 *    @param[in] counter_ids - specifies the array of counter ids
 *    @param[in] number_of_counters - number of counters in the array
 *    @param[out] counters - array of resulting counter values.
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_get_vlan_stats(
    _In_ sai_vlan_id_t vlan_id,
    _In_ const sai_vlan_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters,
    _Out_ uint64_t* counters)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * Routine Description:
 *   @brief Clear vlan statistics counters.
 *
 * Arguments:
 *    @param[in] vlan_id - vlan id
 *    @param[in] counter_ids - specifies the array of counter ids
 *    @param[in] number_of_counters - number of counters in the array
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t  vs_clear_vlan_stats(
    _In_ sai_vlan_id_t vlan_id,
    _In_ const sai_vlan_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief VLAN methods table retrieved with sai_api_query()
 */
const sai_vlan_api_t vs_vlan_api = {
    vs_create_vlan,
    vs_remove_vlan,
    vs_set_vlan_attribute,
    vs_get_vlan_attribute,
    vs_create_vlan_member,
    vs_remove_vlan_member,
    vs_set_vlan_member_attribute,
    vs_get_vlan_member_attribute,
    vs_get_vlan_stats,
    vs_clear_vlan_stats,
};
