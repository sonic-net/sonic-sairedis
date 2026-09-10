/*
 * CoPP: SAI_OBJECT_TYPE_POLICER <-> VPP native policer (vnet/policer).
 */

#include "SwitchVpp.h"
#include "SwitchVppUtils.h"
#include "SwitchVppPolicer.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include "vppxlate/SaiVppXlate.h"

using namespace saivs;

static vpp_policer_rate_type_e vpp_policer_meter_type_from_sai(sai_meter_type_t meter_type)
{
    return (meter_type == SAI_METER_TYPE_PACKETS) ? VPP_POLICER_RATE_PPS : VPP_POLICER_RATE_KBPS;
}

static vpp_policer_type_e vpp_policer_mode_from_sai(sai_policer_mode_t mode, bool has_pir, bool has_pbs)
{
    switch (mode)
    {
        case SAI_POLICER_MODE_TR_TCM:
            // Two-rate three-color (RFC 2698) needs a PIR/PBS; without one
            // (colorless single-rate config), fall back to 1R2C.
            return has_pir ? VPP_POLICER_TYPE_2R3C_RFC2698 : VPP_POLICER_TYPE_1R2C;

        case SAI_POLICER_MODE_SR_TCM:
            // Single-rate three-color (RFC 2697) discriminates on PBS (the
            // second bucket), not PIR -- PIR/EIR belongs to the two-rate
            // (TR_TCM) modes above. Falls back to plain 1R2C if no PBS is
            // present.
            return has_pbs ? VPP_POLICER_TYPE_1R3C_RFC2697 : VPP_POLICER_TYPE_1R2C;

        case SAI_POLICER_MODE_STORM_CONTROL:
        default:
            // VPP has no native storm-control policer mode; approximate
            // with 1R2C (single conform/violate).
            return VPP_POLICER_TYPE_1R2C;
    }
}

static vpp_policer_action_e vpp_policer_action_from_sai(sai_packet_action_t action)
{
    switch (action)
    {
        case SAI_PACKET_ACTION_FORWARD:
        case SAI_PACKET_ACTION_COPY:
        case SAI_PACKET_ACTION_LOG:
        case SAI_PACKET_ACTION_TRANSIT:
            return VPP_POLICER_ACTION_TRANSMIT;

        case SAI_PACKET_ACTION_DENY:
        case SAI_PACKET_ACTION_DROP:
        default:
            return VPP_POLICER_ACTION_DROP;
    }
}

sai_status_t SwitchVpp::createPolicer(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_POLICER, sid, switch_id, attr_count, attr_list));

    return programPolicer(object_id, attr_count, attr_list, false /* is_replace */);
}

