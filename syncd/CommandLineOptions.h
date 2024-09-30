#pragma once

#include "sairedis.h"

#include "swss/sal.h"

#include <string>

#define STRING_SAI_START_TYPE_COLD_BOOT         "cold"
#define STRING_SAI_START_TYPE_WARM_BOOT         "warm"
#define STRING_SAI_START_TYPE_FAST_BOOT         "fast"
#define STRING_SAI_START_TYPE_FASTFAST_BOOT     "fastfast"
#define STRING_SAI_START_TYPE_EXPRESS_BOOT      "express"
#define STRING_SAI_START_TYPE_UNKNOWN           "unknown"

namespace syncd
{
    typedef enum _sai_start_type_t
    {
        SAI_START_TYPE_COLD_BOOT = 0,

        SAI_START_TYPE_WARM_BOOT = 1,

        SAI_START_TYPE_FAST_BOOT = 2,

        /**
         * A special type of boot used by Mellanox platforms to start in 'fastfast'
         * boot mode
         */
        SAI_START_TYPE_FASTFAST_BOOT = 3,

        /**
         * A special type of boot used by Cisco platforms to start in 'express'
         * boot mode
         */
        SAI_START_TYPE_EXPRESS_BOOT = 4,

        /**
         * Set at last, just for error purpose.
         */
        SAI_START_TYPE_UNKNOWN

    } sai_start_type_t;

    class CommandLineOptions
    {
        public:

            CommandLineOptions();

            virtual ~CommandLineOptions() = default;

        public:

            virtual std::string getCommandLineString() const;

        public:

            static sai_start_type_t startTypeStringToStartType(
                    _In_ const std::string& startType);

            static std::string startTypeToString(
                    _In_ sai_start_type_t startType);

        public:

            bool m_enableDiagShell;
            bool m_enableTempView;
            bool m_disableExitSleep;
            bool m_enableUnittests;

            /**
             * When set to true will enable DB vs ASIC consistency check after
             * comparison logic.
             */
            bool m_enableConsistencyCheck;

            bool m_enableSyncMode;

            bool m_enableSaiBulkSupport;

            sai_redis_communication_mode_t m_redisCommunicationMode;

            sai_start_type_t m_startType;

            std::string m_profileMapFile;

            uint32_t m_globalContext;

            std::string m_contextConfig;

            std::string m_breakConfig;

            int64_t m_watchdogWarnTimeSpan;

#ifdef SAITHRIFT
            bool m_runRPCServer;
            std::string m_portMapFile;
#endif // SAITHRIFT

            std::string m_supportingBulkCounterGroups;

    };
}
