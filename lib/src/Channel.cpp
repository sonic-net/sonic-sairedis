#include "Channel.h"

#include "swss/logger.h"

using namespace sairedis;

Channel::Channel(
        _In_ Callback callback):
    m_callback(callback)
{
    SWSS_LOG_ENTER();

    // empty
}

Channel::~Channel()
{
    SWSS_LOG_ENTER();

    // empty
}
