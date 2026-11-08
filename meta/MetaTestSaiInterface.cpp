#include "MetaTestSaiInterface.h"
#include "NumberOidIndexGenerator.h"
#include "SwitchConfigContainer.h"

#include "swss/logger.h"

#include "Globals.h"
#include "sai_serialize.h"

using namespace saimeta;

MetaTestSaiInterface::MetaTestSaiInterface()
{
    SWSS_LOG_ENTER();

    auto sc = std::make_shared<sairedis::SwitchConfig>();

    sc->m_switchIndex = 0;
    sc->m_hardwareInfo = "";

    auto scc = std::make_shared<sairedis::SwitchConfigContainer>();

    scc->insert(sc);

    m_virtualObjectIdManager =
        std::make_shared<sairedis::VirtualObjectIdManager>(0, scc,
                std::make_shared<NumberOidIndexGenerator>());
}

sai_status_t MetaTestSaiInterface::create(
        _In_ sai_object_type_t objectType,
        _Out_ sai_object_id_t* objectId,
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        // for given hardware info we always return same switch id,
        // this is required since we could be performing warm boot here

        auto hwinfo = Globals::getHardwareInfo(attr_count, attr_list);

        switchId = m_virtualObjectIdManager->allocateNewSwitchObjectId(hwinfo);

        *objectId = switchId;

        if (switchId == SAI_NULL_OBJECT_ID)
        {
            SWSS_LOG_ERROR("switch ID allocation failed");

            return SAI_STATUS_FAILURE;
        }
    }
    else
    {
        *objectId = m_virtualObjectIdManager->allocateNewObjectId(objectType, switchId);
    }

    if (*objectId == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("failed to allocated new object id: %s:%s",
                sai_serialize_object_type(objectType).c_str(),
                sai_serialize_object_id(switchId).c_str());

        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t MetaTestSaiInterface::bulkCreate(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t object_count,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_object_id_t *object_id,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < object_count; ++idx)
    {
        object_statuses[idx] = create(object_type, &object_id[idx], switch_id, attr_count[idx], attr_list[idx]);
    }

    return SAI_STATUS_SUCCESS;
}

sai_object_type_t MetaTestSaiInterface::objectTypeQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    return m_virtualObjectIdManager->saiObjectTypeQuery(objectId);
}

sai_object_id_t MetaTestSaiInterface::switchIdQuery(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    return m_virtualObjectIdManager->saiSwitchIdQuery(objectId);
}
