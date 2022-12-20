#include "SwitchStateBase.h"

namespace saivs
{
    class MockSwitchStateBase:
        SwitchStateBase
    {
        public:

            MockSwitchStateBase(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config);

        public:

            static int call_vs_create_tap_device(
                    _In_ const char *dev,
                    _In_ int flags);

            int call_ifup(
                    _In_ const char *dev,
                    _In_ sai_object_id_t port_id,
                    _In_ bool up,
                    _In_ bool explicitNotification);

            bool call_hostif_create_tap_veth_forwarding(
                    _In_ const std::string &tapname,
                    _In_ int tapfd,
                    _In_ sai_object_id_t port_id);

            sai_status_t call_vs_create_hostif_tap_interface(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

    };
}
