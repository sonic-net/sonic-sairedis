#pragma once

#include <string>

namespace saiproxy
{
    class Options
    {
        public:

            Options();

            ~Options() = default;

        public:

            std::string getString() const;

        public:

            std::string m_config;
            std::string m_zmqChannel;
            std::string m_zmqNtfChannel;
    };
}
