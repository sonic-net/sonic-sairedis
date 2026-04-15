#include "CRMTracker.h"

using namespace saivs;

CRMTracker::CRMTracker()
{
    SWSS_LOG_ENTER();
}

void CRMTracker::loadProfileValues(
        const std::map<std::string, std::string>& profileMap)
{
    SWSS_LOG_ENTER();

    if (profileMap.empty())
    {
        SWSS_LOG_NOTICE("CRM: profile map is empty, using defaults");
        return;
    }

    static const std::pair<const char*, uint32_t CRMTracker::*> keys[] = {
        { "SAI_VPP_MAX_IPV4_ROUTE_ENTRIES",             &CRMTracker::m_maxIPv4RouteEntries },
        { "SAI_VPP_MAX_IPV6_ROUTE_ENTRIES",             &CRMTracker::m_maxIPv6RouteEntries },
        { "SAI_VPP_MAX_FDB_ENTRIES",                    &CRMTracker::m_maxFdbEntries },
        { "SAI_VPP_MAX_IPV4_NEIGHBOR_ENTRIES",          &CRMTracker::m_maxIPv4NeighborEntries },
        { "SAI_VPP_MAX_IPV6_NEIGHBOR_ENTRIES",          &CRMTracker::m_maxIPv6NeighborEntries },
        { "SAI_VPP_MAX_IPV4_NEXTHOP_ENTRIES",           &CRMTracker::m_maxIPv4NextHopEntries },
        { "SAI_VPP_MAX_IPV6_NEXTHOP_ENTRIES",           &CRMTracker::m_maxIPv6NextHopEntries },
        { "SAI_VPP_MAX_NEXTHOP_GROUP_ENTRIES",          &CRMTracker::m_maxNextHopGroupEntries },
        { "SAI_VPP_MAX_NEXTHOP_GROUP_MEMBER_ENTRIES",   &CRMTracker::m_maxNextHopGroupMemberEntries },
    };

    for (const auto& kv : keys)
    {
        auto it = profileMap.find(kv.first);
        if (it != profileMap.end())
        {
            this->*(kv.second) = (uint32_t)std::stoul(it->second);
        }
    }

    SWSS_LOG_NOTICE("CRM: profile loaded (IPv4Route=%u, IPv6Route=%u, FDB=%u, "
        "IPv4Nbr=%u, IPv6Nbr=%u, IPv4NH=%u, IPv6NH=%u, NHG=%u, NHGMbr=%u)",
        m_maxIPv4RouteEntries, m_maxIPv6RouteEntries, m_maxFdbEntries,
        m_maxIPv4NeighborEntries, m_maxIPv6NeighborEntries,
        m_maxIPv4NextHopEntries, m_maxIPv6NextHopEntries,
        m_maxNextHopGroupEntries, m_maxNextHopGroupMemberEntries);
}

std::vector<CRMInitValue> CRMTracker::getInitialValues() const
{
    SWSS_LOG_ENTER();

    return {
        { SAI_SWITCH_ATTR_AVAILABLE_IPV4_ROUTE_ENTRY,              m_maxIPv4RouteEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_IPV6_ROUTE_ENTRY,              m_maxIPv6RouteEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_FDB_ENTRY,                     m_maxFdbEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_IPV4_NEIGHBOR_ENTRY,           m_maxIPv4NeighborEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_IPV6_NEIGHBOR_ENTRY,           m_maxIPv6NeighborEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_IPV4_NEXTHOP_ENTRY,            m_maxIPv4NextHopEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_IPV6_NEXTHOP_ENTRY,            m_maxIPv6NextHopEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_NEXT_HOP_GROUP_ENTRY,          m_maxNextHopGroupEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_NEXT_HOP_GROUP_MEMBER_ENTRY,   m_maxNextHopGroupMemberEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_SNAT_ENTRY,                     m_maxSNATEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_DNAT_ENTRY,                     m_maxDNATEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_IPMC_ENTRY,                     m_maxIPMCEntries },
        { SAI_SWITCH_ATTR_AVAILABLE_DOUBLE_NAT_ENTRY,               m_maxDoubleNATEntries },
    };
}

