#pragma once

#include "Event.h"
#include "Signal.h"

#include <mutex>
#include <deque>

namespace saivs
{
    class EventQueue
    {
        public:

            EventQueue(
                    _In_ std::shared_ptr<Signal> signal);

            virtual ~EventQueue() = default;

        public:

            void enqueue(
                    _In_ std::shared_ptr<Event> event);

            std::shared_ptr<Event> dequeue();

            size_t size();

        private:

            std::shared_ptr<Signal> m_signal;

            std::mutex m_mutex;

            std::deque<std::shared_ptr<Event>> m_queue;
    };
}
