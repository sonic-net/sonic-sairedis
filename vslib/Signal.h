#pragma once

#include <condition_variable>
#include <mutex>

namespace saivs
{
    class Signal
    {
        public:

            Signal() = default;

            virtual ~Signal() = default;

        public:

            void notifyAll();

            void notifyOne();

            void wait();

        private:

            std::condition_variable m_cv;

            std::mutex m_mutex;
    };
}
