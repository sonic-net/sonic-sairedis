#include "SwitchVppTrim.h"
#include "SwitchVpp.h"

#include "meta/sai_serialize.h"
#include "swss/logger.h"

#include <algorithm>
#include <cstring>

using namespace saivs;

SwitchVppTrim::SwitchVppTrim(
        _In_ SwitchVpp *switch_db):
    m_switch_db(switch_db)
{
    SWSS_LOG_ENTER();
}

bool SwitchVppTrim::isTrimDataplaneAttr(
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id)
{
    SWSS_LOG_ENTER();

    // A change to any of these re-resolves the per-queue trim admission state
    // pushed to the VPP plugin: the queue's buffer profile / scheduler binding,
    // the buffer profile admission-fail action (eligibility), the scheduler
    // group -> scheduler binding (SONiC applies per-queue schedulers to the
    // queue's leaf scheduler group, not to the queue object), or the scheduler
    // PIR (blocking vs unlimited egress).
    switch (object_type)
    {
        case SAI_OBJECT_TYPE_SWITCH:
            return attr_id == SAI_SWITCH_ATTR_PACKET_TRIM_SIZE ||
                   attr_id == SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_RESOLUTION_MODE ||
                   attr_id == SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE ||
                   attr_id == SAI_SWITCH_ATTR_PACKET_TRIM_TC_VALUE ||
                   attr_id == SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_RESOLUTION_MODE ||
                   attr_id == SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_INDEX;

        case SAI_OBJECT_TYPE_QUEUE:
            return attr_id == SAI_QUEUE_ATTR_BUFFER_PROFILE_ID ||
                   attr_id == SAI_QUEUE_ATTR_SCHEDULER_PROFILE_ID;

        case SAI_OBJECT_TYPE_BUFFER_PROFILE:
            return attr_id == SAI_BUFFER_PROFILE_ATTR_PACKET_ADMISSION_FAIL_ACTION;

        case SAI_OBJECT_TYPE_SCHEDULER_GROUP:
            return attr_id == SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID;

        case SAI_OBJECT_TYPE_SCHEDULER:
            return attr_id == SAI_SCHEDULER_ATTR_MAX_BANDWIDTH_RATE;

        case SAI_OBJECT_TYPE_PORT:
            // The DSCP->queue trim table is composed from the port's bound
            // DSCP_TO_TC and TC_TO_QUEUE qos maps; a (re)bind or unbind at
            // runtime must re-resolve it.
            return attr_id == SAI_PORT_ATTR_QOS_DSCP_TO_TC_MAP ||
                   attr_id == SAI_PORT_ATTR_QOS_TC_TO_QUEUE_MAP ||
                   attr_id == SAI_PORT_ATTR_QOS_DEFAULT_TC;

        case SAI_OBJECT_TYPE_QOS_MAP:
            // An in-place edit of a bound DSCP_TO_TC / TC_TO_QUEUE map likewise
            // changes the resolved DSCP->queue table.
            return attr_id == SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST;

        default:
            return false;
    }
}

sai_status_t SwitchVppTrim::updateTrimPolicy(
        _Inout_ TrimGlobalPolicy &policy,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    // Build a candidate policy without changing committed state or VPP.
    switch (attr->id)
    {
        case SAI_SWITCH_ATTR_PACKET_TRIM_SIZE:
            if (attr->value.u32 > UINT16_MAX)
            {
                SWSS_LOG_ERROR("Trim size %u exceeds VPP's u16 range", attr->value.u32);
                return SAI_STATUS_INVALID_ATTR_VALUE_0;
            }
            policy.trim_size = static_cast<uint16_t>(attr->value.u32);
            policy.enabled   = (attr->value.u32 != 0);
            break;

        case SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_RESOLUTION_MODE:
            if (attr->value.s32 != SAI_PACKET_TRIM_DSCP_RESOLUTION_MODE_DSCP_VALUE)
            {
                SWSS_LOG_ERROR("Unsupported trim DSCP-resolution mode %d", attr->value.s32);
                return SAI_STATUS_NOT_SUPPORTED;
            }
            policy.dscp_mode = 0;
            break;

        case SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_VALUE:
            if (attr->value.u8 > 63)
            {
                SWSS_LOG_ERROR("Trim DSCP value %u exceeds the 6-bit range", attr->value.u8);
                return SAI_STATUS_INVALID_ATTR_VALUE_0;
            }
            policy.dscp_value = attr->value.u8;
            break;

        case SAI_SWITCH_ATTR_PACKET_TRIM_TC_VALUE:
            policy.tc_value = attr->value.u8;
            break;

        case SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_RESOLUTION_MODE:
            if (attr->value.s32 != SAI_PACKET_TRIM_QUEUE_RESOLUTION_MODE_STATIC)
            {
                SWSS_LOG_ERROR("Unsupported trim queue-resolution mode %d", attr->value.s32);
                return SAI_STATUS_NOT_SUPPORTED;
            }
            break;

        case SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_INDEX:
            policy.trim_queue = attr->value.u8;
            break;

        default:
            return SAI_STATUS_SUCCESS;
    }

    return SAI_STATUS_SUCCESS;
}

