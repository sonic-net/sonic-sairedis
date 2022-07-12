#include "NotificationQueueHash.h"
#include "sairediscommon.h"

using namespace syncd;

#define NOTIFICATION_QUEUE_DROP_COUNT_INDICATOR (1000)
#define MUTEX std::lock_guard<std::mutex> _lock(m_mutex);

NotificationQueueHash::NotificationQueueHash():
    m_dropCount(0)
{
    SWSS_LOG_ENTER();

    // empty;
}

NotificationQueueHash::~NotificationQueueHash()
{
    SWSS_LOG_ENTER();

    // empty
}

bool NotificationQueueHash::enqueue(
        _In_ const std::string& key,
        _In_ const swss::KeyOpFieldsValuesTuple& item)
{
    MUTEX;

    SWSS_LOG_ENTER();

    // if key is already in the hash, then we will override it previous content
    // which means we are dropping that item

    if (m_queueHash.find(key) != m_queueHash.end())
    {
        m_dropCount++;

        if (!(m_dropCount % NOTIFICATION_QUEUE_DROP_COUNT_INDICATOR))
        {
            SWSS_LOG_NOTICE("Dropped total of %zu notifications", m_dropCount);
        }
    }

    m_queueHash[key] = item;

    return true;
}

bool NotificationQueueHash::tryDequeue(
        _Out_ swss::KeyOpFieldsValuesTuple& item)
{
    MUTEX;

    SWSS_LOG_ENTER();

    if (m_queueHash.empty())
    {
        return false;
    }

    auto it = m_queueHash.begin();

    item = it->second;

    m_queueHash.erase(it);

    return true;
}

size_t NotificationQueueHash::getQueueSize()
{
    MUTEX;

    SWSS_LOG_ENTER();

    return m_queueHash.size();
}
