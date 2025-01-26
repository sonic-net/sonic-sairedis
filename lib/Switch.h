#pragma once

extern "C" {
#include "otaimetadata.h"
}

#include <string>

namespace otairedis
{
    class Switch
    {
        public:

            Switch(
                    _In_ otai_object_id_t switchId);

            Switch(
                    _In_ otai_object_id_t switchId,
                    _In_ uint32_t attrCount,
                    _In_ const otai_attribute_t *attrList);

            virtual ~Switch() = default;

        public:

            void clearNotificationsPointers();

            /**
             * @brief Update switch notifications from attribute list.
             *
             * A list of attributes which was passed to create switch API.
             */
            void updateNotifications(
                    _In_ uint32_t attrCount,
                    _In_ const otai_attribute_t *attrList);

            const otai_linecard_notifications_t& getSwitchNotifications() const;

            otai_object_id_t getSwitchId() const;

            const std::string& getHardwareInfo() const;

        private:

            otai_object_id_t m_switchId;

            /**
             * @brief Notifications pointers holder.
             *
             * Each switch instance can have it's own notifications defined.
             */
            otai_linecard_notifications_t m_switchNotifications;

            std::string m_hardwareInfo;
    };
}
