#pragma once

#include "swss/sal.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace syncd
{
    // Register before waiting for the group mutex. Retire the registration while
    // still holding it so that the polling thread cannot miss the notification.
    class UpdateLock
    {
        public:

            UpdateLock(
                    _Inout_ std::mutex& mutex,
                    _Inout_ std::atomic<uint32_t>& pending,
                    _Inout_ std::condition_variable& condition);

            ~UpdateLock();

            void unlock();

            void waitForPollEnd(
                    _In_ const bool& pollInProgress,
                    _Inout_ uint32_t& deferred,
                    _In_ const std::atomic<bool>& running);

        private:

            std::atomic<uint32_t>& m_pending;

            std::condition_variable& m_condition;

            std::unique_lock<std::mutex> m_lock;
    };
}
