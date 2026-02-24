#pragma once

extern "C" {
#include <sai.h>
#include <saimetadata.h>
}

#include "swss/table.h"
#include "FlowDump.h"

#include <queue>
#include <mutex>
#include <memory>

/**
 * @brief Default notification queue size limit.
 *
 * Value based on typical L2 deployment with 256k MAC entries and
 * some extra buffer for other events like port-state, q-deadlock etc
 *
 * Note: We recently found a case where SAI continuously sending switch notification events
 *       that also caused the queue to keep growing and eventually exhaust all system memory and crashed.
 *       So a new detection/limit scheme is being implemented by keeping track of the last Event
 *       and if the current Event matches the last Event, then the last Event Count will get
 *       incremented and this count will also be used as part of the equation to ensure this
 *       notification should also be dropped if the queue size limit has already reached and not
 *       just dropping FDB events only.
 * TODO: move to config, also this limit only applies to fdb notifications
 */
#define DEFAULT_NOTIFICATION_QUEUE_SIZE_LIMIT (300000)
#define DEFAULT_NOTIFICATION_CONSECUTIVE_THRESHOLD (1000)

namespace syncd
{
    /**
     * @brief Structure to hold notification data with optional auxiliary data for Flow Dump
     */
    struct NotificationItem
    {
        swss::KeyOpFieldsValuesTuple notification;
        FlowDumpDataPtr auxiliary_data;  // Optional auxiliary data (e.g., flow dump JSON lines)

        NotificationItem() = default;

        NotificationItem(
                _In_ const swss::KeyOpFieldsValuesTuple& ntf,
                _In_ FlowDumpDataPtr aux = nullptr) :
            notification(ntf),
            auxiliary_data(aux)
        {
        }
    };

    class NotificationQueue
    {
        public:

            NotificationQueue(
                    _In_ size_t limit = DEFAULT_NOTIFICATION_QUEUE_SIZE_LIMIT,
                    _In_ size_t consecutiveThresholdLimit = DEFAULT_NOTIFICATION_CONSECUTIVE_THRESHOLD);

            virtual ~NotificationQueue();

        public:

            bool enqueue(
                    _In_ const swss::KeyOpFieldsValuesTuple& msg);

            bool enqueue(
                    _In_ const swss::KeyOpFieldsValuesTuple& msg,
                    _In_ FlowDumpDataPtr auxiliary_data);

            bool tryDequeue(
                    _Out_ swss::KeyOpFieldsValuesTuple& msg);

            bool tryDequeue(
                    _Out_ NotificationItem& item);

            size_t getQueueSize();

        private:

            std::mutex m_mutex;

            std::shared_ptr<std::queue<NotificationItem>> m_queue;

            size_t m_queueSizeLimit;

            size_t m_thresholdLimit;

            size_t m_dropCount;

            size_t m_lastEventCount;

            std::string m_lastEvent;
    };
}
