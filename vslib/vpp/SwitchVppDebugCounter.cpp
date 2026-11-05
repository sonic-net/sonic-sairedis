#include "SwitchVpp.h"
#include "SwitchVppUtils.h"

#include "meta/sai_serialize.h"
#include "swss/logger.h"
#include "vppxlate/SaiIntfStats.h"
#include "vppxlate/SaiVppXlate.h"

#include <set>

using namespace saivs;

namespace
{
    // SAI ingress drop reasons counted by the sonic_ext drop-reason-stats
    // feature, with the stats segment leaf that holds each one.
    const std::map<int32_t, std::string> g_vppInDropReasons = {
        { SAI_IN_DROP_REASON_SIP_LINK_LOCAL, "SIP_LINK_LOCAL" },
        { SAI_IN_DROP_REASON_DIP_LINK_LOCAL, "DIP_LINK_LOCAL" },
    };

    // One flex counter poll reads every port; serve them all from one dump.
    const auto g_dropReasonStatsMaxAge = std::chrono::milliseconds(200);

    std::vector<std::string> vppDropReasonNames(
            _In_ const sai_s32_list_t &list)
    {
        SWSS_LOG_ENTER();

        std::vector<std::string> names;

        for (uint32_t i = 0; i < list.count; i++)
        {
            auto it = g_vppInDropReasons.find(list.list[i]);

            if (it == g_vppInDropReasons.end())
            {
                SWSS_LOG_WARN("drop reason %d is not counted by VPP", list.list[i]);
                continue;
            }

            names.push_back(it->second);
        }

        return names;
    }

    void accumulateDropReasonStat(
            _In_ const char *reason,
            _In_ uint32_t sw_if_index,
            _In_ uint64_t count,
            _In_ void *data)
    {
        SWSS_LOG_ENTER();

        auto stats = static_cast<std::map<std::string, std::map<uint32_t, uint64_t>>*>(data);

        (*stats)[reason][sw_if_index] += count;
    }
}

sai_status_t SwitchVpp::queryDebugCounterCapability(
        _In_ sai_attr_id_t attr_id,
        _Inout_ sai_s32_list_t *enum_values_capability)
{
    SWSS_LOG_ENTER();

    // VPP counts nothing while the feature is off in startup.conf (or when it
    // predates the feature), so advertise nothing rather than counters stuck
    // at 0.
    bool counted = sonicExtFeatureEnabled("drop-reason-stats");

    std::vector<int32_t> values;

    switch (attr_id)
    {
        case SAI_DEBUG_COUNTER_ATTR_IN_DROP_REASON_LIST:

            if (counted)
            {
                for (auto &kvp: g_vppInDropReasons)
                {
                    values.push_back(kvp.first);
                }
            }

            return fillEnumValuesCapability(values, enum_values_capability);

        case SAI_DEBUG_COUNTER_ATTR_OUT_DROP_REASON_LIST:

            // No egress drop reasons are counted yet.
            return fillEnumValuesCapability(values, enum_values_capability);

        case SAI_DEBUG_COUNTER_ATTR_TYPE:

            if (counted)
            {
                values = {
                    SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS,
                    SAI_DEBUG_COUNTER_TYPE_SWITCH_IN_DROP_REASONS };
            }

            return fillEnumValuesCapability(values, enum_values_capability);

        default:
            return SAI_STATUS_NOT_SUPPORTED;
    }
}

