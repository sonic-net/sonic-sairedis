#pragma once

#include "swss/sal.h"

#include <memory>
#include <string>

namespace sairedis
{
    class ClientConfig
    {
        public:

            ClientConfig();

            virtual ~ClientConfig();

        public:

            static std::shared_ptr<ClientConfig> loadFromFile(
                    _In_ const char* path);

        public:

            std::string m_zmqEndpoint;

            std::string m_zmqNtfEndpoint;
    };
}