void SwitchVppTrim::restoreTrimPolicy()
{
    SWSS_LOG_ENTER();

    const auto &attrs = m_switch_db->m_objectHash.at(SAI_OBJECT_TYPE_SWITCH).at(
            sai_serialize_object_id(m_switch_db->m_switch_id));

    for (const auto &entry : attrs)
    {
        const auto *attr = entry.second->getAttr();

        if (isTrimDataplaneAttr(SAI_OBJECT_TYPE_SWITCH, attr->id) &&
            updateTrimPolicy(m_trim_policy, attr) != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_THROW("Invalid recovered trim attribute %s", entry.first.c_str());
        }
    }
}

sai_status_t SwitchVppTrim::createSwitchTrimPolicy(
        _In_ const std::string &serializedObjectId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    const auto previous_policy = m_trim_policy;
    auto candidate = previous_policy;

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_status_t status = updateTrimPolicy(candidate, &attr_list[i]);
        if (status != SAI_STATUS_SUCCESS)
        {
            return status == SAI_STATUS_INVALID_ATTR_VALUE_0 ?
                    status - static_cast<sai_status_t>(i) : status;
        }
    }

    auto &attrs = m_switch_db->m_objectHash.at(SAI_OBJECT_TYPE_SWITCH).at(serializedObjectId);
    const auto previous_attrs = attrs;
    for (uint32_t i = 0; i < attr_count; ++i)
    {
        auto value = std::make_shared<SaiAttrWrap>(SAI_OBJECT_TYPE_SWITCH, &attr_list[i]);
        attrs[value->getAttrMetadata()->attridname] = value;
    }
    m_trim_policy = candidate;

    sai_status_t status = programTrimGlobal();
    if (status != SAI_STATUS_SUCCESS)
    {
        attrs = previous_attrs;
        m_trim_policy = previous_policy;
        m_trim_reconcile_pending = programTrimGlobal() != SAI_STATUS_SUCCESS;
        if (m_trim_reconcile_pending)
        {
            SWSS_LOG_ERROR("Trim switch CREATE failed; rollback programming also failed");
        }
        return status;
    }

    m_trim_reconcile_pending = false;
    m_switch_db->m_object_db.create_or_update(
            SAI_OBJECT_TYPE_SWITCH, serializedObjectId, attr_count, attr_list, true);
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVppTrim::programTrimGlobal()
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;

    int ret = vpp_sonic_ext_trim_global_set(
            m_trim_policy.enabled,
            m_trim_policy.trim_size,
            m_trim_policy.dscp_mode,
            m_trim_policy.dscp_value,
            m_trim_policy.tc_value,
            m_trim_policy.trim_queue);

    if (ret != 0)
    {
        SWSS_LOG_ERROR("sonic_ext_trim_global_set failed (ret=%d): enabled=%d size=%u dscp_mode=%u dscp=%u tc=%u queue=%u",
                ret, m_trim_policy.enabled, m_trim_policy.trim_size,
                m_trim_policy.dscp_mode, m_trim_policy.dscp_value,
                m_trim_policy.tc_value, m_trim_policy.trim_queue);
        return SAI_STATUS_FAILURE;
    }

    if (m_trim_policy.enabled || m_trim_dataplane_armed || m_trim_reconcile_pending)
    {
        sai_status_t refresh_status = refreshTrimDataplane();

        if (refresh_status != SAI_STATUS_SUCCESS)
        {
            status = refresh_status;
        }
        else if (!m_trim_policy.enabled)
        {
            m_trim_dataplane_armed = false;
        }
    }

    return status;
}

