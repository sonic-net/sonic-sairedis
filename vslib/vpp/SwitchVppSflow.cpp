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
    SWSS_LOG_ENTER();

    sai_object_id_t sp_oid = attr->value.oid;

    if(sp_oid == SAI_NULL_OBJECT_ID)
    {
        return sflowEnableDisable(portId, false);
    }

    sai_attribute_t rate_attr;
    rate_attr.id = SAI_SAMPLEPACKET_ATTR_SAMPLE_RATE;
    uint32_t rate = 0;

    auto serialized_id = sai_serialize_object_id(sp_oid);

    if (get(SAI_OBJECT_TYPE_SAMPLEPACKET, serialized_id, 1, &rate_attr) == SAI_STATUS_SUCCESS)
    {
        rate = rate_attr.value.u32;
    }

    if (m_sflow_sample_rate != 0 && m_sflow_sample_rate != rate)
    {
        SWSS_LOG_WARN("sFlow sample rate mismatch: global=%u port %s requesting=%u (last-writer-wins)",
            m_sflow_sample_rate,
            sai_serialize_object_id(portId).c_str(),
            rate);
    }

    m_sflow_sample_rate = rate;

    CHECK_STATUS(sflowEnableDisable(portId, true));
    return sflowSamplingRateSet(rate);
}