#pragma once

extern "C" {
#include "saimetadata.h"
}

namespace saivs
{
    class Switch
    {
        public:

            Switch(
                    _In_ sai_object_id_t switchId);

            Switch(
                    _In_ sai_object_id_t switchId,
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t *attrList);

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
                    _In_ const sai_attribute_t *attrList);

            const sai_switch_notifications_t& getSwitchNotifications() const;

            sai_object_id_t getSwitchId() const;

        private:

            sai_object_id_t m_switchId;

            /**
             * @brief Notifications pointers holder.
             *
             * Each switch instance can have it's own notifications defined.
             */
            sai_switch_notifications_t m_switchNotifications;
    };
}
