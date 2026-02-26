#pragma once

#include "swss/sal.h"

#include <string>
#include <cstdint>

namespace syncd
{
    typedef enum _syncd_restart_type_t
    {
        SYNCD_RESTART_TYPE_COLD,

        SYNCD_RESTART_TYPE_WARM,

        SYNCD_RESTART_TYPE_FAST,

        SYNCD_RESTART_TYPE_PRE_SHUTDOWN,

        SYNCD_RESTART_TYPE_EXPRESS,

        SYNCD_RESTART_TYPE_PRE_EXPRESS_SHUTDOWN,

    } syncd_restart_type_t;

    class RequestShutdownCommandLineOptions
    {
        public:

            RequestShutdownCommandLineOptions();

            virtual ~RequestShutdownCommandLineOptions();

        public:

            syncd_restart_type_t getRestartType() const;

            void setRestartType(
                    _In_ syncd_restart_type_t restartType);

            static syncd_restart_type_t stringToRestartType(
                    _In_ const std::string& restartType);

            static std::string restartTypeToString(
                    _In_ syncd_restart_type_t restartType);

        private:

            syncd_restart_type_t m_restartType;

        public:

            uint32_t m_globalContext;

            std::string m_contextConfig;
    };
}
