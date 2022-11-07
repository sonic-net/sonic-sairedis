#include "ShareMemoryChannel.h"

#include "sairediscommon.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"
#include "swss/select.h"

#include <unistd.h>

using namespace sairedis;

#define MQ_RESPONSE_BUFFER_SIZE (4*1024*1024)
#define MQ_SIZE 100
#define MQ_MAX_RETRY 10

ShareMemoryChannel::ShareMemoryChannel(
        _In_ const std::string& queueName,
        _In_ const std::string& ntfQueueName,
        _In_ Channel::Callback callback):
    Channel(callback),
    m_queueName(queueName),
    m_ntfQueueName(ntfQueueName),
    m_queue(nullptr),
    m_ntfQueue(nullptr)
{
    SWSS_LOG_ENTER();

    m_buffer.resize(MQ_RESPONSE_BUFFER_SIZE);

    // configure message queue for main communication

    m_queue = message_queue(open_or_create,
                               m_queueName,
                               MQ_SIZE,
                               MQ_RESPONSE_BUFFER_SIZE);
    if (m_queue == 0)
    {
        SWSS_LOG_THROW("failed to open or create main message queue %s",
                m_queueName.c_str());
    }

    // configure notification message queue

    m_ntfQueue = message_queue(open_or_create,
                               m_ntfQueueName,
                               MQ_SIZE,
                               MQ_RESPONSE_BUFFER_SIZE);
    if (m_ntfQueue == 0)
    {
        SWSS_LOG_THROW("failed to open or create ntf message queue %s",
                m_ntfQueueName.c_str());
    }

    // start thread

    m_runNotificationThread = true;

    SWSS_LOG_NOTICE("creating notification thread");

    m_notificationThread = std::make_shared<std::thread>(&ZeroMQChannel::notificationThreadFunction, this);
}

ShareMemoryChannel::~ShareMemoryChannel()
{
    SWSS_LOG_ENTER();

    m_runNotificationThread = false;

    try
    {
        message_queue::remove(m_queueName);
    }
    catch (const interprocess_exception& e)
    {
        // message queue still use by some other process
        SWSS_LOG_NOTICE("Failed to close a using message queue '%s': %s", m_queueName.c_str(), e.what());
    }

    // create new message queue, and perform send to break notification recv
    tmpQueue = message_queue(open_or_create,
                               m_ntfQueueName,
                               MQ_SIZE,
                               MQ_RESPONSE_BUFFER_SIZE);

    if (tmpQueue == 0)
    {
        SWSS_LOG_THROW("failed to open or create temp ntf message queue %s",
                m_ntfQueueName.c_str());
    }

    try
    {
        tmpQueue.send("TERM", 5, 0);
    }
    catch (const interprocess_exception& e)
    {
        SWSS_LOG_THROW("tmp queue send error: %s", e.what());
    }

    try
    {
        message_queue::remove(m_ntfQueueName);
    }
    catch (const interprocess_exception& e)
    {
        // message queue still use by some other process
        SWSS_LOG_NOTICE("Failed to close a using message queue '%s': %s", m_ntfQueueName.c_str(), e.what());
    }

    // when zmq context is destroyed, zmq_recv will be interrupted and errno
    // will be set to ETERM, so we don't need actual FD to be used in
    // selectable event

    SWSS_LOG_NOTICE("join ntf thread begin");

    m_notificationThread->join();

    SWSS_LOG_NOTICE("join ntf thread end");

    try
    {
        message_queue::remove(m_ntfQueueName);
    }
    catch (const interprocess_exception& e)
    {
        // message queue still use by some other process
        SWSS_LOG_NOTICE("Failed to close a using message queue '%s': %s", m_ntfQueueName.c_str(), e.what());
    }
}

