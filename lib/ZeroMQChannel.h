#pragma once

#include "Channel.h"

#include "swss/producertable.h"
#include "swss/consumertable.h"
#include "swss/notificationconsumer.h"
#include "swss/selectableevent.h"

#include <memory>
#include <functional>

namespace otairedis
{
    class ZeroMQChannel:
        public Channel
    {
        public:

            ZeroMQChannel(
                    _In_ const std::string& endpoint,
                    _In_ const std::string& ntfEndpoint,
                    _In_ Channel::Callback callback);

            virtual ~ZeroMQChannel();

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

            virtual otai_status_t wait(
                    _In_ const std::string& command,
                    _Out_ swss::KeyOpFieldsValuesTuple& kco) override;

        protected:

            virtual void notificationThreadFunction() override;

        private:

            std::string m_endpoint;

            std::string m_ntfEndpoint;

            std::vector<uint8_t> m_buffer;

            void* m_context;

            void* m_socket;

            void* m_ntfContext;

            void* m_ntfSocket;
    };
}
