#pragma once

extern "C" {
#include <saimetadata.h>
}

#include "swss/table.h"

#include <mutex>
#include <unordered_map>

namespace syncd
{
    class NotificationQueueHash
    {
        public:

            NotificationQueueHash();

            virtual ~NotificationQueueHash();

        public:

            bool enqueue(
                    _In_ const std::string& key,
                    _In_ const swss::KeyOpFieldsValuesTuple& msg);

            bool tryDequeue(
                    _Out_ swss::KeyOpFieldsValuesTuple& msg);

            size_t getQueueSize();

        private:

            std::mutex m_mutex;

            std::unordered_map<std::string, swss::KeyOpFieldsValuesTuple> m_queueHash;

            size_t m_dropCount;
    };
}
