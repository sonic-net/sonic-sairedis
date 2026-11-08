#pragma once

extern "C" {
#include "sai.h"
}

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace saivs
{
    class SaiAttrWrap;
    class SwitchVpp;

    class SwitchVppTrim
    {
        public:

            SwitchVppTrim(
                    _In_ SwitchVpp *switch_db);

            static bool isTrimDataplaneAttr(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_attr_id_t attr_id);

            static bool getTrimEnumValuesCapability(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_attr_id_t attr_id,
                    _Out_ sai_int32_t *value);

            sai_status_t queryAttrEnumValuesCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _In_ sai_attr_id_t attr_id,
                    _Inout_ sai_s32_list_t *enum_values_capability);

            sai_status_t queryStatsCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _Inout_ sai_stat_capability_list_t *stats_capability);

            void restoreTrimPolicy();

            sai_status_t createSwitchTrimPolicy(
                    _In_ const std::string &serializedObjectId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            void refreshAfterCreate(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string &serializedObjectId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            // Temporarily expose the candidate attribute to the resolver.
            // SwitchVpp commits attributes and relationships only on success.
            sai_status_t set(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string &serializedObjectId,
                    _In_ const std::shared_ptr<SaiAttrWrap> &a);

        private:

            struct TrimGlobalPolicy
            {
                bool     enabled    = false;   // derived from trim_size != 0
                uint16_t trim_size  = 0;
                uint8_t  dscp_mode  = 0;        // 0 = DSCP_VALUE, 1 = FROM_TC
                uint8_t  dscp_value = 0;
                uint8_t  tc_value   = 0;
                uint8_t  trim_queue = 0;
            };

            static sai_status_t updateTrimPolicy(
                    _Inout_ TrimGlobalPolicy &policy,
                    _In_ const sai_attribute_t *attr);

            // Push the full global policy, then admission and classifier state.
            // A disabled policy clears all slots without retained queue metadata.
            sai_status_t programTrimGlobal();

            sai_status_t refreshTrimDataplaneOnChange(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            // Dynamically created ports live in the authoritative object store,
            // not the switch's cached port list.
            std::vector<sai_object_id_t> getTrimPortList() const;

            sai_status_t resolveTrimPort(
                    _In_ sai_object_id_t port_oid,
                    _Out_ std::string &hwif);

            sai_status_t refreshTrimDataplane();

            void buildQueueSchedulerGroupMap(
                    _Out_ std::unordered_map<sai_object_id_t, sai_object_id_t> &queueToSg);

            sai_status_t refreshTrimQueue(
                    _In_ const std::string &hwif_name,
                    _In_ sai_object_id_t queue_oid,
                    _In_ const std::unordered_map<sai_object_id_t, sai_object_id_t> &queueToSg);

            sai_status_t refreshTrimDscpToQueueMap();

            SwitchVpp *m_switch_db;
            TrimGlobalPolicy m_trim_policy;

            // A failed refresh may have installed only part of the queue state.
            // Keep cleanup armed until a disabled sweep succeeds.
            bool m_trim_dataplane_armed = false;

            // Relevant SETs and switch CREATE reconcile pending state; there
            // is no timer/backoff or guaranteed automatic caller retry.
            bool m_trim_reconcile_pending = false;
    };
}
