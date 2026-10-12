#pragma once

#include "Channel.h"

#include "swss/producertable.h"
#include "swss/consumertable.h"
#include "swss/notificationconsumer.h"
#include "swss/selectableevent.h"

#include <memory>
#include <functional>

#define ZMQ_RESPONSE_DEFAULT_BUFFER_SIZE (4*1024*1024)

namespace sairedis
{
    class ZeroMQChannel:
        public Channel
    {
        public:

            ZeroMQChannel(
                    _In_ const std::string& endpoint,
                    _In_ const std::string& ntfEndpoint,
                    _In_ Channel::Callback callback,
                    _In_ long zmqResponseBufferSize = ZMQ_RESPONSE_DEFAULT_BUFFER_SIZE);

            /**
             * @brief Request-only channel.
             *
             * Sets up the ZMQ_REQ half only: no notification socket, no
             * notification thread. For a caller that issues requests and reads
             * their replies but has no interest in asynchronous SAI
             * notifications (port state change, FDB events, ...) -- a one-shot
             * CLI, or a test harness.
             *
             * The notification socket BINDS its endpoint, so a second process
             * cannot share the one the primary client already holds; without
             * this overload such a caller has to bind a throwaway address that
             * syncd never pushes to, and pay for a socket, a thread, a second
             * zmq context and the response buffers that go with them.
             *
             * Request/response is unaffected: set()/del()/wait() use only the
             * ZMQ_REQ socket, and wait() is where the SAI status comes from.
             */
            ZeroMQChannel(
                    _In_ const std::string& endpoint,
                    _In_ long zmqResponseBufferSize = ZMQ_RESPONSE_DEFAULT_BUFFER_SIZE);

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

            virtual sai_status_t wait(
                    _In_ const std::string& command,
                    _Out_ swss::KeyOpFieldsValuesTuple& kco) override;

        protected:

            virtual void notificationThreadFunction() override;

        private:

            /**
             * @brief Set up the ZMQ_REQ socket on m_endpoint. Common to both
             * constructors; throws if the endpoint cannot be opened.
             */
            void initRequestChannel();


            std::string m_endpoint;

            std::string m_ntfEndpoint;

            std::vector<uint8_t> m_buffer;

            void* m_context;

            void* m_socket;

            void* m_ntfContext;

            void* m_ntfSocket;

            long m_zmqResponseBufferSize;
    };
}
