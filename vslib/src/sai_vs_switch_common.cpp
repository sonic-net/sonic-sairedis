#include "sai_vs.h"
#include "sai_vs_switch_common.h"

#include <fstream>
#include <cstdio>

#include <net/if.h>

static int get_default_gw_mac_address(sai_mac_t* mac)
{
    SWSS_LOG_ENTER();

    auto file = std::ifstream("/proc/net/route");
    if (!file)
    {
        return -1;
    }

    std::string buf;
    while (std::getline(file, buf))
    {
        char iface[IF_NAMESIZE];
        long destination, gateway;
        if (std::sscanf(buf.c_str(), "%s %lx %lx", iface, &destination, &gateway) == 3)
        {
            if (destination == 0)
            { /* default */
                file = std::ifstream("/sys/class/net/" + std::string(iface) + "/address");
                if ( !file )
                {
                    return -1;
                }
                file >> buf;
                return std::sscanf(buf.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &(*mac)[0], &(*mac)[1], &(*mac)[2], &(*mac)[3], &(*mac)[4], &(*mac)[5]) == 6 ? 0 : -1;
            }
        }
    }

    return -1;
}

sai_status_t set_switch_mac_address(std::shared_ptr<SwitchState> ss)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create switch src mac address");

    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;

    if ( get_default_gw_mac_address(&attr.value.mac) < 0 ) {
        attr.value.mac[0] = 0x22;
        attr.value.mac[1] = 0x33;
        attr.value.mac[2] = 0x44;
        attr.value.mac[3] = 0x55;
        attr.value.mac[4] = 0x66;
        attr.value.mac[5] = 0x77;
    }

    return vs_generic_set(SAI_OBJECT_TYPE_SWITCH, ss->getSwitchId(), &attr);
}
