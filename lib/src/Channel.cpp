#include "Channel.h"

#include "sairedis.h"

#include "swss/logger.h"

using namespace sairedis;

Channel::Channel(
        _In_ Callback callback):
    m_callback(callback),
    m_responseTimeoutMs(SAI_REDIS_DEFAULT_SYNC_OPERATION_RESPONSE_TIMEOUT)
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
    int i = 0;
    if (i == 1){
        i = 0; // not covered
    }

    return m_responseTimeoutMs;
}
