#pragma once

#include "RequestShutdownCommandLineOptions.h"
#include "ContextConfig.h"

#include "swss/sal.h"

#include <memory>

#define SYNCD_NOTIFICATION_CHANNEL_RESTARTQUERY     "RESTARTQUERY"

/**
 * @brief Notification channel 'restartQuery' per DB scope
 *
 * In https://redis.io/docs/manual/pubsub/, it says:
 * "Pub/Sub has no relation to the key space. It was made to not interfere with
 * it on any level, including database numbers."
 */
#define SYNCD_NOTIFICATION_CHANNEL_RESTARTQUERY_PER_DB(dbName) \
    ((dbName) == "ASIC_DB" ? \
     SYNCD_NOTIFICATION_CHANNEL_RESTARTQUERY : \
     (dbName) + "_" + SYNCD_NOTIFICATION_CHANNEL_RESTARTQUERY)

namespace syncd
{
    class RequestShutdown
    {
        public:

            RequestShutdown(
                    _In_ std::shared_ptr<RequestShutdownCommandLineOptions> options);

            virtual ~RequestShutdown();

        public:

            void send();

        private:

            std::shared_ptr<RequestShutdownCommandLineOptions> m_options;

            std::shared_ptr<sairedis::ContextConfig> m_contextConfig;
    };
}
