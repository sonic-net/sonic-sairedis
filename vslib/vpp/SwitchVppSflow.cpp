#include "SwitchVppSflow.h"
#include "SwitchVpp.h"
#include "SwitchVppUtils.h"


#include "meta/sai_serialize.h"
#include "swss/logger.h"
#include "vppxlate/SaiVppXlate.h"

using namespace saivs;

sai_status_t SwitchVpp::samplePacketCreate(
    _In_ sai_object_id_t object_id,
    _In_ sai_object_id_t switch_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_SAMPLEPACKET, sid, switch_id, attr_count, attr_list));

    SWSS_LOG_NOTICE("Sample packet %s created", sid.c_str());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::samplePacketRemove(
    _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_SAMPLEPACKET, serializedObjectId));

    SWSS_LOG_NOTICE("Sample packet %s removed", serializedObjectId.c_str());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::samplePacketSet(
    _In_ sai_object_id_t entry_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(entry_id);

    sai_status_t status = set_internal(SAI_OBJECT_TYPE_SAMPLEPACKET, sid, attr);

    SWSS_LOG_NOTICE("Set sample packet %s status %d", sid.c_str(), status);

    return status;
}

sai_status_t SwitchVpp::sflow_enable_disable(
    _In_ sai_object_id_t port_id,
    _In_ bool enable)
{
    SWSS_LOG_ENTER();

    std::string if_name;

    if(!port_to_hwifname(port_id, if_name))
    {
        SWSS_LOG_ERROR("failed to get hwif name for port %s", sai_serialize_object_id(port_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    int ret = vpp_sflow_enable_disable(if_name.c_str(), enable);
    if(ret != 0){
        SWSS_LOG_ERROR("sflow enable_disable failed for port %s, status %d",
                   sai_serialize_object_id(port_id).c_str(), ret);
        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_NOTICE("Changed port %s to %d",sai_serialize_object_id(port_id).c_str(), enable);

    return SAI_STATUS_SUCCESS;

}

sai_status_t SwitchVpp::sflow_sampling_rate_set(
    _In_ uint32_t rate)
{
    SWSS_LOG_ENTER();

    int ret = vpp_sflow_sampling_rate_set(rate);
    if (ret != 0 ){
        SWSS_LOG_ERROR("sflow sampling_rate_set failed, status %d", ret);
        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_NOTICE("New rate set of 1 in %d packets", rate);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::sflow_hostif_trap_samplepacket_create(
    _In_ sai_object_id_t object_id,
    _In_ sai_object_id_t switch_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    const sai_attribute_value_t *trap_type_attr = nullptr;
    uint32_t attr_index = 0;

    sai_status_t status = find_attrib_in_list(
            attr_count, attr_list,
            SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE,
            &trap_type_attr, &attr_index);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("HOSTIF_TRAP %s missing mandatory TRAP_TYPE attribute", sid.c_str());
        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    if (trap_type_attr->s32 == SAI_HOSTIF_TRAP_TYPE_SAMPLEPACKET)
    {
        SWSS_LOG_NOTICE("HOSTIF_TRAP %s SAMPLEPACKET created (bookkeeping)", sid.c_str());
    }

    // Bookkeeping, store the trap object in the generic object hash
    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_HOSTIF_TRAP, sid, switch_id, attr_count, attr_list));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::sflow_hostif_trap_samplepacket_remove(
    _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_HOSTIF_TRAP, serializedObjectId));

    SWSS_LOG_NOTICE("HOSTIF_TRAP %s removed", serializedObjectId.c_str());

    return SAI_STATUS_SUCCESS;

}