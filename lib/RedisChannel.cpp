#include "RedisChannel.h"

#include "sairediscommon.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"
#include "swss/notificationconsumer.h"
#include "swss/select.h"

#include <exception>

using namespace sairedis;

RedisChannel::RedisChannel(
        _In_ const std::string& dbAsic,
        _In_ Channel::Callback callback,
        _In_ Channel::Callback debugCallback):
    Channel(callback),
    m_dbAsic(dbAsic),
    m_debugCallback(debugCallback)
{
    SWSS_LOG_ENTER();

    // TODO this connection info must be obtained from config

    m_db                    = std::make_shared<swss::DBConnector>(dbAsic, 0);
    m_redisPipeline         = std::make_shared<swss::RedisPipeline>(m_db.get()); // enable default pipeline 128
    m_asicState             = std::make_shared<swss::ProducerTable>(m_redisPipeline.get(), ASIC_STATE_TABLE, true);
    m_getConsumer           = std::make_shared<swss::ConsumerTable>(m_db.get(), REDIS_TABLE_GETRESPONSE);

    m_dbNtf                 = std::make_shared<swss::DBConnector>(dbAsic, 0);
    // RedisChannel runs a dedicated notification thread inside every
    // libsairedis-linked process (orchagent, etc.) that SUBSCRIBE's
    // to "NOTIFICATIONS" -- the same channel orchagent's per-orch
    // NotificationConsumers also SUBSCRIBE to.  Redis pub/sub fans
    // every PUBLISH out to every subscriber; under an FDB-event
    // storm this thread's FIFO queue grew unbounded.  LRU-dedup
    // collapses byte-identical in-flight payloads at admission,
    // bounding queue depth to the count of distinct payloads
    // currently in the queue (popped entries free their dedup-index
    // slot).  saimeta::Meta state transitions are idempotent under
    // byte-identical payloads, so the end state of
    // m_saiObjectCollection is unchanged.
    //
    // pri=100 matches swss-common's 4-arg ctor default; the 5-arg
    // ctor has no defaults so it has to be passed explicitly.  No
    // Select-loop priority change vs. the prior 2-arg call.
    m_notificationConsumer  = std::make_shared<swss::NotificationConsumer>(
            m_dbNtf.get(),
            REDIS_TABLE_NOTIFICATIONS_PER_DB(dbAsic),
            100,                                       // pri -- match swss-common default
            swss::DEFAULT_NC_POP_BATCH_SIZE,
            swss::NotificationQueuePolicy::LruDedup);
    // Orch-qualified label so syslog distinguishes this consumer from
    // any per-orch NotificationConsumer that also SUBSCRIBE's to the
    // same "NOTIFICATIONS" channel.
    m_notificationConsumer->setStatsLabel("libsairedis:RedisChannel:" + std::string(REDIS_TABLE_NOTIFICATIONS_PER_DB(dbAsic)));

    if (m_debugCallback)
    {
        // Optional operator meta FDB debug channel. Kept entirely separate from
        // the ASIC NOTIFICATIONS consumer so a flood of operator commands can
        // never displace real notifications, and vice versa.
        m_dbNtfDebug = std::make_shared<swss::DBConnector>(dbAsic, 0);
        m_debugConsumer = std::make_shared<swss::NotificationConsumer>(
                m_dbNtfDebug.get(),
                SAIREDIS_META_FDB_DEBUG_CHANNEL);

        SWSS_LOG_NOTICE("subscribed meta FDB debug consumer on channel %s", SAIREDIS_META_FDB_DEBUG_CHANNEL);
    }

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

    SWSS_LOG_NOTICE("join ntf thread begin");

    m_notificationThread->join();

    SWSS_LOG_NOTICE("join ntf thread end");
}

std::shared_ptr<swss::DBConnector> RedisChannel::getDbConnector() const
{
    SWSS_LOG_ENTER();

    return m_db;
}

void RedisChannel::notificationThreadFunction()
{
    SWSS_LOG_ENTER();

    swss::Select s;

    s.addSelectable(m_notificationConsumer.get());
    s.addSelectable(&m_notificationThreadShouldEndEvent);

    if (m_debugConsumer)
    {
        s.addSelectable(m_debugConsumer.get());
    }

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

            if (m_debugConsumer && sel == m_debugConsumer.get())
            {
                // Operator meta FDB debug command. Fully isolated and tolerant:
                // a malformed command must never disturb real notification
                // processing, so everything is wrapped in try/catch and we
                // continue the loop regardless of outcome.
                try
                {
                    m_debugConsumer->pop(op, data, values);

                    SWSS_LOG_NOTICE("meta debug command: op = %s, data = %s", op.c_str(), data.c_str());

                    if (m_debugCallback)
                    {
                        m_debugCallback(op, data, values);
                    }
                }
                catch (const std::exception& e)
                {
                    SWSS_LOG_ERROR("meta debug command failed and was ignored: %s", e.what());
                }
                catch (...)
                {
                    SWSS_LOG_ERROR("meta debug command failed and was ignored: unknown exception");
                }

                continue;
            }

            m_notificationConsumer->pop(op, data, values);

            SWSS_LOG_DEBUG("notification: op = %s, data = %s", op.c_str(), data.c_str());

            m_callback(op, data, values);
        }
        else
        {
            SWSS_LOG_ERROR("select failed: %s", swss::Select::resultToString(result).c_str());
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
        SWSS_LOG_DEBUG("wait for %s response", command.c_str());

        swss::Selectable *sel;

        int result = s.select(&sel, (int)m_responseTimeoutMs);

        if (result == swss::Select::OBJECT)
        {
            m_getConsumer->pop(kco);

            const std::string &op = kfvOp(kco);
            const std::string &opkey = kfvKey(kco);

            SWSS_LOG_DEBUG("response: op = %s, key = %s", opkey.c_str(), op.c_str());

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

        SWSS_LOG_ERROR("SELECT operation result: %s on %s", swss::Select::resultToString(result).c_str(), command.c_str());
        break;
    }

    SWSS_LOG_ERROR("failed to get response for %s", command.c_str());

    return SAI_STATUS_FAILURE;
}
