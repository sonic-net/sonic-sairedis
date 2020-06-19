#include "RedisChannel.h"

#include "sairediscommon.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"
#include "swss/select.h"

using namespace sairedis;

/**
 * @brief Get response timeout in milliseconds.
 */
#define REDIS_ASIC_STATE_COMMAND_GETRESPONSE_TIMEOUT_MS (60*1000)

RedisChannel::RedisChannel(
        _In_ const std::string& dbAsic,
        _In_ Callback callback):
    m_callback(callback),
    m_dbAsic(dbAsic)
{
    SWSS_LOG_ENTER();

    // TODO this connection info must be obtained from config

    m_db                    = std::make_shared<swss::DBConnector>(dbAsic, 0);
    m_redisPipeline         = std::make_shared<swss::RedisPipeline>(m_db.get()); //enable default pipeline 128
    m_asicState             = std::make_shared<swss::ProducerTable>(m_redisPipeline.get(), ASIC_STATE_TABLE, true);
    m_getConsumer           = std::make_shared<swss::ConsumerTable>(m_db.get(), REDIS_TABLE_GETRESPONSE);

    m_dbNtf                 = std::make_shared<swss::DBConnector>(dbAsic, 0);
    m_notificationConsumer  = std::make_shared<swss::NotificationConsumer>(m_dbNtf.get(), REDIS_TABLE_NOTIFICATIONS);

    m_runNotificationThread = true;

    SWSS_LOG_NOTICE("creating notification thread");

    m_notificationThread = std::make_shared<std::thread>(&RedisChannel::notificationThreadFunction, this);
}

RedisChannel::~RedisChannel()
{
    SWSS_LOG_ENTER();

    m_runNotificationThread = false;

    // notify thread that it should end
    m_notificationThreadShouldEndEvent.notify();

    m_notificationThread->join();
}

std::shared_ptr<swss::DBConnector> RedisChannel::getDbConnector() const
{
    SWSS_LOG_ENTER();

    return m_db;
}

static std::string getSelectResultAsString(int result)
{
    SWSS_LOG_ENTER();

    std::string res;

    switch (result)
    {
        case swss::Select::ERROR:
            res = "ERROR";
            break;

        case swss::Select::TIMEOUT:
            res = "TIMEOUT";
            break;

        default:
            SWSS_LOG_WARN("non recognized select result: %d", result);
            res = std::to_string(result);
            break;
    }

    return res;
}

void RedisChannel::notificationThreadFunction()
{
    SWSS_LOG_ENTER();

    swss::Select s;

    s.addSelectable(m_notificationConsumer.get());
    s.addSelectable(&m_notificationThreadShouldEndEvent);

    while (m_runNotificationThread)
    {
        swss::Selectable *sel;

        int result = s.select(&sel);

        if (sel == &m_notificationThreadShouldEndEvent)
        {
            // user requested shutdown_switch
            break;
        }

        if (result == swss::Select::OBJECT)
        {
            swss::KeyOpFieldsValuesTuple kco;

            std::string op;
            std::string data;
            std::vector<swss::FieldValueTuple> values;

            m_notificationConsumer->pop(op, data, values);

            SWSS_LOG_DEBUG("notification: op = %s, data = %s", op.c_str(), data.c_str());

            m_callback(op, data, values);
        }
        else
        {
            SWSS_LOG_ERROR("select failed: %s", getSelectResultAsString(result).c_str());
        }
    }
}

void RedisChannel::setBuffered(
        _In_ bool buffered)
{
    SWSS_LOG_ENTER();

    m_asicState->setBuffered(buffered);
}

void RedisChannel::flush()
{
    SWSS_LOG_ENTER();

    m_asicState->flush();
}

void RedisChannel::set(
        _In_ const std::string& key, 
        _In_ const std::vector<swss::FieldValueTuple>& values,
        _In_ const std::string& command)
{
    SWSS_LOG_ENTER();

    m_asicState->set(key, values, command);
}

void RedisChannel::del(
        _In_ const std::string& key,
        _In_ const std::string& command)
{
    SWSS_LOG_ENTER();

    m_asicState->del(key, command);
}

sai_status_t RedisChannel::wait(
        _In_ const std::string& command,
        _Out_ swss::KeyOpFieldsValuesTuple& kco)
{
    SWSS_LOG_ENTER();

    swss::Select s;

    s.addSelectable(m_getConsumer.get());

    while (true)
    {
        SWSS_LOG_INFO("wait for %s response", command.c_str());

        swss::Selectable *sel;

        int result = s.select(&sel, REDIS_ASIC_STATE_COMMAND_GETRESPONSE_TIMEOUT_MS);

        if (result == swss::Select::OBJECT)
        {
            m_getConsumer->pop(kco);

            const std::string &op = kfvOp(kco);
            const std::string &opkey = kfvKey(kco);

            SWSS_LOG_INFO("response: op = %s, key = %s", opkey.c_str(), op.c_str());

            if (op != command)
            {
                SWSS_LOG_WARN("got not expected response: %s:%s", opkey.c_str(), op.c_str());

                // ignore non response messages
                continue;
            }

            sai_status_t status;
            sai_deserialize_status(opkey, status);

            SWSS_LOG_DEBUG("%s status: %s", command.c_str(), opkey.c_str());

            return status;
        }

        SWSS_LOG_ERROR("SELECT operation result: %s on %s", getSelectResultAsString(result).c_str(), command.c_str());
        break;
    }

    SWSS_LOG_ERROR("failed to get response for %s", command.c_str());

    return SAI_STATUS_FAILURE;
}