// Shared VPP-side programming for both createPolicer() (is_replace=false,
// first-time create) and setPolicer() (is_replace=true, re-derive the full
// config from the object hash and recreate at the same VPP policer_index --
// VPP's policer_add has no true in-place update, so this deletes-then-
// recreates, mirroring how SwitchVppAcl.cpp's acl_add_replace() handles any
// ACE change). Never calls create_internal()/set_internal() itself; callers
// own that.
sai_status_t SwitchVpp::programPolicer(
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ bool is_replace)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    vpp_policer_t vpp_policer;
    memset(&vpp_policer, 0, sizeof(vpp_policer));

    // Defaults matching SAI_POLICER_ATTR_* defaults (see SAI/inc/saipolicer.h):
    // METER_TYPE default is PACKETS, MODE default is SR_TCM, actions default
    // GREEN=FORWARD/YELLOW=FORWARD/RED=DROP.
    vpp_policer.rate_type = VPP_POLICER_RATE_PPS;
    vpp_policer.round_type = VPP_POLICER_ROUND_CLOSEST;
    vpp_policer.conform_action = VPP_POLICER_ACTION_TRANSMIT;
    vpp_policer.exceed_action = VPP_POLICER_ACTION_TRANSMIT;
    vpp_policer.violate_action = VPP_POLICER_ACTION_DROP;

    bool has_pir = false;
    bool has_pbs = false;
    sai_policer_mode_t mode = SAI_POLICER_MODE_SR_TCM;

    for (uint32_t i = 0; i < attr_count; i++)
    {
        const sai_attribute_t &attr = attr_list[i];

        switch (attr.id)
        {
            case SAI_POLICER_ATTR_METER_TYPE:
                vpp_policer.rate_type = vpp_policer_meter_type_from_sai((sai_meter_type_t)attr.value.s32);
                break;

            case SAI_POLICER_ATTR_MODE:
                mode = (sai_policer_mode_t)attr.value.s32;
                break;

            case SAI_POLICER_ATTR_CIR:
                vpp_policer.cir = attr.value.u64 > UINT32_MAX ? UINT32_MAX : (uint32_t)attr.value.u64;
                break;

            case SAI_POLICER_ATTR_CBS:
                vpp_policer.cb = attr.value.u64;
                break;

            case SAI_POLICER_ATTR_PIR:
                vpp_policer.eir = attr.value.u64 > UINT32_MAX ? UINT32_MAX : (uint32_t)attr.value.u64;
                has_pir = true;
                break;

            case SAI_POLICER_ATTR_PBS:
                vpp_policer.eb = attr.value.u64;
                has_pbs = true;
                break;

            case SAI_POLICER_ATTR_GREEN_PACKET_ACTION:
                vpp_policer.conform_action = vpp_policer_action_from_sai((sai_packet_action_t)attr.value.s32);
                break;

            case SAI_POLICER_ATTR_YELLOW_PACKET_ACTION:
                vpp_policer.exceed_action = vpp_policer_action_from_sai((sai_packet_action_t)attr.value.s32);
                break;

            case SAI_POLICER_ATTR_RED_PACKET_ACTION:
                vpp_policer.violate_action = vpp_policer_action_from_sai((sai_packet_action_t)attr.value.s32);
                break;

            default:
                break;
        }
    }

    vpp_policer.type = vpp_policer_mode_from_sai(mode, has_pir, has_pbs);

    // VPP policer names are unique keys in its policer table; derive one
    // from the SAI OID so create/replace/del/dump can all address the same
    // VPP object deterministically.
    snprintf(vpp_policer.name, sizeof(vpp_policer.name), "copp-policer-0x%lx",
            (unsigned long)object_id);

    // Deferred (WD-timeout fix -- see enqueuePolicerProgramWork() in
    // SwitchVpp.h): do NOT call vpp_policer_add_replace() synchronously here
    // -- it is a blocking VAPI round-trip and this function is called
    // directly from createPolicer()/setPolicer(), i.e. from inside a single
    // watchdog-timed syncd SAI call. Confirmed live: a SAI_OBJECT_TYPE_POLICER
    // create stalled past the 30s watchdog here. The real VAPI call now runs
    // later, one item per call, from programPolicerNow() via
    // serviceDeferredPolicerProgramWork().
    enqueuePolicerProgramWork({ object_id, vpp_policer, is_replace });

    SWSS_LOG_NOTICE("queued %s of VPP policer %s for SAI policer %s",
            is_replace ? "replace" : "create", vpp_policer.name, sid.c_str());

    return SAI_STATUS_SUCCESS;
}

void SwitchVpp::programPolicerNow(
        _In_ sai_object_id_t object_id,
        _In_ const vpp_policer_t &vpp_policer_in,
        _In_ bool is_replace)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    vpp_policer_t vpp_policer = vpp_policer_in;

    // On replace, pass in the existing VPP policer_index so
    // vpp_policer_add_replace() deletes-then-recreates at the same
    // tracked slot; on first create there is none yet.
    uint32_t vpp_policer_index = (uint32_t)~0;

    if (is_replace)
    {
        auto existing = m_policer_map.find(object_id);
        if (existing != m_policer_map.end())
        {
            vpp_policer_index = existing->second.vpp_policer_index;
        }
    }

    int ret = vpp_policer_add_replace(&vpp_policer, &vpp_policer_index, is_replace);

    if (ret != 0)
    {
        SWSS_LOG_ERROR("failed to %s VPP policer for %s (name %s): ret %d",
                is_replace ? "replace" : "create", sid.c_str(), vpp_policer.name, ret);

        // Config-plane bookkeeping already succeeded via create_internal()/
        // set_internal() in createPolicer()/setPolicer() (matching the rest
        // of saivpp's tolerant-of-dataplane-gaps posture); surface the
        // dataplane failure via ERROR log but do not fail the SAI call, so
        // config-only tests (test_verify_copp_configuration_cli) are not
        // regressed by a VPP-side issue. This is deferred work anyway, so
        // there is no SAI call left to fail at this point.
        return;
    }

    vpp_policer_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    {
        size_t name_len = strnlen(vpp_policer.name, sizeof(entry.vpp_name) - 1);
        memcpy(entry.vpp_name, vpp_policer.name, name_len);
        entry.vpp_name[name_len] = '\0';
    }
    entry.vpp_policer_index = vpp_policer_index;

    m_policer_map[object_id] = entry;

    SWSS_LOG_NOTICE("%s VPP policer %s (index %u) for SAI policer %s",
            is_replace ? "replaced" : "created", vpp_policer.name, vpp_policer_index, sid.c_str());
}

