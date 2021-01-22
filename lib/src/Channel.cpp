#include "Channel.h"

#include "swss/logger.h"

using namespace sairedis;

#define REDIS_ASIC_STATE_COMMAND_GETRESPONSE_TIMEOUT_MS (60*1000)

Channel::Channel(
        _In_ Callback callback):
    m_callback(callback),
    m_responseTimeoutMs(REDIS_ASIC_STATE_COMMAND_GETRESPONSE_TIMEOUT_MS)
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
