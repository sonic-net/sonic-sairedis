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
    m_zmqEnable(CONTEXT_CONFIG_ZMQ_EMPTY),
    m_zmqEndpoint("tcp://127.0.0.1:5555"),
    m_zmqNtfEndpoint("tcp://127.0.0.1:5556")
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

    if (zmqEndpointConflict(m_zmqEndpoint, ctx->m_zmqEndpoint,
            m_zmqEnable, ctx->m_zmqEnable, "zmqEndpoint"))
    {
        return true;
    }

    if (zmqEndpointConflict(m_zmqNtfEndpoint, ctx->m_zmqNtfEndpoint,
            m_zmqEnable, ctx->m_zmqEnable, "zmqNtfEndpoint"))
    {
        return true;
    }

    return false;
}

bool ContextConfig::zmqEndpointConflict(
        _In_ const std::string& endpoint,
        _In_ const std::string& otherEndpoint,
        _In_ context_config_zmq_state_t zmqEnable,
        _In_ context_config_zmq_state_t otherZmqEnable,
        _In_ const char* label)
{
    SWSS_LOG_ENTER();

    // Different endpoints can never collide at bind time.
    if (endpoint != otherEndpoint)
    {
        return false;
    }

    // A context locked to Redis (CONTEXT_CONFIG_ZMQ_DISABLED) never binds or
    // connects ZMQ sockets, so a shared endpoint with such a context can never
    // collide at bind time. Flagging it as a conflict would make
    // ContextConfigContainer::insert() throw, which loadFromFile() catches by
    // discarding the entire parsed context_config.json and falling back to the
    // default container, so a DISABLED context must not trigger that path.
    if (zmqEnable == CONTEXT_CONFIG_ZMQ_DISABLED
            || otherZmqEnable == CONTEXT_CONFIG_ZMQ_DISABLED)
    {
        return false;
    }

    // When both contexts have CONTEXT_CONFIG_ZMQ_EMPTY, neither one has
    // expressed an opinion about ZMQ in the config file. Matching endpoints are
    // usually the shared constructor defaults from an ordinary non-ZMQ
    // multi-context layout that omits the optional zmq_endpoint fields, so the
    // loader must not reject the file. The match is still worth flagging in the
    // log: if a cmdline override later promotes both contexts to ZMQ they will
    // collide at bind time, and an operator scanning startup logs gets a hint
    // at why.
    if (zmqEnable == CONTEXT_CONFIG_ZMQ_EMPTY
            && otherZmqEnable == CONTEXT_CONFIG_ZMQ_EMPTY)
    {
        SWSS_LOG_WARN("%s %s matches between EMPTY contexts; "
                "cmdline ZMQ promotion would collide at bind",
                label, endpoint.c_str());
        return false;
    }

    SWSS_LOG_ERROR("%s %s conflict", label, endpoint.c_str());
    return true;
}
