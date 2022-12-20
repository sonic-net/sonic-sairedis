#include "MockSwitchStateBase.h"

using namespace saivs;

MockSwitchStateBase::MockSwitchStateBase(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config):
    SwitchStateBase(switch_id, manager, config)
{
    SWSS_LOG_ENTER();
}

int MockSwitchStateBase::call_vs_create_tap_device(
        _In_ const char *dev,
        _In_ int flags)
{
    SWSS_LOG_ENTER();

    return SwitchStateBase::vs_create_tap_device(dev, flags);
}

int MockSwitchStateBase::call_ifup(
        _In_ const char *dev,
        _In_ sai_object_id_t port_id,
        _In_ bool up,
        _In_ bool explicitNotification)
{
    SWSS_LOG_ENTER();

    return ifup(dev, port_id, up, explicitNotification);
}

bool MockSwitchStateBase::call_hostif_create_tap_veth_forwarding(
        _In_ const std::string &tapname,
        _In_ int tapfd,
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    return hostif_create_tap_veth_forwarding(tapname, tapfd, port_id);
}

sai_status_t MockSwitchStateBase::call_vs_create_hostif_tap_interface(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return vs_create_hostif_tap_interface(attr_count, attr_list);
}
