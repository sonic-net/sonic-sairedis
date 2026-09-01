#pragma once

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

#include <map>
#include <string>
#include <vector>

namespace saivs
{
    struct CRMInitValue
    {
        sai_switch_attr_t attr_id;
        uint32_t value;
    };

    class CRMTracker
    {
        public:

            CRMTracker();

            void loadProfileValues(const std::map<std::string, std::string>& profileMap);

            std::vector<CRMInitValue> getInitialValues() const;

            uint32_t getAvailable(sai_switch_attr_t attr_id) const;

            bool handles(sai_switch_attr_t attr_id) const;

            // Route tracking
            void onRouteCreated(bool ipv4);
            void onRouteRemoved(bool ipv4);

            // FDB tracking
            void onFdbCreated();
            void onFdbRemoved();

            // Neighbor tracking
            void onNeighborCreated(bool ipv4);
            void onNeighborRemoved(bool ipv4);

            // Nexthop tracking
            void onNexthopCreated(bool ipv4);
            void onNexthopRemoved(bool ipv4);

            // Nexthop group tracking
            void onNhgCreated();
            void onNhgRemoved();

            // Nexthop group member tracking
            void onNhgMemberCreated();
            void onNhgMemberRemoved();

        private:

            // Configurable limits (defaults from SwitchStateBase)
            uint32_t m_maxIPv4RouteEntries = 100000;
            uint32_t m_maxIPv6RouteEntries = 10000;
            uint32_t m_maxFdbEntries = 800;
            uint32_t m_maxIPv4NeighborEntries = 4000;
            uint32_t m_maxIPv6NeighborEntries = 2000;
            uint32_t m_maxIPv4NextHopEntries = 32000;
            uint32_t m_maxIPv6NextHopEntries = 32000;
            uint32_t m_maxNextHopGroupEntries = 400;
            uint32_t m_maxNextHopGroupMemberEntries = 16000;
            uint32_t m_maxSNATEntries = 100;
            uint32_t m_maxDNATEntries = 100;
            uint32_t m_maxIPMCEntries = 100;
            uint32_t m_maxDoubleNATEntries = 50;

            // Usage counters
            uint32_t m_ipv4RouteCount = 0;
            uint32_t m_ipv6RouteCount = 0;
            uint32_t m_fdbCount = 0;
            uint32_t m_ipv4NeighborCount = 0;
            uint32_t m_ipv6NeighborCount = 0;
            uint32_t m_ipv4NexthopCount = 0;
            uint32_t m_ipv6NexthopCount = 0;
            uint32_t m_nhgCount = 0;
            uint32_t m_nhgMemberCount = 0;
    };
}
