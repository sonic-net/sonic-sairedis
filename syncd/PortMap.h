#pragma once

#include "swss/sal.h"

#include <map>
#include <set>
#include <string>
#include <memory>

class PortMap
{
    public:

        PortMap() = default;

        virtual ~PortMap() = default;

    public:

        void insert(
                _In_ const std::set<int>& laneSet,
                _In_ const std::string& name);

        void clear();

        size_t size() const;

        const std::map<std::set<int>, std::string>& getRawPortMap() const;

        /**
         * @brief Set global object for RPC server binding.
         */
        static void setGlobalPortMap(
                _In_ std::shared_ptr<PortMap> portMap);

    private:

        std::map<std::set<int>, std::string> m_portMap;
};
