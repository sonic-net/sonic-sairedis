#include "ContextConfig.h"

#include "swss/logger.h"

using namespace sairedis;

ContextConfig::ContextConfig(
        _In_ uint32_t guid,
        _In_ const std::string& name,
        _In_ const std::string& dbAsic,
        _In_ const std::string& dbCounters,
        _In_ const std::string& dbFlex,
        _In_ const std::string& dbState):
    m_guid(guid),
    m_name(name),
    m_dbAsic(dbAsic),
    m_dbCounters(dbCounters),
    m_dbFlex(dbFlex),
    m_dbState(dbState),
    m_zmqEnable(false)
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

