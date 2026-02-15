#pragma once

#include "swss/sal.h"

#include <memory>
#include <string>

namespace sairedis
{
    class ServerConfig
    {
        public:

            ServerConfig();

            virtual ~ServerConfig();

        public:

            static std::shared_ptr<ServerConfig> loadFromFile(
                    _In_ const char* path);

        public:

            std::string m_zmqEndpoint;

            std::string m_zmqNtfEndpoint;
    };
}
