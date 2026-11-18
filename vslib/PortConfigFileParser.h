#pragma once

#include "PortConfigMap.h"

#include "swss/sal.h"

#include <memory>
#include <string>

namespace saivs
{
    class PortConfigFileParser
    {
        private:
            PortConfigFileParser() = delete;
            ~PortConfigFileParser() = delete;

        public:
            static std::shared_ptr<PortConfigMap> parse(
                    _In_ const char* file);

            static std::shared_ptr<PortConfigMap> parse(
                    _In_ const std::string& file);
    };
}