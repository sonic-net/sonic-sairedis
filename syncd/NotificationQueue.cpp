#include "NotificationQueue.h"
#include "sairediscommon.h"

#define NOTIFICATION_QUEUE_DROP_COUNT_INDICATOR (1000)

using namespace syncd;

#define MUTEX std::lock_guard<std::mutex> _lock(m_mutex);

NotificationQueue::NotificationQueue(
        _In_ size_t queueLimit,
        _In_ size_t consecutiveThresholdLimit):
    m_queueSizeLimit(queueLimit),
    m_thresholdLimit(consecutiveThresholdLimit),
    m_dropCount(0),
    m_lastEventCount(0),
    m_lastEvent(SAI_SWITCH_NOTIFICATION_NAME_FDB_EVENT)
{
    SWSS_LOG_ENTER();

    // empty;
}

NotificationQueue::~NotificationQueue()
{
    SWSS_LOG_ENTER();

    // empty
}

bool NotificationQueue::enqueue(
        _In_ const swss::KeyOpFieldsValuesTuple& item)
{
    MUTEX;

    SWSS_LOG_ENTER();
    bool candidateToDrop = false;
    std::string currentEvent;

    /*
     * If the queue exceeds the limit, then drop all further FDB events This is
     * a temporary solution to handle high memory usage by syncd and the
     * notification queue keeps growing. The permanent solution would be to
     * make this stateful so that only the *latest* event is published.
     *
     * We have also seen other notification storms that can also cause this queue issue
     * So the new scheme is to keep the last notification event and its consecutive count
     * If threshold limit reached and the consecutive count also reached then this notification
     * will also be dropped regardless of its event type to protect the device from crashing due to
     * running out of memory
     */
    auto queueSize = m_queue.size();
    currentEvent = kfvKey(item);
    if (currentEvent == m_lastEvent)
    {
        m_lastEventCount++;
    }
    else
    {
        m_lastEventCount = 1;
        m_lastEvent = currentEvent;
    }
    if (queueSize >= m_queueSizeLimit)
    {
        /* Too many queued up already check if notification fits condition to e dropped
         * 1. All FDB events should be dropped at this point.
         * 2. All other notification events will start to drop if it reached the consecutive threshold limit
         */
        if (currentEvent == SAI_SWITCH_NOTIFICATION_NAME_FDB_EVENT)
        {
            candidateToDrop = true;
        }
        else
        {
            if (m_lastEventCount >= m_thresholdLimit)
            {
                candidateToDrop = true;
            }
        }
    }

    if (!candidateToDrop)
    {
        m_queue.push(item);

        return true;
    }

    m_dropCount++;

    if (!(m_dropCount % NOTIFICATION_QUEUE_DROP_COUNT_INDICATOR))
    {
        SWSS_LOG_NOTICE(
                "Too many messages in queue (%zu), dropped (%zu), lastEventCount (%zu) Dropping %s !",
                queueSize,
                m_dropCount, m_lastEventCount, m_lastEvent.c_str());
    }

    return false;
}

bool NotificationQueue::tryDequeue(
        _Out_ swss::KeyOpFieldsValuesTuple& item)
{
    MUTEX;

    SWSS_LOG_ENTER();

    if (m_queue.empty())
    {
        return false;
    }

    item = m_queue.front();

    m_queue.pop();

    return true;
}

size_t NotificationQueue::getQueueSize()
{
    MUTEX;

    SWSS_LOG_ENTER();

    return m_queue.size();
}
