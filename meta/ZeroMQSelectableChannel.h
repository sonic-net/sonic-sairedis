#pragma once

#include "SelectableChannel.h"

#include "swss/table.h"
#include "swss/selectableevent.h"

#include <deque>
#include <thread>
#include <memory>

#define ZMQ_RESPONSE_DEFAULT_BUFFER_SIZE (4*1024*1024)

namespace sairedis
{
    class ZeroMQSelectableChannel:
        public SelectableChannel
    {
        public:

            ZeroMQSelectableChannel(
                    _In_ const std::string& endpoint,
                    _In_ long zmqResponseBufferSize = ZMQ_RESPONSE_DEFAULT_BUFFER_SIZE);

            virtual ~ZeroMQSelectableChannel();

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

            // virtual bool initializedWithData() override;

            // virtual void updateAfterRead() override;

            // virtual int getPri() const override;

        private:

            void zmqPollThread();

        private:

            std::string m_endpoint;

            void* m_context;

            void* m_socket;

            int m_fd;

            std::queue<std::string> m_queue;

            std::vector<uint8_t> m_buffer;

            volatile bool m_allowZmqPoll;

            volatile bool m_runThread;

            std::shared_ptr<std::thread> m_zmlPollThread;

            swss::SelectableEvent m_selectableEvent;

            long m_zmqResponseBufferSize;
    };
}
