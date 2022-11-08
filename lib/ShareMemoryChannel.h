#pragma once

#include "Channel.h"

#include "swss/producertable.h"
#include "swss/consumertable.h"
#include "swss/notificationconsumer.h"
#include "swss/selectableevent.h"

#include <memory>
#include <functional>

#include <boost/interprocess/ipc/message_queue.hpp>

namespace sairedis
{
    class ShareMemoryChannel:
        public Channel
    {
        public:

            ShareMemoryChannel(
                    _In_ const std::string& queueName,
                    _In_ const std::string& ntfQueueName,
                    _In_ Channel::Callback callback);

            virtual ~ShareMemoryChannel();

        public:

            virtual void setBuffered(
                    _In_ bool buffered) override;

            virtual void flush() override;

            virtual void set(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& values,
                    _In_ const std::string& command) override;

            virtual void del(
                    _In_ const std::string& key,
                    _In_ const std::string& command) override;

            virtual sai_status_t wait(
                    _In_ const std::string& command,
                    _Out_ swss::KeyOpFieldsValuesTuple& kco) override;

        protected:

            virtual void notificationThreadFunction() override;

        private:

            std::string m_queueName;

            std::string m_ntfQueueName;

            std::vector<uint8_t> m_buffer;

            std::shared_ptr<boost::interprocess::message_queue> m_queue;

            std::shared_ptr<boost::interprocess::message_queue> m_ntfQueue;
    };
}
