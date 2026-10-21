#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace syncd
{
    // Owned by one FlexCounter; its scoped guards borrow this state. The owner
    // outlives the guards and joins its polling thread before destroying it.
    struct FlexCounterSyncState
    {
        std::mutex mutex;

        // Registered before acquiring mutex, so the polling thread can yield.
        std::atomic<uint32_t> pendingUpdates{0};

        std::condition_variable condition;

        // These fields require mutex to be held. Same-object support probes may
        // clear counters; keep them after the complete sweep without blocking
        // its checkpoints.
        uint32_t deferredUpdates = 0;

        bool pollInProgress = false;
    };
}
