#pragma once

#include "swss/sal.h"

#include <cstdint>
#include <cstddef>
#include <map>
#include <set>
#include <string>

namespace saivs
{
    class PortConfigMap
    {
        public:
            bool add(
                    _In_ const std::set<uint32_t>& lanes,
                    _In_ const std::string& name);

            std::string getPortName(
                    _In_ const std::set<uint32_t>& lanes) const;

            size_t size() const;

        private:
            std::map<std::set<uint32_t>, std::string> m_lanes_to_name;
    };
}