bool CRMTracker::handles(sai_switch_attr_t attr_id) const
{
    SWSS_LOG_ENTER();

    switch (attr_id)
    {
        case SAI_SWITCH_ATTR_AVAILABLE_IPV4_ROUTE_ENTRY:
        case SAI_SWITCH_ATTR_AVAILABLE_IPV6_ROUTE_ENTRY:
        case SAI_SWITCH_ATTR_AVAILABLE_FDB_ENTRY:
        case SAI_SWITCH_ATTR_AVAILABLE_IPV4_NEIGHBOR_ENTRY:
        case SAI_SWITCH_ATTR_AVAILABLE_IPV6_NEIGHBOR_ENTRY:
        case SAI_SWITCH_ATTR_AVAILABLE_IPV4_NEXTHOP_ENTRY:
        case SAI_SWITCH_ATTR_AVAILABLE_IPV6_NEXTHOP_ENTRY:
        case SAI_SWITCH_ATTR_AVAILABLE_NEXT_HOP_GROUP_ENTRY:
        case SAI_SWITCH_ATTR_AVAILABLE_NEXT_HOP_GROUP_MEMBER_ENTRY:
        case SAI_SWITCH_ATTR_AVAILABLE_SNAT_ENTRY:
        case SAI_SWITCH_ATTR_AVAILABLE_DNAT_ENTRY:
        case SAI_SWITCH_ATTR_AVAILABLE_IPMC_ENTRY:
        case SAI_SWITCH_ATTR_AVAILABLE_DOUBLE_NAT_ENTRY:
            return true;
        default:
            return false;
    }
}

uint32_t CRMTracker::getAvailable(sai_switch_attr_t attr_id) const
{
    SWSS_LOG_ENTER();

    uint32_t max_val = 0;
    uint32_t used = 0;

    switch (attr_id)
    {
        case SAI_SWITCH_ATTR_AVAILABLE_IPV4_ROUTE_ENTRY:
            max_val = m_maxIPv4RouteEntries; used = m_ipv4RouteCount; break;
        case SAI_SWITCH_ATTR_AVAILABLE_IPV6_ROUTE_ENTRY:
            max_val = m_maxIPv6RouteEntries; used = m_ipv6RouteCount; break;
        case SAI_SWITCH_ATTR_AVAILABLE_FDB_ENTRY:
            max_val = m_maxFdbEntries; used = m_fdbCount; break;
        case SAI_SWITCH_ATTR_AVAILABLE_IPV4_NEIGHBOR_ENTRY:
            max_val = m_maxIPv4NeighborEntries; used = m_ipv4NeighborCount; break;
        case SAI_SWITCH_ATTR_AVAILABLE_IPV6_NEIGHBOR_ENTRY:
            max_val = m_maxIPv6NeighborEntries; used = m_ipv6NeighborCount; break;
        case SAI_SWITCH_ATTR_AVAILABLE_IPV4_NEXTHOP_ENTRY:
            max_val = m_maxIPv4NextHopEntries; used = m_ipv4NexthopCount; break;
        case SAI_SWITCH_ATTR_AVAILABLE_IPV6_NEXTHOP_ENTRY:
            max_val = m_maxIPv6NextHopEntries; used = m_ipv6NexthopCount; break;
        case SAI_SWITCH_ATTR_AVAILABLE_NEXT_HOP_GROUP_ENTRY:
            max_val = m_maxNextHopGroupEntries; used = m_nhgCount; break;
        case SAI_SWITCH_ATTR_AVAILABLE_NEXT_HOP_GROUP_MEMBER_ENTRY:
            max_val = m_maxNextHopGroupMemberEntries; used = m_nhgMemberCount; break;
        case SAI_SWITCH_ATTR_AVAILABLE_SNAT_ENTRY:
            max_val = m_maxSNATEntries; used = 0; break;
        case SAI_SWITCH_ATTR_AVAILABLE_DNAT_ENTRY:
            max_val = m_maxDNATEntries; used = 0; break;
        case SAI_SWITCH_ATTR_AVAILABLE_IPMC_ENTRY:
            max_val = m_maxIPMCEntries; used = 0; break;
        case SAI_SWITCH_ATTR_AVAILABLE_DOUBLE_NAT_ENTRY:
            max_val = m_maxDoubleNATEntries; used = 0; break;
        default:
            SWSS_LOG_WARN("CRM: getAvailable called with unhandled attr %d", attr_id);
            return 0;
    }

    return (max_val > used) ? (max_val - used) : 0;
}

// Route tracking

void CRMTracker::onRouteCreated(bool ipv4)
{
    SWSS_LOG_ENTER();

    if (ipv4)
    {
        m_ipv4RouteCount++;
        SWSS_LOG_DEBUG("CRM: IPv4 route created, count: %u", m_ipv4RouteCount);
    }
    else
    {
        m_ipv6RouteCount++;
        SWSS_LOG_DEBUG("CRM: IPv6 route created, count: %u", m_ipv6RouteCount);
    }
}