sai_status_t SwitchVpp::queryPortStatsCapability(
        _Inout_ sai_stat_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();

    // syncd only polls port stats that this reports. The drop monitor polls
    // each debug counter's port stat through the regular port counter context,
    // so the per-index debug counter stats have to be listed here as well.
    sai_stat_capability_list_t base;

    base.count = 0;
    base.list = nullptr;

    sai_status_t status = SwitchStateBase::queryPortStatsCapability(&base);

    if (status != SAI_STATUS_BUFFER_OVERFLOW && status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    std::vector<sai_stat_capability_t> list(base.count);

    base.list = list.data();

    CHECK_STATUS(SwitchStateBase::queryPortStatsCapability(&base));

    list.resize(base.count);

    for (uint32_t i = 0; i < static_cast<uint32_t>(maxDebugCounters); i++)
    {
        sai_stat_capability_t capability;

        capability.stat_enum = static_cast<sai_stat_id_t>(SAI_PORT_STAT_IN_DROP_REASON_RANGE_BASE + i);
        capability.stat_modes = SAI_STATS_MODE_READ_AND_CLEAR | SAI_STATS_MODE_READ;

        list.push_back(capability);
    }

    uint32_t count = static_cast<uint32_t>(list.size());

    if (stats_capability->count < count)
    {
        stats_capability->count = count;
        return SAI_STATUS_BUFFER_OVERFLOW;
    }

    stats_capability->count = count;

    for (uint32_t i = 0; i < count; i++)
    {
        stats_capability->list[i] = list[i];
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::debugCounterCreate(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(createDebugCounter(object_id, switch_id, attr_count, attr_list));

    VppDebugCounter counter{};

    counter.type = SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS;

    sai_attribute_t attr;

    attr.id = SAI_DEBUG_COUNTER_ATTR_INDEX;

    CHECK_STATUS(get(SAI_OBJECT_TYPE_DEBUG_COUNTER, object_id, 1, &attr));

    counter.index = attr.value.u32;

    for (uint32_t i = 0; i < attr_count; i++)
    {
        if (attr_list[i].id == SAI_DEBUG_COUNTER_ATTR_TYPE)
        {
            counter.type = attr_list[i].value.s32;
        }
        else if (attr_list[i].id == SAI_DEBUG_COUNTER_ATTR_IN_DROP_REASON_LIST)
        {
            counter.reasons = vppDropReasonNames(attr_list[i].value.s32list);
        }
    }

    refreshDropReasonStats(true);
    rebaseDebugCounter(counter);

    m_debugCounters[object_id] = counter;

    SWSS_LOG_NOTICE("debug counter %s: index %u, type %d, %zu VPP drop reasons",
            sai_serialize_object_id(object_id).c_str(),
            counter.index,
            counter.type,
            counter.reasons.size());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::debugCounterSet(
        _In_ sai_object_id_t object_id,
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(set_internal(SAI_OBJECT_TYPE_DEBUG_COUNTER, serializedObjectId, attr));

    // createDebugCounter() sets the index before the counter is tracked here.
    auto it = m_debugCounters.find(object_id);

    if (it == m_debugCounters.end() || attr->id != SAI_DEBUG_COUNTER_ATTR_IN_DROP_REASON_LIST)
    {
        return SAI_STATUS_SUCCESS;
    }

    it->second.reasons = vppDropReasonNames(attr->value.s32list);

    refreshDropReasonStats(true);
    rebaseDebugCounter(it->second);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::debugCounterRemove(
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    m_debugCounters.erase(object_id);

    return removeDebugCounter(object_id);
}

void SwitchVpp::refreshDropReasonStats(
        _In_ bool force)
{
    SWSS_LOG_ENTER();

    auto now = std::chrono::steady_clock::now();

    if (!force && m_dropReasonStatsValid &&
            now - m_dropReasonStatsTime < g_dropReasonStatsMaxAge)
    {
        return;
    }

    std::map<std::string, std::map<uint32_t, uint64_t>> fresh;

    // Fails when nothing is exported yet (old plugin, feature off, or no
    // drops registered), which is the same as every count being 0.
    vpp_drop_reason_stats_dump(accumulateDropReasonStat, &fresh);

    m_dropReasonStats.swap(fresh);
    m_dropReasonStatsTime = now;
    m_dropReasonStatsValid = true;
}

uint64_t SwitchVpp::debugCounterRawCount(
        _In_ const VppDebugCounter &counter,
        _In_ uint32_t sw_if_index) const
{
    SWSS_LOG_ENTER();

    uint64_t total = 0;

    for (auto &reason: counter.reasons)
    {
        auto rit = m_dropReasonStats.find(reason);

        if (rit == m_dropReasonStats.end())
        {
            continue;
        }

        auto pit = rit->second.find(sw_if_index);

        if (pit != rit->second.end())
        {
            total += pit->second;
        }
    }

    return total;
}

uint64_t SwitchVpp::debugCounterValue(
        _Inout_ VppDebugCounter &counter,
        _In_ uint32_t sw_if_index)
{
    SWSS_LOG_ENTER();

    uint64_t raw = debugCounterRawCount(counter, sw_if_index);

    // An interface first seen after the counter was created starts from 0 in
    // VPP, so its missing base is 0 too.
    auto &base = counter.base[sw_if_index];

    // VPP restarted or its counters were cleared: start over rather than
    // report a negative difference.
    if (raw < base)
    {
        base = raw;
    }

    return raw - base;
}

void SwitchVpp::rebaseDebugCounter(
        _Inout_ VppDebugCounter &counter)
{
    SWSS_LOG_ENTER();

    counter.base.clear();

    for (auto &reason: counter.reasons)
    {
        auto rit = m_dropReasonStats.find(reason);

        if (rit == m_dropReasonStats.end())
        {
            continue;
        }

        for (auto &kvp: rit->second)
        {
            counter.base[kvp.first] = debugCounterRawCount(counter, kvp.first);
        }
    }
}

void SwitchVpp::addPortDebugCounterStats(
        _In_ const std::string &if_name,
        _Inout_ std::map<sai_stat_id_t, uint64_t> &stats)
{
    SWSS_LOG_ENTER();

    if (m_debugCounters.empty())
    {
        return;
    }

    uint32_t sw_if_index = vpp_get_swif_idx_by_name(if_name.c_str());

    if (sw_if_index == (uint32_t)~0u)
    {
        return;
    }

    refreshDropReasonStats(false);

    for (auto &kvp: m_debugCounters)
    {
        auto &counter = kvp.second;

        if (counter.type != SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS)
        {
            continue;
        }

        stats[static_cast<sai_stat_id_t>(SAI_PORT_STAT_IN_DROP_REASON_RANGE_BASE + counter.index)] =
            debugCounterValue(counter, sw_if_index);
    }
}

void SwitchVpp::setSwitchDebugCounterStats(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    if (m_debugCounters.empty())
    {
        return;
    }

    refreshDropReasonStats(false);

    std::map<sai_stat_id_t, uint64_t> stats;

    for (auto &kvp: m_debugCounters)
    {
        auto &counter = kvp.second;

        if (counter.type != SAI_DEBUG_COUNTER_TYPE_SWITCH_IN_DROP_REASONS)
        {
            continue;
        }

        // Every sw_if_index the counter has seen, including index 0, where
        // VPP keeps the drops it cannot attribute to a port.
        std::set<uint32_t> indices;

        for (auto &base: counter.base)
        {
            indices.insert(base.first);
        }

        for (auto &reason: counter.reasons)
        {
            auto rit = m_dropReasonStats.find(reason);

            if (rit == m_dropReasonStats.end())
            {
                continue;
            }

            for (auto &port: rit->second)
            {
                indices.insert(port.first);
            }
        }

        uint64_t total = 0;

        for (auto sw_if_index: indices)
        {
            total += debugCounterValue(counter, sw_if_index);
        }

        stats[static_cast<sai_stat_id_t>(SAI_SWITCH_STAT_IN_DROP_REASON_RANGE_BASE + counter.index)] = total;
    }

    debugSetStats(switch_id, stats);
}

void SwitchVpp::clearDebugCounterStats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
{
    SWSS_LOG_ENTER();

    if (m_debugCounters.empty())
    {
        return;
    }

    int32_t type;
    uint32_t base_id;
    uint32_t sw_if_index = (uint32_t)~0u;

    if (object_type == SAI_OBJECT_TYPE_PORT)
    {
        type = SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS;
        base_id = SAI_PORT_STAT_IN_DROP_REASON_RANGE_BASE;

        std::string if_name = m_ifaceRegistry.resolveHwIfName(object_id, 0);

        if (if_name.empty())
        {
            return;
        }

        sw_if_index = vpp_get_swif_idx_by_name(if_name.c_str());

        if (sw_if_index == (uint32_t)~0u)
        {
            return;
        }
    }
    else if (object_type == SAI_OBJECT_TYPE_SWITCH)
    {
        type = SAI_DEBUG_COUNTER_TYPE_SWITCH_IN_DROP_REASONS;
        base_id = SAI_SWITCH_STAT_IN_DROP_REASON_RANGE_BASE;
    }
    else
    {
        return;
    }

    // VPP only keeps ever-increasing counts, so a clear is a new baseline
    // taken from the same dump the cleared values were computed from.
    for (uint32_t i = 0; i < number_of_counters; i++)
    {
        uint32_t id = static_cast<uint32_t>(counter_ids[i]);

        if (id < base_id || id >= base_id + static_cast<uint32_t>(maxDebugCounters))
        {
            continue;
        }

        for (auto &kvp: m_debugCounters)
        {
            auto &counter = kvp.second;

            if (counter.index != id - base_id || counter.type != type)
            {
                continue;
            }

            if (object_type == SAI_OBJECT_TYPE_PORT)
            {
                counter.base[sw_if_index] = debugCounterRawCount(counter, sw_if_index);
            }
            else
            {
                rebaseDebugCounter(counter);
            }
        }
    }
}
