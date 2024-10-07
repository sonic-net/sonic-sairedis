#pragma once

#include "NotificationHandler.h"

#include "gmock/gmock.h"

namespace syncd
{
    class MockNotificationHandler : public NotificationHandler
    {
        public:

            MockNotificationHandler(
                    _In_ std::shared_ptr<NotificationProcessor> notificationProcessor)
                : NotificationHandler(notificationProcessor, nullptr) {}

            ~MockNotificationHandler() override {}

            MOCK_METHOD2(onPortStateChangePostLinkEventDamping,
                    void(_In_ uint32_t count,
                         _In_ const sai_port_oper_status_notification_t *data));
    };
}  // namespace syncd
