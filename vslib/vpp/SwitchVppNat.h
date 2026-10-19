#pragma once

extern "C" {
#include "sai.h"
}

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>

namespace saivs
{
    class SwitchVpp;

    /**
     * @brief SAI NAT on VPP's nat44-ed plugin.
     *
     * Owns the NAT state of a SwitchVpp: the NAT_ENTRY objects, the router
     * interfaces with a non-zero NAT zone, and the nat44-ed static mappings
     * and outside interfaces they program. SwitchVpp hands it the NAT_ENTRY
     * create/remove/set/get requests, SAI_SWITCH_ATTR_NAT_ENABLE and router
     * interface zone changes. See SwitchVppNat.cpp for how SAI NAT maps onto
     * nat44-ed.
     */
    class SwitchVppNat
    {
        public:

            SwitchVppNat(
                    _In_ SwitchVpp *switch_db);

            sai_status_t createNatEntry(
                    _In_ const std::string &serializedObjectId,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t removeNatEntry(
                    _In_ const std::string &serializedObjectId);

            sai_status_t setNatEntry(
                    _In_ const std::string &serializedObjectId,
                    _In_ const sai_attribute_t *attr);

            sai_status_t getNatEntry(
                    _In_ const std::string &serializedObjectId,
                    _In_ uint32_t attr_count,
                    _Out_ sai_attribute_t *attr_list);

            /**
             * @brief Apply SAI_SWITCH_ATTR_NAT_ENABLE.
             */
            sai_status_t setEnable(
                    _In_ bool enable);

            /**
             * @brief Apply SAI_ROUTER_INTERFACE_ATTR_NAT_ZONE_ID of a router
             * interface, on create or set.
             */
            void setRouterInterfaceZone(
                    _In_ sai_object_id_t rif_id,
                    _In_ uint32_t zone_id);

            /**
             * @brief Drop the NAT state of a router interface before it is
             * removed.
             */
            void removeRouterInterface(
                    _In_ sai_object_id_t rif_id);

            /**
             * @brief Whether VPP runs nat_plugin.so.
             */
            bool isSupported() const;

        private:

            /*
             * One VPP nat44-ed static mapping. A SAI SOURCE_NAT entry and the
             * DESTINATION_NAT entry that undoes it describe the same mapping, so
             * mappings are reference counted by the SAI entries that need them.
             */
            struct NatMapping
            {
                uint32_t local;         // network order
                uint32_t external;      // network order
                uint8_t protocol;       // 0 with addr_only
                uint16_t local_port;    // host order
                uint16_t external_port; // host order
                uint32_t vrf_id;
                bool addr_only;

                bool operator<(const NatMapping& o) const
                {
                    return std::tie(local, external, protocol, local_port, external_port, vrf_id, addr_only) <
                        std::tie(o.local, o.external, o.protocol, o.local_port, o.external_port, o.vrf_id, o.addr_only);
                }
            };

            struct NatSessionKey
            {
                uint32_t inside;
                uint16_t inside_port;
                uint32_t ext_host;
                uint16_t ext_host_port;
                uint16_t protocol;

                bool operator<(const NatSessionKey& o) const
                {
                    return std::tie(inside, inside_port, ext_host, ext_host_port, protocol) <
                        std::tie(o.inside, o.inside_port, o.ext_host, o.ext_host_port, o.protocol);
                }
            };

            /*
             * Monotonic packet/byte counters of a mapping, built from the
             * sessions VPP reports for its local address: totals of sessions
             * that are gone stay in base_*, live sessions are added on read.
             */
            struct NatMappingCounters
            {
                uint64_t base_pkts = 0;
                uint64_t base_bytes = 0;
                std::map<NatSessionKey, std::pair<uint64_t, uint64_t>> live; // pkts, bytes
            };

            struct NatEntryState
            {
                bool has_mapping = false;
                NatMapping mapping;
                // what SAI last read or cleared, to answer SET to 0 and HIT_BIT
                uint64_t zero_pkts = 0;
                uint64_t zero_bytes = 0;
                uint64_t hit_mark_pkts = 0;
            };

            bool entryMapping(
                    _In_ const sai_nat_entry_t &entry,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list,
                    _Out_ NatMapping &mapping);

            bool resolveRifHwif(
                    _In_ sai_object_id_t rif_id,
                    _Out_ std::string &hwif_name);

            void mappingRef(
                    _In_ const std::string &entry_id,
                    _In_ const NatMapping &mapping);

            void mappingUnref(
                    _In_ const std::string &entry_id);

            int programMapping(
                    _In_ const NatMapping &mapping,
                    _In_ bool is_add);

            int programOutside(
                    _In_ const std::string &hwif_name,
                    _In_ bool is_add);

            void readCounters(
                    _In_ const NatMapping &mapping,
                    _Out_ uint64_t &pkts,
                    _Out_ uint64_t &bytes);

        private:

            SwitchVpp *m_switch_db;

            // whether VPP's nat44-ed plugin is enabled; SAI_SWITCH_ATTR_NAT_ENABLE
            // itself is the switch attribute set_internal() stores
            bool m_vpp_enabled = false;

            // router interfaces with a non-zero NAT zone: the VPP nat44 outside
            // (output-feature) interfaces. rif -> {zone, hwif}
            std::map<sai_object_id_t, std::pair<uint32_t, std::string>> m_zone_rifs;

            // mapping -> SAI entries that need it
            std::map<NatMapping, std::set<std::string>> m_mappings;

            std::map<std::string, NatEntryState> m_entries;

            std::map<NatMapping, NatMappingCounters> m_counters;
    };
}
