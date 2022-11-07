#include "ShareMemoryNotificationProducer.h"

using namespace syncd;
using boost::interprocess;

// TODO: improve POC code, define dupe code in same place
#define MQ_RESPONSE_BUFFER_SIZE (4*1024*1024)
#define MQ_SIZE 100
#define MQ_MAX_RETRY 10

ShareMemoryNotificationProducer::ShareMemoryNotificationProducer(
        _In_ const std::string& ntfQueueName):
    m_ntfQueueName(ntfQueueName),
    m_ntfQueue(nullptr)
{
    SWSS_LOG_ENTER();

    try
    {
        m_ntfQueue = message_queue(open_or_create,
                                   m_ntfQueueName,
                                   MQ_SIZE,
                                   MQ_RESPONSE_BUFFER_SIZE);
    }
    catch (const interprocess_exception& e)
    {
        SWSS_LOG_THROW("failed to open or create ntf message queue %s: %s",
                m_queueName.c_str(),
                e.what());
    }

    SWSS_LOG_NOTICE("opening ntf message queue: %s", m_ntfQueueName.c_str());
}

ShareMemoryNotificationProducer::~ShareMemoryNotificationProducer()
{
    SWSS_LOG_ENTER();

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

void ShareMemoryNotificationProducer::send(
        _In_ const std::string& op,
        _In_ const std::string& data,
        _In_ const std::vector<swss::FieldValueTuple>& values)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> vals = values;

    swss::FieldValueTuple opdata(op, data);

    vals.insert(vals.begin(), opdata);

    std::string msg = swss::JSon::buildJson(vals);

    SWSS_LOG_DEBUG("sending: %s", msg.c_str());

    try
    {
        m_ntfQueue.send(msg.c_str(), msg.length(), 0);
    }
    catch (const interprocess_exception& e)
    {
        SWSS_LOG_THROW("message queue %s send failed: %s",
                m_queueName.c_str(),
                e.what());
    }
}
