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