sai_status_t SwitchVppTrim::refreshTrimDataplaneOnChange(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // Global disable clears armed queues once; ordinary changes while off
    // must not reinstall eligibility from retained bindings.
    if (!m_trim_policy.enabled)
    {
        return SAI_STATUS_SUCCESS;
    }

    for (uint32_t i = 0; i < attr_count; i++)
    {
        if (isTrimDataplaneAttr(object_type, attr_list[i].id))
        {
            return refreshTrimDataplane();
        }
    }

    return SAI_STATUS_SUCCESS;
}

std::vector<sai_object_id_t> SwitchVppTrim::getTrimPortList() const
{
    SWSS_LOG_ENTER();

    // Front-panel ports are created dynamically by orchagent via createPort(),
    // which records them in m_switch_db->m_objectHash but never appends to m_port_list
    // (that cache is only populated by the unused lanemap create_ports() path
    // or a lazy SAI_SWITCH_ATTR_PORT_LIST refresh). Enumerate the authoritative
    // ASIC object store so trim programming sees every port. Mirrors the port
    // enumeration in SwitchStateBase::refresh_port_list().
    std::vector<sai_object_id_t> ports;

    auto port_hash = m_switch_db->m_objectHash.find(SAI_OBJECT_TYPE_PORT);

    if (port_hash != m_switch_db->m_objectHash.end())
    {
        ports.reserve(port_hash->second.size());

        for (const auto &kvp : port_hash->second)
        {
            sai_object_id_t port_oid;
            sai_deserialize_object_id(kvp.first, port_oid);
            ports.push_back(port_oid);
        }
    }

    return ports;
}

