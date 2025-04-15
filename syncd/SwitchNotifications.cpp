#include "SwitchNotifications.h"

#include "swss/logger.h"
#include <array>

using namespace syncd;

SwitchNotifications::SlotBase::SlotBase(
        _In_ const sai_switch_notifications_t& sn):
    m_handler(nullptr),
    m_sn(sn)
{
    SWSS_LOG_ENTER();

    // empty
}

SwitchNotifications::SlotBase::~SlotBase()
{
    SWSS_LOG_ENTER();

    // empty
}

void SwitchNotifications::SlotBase::setHandler(
        _In_ SwitchNotifications* handler)
{
    SWSS_LOG_ENTER();

    m_handler = handler;
}

SwitchNotifications* SwitchNotifications::SlotBase::getHandler() const
{
    SWSS_LOG_ENTER();

    return m_handler;
}


void SwitchNotifications::SlotBase::onFdbEvent(
        _In_ int context,
        _In_ uint32_t count,
        _In_ const sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    return m_slots.at(context)->m_handler->onFdbEvent(count,data);
}

void SwitchNotifications::SlotBase::onNatEvent(
        _In_ int context,
        _In_ uint32_t count,
        _In_ const sai_nat_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    return m_slots.at(context)->m_handler->onNatEvent(count,data);
}

void SwitchNotifications::SlotBase::onPortStateChange(
        _In_ int context,
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();

    return m_slots.at(context)->m_handler->onPortStateChange(count, data);
}

void SwitchNotifications::SlotBase::onPortHostTxReady(
        _In_ int context,
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_id_t port_id,
        _In_ sai_port_host_tx_ready_status_t host_tx_ready_status)
{
    SWSS_LOG_ENTER();

    return m_slots.at(context)->m_handler->onPortHostTxReady(switch_id, port_id, host_tx_ready_status);
}

void SwitchNotifications::SlotBase::onBfdSessionStateChange(
        _In_ int context,
        _In_ uint32_t count,
        _In_ const sai_bfd_session_state_notification_t *data)
{
    SWSS_LOG_ENTER();

    return m_slots.at(context)->m_handler->onBfdSessionStateChange(count, data);
}

void SwitchNotifications::SlotBase::onQueuePfcDeadlock(
        _In_ int context,
        _In_ uint32_t count,
        _In_ const sai_queue_deadlock_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    return m_slots[context]->m_handler->onQueuePfcDeadlock(count, data);
}
void SwitchNotifications::SlotBase::onSwitchAsicSdkHealthEvent(
        _In_ int context,
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_asic_sdk_health_severity_t severity,
        _In_ sai_timespec_t timestamp,
        _In_ sai_switch_asic_sdk_health_category_t category,
        _In_ sai_switch_health_data_t data,
        _In_ const sai_u8_list_t description)
{
    SWSS_LOG_ENTER();

    return m_slots.at(context)->m_handler->onSwitchAsicSdkHealthEvent(switch_id, severity, timestamp, category, data, description);
}

void SwitchNotifications::SlotBase::onSwitchShutdownRequest(
        _In_ int context,
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    return m_slots.at(context)->m_handler->onSwitchShutdownRequest(switch_id);
}

void SwitchNotifications::SlotBase::onSwitchStateChange(
        _In_ int context,
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    SWSS_LOG_ENTER();

    return m_slots.at(context)->m_handler->onSwitchStateChange(switch_id, switch_oper_status);
}

void SwitchNotifications::SlotBase::onTwampSessionEvent(
        _In_ int context,
        _In_ uint32_t count,
        _In_ const sai_twamp_session_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    return m_slots.at(context)->m_handler->onTwampSessionEvent(count, data);
}

void SwitchNotifications::SlotBase::onTamTelTypeConfigChange(
        _In_ int context,
        _In_ sai_object_id_t tam_tel_id)
{
    SWSS_LOG_ENTER();

    return m_slots.at(context)->m_handler->onTamTelTypeConfigChange(tam_tel_id);
}

const sai_switch_notifications_t& SwitchNotifications::SlotBase::getSwitchNotifications() const
{
    SWSS_LOG_ENTER();

    return m_sn;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winline"

template<class B, template<size_t> class D, size_t... i>
static constexpr auto declare_static(std::index_sequence<i...>)
{
    SWSS_LOG_ENTER();
    return std::array<B*, sizeof...(i)>{{new D<i>()...}};
}

template<class B, template<size_t> class D, size_t size>
static constexpr auto declare_static()
{
    SWSS_LOG_ENTER();
    auto arr = declare_static<B,D>(std::make_index_sequence<size>{});
    return std::vector<B*>{arr.begin(), arr.end()};
}

std::vector<SwitchNotifications::SlotBase*> SwitchNotifications::m_slots =
    declare_static<SwitchNotifications::SlotBase, SwitchNotifications::Slot, 0x10>();

#pragma GCC diagnostic pop

SwitchNotifications::SwitchNotifications()
{
    SWSS_LOG_ENTER();

    for (auto& slot: m_slots)
    {
        if (slot->getHandler() == nullptr)
        {
            m_slot = slot;

            m_slot->setHandler(this);

            return;
        }
    }

    SWSS_LOG_THROW("no more available slots, max slots: %zu", m_slots.size());
}

SwitchNotifications::~SwitchNotifications()
{
    SWSS_LOG_ENTER();

    m_slot->setHandler(nullptr);
}

const sai_switch_notifications_t& SwitchNotifications::getSwitchNotifications() const
{
    SWSS_LOG_ENTER();

    return m_slot->getSwitchNotifications();
}
