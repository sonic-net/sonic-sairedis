#pragma once

#include "PortMap.h"

#include <memory>

namespace syncd
{
    class PortMapParser
    {
        private:

            PortMapParser() = delete;
            ~PortMapParser() = delete;

        public:

            static std::shared_ptr<PortMap> parsePortMap(
                    _In_ const std::string& portMapFile);

        private:

            static std::shared_ptr<PortMap> parsePortMapFromJson(
                    _In_ const std::string& jsonFile);
    };
}