sai_status_t SwitchVppTrim::resolveTrimPort(
        _In_ sai_object_id_t port_oid,
        _Out_ std::string &hwif)
{
    SWSS_LOG_ENTER();

    hwif.clear();
    sai_attribute_t attr = {};
    attr.id = SAI_PORT_ATTR_TYPE;
    sai_status_t status = m_switch_db->get(SAI_OBJECT_TYPE_PORT, port_oid, 1, &attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Cannot read trim port type for %s: %d",
                sai_serialize_object_id(port_oid).c_str(), status);
        return status;
    }

    switch (attr.value.s32)
    {
        case SAI_PORT_TYPE_CPU:
        case SAI_PORT_TYPE_FABRIC:
        case SAI_PORT_TYPE_RECYCLE:
            return SAI_STATUS_SUCCESS;
        case SAI_PORT_TYPE_LOGICAL:
            break;
        default:
            SWSS_LOG_ERROR("Unsupported trim port type %d", attr.value.s32);
            return SAI_STATUS_NOT_SUPPORTED;
    }

    if (!m_switch_db->vppGetHwIfNameForPort(port_oid, 0, hwif))
    {
        SWSS_LOG_ERROR("Cannot resolve trim front-panel port %s",
                sai_serialize_object_id(port_oid).c_str());
        return SAI_STATUS_FAILURE;
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVppTrim::refreshTrimDataplane()
{
    SWSS_LOG_ENTER();

    if (m_trim_policy.enabled)
    {
        m_trim_dataplane_armed = true;
    }

    std::vector<sai_object_id_t> port_list = getTrimPortList();

    SWSS_LOG_NOTICE("refreshTrimDataplane: reprogramming trim admission (enabled=%d ports=%zu)",
                    (int)m_trim_policy.enabled, port_list.size());

    // SONiC's QosOrch applies a queue's scheduler (SCHEDULER_BLOCK_DATA_PLANE
    // included) to the queue's parent leaf scheduler group via
    // SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID; the queue object's own
    // SAI_QUEUE_ATTR_SCHEDULER_PROFILE_ID stays unset on sonic-vpp. Build a
    // queue OID -> parent scheduler group reverse map once here so per-queue
    // blocking detection can locate the bound scheduler.
    std::unordered_map<sai_object_id_t, sai_object_id_t> queue_to_sg;
    if (m_trim_policy.enabled)
    {
        buildQueueSchedulerGroupMap(queue_to_sg);
    }

    sai_status_t status = SAI_STATUS_SUCCESS;

    for (auto port_oid : port_list)
    {
        std::string hwif;
        sai_status_t port_status = resolveTrimPort(port_oid, hwif);
        if (port_status != SAI_STATUS_SUCCESS)
        {
            status = port_status;
            continue;
        }
        if (hwif.empty())
        {
            continue;
        }

        if (!m_trim_policy.enabled)
        {
            // Clear even slots whose queue or binding was removed while off.
            for (uint8_t queue = 0; queue < 8; ++queue)
            {
                int ret = vpp_sonic_ext_trim_queue_set(
                        hwif.c_str(), queue, false, UINT64_MAX, UINT64_MAX);
                if (ret != 0)
                {
                    SWSS_LOG_ERROR("sonic_ext_trim_queue_set cleanup failed (ret=%d): port=%s queue=%u",
                            ret, hwif.c_str(), queue);
                    return SAI_STATUS_FAILURE;
                }
            }
            continue;
        }

        sai_attribute_t attr;
        std::vector<sai_object_id_t> queues(MAX_OBJLIST_LEN);

        attr.id                  = SAI_PORT_ATTR_QOS_QUEUE_LIST;
        attr.value.objlist.count = MAX_OBJLIST_LEN;
        attr.value.objlist.list  = queues.data();

        sai_status_t queue_list_status = m_switch_db->get(SAI_OBJECT_TYPE_PORT, port_oid, 1, &attr);
        if (queue_list_status == SAI_STATUS_ITEM_NOT_FOUND)
        {
            // Replay can expose a port before its queue list is populated.
            SWSS_LOG_WARN("refreshTrimDataplane: port 0x%llx (%s) queue list get failed",
                          (unsigned long long)port_oid, hwif.c_str());
            continue;
        }
        if (queue_list_status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("refreshTrimDataplane: port 0x%llx (%s) queue list get failed: %d",
                          (unsigned long long)port_oid, hwif.c_str(), queue_list_status);
            return queue_list_status;
        }

        uint32_t n = attr.value.objlist.count;

        SWSS_LOG_DEBUG("refreshTrimDataplane: port 0x%llx (%s) queues=%u",
                       (unsigned long long)port_oid, hwif.c_str(), n);

        for (uint32_t i = 0; i < n; i++)
        {
            sai_status_t queue_status = refreshTrimQueue(hwif, queues[i], queue_to_sg);

            if (queue_status != SAI_STATUS_SUCCESS)
            {
                return queue_status;
            }
        }
    }

    if (!m_trim_policy.enabled)
    {
        return status;
    }

    sai_status_t map_status = refreshTrimDscpToQueueMap();

    if (map_status != SAI_STATUS_SUCCESS)
    {
        status = map_status;
    }

    return status;
}

void SwitchVppTrim::buildQueueSchedulerGroupMap(
        _Out_ std::unordered_map<sai_object_id_t, sai_object_id_t> &queueToSg)
{
    SWSS_LOG_ENTER();

    queueToSg.clear();

    auto sg_hash = m_switch_db->m_objectHash.find(SAI_OBJECT_TYPE_SCHEDULER_GROUP);

    if (sg_hash == m_switch_db->m_objectHash.end())
    {
        return;
    }

    for (const auto &kvp : sg_hash->second)
    {
        sai_object_id_t sg_oid;
        sai_deserialize_object_id(kvp.first, sg_oid);

        sai_attribute_t attr;

        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT;
        if (m_switch_db->get(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_oid, 1, &attr) != SAI_STATUS_SUCCESS)
        {
            continue;
        }

        uint32_t count = attr.value.u32;

        if (count == 0 || count > MAX_OBJLIST_LEN)
        {
            continue;
        }

        std::vector<sai_object_id_t> children(count);

        attr.id                  = SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST;
        attr.value.objlist.count = count;
        attr.value.objlist.list  = children.data();

        if (m_switch_db->get(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_oid, 1, &attr) != SAI_STATUS_SUCCESS)
        {
            continue;
        }

        // A leaf scheduler group's child list holds queue OIDs; intermediate
        // groups hold child groups. Recording every child is harmless because
        // only queue OIDs are ever looked up during per-queue blocking
        // detection, and each queue appears in exactly one group's child list.
        for (uint32_t i = 0; i < attr.value.objlist.count; i++)
        {
            queueToSg[children[i]] = sg_oid;
        }
    }
}

sai_status_t SwitchVppTrim::refreshTrimQueue(
        _In_ const std::string &hwif_name,
        _In_ sai_object_id_t queue_oid,
        _In_ const std::unordered_map<sai_object_id_t, sai_object_id_t> &queueToSg)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    // Only unicast queues are trim targets. Skip multicast queues so their
    // (colliding) index never clobbers the unicast queue admission state.
    attr.id = SAI_QUEUE_ATTR_TYPE;
    if (m_switch_db->get(SAI_OBJECT_TYPE_QUEUE, queue_oid, 1, &attr) == SAI_STATUS_SUCCESS)
    {
        sai_queue_type_t qtype = (sai_queue_type_t) attr.value.s32;

        if (qtype != SAI_QUEUE_TYPE_UNICAST && qtype != SAI_QUEUE_TYPE_ALL)
        {
            return SAI_STATUS_SUCCESS;
        }
    }

    attr.id = SAI_QUEUE_ATTR_INDEX;
    if (m_switch_db->get(SAI_OBJECT_TYPE_QUEUE, queue_oid, 1, &attr) != SAI_STATUS_SUCCESS)
    {
        return SAI_STATUS_SUCCESS;
    }
    uint8_t queue_index = attr.value.u8;

    // The VPP sonic_ext trim datapath only tracks admission for queues
    // 0..SONIC_EXT_TRIM_MAX_QUEUES-1 (8; see the plugin's sonic_ext.h). SONiC
    // models 10 unicast queues per port (0-9), so indices >= 8 have no VPP hwif
    // queue and the plugin would reject them with VNET_API_ERROR_INVALID_VALUE.
    // Skip them: the trim queue and every eligible (lossy) queue are < 8.
    static const uint8_t VPP_TRIM_MAX_QUEUES = 8;
    if (queue_index >= VPP_TRIM_MAX_QUEUES)
    {
        return SAI_STATUS_SUCCESS;
    }

    // Eligibility: the queue's bound buffer profile uses DROP_AND_TRIM.
    bool eligible = false;

    attr.id = SAI_QUEUE_ATTR_BUFFER_PROFILE_ID;
    if (m_switch_db->get(SAI_OBJECT_TYPE_QUEUE, queue_oid, 1, &attr) == SAI_STATUS_SUCCESS &&
        attr.value.oid != SAI_NULL_OBJECT_ID)
    {
        sai_object_id_t bp_oid = attr.value.oid;

        attr.id = SAI_BUFFER_PROFILE_ATTR_PACKET_ADMISSION_FAIL_ACTION;
        if (m_switch_db->get(SAI_OBJECT_TYPE_BUFFER_PROFILE, bp_oid, 1, &attr) == SAI_STATUS_SUCCESS)
        {
            eligible = (attr.value.s32 ==
                        SAI_BUFFER_PROFILE_PACKET_ADMISSION_FAIL_ACTION_DROP_AND_TRIM);
        }
    }

    // Blocking: the queue's scheduler imposes a PIR (MAX_BANDWIDTH_RATE > 0).
    // SONiC's SCHEDULER_BLOCK_DATA_PLANE holds the queue closed with PIR=1; no
    // PIR means unlimited egress and the queue must never trim.
    //
    // SONiC's QosOrch does not bind per-queue schedulers to the queue object;
    // it applies them to the queue's parent (leaf) scheduler group. Resolve the
    // effective scheduler by preferring a directly-bound queue scheduler (real
    // hardware SAI) and falling back to the parent scheduler group binding
    // (sonic-vpp), which is where SCHEDULER_BLOCK_DATA_PLANE actually lands.
    bool blocking = false;
    sai_object_id_t sched_oid = SAI_NULL_OBJECT_ID;

    attr.id = SAI_QUEUE_ATTR_SCHEDULER_PROFILE_ID;
    if (m_switch_db->get(SAI_OBJECT_TYPE_QUEUE, queue_oid, 1, &attr) == SAI_STATUS_SUCCESS &&
        attr.value.oid != SAI_NULL_OBJECT_ID)
    {
        sched_oid = attr.value.oid;
    }
    else
    {
        auto it = queueToSg.find(queue_oid);

        if (it != queueToSg.end())
        {
            attr.id = SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID;
            if (m_switch_db->get(SAI_OBJECT_TYPE_SCHEDULER_GROUP, it->second, 1, &attr) == SAI_STATUS_SUCCESS &&
                attr.value.oid != SAI_NULL_OBJECT_ID)
            {
                sched_oid = attr.value.oid;
            }
        }
    }

    if (sched_oid != SAI_NULL_OBJECT_ID)
    {
        attr.id = SAI_SCHEDULER_ATTR_MAX_BANDWIDTH_RATE;
        if (m_switch_db->get(SAI_OBJECT_TYPE_SCHEDULER, sched_oid, 1, &attr) == SAI_STATUS_SUCCESS)
        {
            blocking = (attr.value.u64 != 0);
        }
    }

    uint64_t rate;
    uint64_t capacity;

    if (blocking)
    {
        // No admission headroom: the token bucket starts empty and never
        // refills, so every packet on this eligible queue fails admission and
        // is trimmed.
        rate     = 0;
        capacity = 0;
    }
    else
    {
        // Unlimited egress: keep the bucket permanently full so trim-eligible
        // queues carrying normal (uncongested) traffic are never policed.
        rate     = UINT64_MAX;
        capacity = UINT64_MAX;
    }

    int ret = vpp_sonic_ext_trim_queue_set(hwif_name.c_str(), queue_index, eligible, rate, capacity);

    if (ret != 0)
    {
        SWSS_LOG_ERROR("vpp_sonic_ext_trim_queue_set failed (ret=%d) hwif=%s queue=%u eligible=%d rate=%llu cap=%llu",
                ret, hwif_name.c_str(), queue_index, (int)eligible,
                (unsigned long long)rate, (unsigned long long)capacity);
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVppTrim::refreshTrimDscpToQueueMap()
{
    SWSS_LOG_ENTER();

    uint8_t dscp_to_queue[64] = {};
    int best_priority = -1;

    // Optional port attributes may not have been materialized during replay.
    // Use their declared SAI defaults, not the previous dataplane mapping.
    auto get_port_attr = [this](sai_object_id_t port_oid, sai_attribute_t &attr) -> sai_status_t
    {
        sai_status_t status = m_switch_db->get(SAI_OBJECT_TYPE_PORT, port_oid, 1, &attr);

        if (status == SAI_STATUS_ITEM_NOT_FOUND)
        {
            const auto *meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_PORT, attr.id);

            if (meta && meta->defaultvalue)
            {
                attr.value = *meta->defaultvalue;
                return SAI_STATUS_SUCCESS;
            }
        }

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Cannot resolve trim QoS attribute %d on port %s: %d",
                    attr.id, sai_serialize_object_id(port_oid).c_str(), status);
        }

        return status;
    };

    // Keep the existing uniform-QoS reference-port model. Prefer a complete
    // pair during incremental replay, then a remaining TC-to-queue binding.
    for (auto port_oid : getTrimPortList())
    {
        std::string hwif;
        sai_status_t port_status = resolveTrimPort(port_oid, hwif);
        if (port_status != SAI_STATUS_SUCCESS)
        {
            return port_status;
        }
        if (hwif.empty())
        {
            continue;
        }

        sai_attribute_t attr = {};
        attr.id = SAI_PORT_ATTR_QOS_DSCP_TO_TC_MAP;
        sai_status_t status = get_port_attr(port_oid, attr);
        if (status != SAI_STATUS_SUCCESS)
        {
            return status;
        }
        sai_object_id_t dscp_to_tc_oid = attr.value.oid;

        attr.id = SAI_PORT_ATTR_QOS_TC_TO_QUEUE_MAP;
        status = get_port_attr(port_oid, attr);
        if (status != SAI_STATUS_SUCCESS)
        {
            return status;
        }
        sai_object_id_t tc_to_queue_oid = attr.value.oid;

        int priority = (tc_to_queue_oid == SAI_NULL_OBJECT_ID) ? 0 :
                ((dscp_to_tc_oid == SAI_NULL_OBJECT_ID) ? 1 : 2);
        if (priority <= best_priority)
        {
            continue;
        }
        best_priority = priority;

        attr.id = SAI_PORT_ATTR_QOS_DEFAULT_TC;
        status = get_port_attr(port_oid, attr);
        if (status != SAI_STATUS_SUCCESS)
        {
            return status;
        }

        uint8_t tc_by_dscp[64];
        uint8_t queue_by_tc[256] = {};
        memset(tc_by_dscp, attr.value.u8, sizeof(tc_by_dscp));

        if (dscp_to_tc_oid != SAI_NULL_OBJECT_ID)
        {
            std::vector<sai_qos_map_t> entries(64);

            attr.id                 = SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST;
            attr.value.qosmap.count = (uint32_t) entries.size();
            attr.value.qosmap.list  = entries.data();

            status = m_switch_db->get(SAI_OBJECT_TYPE_QOS_MAP, dscp_to_tc_oid, 1, &attr);
            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_ERROR("Cannot read trim DSCP-to-TC map: %d", status);
                return status;
            }

            for (uint32_t i = 0; i < attr.value.qosmap.count; i++)
            {
                uint8_t dscp = entries[i].key.dscp & 0x3f;
                tc_by_dscp[dscp] = entries[i].value.tc;
            }
        }

        if (tc_to_queue_oid != SAI_NULL_OBJECT_ID)
        {
            std::vector<sai_qos_map_t> entries(256);

            attr.id                 = SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST;
            attr.value.qosmap.count = (uint32_t) entries.size();
            attr.value.qosmap.list  = entries.data();

            status = m_switch_db->get(SAI_OBJECT_TYPE_QOS_MAP, tc_to_queue_oid, 1, &attr);
            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_ERROR("Cannot read trim TC-to-queue map: %d", status);
                return status;
            }

            for (uint32_t i = 0; i < attr.value.qosmap.count; i++)
            {
                uint8_t tc = entries[i].key.tc;
                queue_by_tc[tc] = entries[i].value.queue_index;
            }
        }

        for (int d = 0; d < 64; d++)
        {
            uint8_t tc = tc_by_dscp[d];
            dscp_to_queue[d] = queue_by_tc[tc];
        }

        if (priority == 2)
        {
            break;
        }
    }

    for (uint8_t queue : dscp_to_queue)
    {
        if (queue >= 8)
        {
            SWSS_LOG_ERROR("Trim DSCP map resolves unsupported queue %u", queue);
            return SAI_STATUS_NOT_SUPPORTED;
        }
    }

    int ret = vpp_sonic_ext_trim_dscp_map_set(dscp_to_queue);
    if (ret != 0)
    {
        SWSS_LOG_ERROR("vpp_sonic_ext_trim_dscp_map_set failed (ret=%d)", ret);
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

bool SwitchVppTrim::getTrimEnumValuesCapability(
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Out_ sai_int32_t *value)
{
    SWSS_LOG_ENTER();

    // VPP's software admission shim honors only one resolution mode per trim
    // enum: DSCP_VALUE for the DSCP resolution mode and STATIC for the queue
    // resolution mode. The asymmetric FROM_TC / DYNAMIC modes are intentionally
    // omitted so orchagent never programs a mode VPP cannot satisfy. All other
    // (object_type, attr_id) pairs fall through to the base metadata.
    if (object_type != SAI_OBJECT_TYPE_SWITCH)
    {
        return false;
    }

    switch (attr_id)
    {
        case SAI_SWITCH_ATTR_PACKET_TRIM_DSCP_RESOLUTION_MODE:
            if (value)
            {
                *value = (sai_int32_t) SAI_PACKET_TRIM_DSCP_RESOLUTION_MODE_DSCP_VALUE;
            }
            return true;

        case SAI_SWITCH_ATTR_PACKET_TRIM_QUEUE_RESOLUTION_MODE:
            if (value)
            {
                *value = (sai_int32_t) SAI_PACKET_TRIM_QUEUE_RESOLUTION_MODE_STATIC;
            }
            return true;

        default:
            return false;
    }
}

sai_status_t SwitchVppTrim::queryAttrEnumValuesCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Inout_ sai_s32_list_t *enum_values_capability)
{
    SWSS_LOG_ENTER();

    // Advertise only the trim resolution modes the VPP software admission shim
    // actually honors (see getTrimEnumValuesCapability). The buffer-profile
    // DROP_AND_TRIM admission-fail action is advertised as-is by the base
    // metadata.
    sai_int32_t value = 0;

    if (getTrimEnumValuesCapability(object_type, attr_id, &value))
    {
        if (enum_values_capability->count < 1)
        {
            enum_values_capability->count = 1;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }

        enum_values_capability->count   = 1;
        enum_values_capability->list[0] = value;

        return SAI_STATUS_SUCCESS;
    }

    return m_switch_db->SwitchStateBase::queryAttrEnumValuesCapability(
            switch_id, object_type, attr_id, enum_values_capability);
}

