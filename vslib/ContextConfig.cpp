#include "ContextConfig.h"

#include "swss/logger.h"

using namespace saivs;

ContextConfig::ContextConfig(
        _In_ uint32_t guid,
        _In_ const std::string& name):
    m_guid(guid),
    m_name(name)
{
    SWSS_LOG_ENTER();

    m_scc = std::make_shared<SwitchConfigContainer>();
}

ContextConfig::~ContextConfig()
{
    SWSS_LOG_ENTER();

    // empty
}

void ContextConfig::insert(
        _In_ std::shared_ptr<SwitchConfig> config)
{
    SWSS_LOG_ENTER();

    m_scc->insert(config);
}
