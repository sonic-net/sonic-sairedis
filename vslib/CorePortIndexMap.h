#pragma once

#include "swss/sal.h"

#include <inttypes.h>

#include <map>
#include <vector>
#include <string>
#include <memory>

namespace saivs
{
    class CorePortIndexMap
    {
        public:

            constexpr static uint32_t DEFAULT_SWITCH_INDEX = 0;

        public:

            CorePortIndexMap(
                    _In_ uint32_t switchIndex);

            virtual ~CorePortIndexMap() = default;

        public:

            bool add(
                    _In_ const std::string& ifname,
                    _In_ const std::vector<uint32_t>& lanes);

            bool remove(
                    _In_ const std::string& ifname);

            uint32_t getSwitchIndex() const;

            bool isEmpty() const;

            bool hasInterface(
                    _In_ const std::string& ifname) const;

            const std::vector<std::vector<uint32_t>> getCorePortIndexVector() const;

            /**
             * @brief Get interface from core and core port index.
             *
             * @return Interface name or empty string if core and core port index are not found.
             */
            std::string getInterfaceFromCorePortIndex(
                    _In_ const std::vector<uint32_t>& corePortIndex) const;

        public:

            static std::shared_ptr<CorePortIndexMap> getDefaultCorePortIndexMap(
                    _In_ uint32_t switchIndex = DEFAULT_SWITCH_INDEX);

        private:

            uint32_t m_switchIndex;

            std::map<std::vector<uint32_t>, std::string> m_corePortIndexToIfName;

            std::map<std::string, std::vector<uint32_t>> m_ifNameToCorePortIndex;

            std::vector<std::vector<uint32_t>> m_corePortIndexMap;
    };
}