sai_status_t SwitchVppTrim::queryStatsCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _Inout_ sai_stat_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();

    // Fetch the complete base list before applying the caller's capacity:
    // a buffer sized for the filtered result can be smaller than the base list.
    sai_stat_capability_list_t base = {};
    sai_status_t status = m_switch_db->SwitchStateBase::queryStatsCapability(switch_id, object_type, &base);
    if (status != SAI_STATUS_SUCCESS && status != SAI_STATUS_BUFFER_OVERFLOW)
    {
        return status;
    }

    std::vector<sai_stat_capability_t> entries(base.count);
    base.list = entries.data();
    status = m_switch_db->SwitchStateBase::queryStatsCapability(switch_id, object_type, &base);
    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    // VPP does not yet source the SAI trim counters. Leave other statistics
    // and their supported modes unchanged, including generic VS behavior.
    entries.erase(std::remove_if(entries.begin(), entries.end(),
            [object_type](const sai_stat_capability_t &entry) {
                switch (object_type)
                {
                    case SAI_OBJECT_TYPE_SWITCH:
                        return entry.stat_enum == SAI_SWITCH_STAT_DROPPED_TRIM_PACKETS ||
                            entry.stat_enum == SAI_SWITCH_STAT_TX_TRIM_PACKETS;
                    case SAI_OBJECT_TYPE_PORT:
                        return entry.stat_enum == SAI_PORT_STAT_TRIM_PACKETS ||
                            entry.stat_enum == SAI_PORT_STAT_DROPPED_TRIM_PACKETS ||
                            entry.stat_enum == SAI_PORT_STAT_TX_TRIM_PACKETS;
                    case SAI_OBJECT_TYPE_QUEUE:
                        return entry.stat_enum == SAI_QUEUE_STAT_TRIM_PACKETS ||
                            entry.stat_enum == SAI_QUEUE_STAT_DROPPED_TRIM_PACKETS ||
                            entry.stat_enum == SAI_QUEUE_STAT_TX_TRIM_PACKETS;
                    default:
                        return false;
                }
            }), entries.end());

    auto count = static_cast<uint32_t>(entries.size());
    if (stats_capability->count < count)
    {
        stats_capability->count = count;
        return SAI_STATUS_BUFFER_OVERFLOW;
    }
    std::copy(entries.begin(), entries.end(), stats_capability->list);
    stats_capability->count = count;
    return SAI_STATUS_SUCCESS;
}

