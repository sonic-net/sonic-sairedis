#pragma once

#include "swss/sal.h"

#include <string>
#include <vector>

namespace saiasiccmp
{
    class CommandLineOptions
    {
        public:

            CommandLineOptions();

            virtual ~CommandLineOptions() = default;

        public:

            virtual std::string getCommandLineString() const;

        public:

            bool m_enableLogLevelInfo;
            bool m_dumpDiffToStdErr;

            std::vector<std::string> m_args;
    };
}
