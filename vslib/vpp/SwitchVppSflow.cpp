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

sai_status_t SwitchVpp::sflowEnableDisable(
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

sai_status_t SwitchVpp::sflowSamplingRateSet(
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

sai_status_t SwitchVpp::sflowHostifTrapSamplePacketCreate(
    _In_ sai_object_id_t object_id,
    _In_ sai_object_id_t switch_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    SaiCachedObject trap_obj(this, SAI_OBJECT_TYPE_HOSTIF_TRAP, sid, attr_count, attr_list);
    sai_attribute_t attr;
    attr.id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    CHECK_STATUS_QUIET(trap_obj.get_mandatory_attr(attr));

    if (attr.value.s32 == SAI_HOSTIF_TRAP_TYPE_SAMPLEPACKET)
    {
        SWSS_LOG_NOTICE("HOSTIF_TRAP %s SAMPLEPACKET created (bookkeeping)", sid.c_str());
    }

    // Bookkeeping, store the trap object in the generic object hash
    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_HOSTIF_TRAP, sid, switch_id, attr_count, attr_list));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::sflowHostifTrapSamplePacketRemove(
    _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_HOSTIF_TRAP, serializedObjectId));

    SWSS_LOG_NOTICE("HOSTIF_TRAP %s removed", serializedObjectId.c_str());

    return SAI_STATUS_SUCCESS;

}

sai_status_t SwitchVpp::sflowHostifTableEntryCreate(
    _In_ sai_object_id_t object_id,
    _In_ sai_object_id_t switch_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_HOSTIF_TABLE_ENTRY, sid, switch_id, attr_count,attr_list));

    SWSS_LOG_NOTICE("HOSTIF_TABLE_ENTRY %s created (bookkeeping)", sid.c_str());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::sflowHostifTableEntryRemove(
    _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_HOSTIF_TABLE_ENTRY, serializedObjectId));

    SWSS_LOG_NOTICE("HOSTIF_TABLE_ENTRY %s removed", serializedObjectId.c_str());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::sflowPortSamplePacketSet(
    _In_ sai_object_id_t portId,
    _In_ const sai_attribute_t *attr)
{
   /*
    * SAI updates ingress and egress sampling independently, while VPP
    * expects one combined per-port direction mask. Use the incoming
    * attribute for the direction being updated, retrieve the stored
    * attribute for the opposite direction, and combine both before
    * programming the exact sampling rate and direction into VPP.
    */

    SWSS_LOG_ENTER();

    if(attr->id != SAI_PORT_ATTR_EGRESS_SAMPLEPACKET_ENABLE && attr->id != SAI_PORT_ATTR_INGRESS_SAMPLEPACKET_ENABLE)
    {
        SWSS_LOG_ERROR("Unexpected sFlow port attribute ID %u", attr->id);
        return SAI_STATUS_INVALID_PARAMETER;
    }

    bool updating_ingress = attr->id == SAI_PORT_ATTR_INGRESS_SAMPLEPACKET_ENABLE;
    sai_object_id_t updated_oid = attr->value.oid; 

    sai_attribute_t other_attr{};

    other_attr.id = updating_ingress
        ? SAI_PORT_ATTR_EGRESS_SAMPLEPACKET_ENABLE
        : SAI_PORT_ATTR_INGRESS_SAMPLEPACKET_ENABLE;
   
    sai_object_id_t other_oid = SAI_NULL_OBJECT_ID;    

    if (get(SAI_OBJECT_TYPE_PORT, portId, 1, &other_attr) == SAI_STATUS_SUCCESS)
    {
        other_oid = other_attr.value.oid;
    }

    sai_object_id_t ingress_oid = updating_ingress ? updated_oid : other_oid;
    sai_object_id_t egress_oid = updating_ingress ? other_oid : updated_oid;

    constexpr uint32_t SFLOW_DIRECTION_INGRESS = 1U;
    constexpr uint32_t SFLOW_DIRECTION_EGRESS = 2U;
    uint32_t direction = 0;

    if(ingress_oid != SAI_NULL_OBJECT_ID)
    {
        direction |= SFLOW_DIRECTION_INGRESS;
    }

    if(egress_oid != SAI_NULL_OBJECT_ID)
    {
        direction |= SFLOW_DIRECTION_EGRESS;
    }

    if (direction == 0)
    {
        return sflowEnableDisable(portId, false);
    }

    sai_object_id_t active_oid = updated_oid != SAI_NULL_OBJECT_ID ? updated_oid : other_oid;

    auto serialized_id = sai_serialize_object_id(active_oid);

    sai_attribute_t rate_attr{};
    rate_attr.id = SAI_SAMPLEPACKET_ATTR_SAMPLE_RATE;

    CHECK_STATUS(get(SAI_OBJECT_TYPE_SAMPLEPACKET, serialized_id, 1, &rate_attr));

    uint32_t rate = rate_attr.value.u32;

    CHECK_STATUS(sflowInterfaceSamplingRateSet(portId, rate));
    CHECK_STATUS(sflowInterfaceDirectionSet(portId, direction));
    return sflowEnableDisable(portId, true);
}

sai_status_t SwitchVpp::sflowInterfaceSamplingRateSet(
    _In_ sai_object_id_t port_id, 
    _In_ uint32_t rate )
{
    SWSS_LOG_ENTER();

    std::string if_name;

    if(!port_to_hwifname(port_id, if_name))
    {
        SWSS_LOG_ERROR("failed to get hwif name for port %s", sai_serialize_object_id(port_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    int ret = vpp_sflow_interface_sampling_rate_set(if_name.c_str(), rate);
    if (ret != 0)
    {
        SWSS_LOG_ERROR("sflow sampling rate set failed for port %s, status %d", sai_serialize_object_id(port_id).c_str(), ret);
        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_NOTICE("Changed sampling rate to 1-in-%d for port %s", rate, sai_serialize_object_id(port_id).c_str());
    
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::sflowInterfaceDirectionSet(
    _In_ sai_object_id_t port_id,
    _In_ uint32_t direction)
{
    SWSS_LOG_ENTER();

    std::string if_name;

    if(!port_to_hwifname(port_id, if_name))
    {
        SWSS_LOG_ERROR("failed to get hwif name for port %s", sai_serialize_object_id(port_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    int ret = vpp_sflow_interface_direction_set(if_name.c_str(), direction);
    if (ret != 0)
    {
        SWSS_LOG_ERROR("sflow direction set failed for port %s, status %d", sai_serialize_object_id(port_id).c_str(), ret);
        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_NOTICE("Changed direction for port %s", sai_serialize_object_id(port_id).c_str());
    
    return SAI_STATUS_SUCCESS;
}