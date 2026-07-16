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

    /* Determine the RIF type first.  A VLAN SVI RIF has no
     * SAI_ROUTER_INTERFACE_ATTR_PORT_ID, so querying PORT_ID up-front
     * (as the PORT/SUB_PORT path does) fails for VLAN and would cause
     * the neighbor to be dropped before we ever look at the type.  The
     * kernel resolves VLAN host neighbors on the Vlan<id> netdev and
     * neighsyncd/orchagent programs them via SAI; they must be pushed
     * to VPP's bridge-virtual interface (bvi<vlanid>) or routed
     * uplink->downlink traffic has no adjacency on the BVI and is
     * silently dropped.
     */
    attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    CHECK_STATUS(get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nbr_entry.rif_id, 1, &attr));
    sai_int32_t rif_type = attr.value.s32;

    if (rif_type != SAI_ROUTER_INTERFACE_TYPE_SUB_PORT &&
        rif_type != SAI_ROUTER_INTERFACE_TYPE_PORT &&
        rif_type != SAI_ROUTER_INTERFACE_TYPE_VLAN)
    {
        SWSS_LOG_NOTICE("Skipping neighbor add for attr type %d", rif_type);

        return SAI_STATUS_SUCCESS;
    }

    sai_object_id_t port_oid = SAI_NULL_OBJECT_ID;
    uint16_t vlan_id = 0;

    if (rif_type == SAI_ROUTER_INTERFACE_TYPE_SUB_PORT ||
        rif_type == SAI_ROUTER_INTERFACE_TYPE_PORT)
    {
        attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;

        CHECK_STATUS(get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nbr_entry.rif_id, 1, &attr));

        auto port_obj_type = objectTypeQuery(attr.value.oid);
        if (port_obj_type != SAI_OBJECT_TYPE_PORT && port_obj_type != SAI_OBJECT_TYPE_LAG)
        {
            return SAI_STATUS_SUCCESS;
        }
        port_oid = attr.value.oid;

        if (rif_type == SAI_ROUTER_INTERFACE_TYPE_SUB_PORT)
        {
            attr.id = SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID;

            CHECK_STATUS(get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nbr_entry.rif_id, 1, &attr));
            vlan_id = attr.value.u16;
        }
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

    std::string hwif_name;

    if (rif_type == SAI_ROUTER_INTERFACE_TYPE_VLAN)
    {
        /* VLAN SVI: the L3 endpoint in VPP is the bridge-virtual
         * interface bvi<vlanid>.  Resolve the VLAN id from the RIF and
         * target the neighbor at that BVI so ip4/ip6-lookup on the BVI
         * builds a complete rewrite adjacency (dst = host mac,
         * tx = bvi<vlanid>).  BVI->BD delivery to the specific member
         * then happens via the l2fib (unicast if the host mac is
         * learned, flood otherwise), so no explicit l2fib entry is
         * required here.
         */
        attr.id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;
        CHECK_STATUS(get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nbr_entry.rif_id, 1, &attr));
        sai_object_id_t vlan_oid = attr.value.oid;

        attr.id = SAI_VLAN_ATTR_VLAN_ID;
        CHECK_STATUS(get(SAI_OBJECT_TYPE_VLAN, vlan_oid, 1, &attr));
        uint16_t bvi_vlan_id = attr.value.u16;

        hwif_name = std::string("bvi") + std::to_string(bvi_vlan_id);
        SWSS_LOG_NOTICE("Neighbor %s on VLAN RIF -> BVI %s (%s)",
                        serializedObjectId.c_str(), hwif_name.c_str(),
                        is_add ? "add" : "del");
    }
    else
    {
        bool found = vpp_get_hwif_name(port_oid, vlan_id, hwif_name);
        if (found == false)
        {
            SWSS_LOG_ERROR("hw interface for port/lag id %s not found", serializedObjectId.c_str());
            return SAI_STATUS_FAILURE;
        }
    }

    const char *vpp_ifname = hwif_name.c_str();
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
        if (val && (*val == 'n' || *val == 'N')) {
            nbr_active = false;
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
