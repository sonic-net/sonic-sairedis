#pragma once

#include "ResourceLimiterContainer.h"

#include <string>
#include <vector>

namespace saivs
{
    class ResourceLimiterParser
    {
        public:

            ResourceLimiterParser() = delete;

            ~ResourceLimiterParser() = delete;

        public:

            static std::shared_ptr<ResourceLimiterContainer> parseFromFile(
                    _In_ const char* fileName);

        private:

            static void parseLineWithIndex(
                    _In_ std::shared_ptr<ResourceLimiterContainer> container,
                    _In_ const std::vector<std::string>& tokens,
                    _In_ const std::string& strLimit);

            static void parseLineWithNoIndex(
                    _In_ std::shared_ptr<ResourceLimiterContainer> container,
                    _In_ const std::vector<std::string>& tokens,
                    _In_ const std::string& strLimit);

            static void parse(
                    _In_ std::shared_ptr<ResourceLimiterContainer> container,
                    _In_ uint32_t switchIndex,
                    _In_ const std::string& strObjectType,
                    _In_ const std::string& strLimit);
    };
}