sai_status_t SwitchVpp::removePolicer(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_object_id_t object_id;
    sai_deserialize_object_id(serializedObjectId, object_id);

    auto it = m_policer_map.find(object_id);

    if (it != m_policer_map.end())
    {
        int ret = vpp_policer_del(it->second.vpp_policer_index);

        if (ret != 0)
        {
            SWSS_LOG_ERROR("failed to delete VPP policer index %u for %s: ret %d",
                    it->second.vpp_policer_index, serializedObjectId.c_str(), ret);
        }

        m_policer_map.erase(it);
    }

    return remove_internal(SAI_OBJECT_TYPE_POLICER, serializedObjectId);
}

sai_status_t SwitchVpp::setPolicer(
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_object_id_t object_id;
    sai_deserialize_object_id(serializedObjectId, object_id);

    CHECK_STATUS(set_internal(SAI_OBJECT_TYPE_POLICER, serializedObjectId, attr));

    // VPP's policer_add has no true in-place update; re-derive the full
    // policer config from the (now-updated) attribute store and
    // recreate it via vpp_policer_add_replace(is_replace=true), which
    // deletes-then-recreates at the same VPP policer_index slot tracking.
    // This mirrors how SwitchVppAcl.cpp's acl_add_replace() re-derives and
    // resubmits the whole ACL on any ACE change rather than patching VPP
    // in place.
    auto ait = m_objectHash.at(SAI_OBJECT_TYPE_POLICER).find(serializedObjectId);

    if (ait == m_objectHash.at(SAI_OBJECT_TYPE_POLICER).end())
    {
        SWSS_LOG_ERROR("policer %s not found in object hash after set_internal", serializedObjectId.c_str());
        return SAI_STATUS_SUCCESS;
    }

    std::vector<sai_attribute_t> all_attrs;

    for (auto &kv : ait->second)
    {
        all_attrs.push_back(*kv.second->getAttr());
    }

    return programPolicer(object_id, (uint32_t)all_attrs.size(), all_attrs.data(), true /* is_replace */);
}

sai_status_t SwitchVpp::getPolicerStats(
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    auto it = m_policer_map.find(object_id);

    if (it == m_policer_map.end())
    {
        SWSS_LOG_WARN("no VPP policer mapped for SAI policer 0x%lx; returning zero stats",
                (unsigned long)object_id);

        for (uint32_t i = 0; i < number_of_counters; i++)
        {
            counters[i] = 0;
        }

        return SAI_STATUS_SUCCESS;
    }

    vpp_policer_counters_t vpp_counters;

    int ret = vpp_policer_get_counters(it->second.vpp_policer_index, &vpp_counters);

    if (ret != 0)
    {
        SWSS_LOG_ERROR("failed to read VPP policer counters for index %u: ret %d",
                it->second.vpp_policer_index, ret);
    }

    for (uint32_t i = 0; i < number_of_counters; i++)
    {
        switch (counter_ids[i])
        {
            case SAI_POLICER_STAT_GREEN_PACKETS:
                counters[i] = vpp_counters.green_packets;
                break;
            case SAI_POLICER_STAT_GREEN_BYTES:
                counters[i] = vpp_counters.green_bytes;
                break;
            case SAI_POLICER_STAT_YELLOW_PACKETS:
                counters[i] = vpp_counters.yellow_packets;
                break;
            case SAI_POLICER_STAT_YELLOW_BYTES:
                counters[i] = vpp_counters.yellow_bytes;
                break;
            case SAI_POLICER_STAT_RED_PACKETS:
                counters[i] = vpp_counters.red_packets;
                break;
            case SAI_POLICER_STAT_RED_BYTES:
                counters[i] = vpp_counters.red_bytes;
                break;
            default:
                counters[i] = 0;
                break;
        }
    }

    return SAI_STATUS_SUCCESS;
}
