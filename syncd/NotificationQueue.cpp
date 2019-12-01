#include "NotificationQueue.h"

#define NOTIFICATION_QUEUE_DROP_COUNT_INDICATOR (1000)

NotificationQueue::NotificationQueue(
        _In_ size_t queueLimit):
    m_queueSizeLimit(queueLimit),
    m_dropCount(0)
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
    SWSS_LOG_ENTER();

    /*
     * If the queue exceeds the limit, then drop all further FDB events This is
     * a temporary solution to handle high memory usage by syncd and the
     * notification queue keeps growing. The permanent solution would be to
     * make this stateful so that only the *latest* event is published.
     */

    if (getQueueSize() < m_queueSizeLimit || kfvOp(item) != "fdb_event") // TODO use enum instead of strings
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_queue.push(item);

        return true;
    }

    m_dropCount++;

    if (!(m_dropCount % NOTIFICATION_QUEUE_DROP_COUNT_INDICATOR))
    {
        SWSS_LOG_NOTICE(
                "Too many messages in queue (%zu), dropped %zu FDB events!",
                getQueueSize(),
                m_dropCount);
    }

    return false;
}

bool NotificationQueue::tryDequeue(
        _Out_ swss::KeyOpFieldsValuesTuple& item)
{
    SWSS_LOG_ENTER();

    std::lock_guard<std::mutex> lock(m_mutex);

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
    SWSS_LOG_ENTER();

    std::lock_guard<std::mutex> lock(m_mutex);

    return m_queue.size();
}

