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

                    static void onNatEvent(
                            _In_ int context,
                            _In_ uint32_t count,
                            _In_ const sai_nat_event_notification_data_t *data);

                    static void onPortStateChange(
                            _In_ int context,
                            _In_ uint32_t count,
                            _In_ const sai_port_oper_status_notification_t *data);


                    static void onPortHostTxReady(
                            _In_ int context,
                            _In_ sai_object_id_t switch_id,
                            _In_ sai_object_id_t port_id,
                            _In_ sai_port_host_tx_ready_status_t host_tx_ready_status);

                    static void onQueuePfcDeadlock(
                            _In_ int context,
                            _In_ uint32_t count,
                            _In_ const sai_queue_deadlock_notification_data_t *data);

                    static void onSwitchAsicSdkHealthEvent(
                            _In_ int context,
                            _In_ sai_object_id_t switch_id,
                            _In_ sai_switch_asic_sdk_health_severity_t severity,
                            _In_ sai_timespec_t timestamp,
                            _In_ sai_switch_asic_sdk_health_category_t category,
                            _In_ sai_switch_health_data_t data,
                            _In_ const sai_u8_list_t description);

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

                    static void onIcmpEchoSessionStateChange(
                            _In_ int context,
                            _In_ uint32_t count,
                            _In_ const sai_icmp_echo_session_state_notification_t *data);

                    static void onTwampSessionEvent(
                            _In_ int context,
                            _In_ uint32_t count,
                            _In_ const sai_twamp_session_event_notification_data_t *data);

                    static void onTamTelTypeConfigChange(
                        _In_ int context,
                        _In_ sai_object_id_t tam_tel_id);

                    static void onHaSetEvent(
                            _In_ int context,
                            _In_ uint32_t count,
                            _In_ const sai_ha_set_event_data_t *data);

                    static void onHaScopeEvent(
                            _In_ int context,
                            _In_ uint32_t count,
                            _In_ const sai_ha_scope_event_data_t *data);

                    static void onMacsecPostStatus(
                          _In_ int context,
                          _In_ sai_object_id_t macsec_id,
                          _In_ sai_macsec_post_status_t post_status );

                    static void onIpsecPostStatus(
                          _In_ int context,
                          _In_ sai_object_id_t switch_id,
                          _In_ sai_ipsec_post_status_t post_status );

                    static void onSwitchMacsecPostStatus(
                          _In_ int context,
                          _In_ sai_object_id_t switch_id,
                          _In_ sai_switch_macsec_post_status_t post_status );

                    static void onSwitchIpsecPostStatus(
                          _In_ int context,
                          _In_ sai_object_id_t switch_id,
                          _In_ sai_switch_ipsec_post_status_t post_status );
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
                            .on_switch_state_change = &Slot<context>::onSwitchStateChange,
                            .on_switch_shutdown_request = &Slot<context>::onSwitchShutdownRequest,
                            .on_fdb_event = &Slot<context>::onFdbEvent,
                            .on_port_state_change = &Slot<context>::onPortStateChange,
                            .on_packet_event = nullptr,
                            .on_queue_pfc_deadlock = &Slot<context>::onQueuePfcDeadlock,
                            .on_bfd_session_state_change = &Slot<context>::onBfdSessionStateChange,
                            .on_tam_event = nullptr,
                            .on_ipsec_sa_status_change = nullptr,
                            .on_nat_event = &Slot<context>::onNatEvent,
                            .on_switch_asic_sdk_health_event = &Slot<context>::onSwitchAsicSdkHealthEvent,
                            .on_port_host_tx_ready = &Slot<context>::onPortHostTxReady,
                            .on_twamp_session_event = &Slot<context>::onTwampSessionEvent,
                            .on_icmp_echo_session_state_change = &Slot<context>::onIcmpEchoSessionStateChange,
                            .on_extended_port_state_change = nullptr,
                            .on_tam_tel_type_config_change = &Slot<context>::onTamTelTypeConfigChange,
                            .on_macsec_post_status = &Slot<context>::onMacsecPostStatus,
                            .on_ipsec_post_status = &Slot<context>::onIpsecPostStatus,
                            .on_switch_macsec_post_status = &Slot<context>::onSwitchMacsecPostStatus,
                            .on_switch_ipsec_post_status = &Slot<context>::onSwitchIpsecPostStatus,
                            .on_ha_set_event = &Slot<context>::onHaSetEvent,
                            .on_ha_scope_event = &Slot<context>::onHaScopeEvent,
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

                static void onNatEvent(
                        _In_ uint32_t count,
                        _In_ const sai_nat_event_notification_data_t *data)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onNatEvent(context, count, data);
                }

                static void onPortStateChange(
                        _In_ uint32_t count,
                        _In_ const sai_port_oper_status_notification_t *data)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onPortStateChange(context, count, data);
                }

                static void onPortHostTxReady(
                        _In_ sai_object_id_t switch_id,
                        _In_ sai_object_id_t port_id,
                        _In_ sai_port_host_tx_ready_status_t host_tx_ready_status)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onPortHostTxReady(context, switch_id, port_id, host_tx_ready_status);
                }

                static void onBfdSessionStateChange(
                        _In_ uint32_t count,
                        _In_ const sai_bfd_session_state_notification_t *data)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onBfdSessionStateChange(context, count, data);
                }

                static void onIcmpEchoSessionStateChange(
                        _In_ uint32_t count,
                        _In_ const sai_icmp_echo_session_state_notification_t *data)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onIcmpEchoSessionStateChange(context, count, data);
                }
                static void onHaSetEvent(
                        _In_ uint32_t count,
                        _In_ const sai_ha_set_event_data_t *data)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onHaSetEvent(context, count, data);
                }

                static void onHaScopeEvent(
                        _In_ uint32_t count,
                        _In_ const sai_ha_scope_event_data_t *data)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onHaScopeEvent(context, count, data);
                }

                static void onQueuePfcDeadlock(
                        _In_ uint32_t count,
                        _In_ const sai_queue_deadlock_notification_data_t *data)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onQueuePfcDeadlock(context, count, data);
                }

                static void onSwitchAsicSdkHealthEvent(
                        _In_ sai_object_id_t switch_id,
                        _In_ sai_switch_asic_sdk_health_severity_t severity,
                        _In_ sai_timespec_t timestamp,
                        _In_ sai_switch_asic_sdk_health_category_t category,
                        _In_ sai_switch_health_data_t data,
                        _In_ const sai_u8_list_t description)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onSwitchAsicSdkHealthEvent(context,
                                                                 switch_id,
                                                                 severity,
                                                                 timestamp,
                                                                 category,
                                                                 data,
                                                                 description);
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

                static void onTwampSessionEvent(
                        _In_ uint32_t count,
                        _In_ const sai_twamp_session_event_notification_data_t *data)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onTwampSessionEvent(context, count, data);
                }

                static void onTamTelTypeConfigChange(
                        _In_ sai_object_id_t tam_tel_id)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onTamTelTypeConfigChange(context, tam_tel_id);
                }

                static void onMacsecPostStatus(
                         _In_ sai_object_id_t macsec_id,
                         sai_macsec_post_status_t post_status)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onMacsecPostStatus(context, macsec_id, post_status);
                }

                static void onIpsecPostStatus(
                         _In_ sai_object_id_t switch_id,
                         sai_ipsec_post_status_t post_status)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onIpsecPostStatus(context, switch_id, post_status);
                }
                static void onSwitchMacsecPostStatus(
                         _In_ sai_object_id_t switch_id,
                         sai_switch_macsec_post_status_t post_status)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onSwitchMacsecPostStatus(context, switch_id, post_status);
                }
                static void onSwitchIpsecPostStatus(
                         _In_ sai_object_id_t switch_id,
                         _In_ sai_switch_ipsec_post_status_t post_status)
                {
                    SWSS_LOG_ENTER();

                    return SlotBase::onSwitchIpsecPostStatus(context, switch_id, post_status);
                }
        };

            static std::vector<SwitchNotifications::SlotBase*> m_slots;

        public:

            SwitchNotifications();

            virtual ~SwitchNotifications();

        public:

            const sai_switch_notifications_t& getSwitchNotifications() const;

        public: // wrapped methods

            std::function<void(uint32_t, const sai_fdb_event_notification_data_t*)>                 onFdbEvent;
            std::function<void(uint32_t, const sai_nat_event_notification_data_t*)>                 onNatEvent;
            std::function<void(uint32_t, const sai_port_oper_status_notification_t*)>               onPortStateChange;
            std::function<void(sai_object_id_t, sai_object_id_t, sai_port_host_tx_ready_status_t)>  onPortHostTxReady;
            std::function<void(uint32_t, const sai_queue_deadlock_notification_data_t*)>            onQueuePfcDeadlock;
            std::function<void(sai_object_id_t,
                               sai_switch_asic_sdk_health_severity_t,
                               sai_timespec_t,
                               sai_switch_asic_sdk_health_category_t,
                               sai_switch_health_data_t,
                               const sai_u8_list_t)>                                                onSwitchAsicSdkHealthEvent;
            std::function<void(sai_object_id_t)>                                                    onSwitchShutdownRequest;
            std::function<void(sai_object_id_t switch_id, sai_switch_oper_status_t)>                onSwitchStateChange;
            std::function<void(uint32_t, const sai_bfd_session_state_notification_t*)>              onBfdSessionStateChange;
            std::function<void(uint32_t, const sai_icmp_echo_session_state_notification_t*)>        onIcmpEchoSessionStateChange;
            std::function<void(uint32_t, const sai_twamp_session_event_notification_data_t*)>       onTwampSessionEvent;
            std::function<void(sai_object_id_t)>                                                    onTamTelTypeConfigChange;
            std::function<void(uint32_t, const sai_ha_set_event_data_t*)>                          onHaSetEvent;
            std::function<void(uint32_t, const sai_ha_scope_event_data_t*)>                        onHaScopeEvent;
            std::function<void(sai_object_id_t, const sai_macsec_post_status_t)>                   onMacsecPostStatus;
            std::function<void(sai_object_id_t, const sai_ipsec_post_status_t)>                    onIpsecPostStatus;
            std::function<void(sai_object_id_t, const sai_switch_macsec_post_status_t)>            onSwitchMacsecPostStatus;
            std::function<void(sai_object_id_t, const sai_switch_ipsec_post_status_t)>             onSwitchIpsecPostStatus;
    private:

            SlotBase*m_slot;
    };
}
