#include "RedisSelectableChannel.h"

#include "swss/logger.h"

using namespace syncd;

RedisSelectableChannel::RedisSelectableChannel(
        _In_ std::shared_ptr<swss::DBConnector> dbAsic,
        _In_ const std::string& asicStateTable,
        _In_ const std::string& getResponseTable,
        _In_ const std::string& tempPrefix,
        _In_ bool modifyRedis):
    m_dbAsic(dbAsic),
    m_tempPrefix(tempPrefix),
    m_modifyRedis(modifyRedis)
{
    SWSS_LOG_ENTER();

    m_asicState = std::make_shared<swss::ConsumerTable>(m_dbAsic.get(), asicStateTable);

    /*
     * At the end we cant use producer consumer concept since if one process
     * will restart there may be something in the queue also "remove" from
     * response queue will also trigger another "response".
     */

    m_getResponse  = std::make_shared<swss::ProducerTable>(m_dbAsic.get(), getResponseTable);
}

bool RedisSelectableChannel::empty() 
{
    SWSS_LOG_ENTER();

    return m_asicState->empty();
}

void RedisSelectableChannel::set(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& values,
        _In_ const std::string& op)
{
    SWSS_LOG_ENTER();

    m_getResponse->set(key, values, op);
}

void RedisSelectableChannel::pop(
        _Out_ swss::KeyOpFieldsValuesTuple& kco,
        _In_ bool initViewMode)
{
    SWSS_LOG_ENTER();

    if (initViewMode)
    {
        m_asicState->pop(kco, m_tempPrefix);
    }
    else
    {
        m_asicState->pop(kco);
    }
}

// Selectable overrides

int RedisSelectableChannel::getFd()
{
    SWSS_LOG_ENTER();

    return m_asicState->getFd();
}

uint64_t RedisSelectableChannel::readData()
{
    SWSS_LOG_ENTER();

    return m_asicState->readData();
}

bool RedisSelectableChannel::hasData()
{
    SWSS_LOG_ENTER();

    return m_asicState->hasData();
}

bool RedisSelectableChannel::hasCachedData()
{
    SWSS_LOG_ENTER();

    return m_asicState->hasCachedData();
}

bool RedisSelectableChannel::initializedWithData()
{
    SWSS_LOG_ENTER();

    return m_asicState->initializedWithData();
}

void RedisSelectableChannel::updateAfterRead()
{
    SWSS_LOG_ENTER();

    return m_asicState->updateAfterRead();
}

int RedisSelectableChannel::getPri() const
{
    SWSS_LOG_ENTER();

    return m_asicState->getPri();
}
