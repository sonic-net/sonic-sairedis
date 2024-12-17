#include "SwitchVpp.h"

#include "meta/sai_serialize.h"
#include "meta/NotificationPortStateChange.h"

#include "swss/logger.h"
#include "swss/exec.h"
#include "swss/converter.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>

#include "vppxlate/SaiVppXlate.h"

using namespace saivs;

sai_status_t SwitchVpp::addRemoveIpNbr(
        _In_ const std::string &serializedObjectId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    sai_neighbor_entry_t nbr_entry;

    sai_deserialize_neighbor_entry(serializedObjectId, nbr_entry);

    attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;

    CHECK_STATUS(get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nbr_entry.rif_id, 1, &attr));

    if (objectTypeQuery(attr.value.oid) != SAI_OBJECT_TYPE_PORT)
    {
        return SAI_STATUS_SUCCESS;
    }
    auto port_oid = attr.value.oid;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;

    CHECK_STATUS(get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nbr_entry.rif_id, 1, &attr));
    if (attr.value.s32 != SAI_ROUTER_INTERFACE_TYPE_SUB_PORT &&
        attr.value.s32 != SAI_ROUTER_INTERFACE_TYPE_PORT)
    {
        SWSS_LOG_NOTICE("Skipping neighbor add for attr type %d", attr.value.s32);

        return SAI_STATUS_SUCCESS;
    }

    uint16_t vlan_id = 0;
    if (attr.value.s32 == SAI_ROUTER_INTERFACE_TYPE_SUB_PORT)
    {
        attr.id = SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID;

        CHECK_STATUS(get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nbr_entry.rif_id, 1, &attr));
        vlan_id = attr.value.u16;
    }

    sai_mac_t nbr_mac;
    bool no_mac = true;

    if (is_add)
    {
        for (uint32_t i = 0; i < attr_count; i++)
        {
            switch (attr_list[i].id)
            {
            case SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS:
                memcpy(nbr_mac, attr_list[i].value.mac, sizeof(sai_mac_t));
                no_mac = false;
                break;

            default:
                break;
            }
        }
    } else {
        attr.id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;

        if (get(SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, serializedObjectId, 1, &attr) == SAI_STATUS_SUCCESS) {
            memcpy(nbr_mac, attr.value.mac, sizeof(sai_mac_t));
            no_mac = false;
        }
    }

    if (no_mac == true)
    {
        SWSS_LOG_ERROR("No mac address passed for neighbor %s", serializedObjectId.c_str());
        return SAI_STATUS_FAILURE;
    }

    std::string if_name;
    bool found = getTapNameFromPortId(port_oid, if_name);
    if (found == false)
    {
        SWSS_LOG_ERROR("host interface for port id %s not found", serializedObjectId.c_str());
        return SAI_STATUS_FAILURE;
    }

    const char *hwif_name = tap_to_hwif_name(if_name.c_str());
    const char *vpp_ifname;
    char subifname[32];

    if (vlan_id)
    {
        snprintf(subifname, sizeof(subifname), "%s.%u", hwif_name, vlan_id);

        vpp_ifname = subifname;
    } else {
        vpp_ifname = hwif_name;
    }
    init_vpp_client();

    switch (nbr_entry.ip_address.addr_family) {
    case SAI_IP_ADDR_FAMILY_IPV4:
        struct sockaddr_in sin;

        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = nbr_entry.ip_address.addr.ip4;

        ip4_nbr_add_del(vpp_ifname, ~0, &sin, false, false, nbr_mac, is_add);

        break;

    case SAI_IP_ADDR_FAMILY_IPV6:
        struct sockaddr_in6 sin6;

        sin6.sin6_family = AF_INET6;
        memcpy(sin6.sin6_addr.s6_addr, nbr_entry.ip_address.addr.ip6, sizeof(sin6.sin6_addr.s6_addr));

        ip6_nbr_add_del(vpp_ifname, ~0, &sin6, false, false, nbr_mac, is_add);

        break;
    }

    return SAI_STATUS_SUCCESS;
}

bool SwitchVpp::is_ip_nbr_active()
{
    SWSS_LOG_ENTER();

    if (nbr_env_read == false)
    {
        const char *val;

        val = getenv("NO_LINUX_NL");
        if (val && (*val == 'y' || *val == 'Y')) {
            nbr_active = true;
        }
        nbr_env_read = true;
    }
    return nbr_active;
}

sai_status_t SwitchVpp::addIpNbr(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (is_ip_nbr_active() == true) {
        SWSS_LOG_NOTICE("Add neighbor in VS %s", serializedObjectId.c_str());
        addRemoveIpNbr(serializedObjectId, attr_count, attr_list, true);
    }

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, serializedObjectId, switch_id, attr_count, attr_list));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeIpNbr(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    if (is_ip_nbr_active() == true) {
        SWSS_LOG_NOTICE("Remove neighbor in VS %s", serializedObjectId.c_str());
        addRemoveIpNbr(serializedObjectId, 0, NULL, false);
    }

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, serializedObjectId));

    return SAI_STATUS_SUCCESS;
}
