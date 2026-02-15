#pragma once

#include "CommandLineOptions.h"

#include <memory>

namespace syncd
{
    class CommandLineOptionsParser
    {
        private:

            CommandLineOptionsParser() = delete;

            ~CommandLineOptionsParser() = delete;

        public:

            static std::shared_ptr<CommandLineOptions> parseCommandLine(
                    _In_ int argc,
                    _In_ char **argv);

            static void printUsage();
    };
}
