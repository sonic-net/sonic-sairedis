#pragma once

#include "SwitchConfig.h"

#include "swss/sal.h"

#include <map>
#include <memory>

namespace sairedis
{
    class SwitchConfigContainer
    {
        public:

            SwitchConfigContainer() = default;

            virtual ~SwitchConfigContainer() = default;

        public:

            void insert(
                    _In_ std::shared_ptr<SwitchConfig> config);

            std::shared_ptr<SwitchConfig> getConfig(
                    _In_ uint32_t switchIndex) const;

            std::shared_ptr<SwitchConfig> getConfig(
                    _In_ const std::string& hardwareInfo) const;

        private:

            std::map<uint32_t, std::shared_ptr<SwitchConfig>> m_indexToConfig;

            std::map<std::string, std::shared_ptr<SwitchConfig>> m_hwinfoToConfig;
    };
}
