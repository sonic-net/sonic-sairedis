#pragma once

#include "CorePortIndexMapContainer.h"

#include <vector>
#include <memory>

namespace saivs
{
    class CorePortIndexMapFileParser
    {
        private:

            CorePortIndexMapFileParser() = delete;
            ~CorePortIndexMapFileParser() = delete;

        public:

            static std::shared_ptr<CorePortIndexMapContainer> parseCorePortIndexMapFile(
                    _In_ const char* file);

            static std::shared_ptr<CorePortIndexMapContainer> parseCorePortIndexMapFile(
                    _In_ const std::string& file);

            static bool isInterfaceNameValid(
                    _In_ const std::string& name);

        private:

            static void parseLineWithIndex(
                    _In_ std::shared_ptr<CorePortIndexMapContainer> container,
                    _In_ const std::vector<std::string>& tokens);

            static void parseLineWithNoIndex(
                    _In_ std::shared_ptr<CorePortIndexMapContainer> container,
                    _In_ const std::vector<std::string>& tokens);

            static void parse(
                    _In_ std::shared_ptr<CorePortIndexMapContainer> container,
                    _In_ uint32_t switchIndex,
                    _In_ const std::string& ifname,
                    _In_ const std::string& slanes);
    };
}
