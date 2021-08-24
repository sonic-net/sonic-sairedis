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
    m_zmqEnable(false),
    m_zmqEndpoint("ipc:///tmp/zmq_ep"),
    m_zmqNtfEndpoint("ipc:///tmp/zmq_ntf_ep")
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

bool ContextConfig::hasConflict(
        _In_ std::shared_ptr<const ContextConfig> ctx) const
{
    SWSS_LOG_ENTER();

    if (m_guid == ctx->m_guid)
    {
        SWSS_LOG_ERROR("guid %u conflict", m_guid);
        return true;
    }

    if (m_name == ctx->m_name)
    {
        SWSS_LOG_ERROR("name %s conflict", m_name.c_str());
        return true;
    }

    if (m_dbAsic == ctx->m_dbAsic)
    {
        SWSS_LOG_ERROR("dbAsic %s conflict", m_dbAsic.c_str());
        return true;
    }

    if (m_dbCounters == ctx->m_dbCounters)
    {
        SWSS_LOG_ERROR("dbCounters %s conflict", m_dbCounters.c_str());
        return true;
    }

    if (m_dbFlex == ctx->m_dbFlex)
    {
        SWSS_LOG_ERROR("dbFlex %s conflict", m_dbFlex.c_str());
        return true;
    }

    // state database can be shared

    if (m_zmqEndpoint == ctx->m_zmqEndpoint)
    {
        SWSS_LOG_ERROR("zmqEndpoint %s conflict", m_zmqEndpoint.c_str());
        return true;
    }

    if (m_zmqNtfEndpoint == ctx->m_zmqNtfEndpoint)
    {
        SWSS_LOG_ERROR("zmqNtfEndpoint %s conflict", m_zmqNtfEndpoint.c_str());
        return true;
    }

    return false;
}
