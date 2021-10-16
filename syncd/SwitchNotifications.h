#pragma once

extern "C"{
#include "saimetadata.h"
}

#include "swss/logger.h"

#include <functional>
#include <vector>

namespace syncd
{
    class SwitchNotifications
    {
        private:

            class SlotBase
            {
                public:

                    SlotBase(
                            _In_ const sai_switch_notifications_t& sn);

                    virtual ~SlotBase();

                public:

                    void setHandler(
                            _In_ SwitchNotifications* handler);

                    SwitchNotifications* getHandler() const;

                    const sai_switch_notifications_t& getSwitchNotifications() const;

                protected:

                    static void onFdbEvent(
                            _In_ int context,
                            _In_ uint32_t count,
                            _In_ const sai_fdb_event_notification_data_t *data);

                    static void onPortStateChange(
                            _In_ int context,
                            _In_ uint32_t count,
                            _In_ const sai_port_oper_status_notification_t *data);

                    static void onQueuePfcDeadlock(
                            _In_ int context,
                            _In_ uint32_t count,
                            _In_ const sai_queue_deadlock_notification_data_t *data);

                    static void onSwitchShutdownRequest(
                            _In_ int context,
                            _In_ sai_object_id_t switch_id);

                    static void onSwitchStateChange(
                            _In_ int context,
                            _In_ sai_object_id_t switch_id,
                            _In_ sai_switch_oper_status_t switch_oper_status);

                    static void onBfdSessionStateChange(
                            _In_ int context,
                            _In_ uint32_t count,
                            _In_ const sai_bfd_session_state_notification_t *data);

                protected:

                    SwitchNotifications* m_handler;

                    sai_switch_notifications_t m_sn;
            };

            template<size_t context>
                class Slot:
                    public SlotBase
        {
            public:

                Slot():
                    SlotBase({
                            .on_bfd_session_state_change = &Slot<context>::onBfdSessionStateChange,
                            .on_fdb_event = &Slot<context>::onFdbEvent,
                            .on_ipsec_sa_status_change = nullptr,
                            .on_packet_event = nullptr,
                            .on_port_state_change = &Slot<context>::onPortStateChange,
                            .on_queue_pfc_deadlock = &Slot<context>::onQueuePfcDeadlock,
                            .on_switch_shutdown_request = &Slot<context>::onSwitchShutdownRequest,
                            .on_switch_state_change = &Slot<context>::onSwitchStateChange,
                            .on_tam_event = nullptr,
                            }) { }

                virtual ~Slot() {}

            private:

                static void onFdbEvent(
                        _In_ uint32_t count,
                        _In_ const sai_fdb_event_notification_data_t *data)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onFdbEvent(context, count, data);
                }

                static void onPortStateChange(
                        _In_ uint32_t count,
                        _In_ const sai_port_oper_status_notification_t *data)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onPortStateChange(context, count, data);
                }

                static void onBfdSessionStateChange(
                        _In_ uint32_t count,
                        _In_ const sai_bfd_session_state_notification_t *data)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onBfdSessionStateChange(context, count, data);
                }

                static void onQueuePfcDeadlock(
                        _In_ uint32_t count,
                        _In_ const sai_queue_deadlock_notification_data_t *data)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onQueuePfcDeadlock(context, count, data);
                }

                static void onSwitchShutdownRequest(
                        _In_ sai_object_id_t switch_id)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onSwitchShutdownRequest(context, switch_id);
                }

                static void onSwitchStateChange(
                        _In_ sai_object_id_t switch_id,
                        _In_ sai_switch_oper_status_t switch_oper_status)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onSwitchStateChange(context, switch_id, switch_oper_status);
                }
        };

            static std::vector<SwitchNotifications::SlotBase*> m_slots;

        public:

            SwitchNotifications();

            virtual ~SwitchNotifications();

        public:

            const sai_switch_notifications_t& getSwitchNotifications() const;

        public: // wrapped methods

            std::function<void(uint32_t, const sai_fdb_event_notification_data_t*)>         onFdbEvent;
            std::function<void(uint32_t, const sai_port_oper_status_notification_t*)>       onPortStateChange;
            std::function<void(uint32_t, const sai_queue_deadlock_notification_data_t*)>    onQueuePfcDeadlock;
            std::function<void(sai_object_id_t)>                                            onSwitchShutdownRequest;
            std::function<void(sai_object_id_t switch_id, sai_switch_oper_status_t)>        onSwitchStateChange;
            std::function<void(uint32_t, const sai_bfd_session_state_notification_t*)>      onBfdSessionStateChange;

        private:

            SlotBase*m_slot;
    };
}