void ShareMemoryChannel::notificationThreadFunction()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("start listening for notifications");

    std::vector<uint8_t> buffer;

    buffer.resize(MQ_RESPONSE_BUFFER_SIZE);

    while (m_runNotificationThread)
    {
        // NOTE: this entire loop internal could be encapsulated into separate class
        // which will inherit from Selectable class, and name this as ntf receiver
        unsigned int priority;
        message_queue::size_type recvd_size;
        
        try
        {
            m_ntfQueue.receive(buffer.data(), MQ_RESPONSE_BUFFER_SIZE, recvd_size, priority);
        }
        catch (const interprocess_exception& e)
        {
            SWSS_LOG_ERROR("message queue %s receive failed: %s", m_ntfQueueName.c_str(), e.what());

            // at this point we don't know if next receive will succeed
            if (m_runNotificationThread)
            {
                continue;
            }
            else
            
            {
                break;
            }
        }

        if (!m_runNotificationThread)
        {
            break;
        }

        // TODO: POC code, improve this by user a thread safe flag.
        if (recvd_size == 5)
        {
            string_view message(reinterpret_cast<char*>(buffer.data()), buffer.size());
            if (message == "TERM")
            {
                SWSS_LOG_NOTICE("message queu receive interrupted with TERM, ending thread");
                break;
            }
        }

        if (recvd_size >= MQ_RESPONSE_BUFFER_SIZE)
        {
            SWSS_LOG_WARN("queue message was truncated (over %d bytes, received %d), increase buffer size, message DROPPED",
                    MQ_RESPONSE_BUFFER_SIZE,
                    recvd_size);

            continue;
        }

        buffer.at(recvd_size) = 0; // make sure that we end string with zero before parse

        SWSS_LOG_DEBUG("ntf: %s", buffer.data());

        std::vector<swss::FieldValueTuple> values;

        swss::JSon::readJson((char*)buffer.data(), values);

        swss::FieldValueTuple fvt = values.at(0);

        const std::string& op = fvField(fvt);
        const std::string& data = fvValue(fvt);

        values.erase(values.begin());

        SWSS_LOG_DEBUG("notification: op = %s, data = %s", op.c_str(), data.c_str());

        m_callback(op, data, values);
    }

    SWSS_LOG_NOTICE("exiting notification thread");
}

void ShareMemoryChannel::setBuffered(
        _In_ bool buffered)
{
    SWSS_LOG_ENTER();

    // not supported
}

void ShareMemoryChannel::flush()
{
    SWSS_LOG_ENTER();

    // not supported
}

void ShareMemoryChannel::set(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& values,
        _In_ const std::string& command)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> copy = values;

    swss::FieldValueTuple opdata(key, command);

    copy.insert(copy.begin(), opdata);

    std::string msg = swss::JSon::buildJson(copy);

    SWSS_LOG_DEBUG("sending: %s", msg.c_str());

    for (int i = 0; true ; ++i)
    {
        try
        {
            m_queue.send(msg.c_str(), msg.length(), 0);
        }
        catch (const interprocess_exception& e)
        {
            SWSS_LOG_THROW("message queue %s send failed: %s",
                    m_queueName.c_str(),
                    e.what());
        }

        if (i < MQ_MAX_RETRY)
        {
            continue;
        }
        break;
    }
}

void ShareMemoryChannel::del(
        _In_ const std::string& key,
        _In_ const std::string& command)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> values;

    set(key, values, command);
}

sai_status_t ShareMemoryChannel::wait(
        _In_ const std::string& command,
        _Out_ swss::KeyOpFieldsValuesTuple& kco)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("wait for %s response", command.c_str());

    for (int i = 0; true ; ++i)
    {
        unsigned int priority;
        message_queue::size_type recvd_size;

        try
        {
            m_queue.receive(buffer.data(), MQ_RESPONSE_BUFFER_SIZE, recvd_size, priority);
        }
        catch (const interprocess_exception& e)
        {
            SWSS_LOG_ERROR("message queue %s receive failed: %s", m_queueName.c_str(), e.what());
        }

        if (i < MQ_MAX_RETRY)
        {
            continue;
        }

        if (recvd_size >= MQ_RESPONSE_BUFFER_SIZE)
        {
            SWSS_LOG_THROW("message queue message was truncated (over %d bytes, received %d), increase buffer size, message DROPPED",
                    MQ_RESPONSE_BUFFER_SIZE,
                    recvd_size);
        }
        break;
    }

    m_buffer.at(recvd_size) = 0; // make sure that we end string with zero before parse

    SWSS_LOG_DEBUG("response: %s", m_buffer.data());

    std::vector<swss::FieldValueTuple> values;

    swss::JSon::readJson((char*)m_buffer.data(), values);

    swss::FieldValueTuple fvt = values.at(0);

    const std::string& opkey = fvField(fvt);
    const std::string& op= fvValue(fvt);

    values.erase(values.begin());

    kfvFieldsValues(kco) = values;
    kfvOp(kco) = op;
    kfvKey(kco) = opkey;

    SWSS_LOG_INFO("response: op = %s, key = %s", opkey.c_str(), op.c_str());

    if (op != command)
    {
        // we can hit this place if there were some timeouts
        // as well, if there will be multiple "GET" messages, then
        // we can receive response from not the expected GET

        SWSS_LOG_THROW("got not expected response: %s:%s, expected: %s", opkey.c_str(), op.c_str(), command.c_str());
    }

    sai_status_t status;
    sai_deserialize_status(opkey, status);

    SWSS_LOG_DEBUG("%s status: %s", command.c_str(), opkey.c_str());

    return status;
}
