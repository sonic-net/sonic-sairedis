#include "EventQueue.h"

#include "swss/logger.h"

using namespace saivs;

#define QUEUE_MUTEX std::lock_guard<std::mutex> _lock(m_mutex);

EventQueue::EventQueue(
        _In_ std::shared_ptr<Signal> signal):
    m_signal(signal)
{
    SWSS_LOG_ENTER();

    if (signal == nullptr)
    {
        SWSS_LOG_THROW("signal can't be nullptr");
    }
}

void EventQueue::enqueue(
        _In_ std::shared_ptr<Event> event)
{
    SWSS_LOG_ENTER();

    QUEUE_MUTEX;

    m_queue.push_back(event);

    m_signal->notifyAll();
}

std::shared_ptr<Event> EventQueue::dequeue()
{
    SWSS_LOG_ENTER();

    QUEUE_MUTEX;

    if (m_queue.size())
    {
        auto item = m_queue.front();

        m_queue.pop_front();

        return item;
    }

    return nullptr;
}

size_t EventQueue::size()
{
    SWSS_LOG_ENTER();

    QUEUE_MUTEX;

    return m_queue.size();
}
