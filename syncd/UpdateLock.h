#pragma once

#include "FlexCounterSyncState.h"

#include "swss/sal.h"

#include <atomic>
#include <mutex>

namespace syncd
{
    // Register before waiting for the group mutex. Retire the registration while
    // still holding it so that the polling thread cannot miss the notification.
    class UpdateLock
    {
        public:

            UpdateLock(
                    _Inout_ FlexCounterSyncState& state);

            ~UpdateLock();

            void unlock();

            void waitForPollEnd(
                    _In_ const std::atomic<bool>& running);

        private:

            FlexCounterSyncState& m_state;

            std::unique_lock<std::mutex> m_lock;
    };
}
