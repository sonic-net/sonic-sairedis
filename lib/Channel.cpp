#include "Channel.h"

#include "sairedis.h"

#include "swss/logger.h"

using namespace sairedis;

Channel::Channel(
        _In_ Callback callback):
    m_callback(callback),
    m_responseTimeoutMs(SAI_REDIS_DEFAULT_SYNC_OPERATION_RESPONSE_TIMEOUT),
    m_shutdown(false)
{
    SWSS_LOG_ENTER();

    // empty
}

Channel::~Channel()
{
    SWSS_LOG_ENTER();

    // empty
}

void Channel::setResponseTimeout(
        _In_ uint64_t responseTimeout)
{
    SWSS_LOG_ENTER();

    m_responseTimeoutMs = responseTimeout;
}

uint64_t Channel::getResponseTimeout() const
{
    SWSS_LOG_ENTER();

    return m_responseTimeoutMs;
}

void Channel::setShutdown()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("setting channel shutdown flag");

    m_shutdown = true;
}

bool Channel::isShutdown() const
{
    return m_shutdown;
}
