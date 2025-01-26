#pragma once

#include "RedisRemoteOtaiInterface.h"
#include "ContextConfig.h"

#include "meta/Notification.h"
#include "meta/Meta.h"

namespace otairedis
{
    class Context
    {
        public:

            Context(
                    _In_ std::shared_ptr<ContextConfig> contextConfig,
                    _In_ std::shared_ptr<Recorder> recorder,
                    _In_ std::function<otai_linecard_notifications_t(std::shared_ptr<Notification>, Context*)> notificationCallback);

            virtual ~Context();

        public:

            void populateMetadata(
                    _In_ otai_object_id_t switchId);

        private:

            otai_linecard_notifications_t handle_notification(
                    _In_ std::shared_ptr<Notification> notification);

        private:

            std::shared_ptr<ContextConfig> m_contextConfig;

            std::shared_ptr<Recorder> m_recorder;

        public:

            std::shared_ptr<otaimeta::Meta> m_meta;

            std::shared_ptr<RedisRemoteOtaiInterface> m_redisOtai;

            std::function<otai_linecard_notifications_t(std::shared_ptr<Notification>, Context*)> m_notificationCallback;
    };
}
