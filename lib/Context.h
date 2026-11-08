#pragma once

#include "RedisRemoteSaiInterface.h"
#include "ContextConfig.h"

#include "meta/Notification.h"
#include "meta/Meta.h"

namespace sairedis
{
    class Context
    {
        public:

            Context(
                    _In_ std::shared_ptr<ContextConfig> contextConfig,
                    _In_ std::shared_ptr<Recorder> recorder,
                    _In_ std::function<sai_switch_notifications_t(std::shared_ptr<Notification>, Context*)> notificationCallback);

            virtual ~Context();

        public:

            void populateMetadata(
                    _In_ sai_object_id_t switchId);

        private:

            sai_switch_notifications_t handle_notification(
                    _In_ std::shared_ptr<Notification> notification);

        private:

            std::shared_ptr<ContextConfig> m_contextConfig;

            std::shared_ptr<Recorder> m_recorder;

        public:

            std::shared_ptr<saimeta::Meta> m_meta;

            std::shared_ptr<RedisRemoteSaiInterface> m_redisSai;

            std::function<sai_switch_notifications_t(std::shared_ptr<Notification>, Context*)> m_notificationCallback;
    };
}