void SwitchVppTrim::refreshAfterCreate(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serializedObjectId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // Keep CREATE successful, but reconcile a failed refresh before the next
    // trim SET so partial programming is not silently retained.
    if (refreshTrimDataplaneOnChange(object_type, attr_count, attr_list) != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Trim refresh failed after creating %s %s; reconciliation required",
                sai_serialize_object_type(object_type).c_str(), serializedObjectId.c_str());
        m_trim_reconcile_pending = true;
    }
}

sai_status_t SwitchVppTrim::set(
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ const std::shared_ptr<SaiAttrWrap> &a)
{
    SWSS_LOG_ENTER();

    const auto *attr = a->getAttr();
    const std::string name = a->getAttrMetadata()->attridname;
    auto &attrHash = m_switch_db->m_objectHash.at(objectType).at(serializedObjectId);
    const auto previous_policy = m_trim_policy;
    auto next_policy = previous_policy;
    bool global = objectType == SAI_OBJECT_TYPE_SWITCH;

    if (global)
    {
        sai_status_t status = updateTrimPolicy(next_policy, attr);
        if (status != SAI_STATUS_SUCCESS)
        {
            return status;
        }
    }

    // Reconciliation is event-driven; there is no timer retry. An explicit
    // disable can clear a broken enabled policy without replaying it first,
    // allowing disable -> repair configuration -> re-enable recovery.
    bool disable = global && attr->id == SAI_SWITCH_ATTR_PACKET_TRIM_SIZE &&
            !next_policy.enabled;
    if (m_trim_reconcile_pending && !disable)
    {
        sai_status_t status = programTrimGlobal();
        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Trim reconciliation still pending; refusing SET");
            return status;
        }
        m_trim_reconcile_pending = false;
    }

    auto previous = attrHash.find(name);
    std::shared_ptr<SaiAttrWrap> previous_attr =
            previous == attrHash.end() ? nullptr : previous->second;
    auto restore_attr = [&]()
    {
        if (previous_attr)
        {
            attrHash[name] = previous_attr;
        }
        else
        {
            attrHash.erase(name);
        }
    };

    // Refresh reads the candidate attributes, but relationships remain
    // committed to the old state until the dataplane accepts the change.
    attrHash[name] = a;
    m_trim_policy = next_policy;
    sai_status_t status = global ? programTrimGlobal() :
            refreshTrimDataplaneOnChange(objectType, 1, attr);
    restore_attr();

    if (status != SAI_STATUS_SUCCESS)
    {
        m_trim_policy = previous_policy;
        m_trim_reconcile_pending = programTrimGlobal() != SAI_STATUS_SUCCESS;
        if (m_trim_reconcile_pending)
        {
            SWSS_LOG_ERROR("Trim SET failed; rollback programming also failed");
        }
        return status;
    }
    m_trim_reconcile_pending = false;

    return SAI_STATUS_SUCCESS;
}
