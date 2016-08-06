#include "sai_vs.h"

/**
 * @brief Create Qos Map
 *
 * @param[out] qos_map_id Qos Map Id
 * @param[in] attr_count number of attributes
 * @param[in] attr_list array of attributes
 *
 * @return  SAI_STATUS_SUCCESS on success
 *          Failure status code on error
 */
sai_status_t  vs_create_qos_map(
    _Out_ sai_object_id_t* qos_map_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create(
            SAI_OBJECT_TYPE_QOS_MAPS,
            qos_map_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief Remove Qos Map
 *
 *  @param[in] qos_map_id Qos Map id to be removed.
 *
 *  @return  SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t  vs_remove_qos_map (
    _In_ sai_object_id_t qos_map_id)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove(
            SAI_OBJECT_TYPE_QOS_MAPS,
            qos_map_id);

    return status;
}

/**
 * @brief Set attributes for qos map
 *
 * @param[in] qos_map_id Qos Map Id
 * @param[in] attr attribute to set
 *
 * @return  SAI_STATUS_SUCCESS on success
 *          Failure status code on error
 */
sai_status_t  vs_set_qos_map_attribute(
    _In_ sai_object_id_t qos_map_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_QOS_MAPS,
            qos_map_id,
            attr);

    return status;
}

/**
 * @brief  Get attrbutes of qos map
 *
 * @param[in] qos_map_id  map id
 * @param[in] attr_count  number of attributes
 * @param[inout] attr_list  array of attributes
 *
 * @return SAI_STATUS_SUCCESS on success
 *        Failure status code on error
 */
sai_status_t  vs_get_qos_map_attribute(
     _In_ sai_object_id_t qos_map_id,
     _In_ uint32_t attr_count,
     _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_QOS_MAPS,
            qos_map_id,
            attr_count,
            attr_list);

    return status;
}

/**
 *  Qos Map methods table retrieved with sai_api_query()
 */
const sai_qos_map_api_t vs_qos_map_api = {
    vs_create_qos_map,
    vs_remove_qos_map,
    vs_set_qos_map_attribute,
    vs_get_qos_map_attribute,
};
