#pragma once

#include "NotificationProducerBase.h"

#include "swss/dbconnector.h"
#include "swss/notificationproducer.h"

namespace syncd
{
    class ZeroMQNotificationProducer:
        public NotificationProducerBase
    {
        public:

            ZeroMQNotificationProducer(
                    _In_ const std::string& ntfEndpoint);

            virtual ~ZeroMQNotificationProducer();

        public:

            virtual void send(
                    _In_ const std::string& op,
                    _In_ const std::string& data,
                    _In_ const std::vector<swss::FieldValueTuple>& values) override;

        private:

            void* m_ntfContext;

            void* m_ntfSocket;
    };
}
