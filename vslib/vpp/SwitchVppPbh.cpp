/*
 *------------------------------------------------------------------
 * SwitchVppPbh.cpp
 *
 * Policy Based Hashing (SAI PBH) for the VPP switch.
 *
 * PbhOrch models PBH as an ACL table of its own type plus a set of
 * SAI_OBJECT_TYPE_HASH profiles referenced by the entries' SET_ECMP_HASH_ID
 * and SET_LAG_HASH_ID actions. None of it reaches the VPP ACL plugin: the
 * plugin can neither follow the variable offsets PBH qualifies on nor steer
 * a packet to a chosen load-balance bucket. The sonic_ext plugin's PBH
 * feature does both, so a PBH table is recognised here and routed there.
 *
 * Copyright (c) 2026 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *------------------------------------------------------------------
 */

#include "SwitchVpp.h"
#include "SwitchVppPbh.h"
#include "SwitchVppUtils.h"
#include "SaiObjectDB.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include "vppxlate/SaiVppXlate.h"
#include "vppxlate/SaiAclStats.h"

#include <algorithm>
#include <vector>

using namespace saivs;

#define PBH_INVALID_INDEX ((uint32_t) ~0)

/*
 * SAI_OBJECT_TYPE_FINE_GRAINED_HASH_FIELD carries a sai_native_hash_field_t,
 * most of which PBH cannot express: the sonic_ext profile hashes inner fields
 * only, because the outer header is exactly what PBH exists to look past.
 * An outer or unsupported field is rejected rather than silently dropped --
 * a profile that quietly hashes fewer fields than asked still forwards, so
 * the mistake would only ever show up as an unexplained traffic imbalance.
 */
static sai_status_t pbh_native_field_to_vpp(
        _In_ sai_native_hash_field_t field,
        _Out_ uint8_t &vpp_field)
{
    SWSS_LOG_ENTER();

    switch (field)
    {
        case SAI_NATIVE_HASH_FIELD_INNER_IP_PROTOCOL:
            vpp_field = VPP_PBH_HF_INNER_IP_PROTOCOL;
            break;

        case SAI_NATIVE_HASH_FIELD_INNER_L4_SRC_PORT:
            vpp_field = VPP_PBH_HF_INNER_L4_SRC_PORT;
            break;

        case SAI_NATIVE_HASH_FIELD_INNER_L4_DST_PORT:
            vpp_field = VPP_PBH_HF_INNER_L4_DST_PORT;
            break;

        case SAI_NATIVE_HASH_FIELD_INNER_SRC_IPV4:
            vpp_field = VPP_PBH_HF_INNER_SRC_IPV4;
            break;

        case SAI_NATIVE_HASH_FIELD_INNER_DST_IPV4:
            vpp_field = VPP_PBH_HF_INNER_DST_IPV4;
            break;

        case SAI_NATIVE_HASH_FIELD_INNER_SRC_IPV6:
            vpp_field = VPP_PBH_HF_INNER_SRC_IPV6;
            break;

        case SAI_NATIVE_HASH_FIELD_INNER_DST_IPV6:
            vpp_field = VPP_PBH_HF_INNER_DST_IPV6;
            break;

        default:
            SWSS_LOG_ERROR("Unsupported PBH native hash field %d", field);
            return SAI_STATUS_NOT_SUPPORTED;
    }

    return SAI_STATUS_SUCCESS;
}

SwitchVppPbh::SwitchVppPbh(
        _In_ SwitchVpp *switch_db):
    m_switch_db(switch_db)
{
    SWSS_LOG_ENTER();
}

void SwitchVppPbh::featureQuery(void)
{
    SWSS_LOG_ENTER();

    bool enabled = false;

    if (vpp_sonic_ext_feature_get("pbh", &enabled) != 0)
    {
        SWSS_LOG_NOTICE("sonic-ext feature query failed, PBH disabled");
        m_supported = false;
        return;
    }

    m_supported = enabled;

    SWSS_LOG_NOTICE("PBH %s", m_supported ? "supported" : "not supported");
}

/*
 * A PBH table is told apart by its declared match fields rather than by any
 * SAI attribute that says "PBH", because SAI has no such attribute. GRE_KEY
 * and INNER_ETHER_TYPE together are the signature PbhOrch builds and no
 * ordinary SONiC ACL table asks for; requiring both rather than either keeps
 * a P4Orch table, which uses INNER_ETHER_TYPE alone, out of this path.
 */
