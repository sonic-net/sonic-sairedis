#pragma once

#include "swss/producertable.h"
#include "swss/consumertable.h"
#include "swss/notificationconsumer.h"
#include "swss/selectableevent.h"
#include "swss/sal.h"

extern "C" {
#include "sai.h"
}

#include <memory>
#include <functional>

namespace sairedis
{
    class Channel
    {
        public:

            typedef std::function<void(const std::string&,const std::string&, const std::vector<swss::FieldValueTuple>&)> Callback;

        public:

            Channel(
                    _In_ Callback callback);

            virtual ~Channel();

        public:

            void setResponseTimeout(
                    _In_ uint64_t responseTimeout);

            uint64_t getResponseTimeout() const;

            /**
             * @brief Signal that the system is shutting down.
             *
             * When set, blocking operations (like zmq_poll) will not retry
             * on EINTR and will return failure immediately, allowing the
             * caller to exit promptly.
             */
            void setShutdown();

            bool isShutdown() const;

        public:

            virtual void setBuffered(
                    _In_ bool buffered) = 0;

            virtual void flush() = 0;

            virtual void set(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& values,
                    _In_ const std::string& command) = 0;

            virtual void del(
                    _In_ const std::string& key,
                    _In_ const std::string& command) = 0;

            virtual sai_status_t wait(
                    _In_ const std::string& command,
                    _Out_ swss::KeyOpFieldsValuesTuple& kco) = 0;

        protected:

            virtual void notificationThreadFunction() = 0;

        protected:

            Callback m_callback;

            uint64_t m_responseTimeoutMs;

            volatile bool m_shutdown;

        protected: // notification

            /**
             * @brief Indicates whether notification thread should be running.
             */
            volatile bool m_runNotificationThread;

            /**
             * @brief Event used to nice end notifications thread.
             */
            swss::SelectableEvent m_notificationThreadShouldEndEvent;

            /**
             * @brief Notification thread
             */
            std::shared_ptr<std::thread> m_notificationThread;
    };
}
