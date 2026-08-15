#pragma once

#include "Notification.h"

namespace sairedis
{
    /**
     * @brief Synthetic notification carrying an operator FDB debug command.
     *
     * This is NOT a SAI switch notification. It is a lightweight vehicle used
     * to route operator-issued meta-layer FDB diagnostics ("dump") through the
     * existing, already-locked notification callback chain so that
     * Meta::processDebugCommand runs under the sairedis API mutex.
     *
     * It is constructed directly (never via NotificationFactory), so it can
     * never trigger the factory's throw-on-unknown-name path. getSwitchId()/
     * getAnyObjectId() return SAI_NULL_OBJECT_ID (handled gracefully by
     * saiSwitchIdQuery), and executeCallback() is a deliberate no-op.
     */
    class NotificationMetaFdbDebug:
        public Notification
    {
        public:

            NotificationMetaFdbDebug(
                    _In_ const std::string& op,
                    _In_ const std::string& data);

            virtual ~NotificationMetaFdbDebug() = default;

        public:

            virtual sai_object_id_t getSwitchId() const override;

            virtual sai_object_id_t getAnyObjectId() const override;

            virtual void processMetadata(
                    _In_ std::shared_ptr<saimeta::Meta> meta) const override;

            virtual void executeCallback(
                    _In_ const sai_switch_notifications_t& switchNotifications) const override;

        private:

            std::string m_op;

            std::string m_data;
    };
}