bool SwitchVppPbh::isPbhTable(
        _In_ sai_object_id_t tbl_oid)
{
    SWSS_LOG_ENTER();

    if (m_tables.find(tbl_oid) != m_tables.end())
    {
        return true;
    }

    sai_attribute_t attr;

    attr.id = SAI_ACL_TABLE_ATTR_FIELD_GRE_KEY;
    if (m_switch_db->get(SAI_OBJECT_TYPE_ACL_TABLE, tbl_oid, 1, &attr) != SAI_STATUS_SUCCESS ||
        !attr.value.booldata)
    {
        return false;
    }

    attr.id = SAI_ACL_TABLE_ATTR_FIELD_INNER_ETHER_TYPE;
    if (m_switch_db->get(SAI_OBJECT_TYPE_ACL_TABLE, tbl_oid, 1, &attr) != SAI_STATUS_SUCCESS ||
        !attr.value.booldata)
    {
        return false;
    }

    return true;
}

bool SwitchVppPbh::isPbhCounterOid(
        _In_ sai_object_id_t cntr_oid)
{
    SWSS_LOG_ENTER();

    return m_cntr_info_map.find(cntr_oid) != m_cntr_info_map.end();
}

void SwitchVppPbh::forgetTableCounters(
        _In_ sai_object_id_t tbl_oid)
{
    SWSS_LOG_ENTER();

    for (auto it = m_cntr_info_map.begin(); it != m_cntr_info_map.end(); )
    {
        if (it->second.tbl_oid == tbl_oid)
        {
            it = m_cntr_info_map.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

sai_status_t SwitchVppPbh::createHash(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    /*
     * The hash is not in the object store yet, so it is read through a
     * SaiCachedObject over the caller's attribute list. Everything that can
     * fail happens before create_internal(), which leaves a single failure
     * after the commit point instead of a rollback at every step.
     *
     * Only a fine-grained hash is a PBH profile. A hash carrying
     * SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST is the switch-global ECMP/LAG hash
     * and is handled by the existing vpp_ip_flow_hash_set() path, so it is
     * stored and otherwise left alone.
     */
    SaiCachedObject hash_obj(m_switch_db, SAI_OBJECT_TYPE_HASH, sid, attr_count, attr_list);

    sai_attribute_t fg_attr;
    sai_object_id_t fg_list[MAX_OBJLIST_LEN];

    fg_attr.id = SAI_HASH_ATTR_FINE_GRAINED_HASH_FIELD_LIST;
    fg_attr.value.objlist.count = MAX_OBJLIST_LEN;
    fg_attr.value.objlist.list = fg_list;

    if (hash_obj.get_attr(fg_attr) != SAI_STATUS_SUCCESS || fg_attr.value.objlist.count == 0)
    {
        return m_switch_db->create_internal(SAI_OBJECT_TYPE_HASH, sid, switch_id, attr_count, attr_list);
    }

    if (!m_supported)
    {
        if (!m_unsupported_logged)
        {
            SWSS_LOG_NOTICE("PBH is disabled in VPP, refusing fine grained hash %s", sid.c_str());
            m_unsupported_logged = true;
        }

        return SAI_STATUS_NOT_SUPPORTED;
    }

    auto field_objs = hash_obj.get_linked_objects(SAI_OBJECT_TYPE_FINE_GRAINED_HASH_FIELD,
                                                  SAI_HASH_ATTR_FINE_GRAINED_HASH_FIELD_LIST);

    /*
     * get_linked_objects() silently drops object IDs it cannot resolve, so a
     * short result means a field was never created. Hashing on a subset of
     * what was asked for would be worse than refusing.
     */
    if (field_objs.size() != fg_attr.value.objlist.count)
    {
        SWSS_LOG_ERROR("Hash %s names %u fine grained hash fields but only %zu exist",
                       sid.c_str(), fg_attr.value.objlist.count, field_objs.size());
        return SAI_STATUS_FAILURE;
    }

    std::vector<vpp_pbh_hash_field_t> fields;

    for (const auto &field_obj: field_objs)
    {
        vpp_pbh_hash_field_t field = {};
        sai_attribute_t attr;

        attr.id = SAI_FINE_GRAINED_HASH_FIELD_ATTR_NATIVE_HASH_FIELD;
        CHECK_STATUS(field_obj->get_mandatory_attr(attr));

        auto native_field = static_cast<sai_native_hash_field_t>(attr.value.s32);

        CHECK_STATUS(pbh_native_field_to_vpp(native_field, field.field));

        attr.id = SAI_FINE_GRAINED_HASH_FIELD_ATTR_SEQUENCE_ID;
        if (field_obj->get_attr(attr) == SAI_STATUS_SUCCESS)
        {
            field.sequence_id = attr.value.u32;
        }

        /*
         * The mask is carried as 16 wire-order bytes whatever the family, so
         * it cannot disagree with the field it masks. SAI only mandates a
         * mask for the address fields; everything else hashes whole.
         */
        if (native_field == SAI_NATIVE_HASH_FIELD_INNER_SRC_IPV4 ||
            native_field == SAI_NATIVE_HASH_FIELD_INNER_DST_IPV4)
        {
            attr.id = SAI_FINE_GRAINED_HASH_FIELD_ATTR_IPV4_MASK;
            if (field_obj->get_attr(attr) == SAI_STATUS_SUCCESS)
            {
                memcpy(field.mask, &attr.value.ip4, sizeof(attr.value.ip4));
            }
        }
        else if (native_field == SAI_NATIVE_HASH_FIELD_INNER_SRC_IPV6 ||
                 native_field == SAI_NATIVE_HASH_FIELD_INNER_DST_IPV6)
        {
            attr.id = SAI_FINE_GRAINED_HASH_FIELD_ATTR_IPV6_MASK;
            if (field_obj->get_attr(attr) == SAI_STATUS_SUCCESS)
            {
                memcpy(field.mask, attr.value.ip6, sizeof(attr.value.ip6));
            }
        }

        fields.push_back(field);
    }

    uint32_t profile_index = PBH_INVALID_INDEX;

    if (vpp_pbh_profile_add_del(true, fields.data(), (uint32_t) fields.size(), &profile_index) != 0)
    {
        SWSS_LOG_ERROR("PBH profile create failed for hash %s", sid.c_str());
        return SAI_STATUS_FAILURE;
    }

    sai_status_t status = m_switch_db->create_internal(SAI_OBJECT_TYPE_HASH, sid, switch_id, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        /*
         * Nothing can reference this profile and removeHash() will never run
         * for an object that was not stored, so undo it here.
         */
        vpp_pbh_profile_add_del(false, nullptr, 0, &profile_index);
        return status;
    }

    m_profile_map[object_id] = profile_index;

    SWSS_LOG_NOTICE("PBH profile %u created for hash %s with %zu fields",
                    profile_index, sid.c_str(), fields.size());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVppPbh::removeHash(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_object_id_t hash_oid;

    sai_deserialize_object_id(serializedObjectId, hash_oid);

    auto it = m_profile_map.find(hash_oid);

    if (it != m_profile_map.end())
    {
        uint32_t profile_index = it->second;

        if (vpp_pbh_profile_add_del(false, nullptr, 0, &profile_index) != 0)
        {
            SWSS_LOG_ERROR("PBH profile %u remove failed for hash %s", profile_index,
                           serializedObjectId.c_str());
            return SAI_STATUS_FAILURE;
        }

        m_profile_map.erase(it);

        SWSS_LOG_NOTICE("PBH profile %u removed for hash %s", profile_index,
                        serializedObjectId.c_str());
    }

    return m_switch_db->remove_internal(SAI_OBJECT_TYPE_HASH, serializedObjectId);
}

sai_status_t SwitchVppPbh::tableCreate(
        _In_ sai_object_id_t tbl_oid)
{
    SWSS_LOG_ENTER();

    if (!m_supported)
    {
        if (!m_unsupported_logged)
        {
            SWSS_LOG_NOTICE("PBH is disabled in VPP, refusing PBH ACL table %s",
                            sai_serialize_object_id(tbl_oid).c_str());
            m_unsupported_logged = true;
        }

        return SAI_STATUS_NOT_SUPPORTED;
    }

    /*
     * The table is created empty and immediately, rather than on the first
     * entry: PbhOrch binds the table to its interfaces as part of creating
     * it, and an attach needs a table to name. An empty PBH table matches
     * nothing, so it is inert until the rules arrive.
     */
    auto sid = sai_serialize_object_id(tbl_oid);
    char name[64];
    uint32_t table_index = PBH_INVALID_INDEX;

    snprintf(name, sizeof(name), "sonic_pbh_%s", sid.c_str());

    if (vpp_pbh_table_add_replace(name, nullptr, 0, &table_index) != 0)
    {
        SWSS_LOG_ERROR("PBH table create failed for %s", sid.c_str());
        return SAI_STATUS_FAILURE;
    }

    m_tables.insert(tbl_oid);
    m_table_index_map[tbl_oid] = table_index;

    SWSS_LOG_NOTICE("PBH table %s created with index %u", sid.c_str(), table_index);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVppPbh::tableRemove(
        _In_ sai_object_id_t tbl_oid)
{
    SWSS_LOG_ENTER();

    auto it = m_switch_db->m_acl_tbl_rules_map.find(tbl_oid);

    if (it != m_switch_db->m_acl_tbl_rules_map.end())
    {
        m_switch_db->m_acl_tbl_rules_map.erase(it);
    }

    forgetTableCounters(tbl_oid);

    m_tables.erase(tbl_oid);

    auto idx_it = m_table_index_map.find(tbl_oid);

    if (idx_it == m_table_index_map.end())
    {
        SWSS_LOG_WARN("No PBH table configured for %s",
                      sai_serialize_object_id(tbl_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    uint32_t table_index = idx_it->second;

    m_table_index_map.erase(idx_it);

    if (vpp_pbh_table_del(table_index) != 0)
    {
        SWSS_LOG_ERROR("PBH table %s remove failed, index %u",
                       sai_serialize_object_id(tbl_oid).c_str(), table_index);
        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_NOTICE("PBH table %s removed, index %u",
                    sai_serialize_object_id(tbl_oid).c_str(), table_index);

    return SAI_STATUS_SUCCESS;
}

/*
 * Resolve the PBH profile a SET_*_HASH_ID action names.
 *
 * A missing profile is an error rather than "no action": the rule would then
 * match and hash on nothing, which looks identical to PBH working while the
 * traffic takes the switch-global hash.
 */
static sai_status_t pbh_resolve_profile(
        _In_ const std::map<sai_object_id_t, uint32_t> &profile_map,
        _In_ sai_object_id_t hash_oid,
        _Out_ uint32_t &profile_index)
{
    SWSS_LOG_ENTER();

    auto it = profile_map.find(hash_oid);

    if (it == profile_map.end())
    {
        SWSS_LOG_ERROR("PBH profile for hash %s not found",
                       sai_serialize_object_id(hash_oid).c_str());
        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    profile_index = it->second;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVppPbh::tableConfig(
        _In_ sai_object_id_t tbl_oid)
{
    SWSS_LOG_ENTER();

    auto idx_it = m_table_index_map.find(tbl_oid);

    if (idx_it == m_table_index_map.end())
    {
        SWSS_LOG_ERROR("No PBH table configured for %s",
                       sai_serialize_object_id(tbl_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    uint32_t table_index = idx_it->second;

    std::list<sai_object_id_t> entries;

    auto rules_it = m_switch_db->m_acl_tbl_rules_map.find(tbl_oid);

    if (rules_it != m_switch_db->m_acl_tbl_rules_map.end())
    {
        entries = rules_it->second;
    }

    std::vector<vpp_pbh_rule_t> rules;
    std::vector<sai_object_id_t> rule_counters;
    std::vector<sai_object_id_t> rule_entries;

    for (auto entry_oid: entries)
    {
        /*
         * removeAclEntry() drops the entry from m_acl_tbl_rules_map before it
         * calls remove_internal(), so every OID listed here is still in the
         * store. A miss is a bookkeeping bug, and building a rule out of the
         * resulting defaults would install a match-all with no action.
         */
        auto entry_obj = m_switch_db->get_sai_object(SAI_OBJECT_TYPE_ACL_ENTRY,
                                                     sai_serialize_object_id(entry_oid));

        if (!entry_obj)
        {
            SWSS_LOG_ERROR("PBH table %s lists ACL entry %s which is not in the object store",
                           sai_serialize_object_id(tbl_oid).c_str(),
                           sai_serialize_object_id(entry_oid).c_str());
            return SAI_STATUS_FAILURE;
        }

        vpp_pbh_rule_t rule = {};
        sai_attribute_t attr;

        rule.ecmp_profile = PBH_INVALID_INDEX;
        rule.lag_profile = PBH_INVALID_INDEX;

        /*
         * The rule id is the low half of the entry OID. It is only ever shown
         * by `show sonic-ext pbh` and used to break priority ties, so it needs
         * to be stable per entry rather than globally meaningful.
         */
        rule.rule_id = (uint32_t) (entry_oid & 0xffffffff);

        attr.id = SAI_ACL_ENTRY_ATTR_PRIORITY;
        if (entry_obj->get_attr(attr) == SAI_STATUS_SUCCESS)
        {
            rule.priority = attr.value.u32;
        }

        attr.id = SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE;
        if (entry_obj->get_attr(attr) == SAI_STATUS_SUCCESS &&
            attr.value.aclfield.enable)
        {
            rule.qualifiers |= VPP_PBH_Q_ETHER_TYPE;
            rule.ether_type = attr.value.aclfield.data.u16;
        }

        attr.id = SAI_ACL_ENTRY_ATTR_FIELD_IP_PROTOCOL;
        if (entry_obj->get_attr(attr) == SAI_STATUS_SUCCESS &&
            attr.value.aclfield.enable)
        {
            rule.qualifiers |= VPP_PBH_Q_IP_PROTOCOL;
            rule.ip_protocol = attr.value.aclfield.data.u8;
        }

        attr.id = SAI_ACL_ENTRY_ATTR_FIELD_IPV6_NEXT_HEADER;
        if (entry_obj->get_attr(attr) == SAI_STATUS_SUCCESS &&
            attr.value.aclfield.enable)
        {
            rule.qualifiers |= VPP_PBH_Q_IPV6_NEXT_HEADER;
            rule.ipv6_next_header = attr.value.aclfield.data.u8;
        }

        attr.id = SAI_ACL_ENTRY_ATTR_FIELD_L4_DST_PORT;
        if (entry_obj->get_attr(attr) == SAI_STATUS_SUCCESS &&
            attr.value.aclfield.enable)
        {
            rule.qualifiers |= VPP_PBH_Q_L4_DST_PORT;
            rule.l4_dst_port = attr.value.aclfield.data.u16;
        }

        attr.id = SAI_ACL_ENTRY_ATTR_FIELD_INNER_ETHER_TYPE;
        if (entry_obj->get_attr(attr) == SAI_STATUS_SUCCESS &&
            attr.value.aclfield.enable)
        {
            rule.qualifiers |= VPP_PBH_Q_INNER_ETHER_TYPE;
            rule.inner_ether_type = attr.value.aclfield.data.u16;
        }

        attr.id = SAI_ACL_ENTRY_ATTR_FIELD_GRE_KEY;
        if (entry_obj->get_attr(attr) == SAI_STATUS_SUCCESS &&
            attr.value.aclfield.enable)
        {
            rule.qualifiers |= VPP_PBH_Q_GRE_KEY;
            rule.gre_key = attr.value.aclfield.data.u32;
            rule.gre_key_mask = attr.value.aclfield.mask.u32;
        }

        attr.id = SAI_ACL_ENTRY_ATTR_ACTION_SET_ECMP_HASH_ID;
        if (entry_obj->get_attr(attr) == SAI_STATUS_SUCCESS &&
            attr.value.aclaction.enable)
        {
            CHECK_STATUS(pbh_resolve_profile(m_profile_map,
                                             attr.value.aclaction.parameter.oid,
                                             rule.ecmp_profile));
        }

        attr.id = SAI_ACL_ENTRY_ATTR_ACTION_SET_LAG_HASH_ID;
        if (entry_obj->get_attr(attr) == SAI_STATUS_SUCCESS &&
            attr.value.aclaction.enable)
        {
            CHECK_STATUS(pbh_resolve_profile(m_profile_map,
                                             attr.value.aclaction.parameter.oid,
                                             rule.lag_profile));
        }

        sai_object_id_t cntr_oid = SAI_NULL_OBJECT_ID;

        attr.id = SAI_ACL_ENTRY_ATTR_ACTION_COUNTER;
        if (entry_obj->get_attr(attr) == SAI_STATUS_SUCCESS &&
            attr.value.aclaction.enable)
        {
            cntr_oid = attr.value.aclaction.parameter.oid;
            rule.flow_counter = true;
        }

        rules.push_back(rule);
        rule_counters.push_back(cntr_oid);
        rule_entries.push_back(entry_oid);
    }

    /*
     * The plugin re-sorts the rules by descending priority, breaking ties on
     * rule_id, and indexes its per-rule counters by the resulting position.
     * Sort identically here so the counter index recorded below names the same
     * rule the dataplane is counting. Changing either comparator alone would
     * leave the counters reading a neighbouring rule, which is the kind of
     * fault that never fails a test, only reports the wrong number.
     */
    std::vector<uint32_t> order(rules.size());

    for (uint32_t i = 0; i < order.size(); i++)
    {
        order[i] = i;
    }

    std::stable_sort(order.begin(), order.end(),
                     [&rules](uint32_t a, uint32_t b) {
                         if (rules[a].priority != rules[b].priority)
                         {
                             return rules[a].priority > rules[b].priority;
                         }
                         return rules[a].rule_id < rules[b].rule_id;
                     });

    std::vector<vpp_pbh_rule_t> sorted_rules;
    std::vector<sai_object_id_t> sorted_counters;
    std::vector<sai_object_id_t> sorted_entries;

    sorted_rules.reserve(rules.size());
    sorted_counters.reserve(rules.size());
    sorted_entries.reserve(rules.size());

    for (auto i: order)
    {
        sorted_rules.push_back(rules[i]);
        sorted_counters.push_back(rule_counters[i]);
        sorted_entries.push_back(rule_entries[i]);
    }

    if (vpp_pbh_table_add_replace(nullptr, sorted_rules.data(),
                                  (uint32_t) sorted_rules.size(), &table_index) != 0)
    {
        SWSS_LOG_ERROR("PBH table %s program failed, index %u, %zu rules",
                       sai_serialize_object_id(tbl_oid).c_str(), table_index,
                       sorted_rules.size());
        return SAI_STATUS_FAILURE;
    }

    /*
     * Only now that VPP holds this rule set do the counter positions become
     * true, so the map is rebuilt here rather than while the rules were being
     * assembled.
     */
    forgetTableCounters(tbl_oid);

    for (uint32_t i = 0; i < sorted_counters.size(); i++)
    {
        if (sorted_counters[i] == SAI_NULL_OBJECT_ID)
        {
            continue;
        }

        m_cntr_info_map[sorted_counters[i]] = {
            tbl_oid, sorted_entries[i], table_index, i
        };
    }

    SWSS_LOG_NOTICE("PBH table %s programmed, index %u, %zu rules",
                    sai_serialize_object_id(tbl_oid).c_str(), table_index,
                    sorted_rules.size());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVppPbh::bindUnbindPort(
        _In_ const std::string &hwif_name,
        _In_ sai_object_id_t tbl_oid,
        _In_ bool is_bind)
{
    SWSS_LOG_ENTER();

    auto idx_it = m_table_index_map.find(tbl_oid);

    if (idx_it == m_table_index_map.end())
    {
        SWSS_LOG_ERROR("No PBH table configured for %s",
                       sai_serialize_object_id(tbl_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    uint32_t table_index = idx_it->second;

    if (vpp_pbh_interface_attach_detach(hwif_name.c_str(), table_index, is_bind) != 0)
    {
        SWSS_LOG_ERROR("PBH table index %u %s failed for port %s", table_index,
                       is_bind ? "attach" : "detach", hwif_name.c_str());
        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_NOTICE("PBH table index %u %s port %s", table_index,
                    is_bind ? "attached to" : "detached from", hwif_name.c_str());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVppPbh::getEntryStats(
        _In_ sai_object_id_t cntr_oid,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto it = m_cntr_info_map.find(cntr_oid);

    if (it == m_cntr_info_map.end())
    {
        SWSS_LOG_WARN("PBH counter %s not found", sai_serialize_object_id(cntr_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    const auto &info = it->second;
    vpp_ace_stats_t stats;

    /*
     * One SAI entry is one VPP rule, so unlike the ACL path this is a single
     * query rather than a sum over a span.
     */
    if (vpp_rule_stats_query(VPP_RULE_STATS_PBH, info.table_index, info.rule_index, &stats) != 0)
    {
        SWSS_LOG_WARN("PBH stats query failed for table %u rule %u",
                      info.table_index, info.rule_index);
        return SAI_STATUS_FAILURE;
    }

    for (uint32_t i = 0; i < attr_count; i++)
    {
        if (attr_list[i].id == SAI_ACL_COUNTER_ATTR_PACKETS)
        {
            attr_list[i].value.u64 = stats.packets;
        }
        else if (attr_list[i].id == SAI_ACL_COUNTER_ATTR_BYTES)
        {
            attr_list[i].value.u64 = stats.bytes;
        }
    }

    SWSS_LOG_INFO("PBH counter %s: table %u rule %u packets %lu bytes %lu",
                  sai_serialize_object_id(cntr_oid).c_str(), info.table_index,
                  info.rule_index, stats.packets, stats.bytes);

    return SAI_STATUS_SUCCESS;
}