void CRMTracker::onRouteRemoved(bool ipv4)
{
    SWSS_LOG_ENTER();

    if (ipv4)
    {
        if (m_ipv4RouteCount > 0) m_ipv4RouteCount--;
        SWSS_LOG_DEBUG("CRM: IPv4 route removed, count: %u", m_ipv4RouteCount);
    }
    else
    {
        if (m_ipv6RouteCount > 0) m_ipv6RouteCount--;
        SWSS_LOG_DEBUG("CRM: IPv6 route removed, count: %u", m_ipv6RouteCount);
    }
}

// FDB tracking

void CRMTracker::onFdbCreated()
{
    SWSS_LOG_ENTER();

    m_fdbCount++;
    SWSS_LOG_DEBUG("CRM: FDB entry created, count: %u", m_fdbCount);
}

void CRMTracker::onFdbRemoved()
{
    SWSS_LOG_ENTER();

    if (m_fdbCount > 0) m_fdbCount--;
    SWSS_LOG_DEBUG("CRM: FDB entry removed, count: %u", m_fdbCount);
}

// Neighbor tracking

void CRMTracker::onNeighborCreated(bool ipv4)
{
    SWSS_LOG_ENTER();

    if (ipv4)
    {
        m_ipv4NeighborCount++;
        SWSS_LOG_DEBUG("CRM: IPv4 neighbor created, count: %u", m_ipv4NeighborCount);
    }
    else
    {
        m_ipv6NeighborCount++;
        SWSS_LOG_DEBUG("CRM: IPv6 neighbor created, count: %u", m_ipv6NeighborCount);
    }
}

void CRMTracker::onNeighborRemoved(bool ipv4)
{
    SWSS_LOG_ENTER();

    if (ipv4)
    {
        if (m_ipv4NeighborCount > 0) m_ipv4NeighborCount--;
        SWSS_LOG_DEBUG("CRM: IPv4 neighbor removed, count: %u", m_ipv4NeighborCount);
    }
    else
    {
        if (m_ipv6NeighborCount > 0) m_ipv6NeighborCount--;
        SWSS_LOG_DEBUG("CRM: IPv6 neighbor removed, count: %u", m_ipv6NeighborCount);
    }
}

// Nexthop tracking

void CRMTracker::onNexthopCreated(bool ipv4)
{
    SWSS_LOG_ENTER();

    if (ipv4)
    {
        m_ipv4NexthopCount++;
        SWSS_LOG_DEBUG("CRM: IPv4 nexthop created, count: %u", m_ipv4NexthopCount);
    }
    else
    {
        m_ipv6NexthopCount++;
        SWSS_LOG_DEBUG("CRM: IPv6 nexthop created, count: %u", m_ipv6NexthopCount);
    }
}

void CRMTracker::onNexthopRemoved(bool ipv4)
{
    SWSS_LOG_ENTER();

    if (ipv4)
    {
        if (m_ipv4NexthopCount > 0) m_ipv4NexthopCount--;
        SWSS_LOG_DEBUG("CRM: IPv4 nexthop removed, count: %u", m_ipv4NexthopCount);
    }
    else
    {
        if (m_ipv6NexthopCount > 0) m_ipv6NexthopCount--;
        SWSS_LOG_DEBUG("CRM: IPv6 nexthop removed, count: %u", m_ipv6NexthopCount);
    }
}

// Nexthop group tracking

void CRMTracker::onNhgCreated()
{
    SWSS_LOG_ENTER();

    m_nhgCount++;
    SWSS_LOG_DEBUG("CRM: NHG created, count: %u", m_nhgCount);
}

void CRMTracker::onNhgRemoved()
{
    SWSS_LOG_ENTER();

    if (m_nhgCount > 0) m_nhgCount--;
    SWSS_LOG_DEBUG("CRM: NHG removed, count: %u", m_nhgCount);
}

// Nexthop group member tracking

void CRMTracker::onNhgMemberCreated()
{
    SWSS_LOG_ENTER();

    m_nhgMemberCount++;
    SWSS_LOG_DEBUG("CRM: NHG member created, count: %u", m_nhgMemberCount);
}

void CRMTracker::onNhgMemberRemoved()
{
    SWSS_LOG_ENTER();

    if (m_nhgMemberCount > 0) m_nhgMemberCount--;
    SWSS_LOG_DEBUG("CRM: NHG member removed, count: %u", m_nhgMemberCount);
}
