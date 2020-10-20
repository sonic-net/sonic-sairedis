#pragma once

#include "BreakConfig.h"

#include <memory>

namespace syncd
{
    class BreakConfigParser
    {
        private:

            BreakConfigParser() = delete;

            ~BreakConfigParser() = delete;

        public:

            static std::shared_ptr<BreakConfig> parseBreakConfig(
                    _In_ const std::string& filePath);

    };
}
