#pragma once

#include "NotificationProducerBase.h"

#include "swss/dbconnector.h"
#include "swss/notificationproducer.h"

class boost::interprocess::message_queue;

namespace syncd
{
    class ShareMemoryNotificationProducer:
        public NotificationProducerBase
    {
        public:

            ShareMemoryNotificationProducer(
                    _In_ const std::string& ntfQueueName);

            virtual ~ShareMemoryNotificationProducer();

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
