#include "Context.h"

#include "swss/logger.h"

using namespace saivs;

Context::Context(
        _In_ std::shared_ptr<ContextConfig> contextConfig):
    m_contextConfig(contextConfig)
{
    SWSS_LOG_ENTER();

    // empty
}

Context::~Context()
{
    SWSS_LOG_ENTER();

    // empty
}

std::shared_ptr<ContextConfig> Context::getContextConfig() const
{
    SWSS_LOG_ENTER();

    return m_contextConfig;
}
