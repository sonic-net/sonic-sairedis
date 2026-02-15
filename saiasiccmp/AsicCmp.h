#pragma once

#include "swss/sal.h"

#include "CommandLineOptions.h"

#include <memory>

namespace saiasiccmp
{
    class AsicCmp
    {
        public:

            AsicCmp(
                    _In_ std::shared_ptr<CommandLineOptions> options);

        public:

            bool compare();

        private:

            std::shared_ptr<CommandLineOptions> m_commandLineOptions;
    };
}
