#pragma once

#include "SwitchConfig.h"

#include <map>
#include <set>
#include <memory>

namespace saivs
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

            std::set<std::shared_ptr<SwitchConfig>> getSwitchConfigs() const;

        private:

            std::map<uint32_t, std::shared_ptr<SwitchConfig>> m_indexToConfig;

            std::map<std::string, std::shared_ptr<SwitchConfig>> m_hwinfoToConfig;
    };
}
