#pragma once

#include "ResourceLimiter.h"

#include <memory>

namespace saivs
{
    class ResourceLimiterContainer
    {
        public:

            ResourceLimiterContainer() = default;

            virtual ~ResourceLimiterContainer() = default;

        public:

            void insert(
                    _In_ uint32_t switchIndex,
                    _In_ std::shared_ptr<ResourceLimiter> rl);

            void remove(
                    _In_ uint32_t switchIndex);

            std::shared_ptr<ResourceLimiter> getResourceLimiter(
                    _In_ uint32_t switchIndex) const;

            void clear();

        private:

            std::map<uint32_t, std::shared_ptr<ResourceLimiter>> m_container;
    };
}
