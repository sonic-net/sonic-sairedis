#pragma once

extern "C" {
#include "otai.h"
}

#include <map>
#include <set>
#include <vector>

namespace otaimeta
{
    class PortRelatedSet
    {
        public:

            PortRelatedSet() = default;

            virtual ~PortRelatedSet() = default;

        public:

            void insert(
                    _In_ otai_object_id_t portId,
                    _In_ otai_object_id_t relatedObjectId);

            const std::set<otai_object_id_t> getPortRelatedObjects(
                    _In_ otai_object_id_t portId) const;

            void clear();

            void removePort(
                    _In_ otai_object_id_t portId);

            std::vector<otai_object_id_t> getAllPorts() const;

        private:

            std::map<otai_object_id_t, std::set<otai_object_id_t>> m_mapset;
    };
}
