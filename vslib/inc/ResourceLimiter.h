#pragma once

extern "C" {
#include "sai.h"
}

#include <map>

namespace saivs
{
    class ResourceLimiter
    {
        public:

             constexpr static uint32_t DEFAULT_SWITCH_INDEX = 0;

        public:

            ResourceLimiter(
                    _In_ uint32_t switchIndex);

            virtual ~ResourceLimiter() = default;

        public:

            size_t getObjectTypeLimit(
                    _In_ sai_object_type_t objectType) const;

            void setObjectTypeLimit(
                    _In_ sai_object_type_t objectType,
                    _In_ size_t limit);

            void removeObjectTypeLimit(
                    _In_ sai_object_type_t objectType);

            void clearLimits();

        private:

            uint32_t m_switchIndex;

            std::map<sai_object_type_t, size_t> m_objectTypeLimits;
    };
}
