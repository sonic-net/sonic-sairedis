#pragma once

#include "RequestShutdownCommandLineOptions.h"

#include "swss/sal.h"

#include <memory>

namespace syncd
{
    class RequestShutdownCommandLineOptionsParser
    {
        private:

            RequestShutdownCommandLineOptionsParser() = delete;

            ~RequestShutdownCommandLineOptionsParser() = delete;

        public:

            static std::shared_ptr<RequestShutdownCommandLineOptions> parseCommandLine(
                    _In_ int argc,
                    _In_ char **argv);

            static void printUsage();
    };
}
