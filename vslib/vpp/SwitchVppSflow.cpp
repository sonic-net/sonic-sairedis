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

    std::string if_name = m_ifaceRegistry.resolveHwIfName(port_id, 0);

    if (if_name.empty())
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

sai_status_t SwitchVpp::sflowDropMonitoringSet(
    _In_ bool enable)
{
    SWSS_LOG_ENTER();

    // Drop monitoring is a global flag in the VPP sflow plugin, which puts
    // sflow-drop on every interface while it is on, independent of the
    // per-port sFlow enables.

    int ret = vpp_sflow_drop_monitoring_set(enable);
    if (ret != 0)
    {
        SWSS_LOG_ERROR("sflow drop_monitoring_set failed, status %d", ret);
        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_NOTICE("sflow drop monitoring %s", enable ? "enabled" : "disabled");

    return SAI_STATUS_SUCCESS;
}

// SflowOrch does not hand the drop_monitor_limit policer to the switch
// directly; it hangs off the chain it builds before binding the TAM object:
// TAM -> TAM_EVENT (PACKET_DROP) -> TAM_COLLECTOR -> HOSTIF_USER_DEFINED_TRAP
// -> HOSTIF_TRAP_GROUP -> POLICER.
sai_object_id_t SwitchVpp::sflowDropMonitorPolicer(
    _In_ sai_object_id_t tam_id)
{
    SWSS_LOG_ENTER();

    auto tam_obj = get_sai_object(SAI_OBJECT_TYPE_TAM, sai_serialize_object_id(tam_id));
    if (!tam_obj)
    {
        return SAI_NULL_OBJECT_ID;
    }

    for (auto& event: tam_obj->get_linked_objects(SAI_OBJECT_TYPE_TAM_EVENT, SAI_TAM_ATTR_EVENT_OBJECTS_LIST))
    {
        sai_attribute_t attr;
        attr.id = SAI_TAM_EVENT_ATTR_TYPE;
        if (event->get_attr(attr) != SAI_STATUS_SUCCESS ||
                attr.value.s32 != SAI_TAM_EVENT_TYPE_PACKET_DROP)
        {
            continue;
        }

        for (auto& collector: event->get_linked_objects(SAI_OBJECT_TYPE_TAM_COLLECTOR,
                    SAI_TAM_EVENT_ATTR_COLLECTOR_LIST))
        {
            auto trap = collector->get_linked_object(SAI_OBJECT_TYPE_HOSTIF_USER_DEFINED_TRAP,
                    SAI_TAM_COLLECTOR_ATTR_HOSTIF_TRAP);
            if (!trap)
            {
                continue;
            }

            auto group = trap->get_linked_object(SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP,
                    SAI_HOSTIF_USER_DEFINED_TRAP_ATTR_TRAP_GROUP);
            if (!group)
            {
                continue;
            }

            auto policer = group->get_linked_object(SAI_OBJECT_TYPE_POLICER, SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER);
            if (policer)
            {
                sai_object_id_t policer_id;
                sai_deserialize_object_id(policer->get_id(), policer_id);
                return policer_id;
            }
        }
    }

    return SAI_NULL_OBJECT_ID;
}

// Meter drop notifications with the VPP policer createPolicer() made for
// m_sflow_drop_policer. The plugin resolves it by name on use, so in-place
// replaces by setPolicer() need no re-push. No policer means no limit.
sai_status_t SwitchVpp::sflowDropPolicerApply()
{
    SWSS_LOG_ENTER();

    std::string vpp_name;

    if (m_sflow_drop_policer != SAI_NULL_OBJECT_ID)
    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);
        auto it = m_policer_map.find(m_sflow_drop_policer);
        if (it != m_policer_map.end())
        {
            vpp_name = it->second.vpp_name;
        }
    }

    if (m_sflow_drop_policer != SAI_NULL_OBJECT_ID && vpp_name.empty())
    {
        SWSS_LOG_WARN("sflow drop monitor policer %s has no VPP policer, not rate-limiting",
                sai_serialize_object_id(m_sflow_drop_policer).c_str());
    }

    int ret = vpp_sflow_drop_policer_set(vpp_name.c_str());
    if (ret != 0)
    {
        SWSS_LOG_ERROR("sflow drop_policer_set failed, status %d", ret);
        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_NOTICE("sflow drop policer '%s'", vpp_name.c_str());

    return SAI_STATUS_SUCCESS;
}

// Every link of the sflowDropMonitorPolicer() chain but the TAM_EVENT
// collector list (CREATE_ONLY) can be re-pointed after creation, which can
// leave a different policer at the end of the bound TAM object's chain.
// Called by set() after a set on one of the chain's object types has been
// applied.
sai_status_t SwitchVpp::sflowDropMonitorChainSet(
    _In_ sai_object_type_t object_type,
    _In_ sai_attr_id_t attr_id)
{
    SWSS_LOG_ENTER();

    bool is_link =
        (object_type == SAI_OBJECT_TYPE_TAM && attr_id == SAI_TAM_ATTR_EVENT_OBJECTS_LIST) ||
        (object_type == SAI_OBJECT_TYPE_TAM_COLLECTOR && attr_id == SAI_TAM_COLLECTOR_ATTR_HOSTIF_TRAP) ||
        (object_type == SAI_OBJECT_TYPE_HOSTIF_USER_DEFINED_TRAP &&
            attr_id == SAI_HOSTIF_USER_DEFINED_TRAP_ATTR_TRAP_GROUP) ||
        (object_type == SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP && attr_id == SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER);

    if (!is_link || m_sflow_drop_tam == SAI_NULL_OBJECT_ID)
    {
        return SAI_STATUS_SUCCESS;
    }

    auto policer = sflowDropMonitorPolicer(m_sflow_drop_tam);
    if (policer == m_sflow_drop_policer)
    {
        return SAI_STATUS_SUCCESS;
    }

    SWSS_LOG_NOTICE("sflow drop monitor policer changed from %s to %s",
            sai_serialize_object_id(m_sflow_drop_policer).c_str(),
            sai_serialize_object_id(policer).c_str());

    m_sflow_drop_policer = policer;
    return sflowDropPolicerApply();
}

