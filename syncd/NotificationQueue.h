#pragma once

extern "C" {
#include <sai.h>
}

#include "swss/table.h"

#include <queue>
#include <mutex>

/**
 * @brief Default notification queue size limit.
 *
 * Value based on typical L2 deployment with 256k MAC entries and
 * some extra buffer for other events like port-state, q-deadlock etc
 *
 * TODO: move to config, also this limit only applies to fdb notifications
 */
#define DEFAULT_NOTIFICATION_QUEUE_SIZE_LIMIT (300000)

class NotificationQueue
{
    public:

        NotificationQueue(
                _In_ size_t limit = DEFAULT_NOTIFICATION_QUEUE_SIZE_LIMIT);

        virtual ~NotificationQueue();

    public:

        bool enqueue(
                _In_ const swss::KeyOpFieldsValuesTuple& msg);

        bool tryDequeue(
                _Out_ swss::KeyOpFieldsValuesTuple& msg);

        size_t getQueueSize();

    private:

        std::mutex m_mutex;

        std::queue<swss::KeyOpFieldsValuesTuple> m_queue;

        size_t m_queueSizeLimit;

        size_t m_dropCount;
};

