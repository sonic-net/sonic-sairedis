#include "NetMsgRegistrar.h"
#include "SwitchState.h"
#include "RealObjectIdManager.h"
#include "SwitchStateBase.h"
#include "RealObjectIdManager.h"
#include "EventPayloadNetLinkMsg.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"

#include <netlink/route/link.h>
#include <netlink/route/addr.h>
#include <linux/if.h>

using namespace saivs;

#define VS_COUNTERS_COUNT_MSB (0x80000000)

const std::map<sai_stat_id_t, std::string> SwitchState::m_statIdMap =
{
        { SAI_PORT_STAT_IF_IN_OCTETS, "rx_bytes" },
        { SAI_PORT_STAT_IF_IN_UCAST_PKTS, "rx_packets" },
        { SAI_PORT_STAT_IF_IN_ERRORS, "rx_errors" },
        { SAI_PORT_STAT_IF_IN_DISCARDS, "rx_dropped" },
        { SAI_PORT_STAT_IF_OUT_OCTETS, "tx_bytes" },
        { SAI_PORT_STAT_IF_OUT_UCAST_PKTS, "tx_packets" },
        { SAI_PORT_STAT_IF_OUT_ERRORS, "tx_errors" },
        { SAI_PORT_STAT_IF_OUT_DISCARDS, "tx_dropped" }
};

SwitchState::SwitchState(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<SwitchConfig> config):
    m_switch_id(switch_id),
    m_linkCallbackIndex(-1),
    m_switchConfig(config)
{
    SWSS_LOG_ENTER();

    if (RealObjectIdManager::objectTypeQuery(switch_id) != SAI_OBJECT_TYPE_SWITCH)
    {
        SWSS_LOG_THROW("object %s is not SWITCH, its %s",
                sai_serialize_object_id(switch_id).c_str(),
                sai_serialize_object_type(RealObjectIdManager::objectTypeQuery(switch_id)).c_str());
    }

    for (size_t i = 0; i < sai_metadata_enum_sai_object_type_t.valuescount; ++i)
    {
        /*
         * Populate empty maps for each object to avoid checking if
         * objecttype exists.
         */

        m_objectHash[(sai_object_type_t)sai_metadata_enum_sai_object_type_t.values[i]] = { };
    }

    /*
     * Create switch by default, it will require special treat on
     * creating.
     */

    m_objectHash[SAI_OBJECT_TYPE_SWITCH][sai_serialize_object_id(switch_id)] = {};

    if (m_switchConfig->m_useTapDevice)
    {
        m_linkCallbackIndex = NetMsgRegistrar::getInstance().registerCallback(
                std::bind(&SwitchState::asyncOnLinkMsg, this, std::placeholders::_1, std::placeholders::_2));
    }

    if (m_switchConfig->m_resourceLimiter)
    {
        SWSS_LOG_NOTICE("resource limiter is SET on switch %s",
                sai_serialize_object_id(switch_id).c_str());
    }
}

SwitchState::~SwitchState()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("begin");

    if (m_switchConfig->m_useTapDevice)
    {
        NetMsgRegistrar::getInstance().unregisterCallback(m_linkCallbackIndex);

        // if unregister ended then no new messages will arrive for this class
        // so there is no need to protect this using mutex
    }

    SWSS_LOG_NOTICE("switch %s",
            sai_serialize_object_id(m_switch_id).c_str());

    SWSS_LOG_NOTICE("end");
}

void SwitchState::setMeta(
        std::weak_ptr<saimeta::Meta> meta)
{
    SWSS_LOG_ENTER();

    m_meta = meta;
}

sai_object_id_t SwitchState::getSwitchId() const
{
    SWSS_LOG_ENTER();

    return m_switch_id;
}

void SwitchState::setIfNameToPortId(
        _In_ const std::string& ifname,
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    m_ifname_to_port_id_map[ifname] = port_id;
}


void SwitchState::removeIfNameToPortId(
        _In_ const std::string& ifname)
{
    SWSS_LOG_ENTER();

    m_ifname_to_port_id_map.erase(ifname);
}

sai_object_id_t SwitchState::getPortIdFromIfName(
        _In_ const std::string& ifname) const
{
    SWSS_LOG_ENTER();

    auto it = m_ifname_to_port_id_map.find(ifname);

    if (it == m_ifname_to_port_id_map.end())
    {
        return SAI_NULL_OBJECT_ID;
    }

    return it->second;
}

void SwitchState::setPortIdToTapName(
        _In_ sai_object_id_t port_id,
        _In_ const std::string& tapname)
{
    SWSS_LOG_ENTER();

    m_port_id_to_tapname[port_id] = tapname;
}

void SwitchState::removePortIdToTapName(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    m_port_id_to_tapname.erase(port_id);
}

bool SwitchState::getTapNameFromPortId(
        _In_ const sai_object_id_t port_id,
        _Out_ std::string& if_name)
{
    SWSS_LOG_ENTER();

    if (m_port_id_to_tapname.find(port_id) != m_port_id_to_tapname.end())
    {
        if_name = m_port_id_to_tapname[port_id];

        return true;
    }

    return false;
}

