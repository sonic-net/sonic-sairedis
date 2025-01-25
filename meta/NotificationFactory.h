#pragma once

#include "Notification.h"

#include <memory>

namespace otairedis
{
    class NotificationFactory
    {
        private:

            NotificationFactory() = delete;
            ~NotificationFactory() = delete;

        public:

            static std::shared_ptr<Notification> deserialize(
                    _In_ const std::string& name,
                    _In_ const std::string& serializedNotification);
    };
}
