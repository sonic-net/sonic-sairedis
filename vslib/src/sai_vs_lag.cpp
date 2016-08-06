#include "sai_vs.h"


/*
    \brief Create LAG
    \param[out] lag_id LAG id
    \param[in] attr_count number of attributes
    \param[in] attr_list array of attributes
    \return Success: SAI_STATUS_SUCCESS
            Failure: Failure status code on error
*/
sai_status_t vs_create_lag(
    _Out_ sai_object_id_t* lag_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create(
            SAI_OBJECT_TYPE_LAG,
            lag_id,
            attr_count,
            attr_list);

    return status;
}

/*
    \brief Remove LAG
    \param[in] lag_id LAG id
    \return Success: SAI_STATUS_SUCCESS
            Failure: Failure status code on error
*/
sai_status_t vs_remove_lag(
    _In_ sai_object_id_t lag_id)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove(
            SAI_OBJECT_TYPE_LAG,
            lag_id);

    return status;
}

/*
    \brief Set LAG Attribute
    \param[in] lag_id LAG id
    \param[in] attr Structure containing ID and value to be set
    \return Success: SAI_STATUS_SUCCESS
            Failure: Failure status code on error
*/
sai_status_t  vs_set_lag_attribute(
    _In_ sai_object_id_t  lag_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_LAG,
            lag_id,
            attr);

    return status;
}

/*
    \brief Get LAG Attribute
    \param[in] lag_id LAG id
    \param[in] attr_count Number of attributes to be get
    \param[in,out] attr_list List of structures containing ID and value to be get
    \return Success: SAI_STATUS_SUCCESS
            Failure: Failure status code on error
*/

sai_status_t  vs_get_lag_attribute(
    _In_ sai_object_id_t lag_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_LAG,
            lag_id,
            attr_count,
            attr_list);

    return status;
}

/*
    \brief Create LAG Member
    \param[out] lag_member_id LAG Member id
    \param[in] attr_count number of attributes
    \param[in] attr_list array of attributes
    \return Success: SAI_STATUS_SUCCESS
            Failure: Failure status code on error
*/
sai_status_t vs_create_lag_member(
    _Out_ sai_object_id_t* lag_member_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create(
            SAI_OBJECT_TYPE_LAG_MEMBER,
            lag_member_id,
            attr_count,
            attr_list);

    return status;
}

/*
    \brief Remove LAG Member
    \param[in] lag_member_id LAG Member id
    \return Success: SAI_STATUS_SUCCESS
            Failure: Failure status code on error
*/
sai_status_t vs_remove_lag_member(
    _In_ sai_object_id_t lag_member_id)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove(
            SAI_OBJECT_TYPE_LAG_MEMBER,
            lag_member_id);

    return status;
}

/*
    \brief Set LAG Member Attribute
    \param[in] lag_member_id LAG Member id
    \param[in] attr Structure containing ID and value to be set
    \return Success: SAI_STATUS_SUCCESS
            Failure: Failure status code on error
*/
sai_status_t  vs_set_lag_member_attribute(
    _In_ sai_object_id_t  lag_member_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_LAG_MEMBER,
            lag_member_id,
            attr);

    return status;
}

/*
    \brief Get LAG Member Attribute
    \param[in] lag_member_id LAG Member id
    \param[in] attr_count Number of attributes to be get
    \param[in,out] attr_list List of structures containing ID and value to be get
    \return Success: SAI_STATUS_SUCCESS
            Failure: Failure status code on error
*/

sai_status_t  vs_get_lag_member_attribute(
    _In_ sai_object_id_t lag_member_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_LAG_MEMBER,
            lag_member_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief LAG methods table retrieved with sai_api_query()
 */
const sai_lag_api_t vs_lag_api = {
   vs_create_lag,
   vs_remove_lag,
   vs_set_lag_attribute,
   vs_get_lag_attribute,
   vs_create_lag_member,
   vs_remove_lag_member,
   vs_set_lag_member_attribute,
   vs_get_lag_member_attribute,
};