void SwitchState::asyncOnLinkMsg(
        _In_ int nlmsg_type,
        _In_ struct nl_object *obj)
{
    SWSS_LOG_ENTER();

    switch (nlmsg_type)
    {
        case RTM_NEWLINK:
        case RTM_DELLINK:
            break;

        default:

            SWSS_LOG_WARN("unsupported nlmsg_type: %d", nlmsg_type);
            return;
    }

    struct rtnl_link *link = (struct rtnl_link *)obj;

    int             if_index = rtnl_link_get_ifindex(link);
    unsigned int    if_flags = rtnl_link_get_flags(link); // IFF_LOWER_UP and IFF_RUNNING
    const char*     if_name  = rtnl_link_get_name(link);

    SWSS_LOG_NOTICE("received %s ifname: %s, ifflags: 0x%x, ifindex: %d",
            (nlmsg_type == RTM_NEWLINK ? "RTM_NEWLINK" : "RTM_DELLINK"),
            if_name,
            if_flags,
            if_index);

    auto payload = std::make_shared<EventPayloadNetLinkMsg>(m_switch_id, nlmsg_type, if_index, if_flags, if_name);

    m_switchConfig->m_eventQueue->enqueue(std::make_shared<Event>(EVENT_TYPE_NET_LINK_MSG, payload));
}

sai_status_t SwitchState::getNetStat(
        _In_ sai_stat_id_t counterId,
        _In_ std::string& ifName,
        _Out_ uint64_t& counter)
{
    SWSS_LOG_ENTER();

    auto mapit = SwitchState::m_statIdMap.find(counterId);

    if (mapit != SwitchState::m_statIdMap.end())
    {
        std::string filename = "/sys/class/net/" + ifName + "/statistics/" + mapit->second;
        std::ifstream istrm(filename.c_str(), std::ifstream::in);

        if (istrm.good())
        {
            istrm >> counter;
        }
        else
        {
            SWSS_LOG_ERROR("failed to open ifstream in file %s", filename.c_str());
            counter = -1;
            return SAI_STATUS_FAILURE;
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchState::getPortStat(
        _In_ sai_object_id_t portId,
        _In_ const sai_stat_id_t counterId,
        _Out_ uint64_t& counter)
{
    SWSS_LOG_ENTER();

    std::string ifName;

    /* zero out counter */
    counter = 0;

    if (getTapNameFromPortId(portId, ifName) == false)
    {
        /*
         * Hostif not available is expected during init.
         * Hostif missing after init is failure.
         * In both case return counter zero with debug log.
         */
        SWSS_LOG_DEBUG("Hostif is not ready %s", sai_serialize_object_id(portId).c_str());
        return SAI_STATUS_SUCCESS;
    }

    if (getNetStat(counterId, ifName, counter) != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Port stat get failed %s", ifName.c_str());
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchState::getStatsExt(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t* counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    bool perform_set = false;

    auto info = sai_metadata_get_object_type_info(object_type);

    bool enabled = false;

    auto meta = m_meta.lock();

    if (meta)
    {
        enabled = meta->meta_unittests_enabled();
    }
    else
    {
        SWSS_LOG_WARN("meta pointer expired");
    }

    if (enabled && (number_of_counters & VS_COUNTERS_COUNT_MSB ))
    {
        number_of_counters &= ~VS_COUNTERS_COUNT_MSB;

        SWSS_LOG_NOTICE("unittests are enabled and counters count MSB is set to 1, performing SET on %s counters (%s)",
                sai_serialize_object_id(object_id).c_str(),
                info->statenum->name);

        perform_set = true;
    }

    auto str_object_id = sai_serialize_object_id(object_id);

    auto mapit = m_countersMap.find(str_object_id);

    if (mapit == m_countersMap.end())
        m_countersMap[str_object_id] = { };

    auto& localcounters = m_countersMap[str_object_id];

    for (uint32_t i = 0; i < number_of_counters; ++i)
    {
        int32_t id = counter_ids[i];
        uint64_t counter;

        if (perform_set)
        {
            localcounters[ id ] = counters[i];
        }
        else
        {
            auto it = localcounters.find(id);

            if (it == localcounters.end())
            {
                // if counter is not found on list, just return 0
                counters[i] = 0;
            }
            else
            {
                counters[i] = it->second;
            }

            /* In non unit test mode, fetch port counters from host interface */
            if (!enabled && (object_type == SAI_OBJECT_TYPE_PORT))
            {
                if (getPortStat(object_id, id, counter) != SAI_STATUS_SUCCESS)
                {
                    return SAI_STATUS_FAILURE;
                }

                localcounters[ id ] = counter;
            }

            if (mode == SAI_STATS_MODE_READ_AND_CLEAR)
            {
                localcounters[ id ] = 0;
            }
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchState::queryStatsCapability(
        _In_ sai_object_id_t switchId,
        _In_ sai_object_type_t objectType,
        _Inout_ sai_stat_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();

    auto info = sai_metadata_get_object_type_info(objectType);

    if (stats_capability->count == 0 || stats_capability->list == nullptr)
    {
        stats_capability->count = (uint32_t)info->statenum->valuescount;
        return SAI_STATUS_BUFFER_OVERFLOW;
    }

    SWSS_LOG_NOTICE("query counter capability for object ID %s of counter type %s",
            sai_serialize_object_id(switchId).c_str(),
            info->statenum->name);

    auto statenumlist = info->statenum->values;

    for (uint32_t i = 0; i < stats_capability->count; i++)
    {
        stats_capability->list[i].stat_enum = statenumlist[i];
        stats_capability->list[i].stat_modes = SAI_STATS_MODE_READ_AND_CLEAR;
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchState::queryStatsStCapability(
    _In_ sai_object_id_t switchId,
    _In_ sai_object_type_t objectType,
    _Inout_ sai_stat_st_capability_list_t *stats_capability)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

std::shared_ptr<saimeta::Meta> SwitchState::getMeta()
{
    SWSS_LOG_ENTER();

    auto meta = m_meta.lock();

    if (!meta)
    {
        SWSS_LOG_WARN("meta pointer expired");
    }

    return meta;
}
