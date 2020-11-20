#include "ServiceMethodTable.h"

#include "swss/logger.h"

#include <utility>

using namespace syncd;

ServiceMethodTable::SlotBase::SlotBase(
        _In_ sai_service_method_table_t smt):
    m_handler(nullptr),
    m_smt(smt)
{
    SWSS_LOG_ENTER();

    // empty
}

ServiceMethodTable::SlotBase::~SlotBase()
{
    SWSS_LOG_ENTER();

    // empty
}

void ServiceMethodTable::SlotBase::setHandler(
        _In_ ServiceMethodTable* handler)
{
    SWSS_LOG_ENTER();

    m_handler = handler;
}

ServiceMethodTable* ServiceMethodTable::SlotBase::getHandler() const
{
    SWSS_LOG_ENTER();

    return m_handler;
}

const char* ServiceMethodTable::SlotBase::profileGetValue(
        _In_ int context,
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    return m_slots.at(context)->m_handler->profileGetValue(profile_id, variable);
}

int ServiceMethodTable::SlotBase::profileGetNextValue(
        _In_ int context,
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    return m_slots.at(context)->m_handler->profileGetNextValue(profile_id, variable, value);
}

const sai_service_method_table_t& ServiceMethodTable::SlotBase::getServiceMethodTable() const
{
    SWSS_LOG_ENTER();

    return m_smt;
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

std::vector<ServiceMethodTable::SlotBase*> ServiceMethodTable::m_slots =
    declare_static<ServiceMethodTable::SlotBase, ServiceMethodTable::Slot, 10>();

#pragma GCC diagnostic pop

ServiceMethodTable::ServiceMethodTable()
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

ServiceMethodTable::~ServiceMethodTable()
{
    SWSS_LOG_ENTER();

    m_slot->setHandler(nullptr);
}

const sai_service_method_table_t& ServiceMethodTable::getServiceMethodTable() const
{
    SWSS_LOG_ENTER();

    return m_slot->getServiceMethodTable();
}
