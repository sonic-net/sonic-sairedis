#include "ShareMemorySelectableChannel.h"

#include "swss/logger.h"
#include "swss/json.h"

#include <unistd.h>
#include <boost/interprocess/ipc/message_queue.hpp>

#define MQ_POLL_TIMEOUT (1000)

// TODO: improve POC code, define dupe code in same place
#define MQ_RESPONSE_BUFFER_SIZE (4*1024*1024)
#define MQ_SIZE 100
#define MQ_MAX_RETRY 10

using namespace sairedis;
using namespace boost::interprocess;

ShareMemorySelectableChannel::ShareMemorySelectableChannel(
        _In_ const std::string& queueName):
    m_queueName(queueName),
    m_messageQueue(nullptr),
    m_allowMqPoll(false),
    m_runThread(true)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("binding on %s", m_queueName.c_str());

    m_buffer.resize(MQ_RESPONSE_BUFFER_SIZE);

    try
    {
        m_messageQueue = std::make_shared<message_queue>(open_or_create,
                                                           m_queueName.c_str(),
                                                           MQ_SIZE,
                                                           MQ_RESPONSE_BUFFER_SIZE);
    }
    catch (const interprocess_exception& e)
    {
        SWSS_LOG_THROW("failed to open or create message queue %s: %s",
                m_queueName.c_str(),
                e.what());
    }

    m_mqPollThread = std::make_shared<std::thread>(&ShareMemorySelectableChannel::mqPollThread, this);
}

ShareMemorySelectableChannel::~ShareMemorySelectableChannel()
{
    SWSS_LOG_ENTER();

    m_runThread = false;
    m_allowMqPoll = true;

    try
    {
        message_queue::remove(m_queueName.c_str());
    }
    catch (const interprocess_exception& e)
    {
        // message queue still use by some other process
        SWSS_LOG_NOTICE("Failed to close a using message queue '%s': %s", m_queueName.c_str(), e.what());
    }

    SWSS_LOG_NOTICE("ending mq poll thread for channel %s", m_queueName.c_str());

    m_mqPollThread->join();

    SWSS_LOG_NOTICE("ended mq poll thread for channel %s", m_queueName.c_str());
}

void ShareMemorySelectableChannel::mqPollThread()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("begin");

    while (m_runThread)
    {
        m_allowMqPoll = false;

        // TODO we should have loop here in case we get multiple events since
        // zmq poll will only signal events once, but in our case we don't
        // expect multiple events, since we want to send/receive
        unsigned int priority;
        message_queue::size_type recvd_size;

        try
        {
            bool received = m_messageQueue->timed_receive(m_buffer.data(),
                                                MQ_RESPONSE_BUFFER_SIZE,
                                                recvd_size,
                                                priority,
                                                boost::posix_time::ptime(microsec_clock::universal_time()) + boost::posix_time::milliseconds(MQ_POLL_TIMEOUT));

            if (recvd_size >= MQ_RESPONSE_BUFFER_SIZE)
            {
                SWSS_LOG_THROW("message queue received message was truncated (over %d bytes, received %d), increase buffer size, message DROPPED",
                        MQ_RESPONSE_BUFFER_SIZE,
                        recvd_size);
            }

            m_buffer.at(recvd_size) = 0; // make sure that we end string with zero before parse
            m_queue.push((char*)m_buffer.data());

            if (m_runThread == false)
            {
                SWSS_LOG_NOTICE("ending pool thread, since run is false");
                break;
            }

            if (!received)
            {
                // Not receive message
                SWSS_LOG_DEBUG("message queue timed receive: no events, continue");
                continue;
            }

            m_selectableEvent.notify(); // will release epoll
            while (m_runThread && !m_allowMqPoll)
            {
                usleep(10); // could be increased or replaced by spin lock
            }
        }
        catch (const interprocess_exception& e)
        {
            SWSS_LOG_ERROR("message queue %s timed receive failed: %s", m_queueName.c_str(), e.what());
            break;
        }
    }

    SWSS_LOG_NOTICE("end");
}

// SelectableChannel overrides

bool ShareMemorySelectableChannel::empty()
{
    SWSS_LOG_ENTER();

    return m_queue.size() == 0;
}

void ShareMemorySelectableChannel::pop(
        _Out_ swss::KeyOpFieldsValuesTuple& kco,
        _In_ bool initViewMode)
{
    SWSS_LOG_ENTER();

    if (m_queue.empty())
    {
        SWSS_LOG_THROW("queue is empty, can't pop");
    }

    std::string msg = m_queue.front();
    m_queue.pop();

    auto& values = kfvFieldsValues(kco);

    values.clear();

    swss::JSon::readJson(msg, values);

    swss::FieldValueTuple fvt = values.at(0);

    kfvKey(kco) = fvField(fvt);
    kfvOp(kco) = fvValue(fvt);

    values.erase(values.begin());
}

void ShareMemorySelectableChannel::set(
        _In_ const std::string& key,
        _In_ const std::vector<swss::FieldValueTuple>& values,
        _In_ const std::string& op)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> copy = values;

    swss::FieldValueTuple opdata(key, op);

    copy.insert(copy.begin(), opdata);

    std::string msg = swss::JSon::buildJson(copy);

    SWSS_LOG_DEBUG("sending: %s", msg.c_str());

    try
    {
        m_messageQueue->send(msg.c_str(), msg.length(), 0);

        // at this point we already did send/receive pattern, so we can notify
        // thread that we can poll again
        m_allowMqPoll = true;
    }
    catch (const interprocess_exception& e)
    {
        SWSS_LOG_THROW("message queue $s send failed: %s",
                m_queueName.c_str(),
                e.what());
    }
}

// Selectable overrides

int ShareMemorySelectableChannel::getFd()
{
    SWSS_LOG_ENTER();

    return m_selectableEvent.getFd();
}

uint64_t ShareMemorySelectableChannel::readData()
{
    SWSS_LOG_ENTER();

    // clear selectable event so it could be triggered in next select()
    m_selectableEvent.readData();

    unsigned int priority;
    message_queue::size_type recvd_size;

    try
    {
        if (!m_messageQueue->try_receive(m_buffer.data(), MQ_RESPONSE_BUFFER_SIZE, recvd_size, priority))
        {
            // message already received by the pool thread
            return 0;
        }
    }
    catch (const interprocess_exception& e)
    {
        SWSS_LOG_ERROR("message queue %s receive failed: %s", m_queueName.c_str(), e.what());
    }

    if (recvd_size >= MQ_RESPONSE_BUFFER_SIZE)
    {
        SWSS_LOG_THROW("message queue received message was truncated (over %d bytes, received %d), increase buffer size, message DROPPED",
                MQ_RESPONSE_BUFFER_SIZE,
                recvd_size);
    }

    m_buffer.at(recvd_size) = 0; // make sure that we end string with zero before parse

    m_queue.push((char*)m_buffer.data());

    return 0;
}

bool ShareMemorySelectableChannel::hasData()
{
    SWSS_LOG_ENTER();

    return m_queue.size() > 0;
}

bool ShareMemorySelectableChannel::hasCachedData()
{
    SWSS_LOG_ENTER();

    return m_queue.size() > 1;
}
