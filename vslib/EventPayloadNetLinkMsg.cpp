#include "EventPayloadNetLinkMsg.h"

#include "swss/logger.h"

using namespace saivs;

EventPayloadNetLinkMsg::EventPayloadNetLinkMsg(
        _In_ sai_object_id_t switchId,
        _In_ int nlmsgType,
        _In_ int ifIndex,
        _In_ unsigned int ifFlags,
        _In_ const std::string& ifName):
    m_switchId(switchId),
    m_nlmsgType(nlmsgType),
    m_ifIndex(ifIndex),
    m_ifFlags(ifFlags),
    m_ifName(ifName)
{
    SWSS_LOG_ENTER();

    // empty
}

sai_object_id_t EventPayloadNetLinkMsg::getSwitchId() const
{
    SWSS_LOG_ENTER();

    return m_switchId;
}

int EventPayloadNetLinkMsg::getNlmsgType() const
{
    SWSS_LOG_ENTER();

    return m_nlmsgType;
}

int EventPayloadNetLinkMsg::getIfIndex() const
{
    SWSS_LOG_ENTER();

    return m_ifIndex;
}

unsigned int EventPayloadNetLinkMsg::getIfFlags() const
{
    SWSS_LOG_ENTER();

    return m_ifFlags;
}

const std::string& EventPayloadNetLinkMsg::getIfName() const
{
    SWSS_LOG_ENTER();

    return m_ifName;
}
