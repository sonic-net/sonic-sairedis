#include "SwitchMrvlTL7.h"

#include "swss/logger.h"
#include "meta/sai_serialize.h"

using namespace saivs;

SwitchMrvlTL7::SwitchMrvlTL7(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config):
    SwitchStateBase(switch_id, manager, config)
{
    SWSS_LOG_ENTER();

    // empty
}

SwitchMrvlTL7::SwitchMrvlTL7(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config,
        _In_ std::shared_ptr<WarmBootState> warmBootState):
    SwitchStateBase(switch_id, manager, config, warmBootState)
{
    SWSS_LOG_ENTER();

    // empty
}

sai_status_t SwitchMrvlTL7::initialize_default_objects(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(set_switch_mac_address());
    CHECK_STATUS(create_cpu_port());
    CHECK_STATUS(create_default_vlan());
    CHECK_STATUS(create_default_virtual_router());
    CHECK_STATUS(create_default_stp_instance());
    CHECK_STATUS(create_default_1q_bridge());
    CHECK_STATUS(create_default_trap_group());
    CHECK_STATUS(create_ports());
    CHECK_STATUS(set_port_list());
    CHECK_STATUS(create_bridge_ports());
    CHECK_STATUS(create_vlan_members());
    CHECK_STATUS(set_switch_default_attributes());
    CHECK_STATUS(set_static_crm_values());

    return SAI_STATUS_SUCCESS;
}

