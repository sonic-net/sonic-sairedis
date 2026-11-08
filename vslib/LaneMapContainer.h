#pragma once

#include "LaneMap.h"

#include <memory>
#include <map>

namespace saivs
{
    class LaneMapContainer
    {
        public:

            LaneMapContainer() = default;

            virtual ~LaneMapContainer() = default;

        public:

            bool insert(
                    _In_ std::shared_ptr<LaneMap> laneMap);

            bool remove(
                    _In_ uint32_t switchIndex);

            std::shared_ptr<LaneMap> getLaneMap(
                    _In_ uint32_t switchIndex) const;

            void clear();

            bool hasLaneMap(
                    _In_ uint32_t switchIndex) const;

            size_t size() const;

            void removeEmptyLaneMaps();

        private:

            std::map<uint32_t, std::shared_ptr<LaneMap>> m_map;
    };
}
