#include "LaneMap.h"

#include "swss/logger.h"

#include <set>

using namespace saivs;

LaneMap::LaneMap(
        _In_ uint32_t switchIndex):
    m_switchIndex(switchIndex)
{
    SWSS_LOG_ENTER();

    // empty
}

uint32_t LaneMap::getSwitchIndex() const
{
    SWSS_LOG_ENTER();

    return m_switchIndex;
}

bool LaneMap::add(
        _In_ const std::string& ifname,
        _In_ const std::vector<uint32_t>& lanes)
{
    SWSS_LOG_ENTER();

    auto n = lanes.size();

    if (n != 1 && n != 2 && n != 4 && n != 8)
    {
        SWSS_LOG_ERROR("invalid number of lanes (%zu) assigned to interface %s", n, ifname.c_str());
        return false;
    }

    if (m_ifname_to_lanes.find(ifname) != m_ifname_to_lanes.end())
    {
        SWSS_LOG_ERROR("interface %s already in map", ifname.c_str());
        return false;
    }

    std::set<uint32_t> uniq;

    for (auto lane: lanes)
    {
        uniq.insert(lane);

        if (m_lane_to_ifname.find(lane) != m_lane_to_ifname.end())
        {
            SWSS_LOG_ERROR("lane %u already assigned on interface %s", lane, m_lane_to_ifname[lane].c_str());
            return false;
        }
    }

    if (uniq.size() != lanes.size())
    {
        SWSS_LOG_ERROR("lanes are not unique on interface %s", ifname.c_str());
        return false;
    }

    m_laneMap.push_back(lanes);

    for (auto lane: lanes)
    {
        m_lane_to_ifname[lane] = ifname;
    }

    m_ifname_to_lanes[ifname] = lanes;

    return true;
}

bool LaneMap::remove(
        _In_ const std::string& ifname)
{
    SWSS_LOG_ENTER();

    auto it = m_ifname_to_lanes.find(ifname);

    if (it == m_ifname_to_lanes.end())
    {
        SWSS_LOG_ERROR("interfce %s not found in laneMap %u", ifname.c_str(), m_switchIndex);
        return false;
    }

    for (auto lane: it->second)
    {
        m_lane_to_ifname.erase(lane);
    }

    for (size_t idx = 0; idx < m_laneMap.size(); idx++)
    {
        if (m_laneMap[idx][0] == it->second[0])
        {
            m_laneMap.erase(m_laneMap.begin() + idx);
            break;
        }
    }

    m_ifname_to_lanes.erase(it);

    return true;
}

std::shared_ptr<LaneMap> LaneMap::getDefaultLaneMap(
        _In_ uint32_t switchIndex)
{
    SWSS_LOG_ENTER();

    const uint32_t defaultPortCount = 32;
    const uint32_t lanesPerPort = 4;

    uint32_t defaultLanes[defaultPortCount * lanesPerPort] = {
        29,30,31,32,
        25,26,27,28,
        37,38,39,40,
        33,34,35,36,
        41,42,43,44,
        45,46,47,48,
        5,6,7,8,
        1,2,3,4,
        9,10,11,12,
        13,14,15,16,
        21,22,23,24,
        17,18,19,20,
        49,50,51,52,
        53,54,55,56,
        61,62,63,64,
        57,58,59,60,
        65,66,67,68,
        69,70,71,72,
        77,78,79,80,
        73,74,75,76,
        105,106,107,108,
        109,110,111,112,
        117,118,119,120,
        113,114,115,116,
        121,122,123,124,
        125,126,127,128,
        85,86,87,88,
        81,82,83,84,
        89,90,91,92,
        93,94,95,96,
        97,98,99,100,
        101,102,103,104
    };

    auto lm = std::make_shared<LaneMap>(switchIndex);

    for (uint32_t idx = 0; idx < defaultPortCount; idx++)
    {
        auto ifname = "eth" + std::to_string(idx);

        std::vector<uint32_t> lanes;

        for (uint32_t index = 0; index < lanesPerPort; index++)
        {
            lanes.push_back(defaultLanes[idx * lanesPerPort + index]);
        }

        lm->add(ifname, lanes);
    }

    return lm;
}

bool LaneMap::isEmpty() const
{
    SWSS_LOG_ENTER();

    return m_ifname_to_lanes.size() == 0;
}

bool LaneMap::hasInterface(
        _In_ const std::string& ifname) const
{
    SWSS_LOG_ENTER();

    return m_ifname_to_lanes.find(ifname) != m_ifname_to_lanes.end();
}

const std::vector<std::vector<uint32_t>> LaneMap::getLaneVector() const
{
    SWSS_LOG_ENTER();

    return m_laneMap;
}

std::string LaneMap::getInterfaceFromLaneNumber(
        _In_ uint32_t laneNumber) const
{
    SWSS_LOG_ENTER();

    auto it = m_lane_to_ifname.find(laneNumber);

    if (it == m_lane_to_ifname.end())
    {
        SWSS_LOG_WARN("lane %u not found on index %u", laneNumber, m_switchIndex);

        return "";
    }

    return it->second;
}
