#pragma once

extern "C" {
#include <sai.h>
#include<saimetadata.h>
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
 * Note: We recently found a case where SAI continuously sending switchnotification events
 *       that also caused the enqueue to eventually exhaust all system memory and crashed.
 *       So a new detection/limit scheme is being implemented by keeping track of the lastEvent
 *       and if the currentEvent matches the lastEvent, then the lastEventCount will get
 *       incremented and this count will also be used as part of the equation to ensure this
 *       notification should also be dropped if the queue size limit has already reached and not
 *       just dropping FDB events only.
 * TODO: move to config, also this limit only applies to fdb notifications
 */
#define DEFAULT_NOTIFICATION_QUEUE_SIZE_LIMIT (300000)

namespace syncd
{
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

            size_t m_lastEventCount;

            std::string m_lastEvent;
    };
}
