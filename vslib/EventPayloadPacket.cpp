#include "EventPayloadPacket.h"

#include "swss/logger.h"

using namespace saivs;

EventPayloadPacket::EventPayloadPacket(
        _In_ sai_object_id_t port,
        _In_ int ifIndex,
        _In_ const std::string& ifName,
        _In_ const Buffer& buffer):
    m_port(port),
    m_ifIndex(ifIndex),
    m_ifName(ifName),
    m_buffer(buffer)
{
    SWSS_LOG_ENTER();

    // empty
}

sai_object_id_t EventPayloadPacket::getPort() const
{
    SWSS_LOG_ENTER();

    return m_port;
}


int EventPayloadPacket::getIfIndex() const
{
    SWSS_LOG_ENTER();

    return m_ifIndex;
}

const std::string& EventPayloadPacket::getIfName() const
{
    SWSS_LOG_ENTER();

    return m_ifName;
}

const Buffer& EventPayloadPacket::getBuffer() const
{
    SWSS_LOG_ENTER();

    return m_buffer;
}
