#pragma once

#include "NotificationProducerBase.h"

#include "swss/dbconnector.h"
#include "swss/notificationproducer.h"

#include <boost/interprocess/ipc/message_queue.hpp>

namespace syncd
{
    class ShmNotificationProducer:
        public NotificationProducerBase
    {
        public:

            ShmNotificationProducer(
                    _In_ const std::string& ntfQueueName);

            virtual ~ShmNotificationProducer();

        public:

            virtual void send(
                    _In_ const std::string& op,
                    _In_ const std::string& data,
                    _In_ const std::vector<swss::FieldValueTuple>& values) override;

        private:

            std::string m_ntfQueueName;

            std::shared_ptr<boost::interprocess::message_queue> m_ntfQueue;
    };
}
