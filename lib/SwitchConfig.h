#pragma once

#include "swss/sal.h"

#include <string>
#include <memory>

namespace sairedis
{
    class SwitchConfig
    {
        public:

            SwitchConfig();

            SwitchConfig(
                    _In_ uint32_t switchIndex,
                    _In_ const std::string& hwinfo);

            virtual ~SwitchConfig() = default;

        public:

            uint32_t m_switchIndex;

            std::string m_hardwareInfo;
    };
}