bool SwitchVpp::getSflowEnumValuesCapability(
    _In_ sai_object_type_t object_type,
    _In_ sai_attr_id_t attr_id,
    _Inout_ std::vector<sai_int32_t> &values)
{
    SWSS_LOG_ENTER();

    // SwitchOrch::querySwitchMirrorOnDropCapability() probes these two enums to
    // decide whether to advertise MIRROR_ON_DROP_CAPABLE in STATE_DB, which in
    // turn gates "config sflow drop-monitor-limit". The SAI TAM object graph
    // SflowOrch then builds is bookkeeping here - the VPP sflow plugin delivers
    // drops to hsflowd over NET_DM directly - so each probe is answered with
    // exactly the value it looks for and nothing else.

    if (object_type == SAI_OBJECT_TYPE_TAM_EVENT &&
            attr_id == SAI_TAM_EVENT_ATTR_TYPE)
    {
        values.push_back((sai_int32_t) SAI_TAM_EVENT_TYPE_PACKET_DROP);

        return true;
    }

    if (object_type == SAI_OBJECT_TYPE_HOSTIF_USER_DEFINED_TRAP &&
            attr_id == SAI_HOSTIF_USER_DEFINED_TRAP_ATTR_TYPE)
    {
        values.push_back((sai_int32_t) SAI_HOSTIF_USER_DEFINED_TRAP_TYPE_TAM);

        return true;
    }

    return false;
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

   /*
    * SAI updates ingress and egress sampling independently, while VPP
    * expects one combined per-port direction mask. Use the incoming
    * attribute for the direction being updated, retrieve the stored
    * attribute for the opposite direction, and combine both before
    * programming the exact sampling rate and direction into VPP.
    */

    if(attr->id != SAI_PORT_ATTR_EGRESS_SAMPLEPACKET_ENABLE && attr->id != SAI_PORT_ATTR_INGRESS_SAMPLEPACKET_ENABLE)
    {
        SWSS_LOG_ERROR("Unexpected sFlow port attribute ID %u", attr->id);
        return SAI_STATUS_INVALID_PARAMETER;
    }

    bool updating_ingress = attr->id == SAI_PORT_ATTR_INGRESS_SAMPLEPACKET_ENABLE;

    SaiModDBObject port_obj(this, SAI_OBJECT_TYPE_PORT, sai_serialize_object_id(portId), 1, attr);

    auto ingress_sp = port_obj.get_linked_object(SAI_OBJECT_TYPE_SAMPLEPACKET, SAI_PORT_ATTR_INGRESS_SAMPLEPACKET_ENABLE);
    auto egress_sp  = port_obj.get_linked_object(SAI_OBJECT_TYPE_SAMPLEPACKET, SAI_PORT_ATTR_EGRESS_SAMPLEPACKET_ENABLE);

    constexpr uint32_t SFLOW_DIRECTION_INGRESS = 1U;
    constexpr uint32_t SFLOW_DIRECTION_EGRESS = 2U;
    uint32_t direction = 0;

    if(ingress_sp)
    {
        direction |= SFLOW_DIRECTION_INGRESS;
    }

    if(egress_sp)
    {
        direction |= SFLOW_DIRECTION_EGRESS;
    }

    if (direction == 0)
    {
        return sflowEnableDisable(portId, false);
    }

    /*
    * VPP has a single per-interface sampling rate, so one samplepacket has to
    * supply it: the one named by this update, or if the update cleared this
    * direction, the one still bound to the opposite direction.
    */
    const auto& updated_sp = updating_ingress ? ingress_sp : egress_sp;
    const auto& other_sp   = updating_ingress ? egress_sp  : ingress_sp;
    const auto& sp_obj     = updated_sp ? updated_sp : other_sp;

    sai_attribute_t rate_attr{};
    rate_attr.id = SAI_SAMPLEPACKET_ATTR_SAMPLE_RATE;

    CHECK_STATUS_QUIET(sp_obj->get_mandatory_attr(rate_attr));

    uint32_t rate = rate_attr.value.u32;

    CHECK_STATUS(sflowInterfaceSamplingRateSet(portId, rate));
    CHECK_STATUS(sflowInterfaceDirectionSet(portId, direction));
    return sflowEnableDisable(portId, true);
}

sai_status_t SwitchVpp::sflowInterfaceSamplingRateSet(
    _In_ sai_object_id_t port_id,
    _In_ uint32_t rate)
{
    SWSS_LOG_ENTER();

    std::string if_name = m_ifaceRegistry.resolveHwIfName(port_id, 0);

    if (if_name.empty())
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

    std::string if_name = m_ifaceRegistry.resolveHwIfName(port_id, 0);

    if (if_name.empty())
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