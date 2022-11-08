#pragma once

#include "SelectableChannel.h"

#include "swss/table.h"
#include "swss/selectableevent.h"

#include <deque>
#include <thread>
#include <memory>

#include <boost/interprocess/ipc/message_queue.hpp>

namespace sairedis
{
    class ShmSelectableChannel:
        public SelectableChannel
    {
        public:

            ShmSelectableChannel(
                    _In_ const std::string& queueName);

            virtual ~ShmSelectableChannel();

        public: // SelectableChannel overrides

            virtual bool empty() override;

            virtual void pop(
                    _Out_ swss::KeyOpFieldsValuesTuple& kco,
                    _In_ bool initViewMode) override;

            virtual void set(
                    _In_ const std::string& key,
                    _In_ const std::vector<swss::FieldValueTuple>& values,
                    _In_ const std::string& op) override;

        public: // Selectable overrides

            virtual int getFd() override;

            virtual uint64_t readData() override;

            virtual bool hasData() override;

            virtual bool hasCachedData() override;

        private:

            void mqPollThread();

        private:

            std::string m_queueName;

            std::shared_ptr<boost::interprocess::message_queue> m_messageQueue;

            std::queue<std::string> m_queue;

            std::vector<uint8_t> m_buffer;

            volatile bool m_allowMqPoll;

            volatile bool m_runThread;

            std::shared_ptr<std::thread> m_mqPollThread;

            swss::SelectableEvent m_selectableEvent;
    };
}
