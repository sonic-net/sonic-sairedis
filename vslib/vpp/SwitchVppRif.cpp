#include "SwitchVpp.h"
#include "SwitchVppUtils.h"

#include "meta/sai_serialize.h"
#include "meta/NotificationPortStateChange.h"

#include "swss/logger.h"
#include "swss/exec.h"
#include "swss/converter.h"

#include <arpa/inet.h>
#include <sys/ioctl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/if_tun.h>

#include "vppxlate/SaiVppXlate.h"

#include <iostream>
#include <cstring>
#include <cerrno>
#include <regex>
#include <fstream>
#include <unistd.h>
#include <thread>
#include <chrono>

using namespace saivs;

/* Forward declarations for static helpers defined later in this file. */
static void vpp_warm_arp_for_subnet_peers(const std::string &netdev,
                                          const swss::IpPrefix &ip_prefix);

/*
 * Naming helpers for LAG sub-port code. The VPP / LCP-tap / SONiC-teamd
 * trio of names ("BondEthernet<id>[.<vlan>]" / "be<id>[.<vlan>]" /
 * "PortChannel<id>[.<vlan>]") was previously open-coded via snprintf at
 * 5+ call sites. Centralize the format strings here so future changes
 * (e.g. larger bond ids, name prefix updates) only need to touch one
 * place. vlan_id == 0 means "no sub-interface suffix".
 */
static inline void format_bond_hwif(char *buf, size_t len,
                                    uint32_t bond_id, uint32_t vlan_id)
{
    SWSS_LOG_ENTER();

    if (vlan_id)
    {
        snprintf(buf, len, "%s%u.%u", BONDETHERNET_PREFIX, bond_id, vlan_id);
    }
    else
    {
        snprintf(buf, len, "%s%u", BONDETHERNET_PREFIX, bond_id);
    }
}

static inline void format_be_lcp(char *buf, size_t len,
                                 uint32_t bond_id, uint32_t vlan_id)
{
    SWSS_LOG_ENTER();

    if (vlan_id)
    {
        snprintf(buf, len, "be%u.%u", bond_id, vlan_id);
    }
    else
    {
        snprintf(buf, len, "be%u", bond_id);
    }
}

static inline std::string format_portchannel_name(uint32_t bond_id,
                                                  uint32_t vlan_id = 0)
{
    std::string s = std::string(PORTCHANNEL_PREFIX) + std::to_string(bond_id);
    if (vlan_id)
    {
        s += "." + std::to_string(vlan_id);
    }
    return s;
}

int SwitchVpp::currentMaxInstance = 0;

// TODO to cpp
IpVrfInfo::IpVrfInfo(
    _In_ sai_object_id_t obj_id,
    _In_ uint32_t vrf_id,
    _In_ std::string &vrf_name,
    _In_ bool is_ipv6):
    m_obj_id(obj_id),
    m_vrf_id(vrf_id),
    m_vrf_name(vrf_name),
    m_is_ipv6(is_ipv6)
{
    SWSS_LOG_ENTER();
}

IpVrfInfo::~IpVrfInfo()
{
    SWSS_LOG_ENTER();
}

bool vpp_get_intf_all_ip_prefixes (
    const std::string& linux_ifname,
    std::vector<swss::IpPrefix>& ip_prefixes)
{
    SWSS_LOG_ENTER();

    std::stringstream cmd;
    std::string res;

    cmd << IP_CMD << " addr show dev " << linux_ifname << " scope global | awk '/inet/ {print $2}'";

    int ret = swss::exec(cmd.str(), res);
    if (ret)
    {
        SWSS_LOG_ERROR("Command '%s' failed with rc %d", cmd.str().c_str(), ret);
        return false;
    }
    std::vector<std::string> ipAddresses;
    std::istringstream iss(res);
    std::string line;
    while (std::getline(iss, line)) {
        swss::IpPrefix prefix(line);
        ip_prefixes.push_back(prefix);
    }
    return true;
}

bool vpp_get_intf_ip_address (
    const char *linux_ifname,
    sai_ip_prefix_t& ip_prefix,
    bool is_v6,
    std::string& res)
{
    SWSS_LOG_ENTER();

    std::stringstream cmd;

    swss::IpPrefix prefix = getIpPrefixFromSaiPrefix(ip_prefix);

    if (is_v6)
    {
        cmd << IP_CMD << " -6 " << " addr show dev " << linux_ifname << " to " << prefix.to_string() << " scope global | awk '/inet6 / {print $2}'";
    } else {
        cmd << IP_CMD << " addr show dev " << linux_ifname << " to " << prefix.to_string() << " scope global | awk '/inet / {print $2}'";
    }
    int ret = swss::exec(cmd.str(), res);
    if (ret)
    {
        SWSS_LOG_ERROR("Command '%s' failed with rc %d", cmd.str().c_str(), ret);
        return false;
    }

    if (res.length() != 0)
    {
        SWSS_LOG_NOTICE("%s address of %s is %s", (is_v6 ? "IPv6" : "IPv4"), linux_ifname, res.c_str());
        return true;
    } else {
        return false;
    }
}

bool vpp_get_intf_name_for_prefix (
    sai_ip_prefix_t& ip_prefix,
    bool is_v6,
    std::string& ifname)
{
    SWSS_LOG_ENTER();

    std::stringstream cmd;

    swss::IpPrefix prefix = getIpPrefixFromSaiPrefix(ip_prefix);

    if (is_v6)
    {
        cmd << IP_CMD << " -6 " << " addr show " << " to " << prefix.to_string();
        cmd << " scope global | awk -F':' '/[0-9]+: [a-zA-Z]+/ { printf \"%s\", $2 }' | cut -d' ' -f2 -z | sed 's/@[a-zA-Z].*//g'";
    } else {
        cmd << IP_CMD << " addr show " << " to " << prefix.to_string();
        cmd << " scope global | awk -F':' '/[0-9]+: [a-zA-Z]+/ { printf \"%s\", $2 }' | cut -d' ' -f2 -z | sed 's/@[a-zA-Z].*//g'";
    }
    int ret = swss::exec(cmd.str(), ifname);
    if (ret)
    {
        SWSS_LOG_ERROR("Command '%s' failed with rc %d", cmd.str().c_str(), ret);
        return false;
    }

    if (ifname.length() != 0)
    {
        SWSS_LOG_NOTICE("%s interface name with prefix %s is %s", (is_v6 ? "IPv6" : "IPv4"), prefix.to_string().c_str(), ifname.c_str());
        return true;
    } else {
        return false;
    }
}

// wrapper for vpp_get_intf_name_for_prefix
std::string get_intf_name_for_prefix (
    _In_ sai_route_entry_t& route_entry)
{
    SWSS_LOG_ENTER();

    bool is_v6 = false;
    is_v6 = (route_entry.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV6) ? true : false;

    std::string full_if_name = "";
    bool found = vpp_get_intf_name_for_prefix(route_entry.destination, is_v6, full_if_name);
    if (found == false)
    {
        auto prefix_str = sai_serialize_ip_prefix(route_entry.destination);
        SWSS_LOG_INFO("host interface for prefix not found: %s", prefix_str.c_str());
    }
    return full_if_name;

}

// Function to convert an IPv4 address from unsigned integer to string representation
std::string SwitchVpp::convertIPToString (
        _In_ const sai_ip_addr_t &ipAddress)
{
    SWSS_LOG_ENTER();

    char ipStr[INET6_ADDRSTRLEN];

    if (inet_ntop(AF_INET, &(ipAddress.ip4), ipStr, INET_ADDRSTRLEN) != nullptr) {
        // IPv4 address
        return std::string(ipStr);
    } else if (inet_ntop(AF_INET6, &(ipAddress.ip6), ipStr, INET6_ADDRSTRLEN) != nullptr) {
        // IPv6 address
        return std::string(ipStr);
    }

    // Unsupported address family or conversion failure
    return "";
}

// Function to convert an IPv6 address from unsigned integer to string representation
std::string SwitchVpp::convertIPv6ToString (
        _In_ const sai_ip_addr_t &ipAddress,
        _In_ int ipFamily)
{
    SWSS_LOG_ENTER();

    if (ipFamily == AF_INET) {
        // IPv4 address
        char ipStr[INET_ADDRSTRLEN];
        struct sockaddr_in sa;
        sa.sin_family = AF_INET;
        memcpy(&sa.sin_addr, &(ipAddress.ip4), 4);

        if (inet_ntop(AF_INET, &(sa.sin_addr), ipStr, INET_ADDRSTRLEN) != nullptr)
        {
            return std::string(ipStr);
        }

    } else {
        // IPv6 address
        char ipStr[INET6_ADDRSTRLEN];
        struct sockaddr_in6 sa6;
        sa6.sin6_family = AF_INET6;
        memcpy(&sa6.sin6_addr, &(ipAddress.ip6), 16);

        if (inet_ntop(AF_INET6, &(sa6.sin6_addr), ipStr, INET6_ADDRSTRLEN) != nullptr)
        {
            return std::string(ipStr);
        }
    }

    // Conversion failure
    SWSS_LOG_ERROR("Failed to convert IPv6 address to string");
    return "";
}

std::string SwitchVpp::extractDestinationIP (
    const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_route_entry_t routeEntry;
    sai_deserialize_route_entry(serializedObjectId, routeEntry);

    std::string destIPAddress = "";
    if (routeEntry.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
       destIPAddress = convertIPToString(routeEntry.destination.addr);
    } else if (routeEntry.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
       destIPAddress = convertIPv6ToString(routeEntry.destination.addr, routeEntry.destination.addr_family);
    } else {
        SWSS_LOG_ERROR("Could not determine IP address family!  destIPStream:%s", destIPAddress.c_str());
    }
    return destIPAddress;
}

void create_route_prefix (
    sai_route_entry_t *route_entry,
    vpp_ip_route_t *ip_route)
{
    SWSS_LOG_ENTER();

    const sai_ip_prefix_t *ip_address = &route_entry->destination;

    switch (ip_address->addr_family) {
    case SAI_IP_ADDR_FAMILY_IPV4:
    {
        struct sockaddr_in *sin =  &ip_route->prefix_addr.addr.ip4;

        ip_route->prefix_addr.sa_family = AF_INET;
        sin->sin_addr.s_addr = ip_address->addr.ip4;
        ip_route->prefix_len = getPrefixLenFromAddrMask(reinterpret_cast<const uint8_t*>(&ip_address->mask.ip4), 4);

        break;
    }
    case SAI_IP_ADDR_FAMILY_IPV6:
    {
        struct sockaddr_in6 *sin6 =  &ip_route->prefix_addr.addr.ip6;

        ip_route->prefix_addr.sa_family = AF_INET6;
        memcpy(sin6->sin6_addr.s6_addr, ip_address->addr.ip6, sizeof(sin6->sin6_addr.s6_addr));
        ip_route->prefix_len = getPrefixLenFromAddrMask(ip_address->mask.ip6, 16);

        break;
    }
    }
}

int SwitchVpp::getNextLoopbackInstance ()
{
    SWSS_LOG_ENTER();

    int nextInstance = 0;

    if (!availableInstances.empty()) {
        nextInstance = *availableInstances.begin();
        availableInstances.erase(availableInstances.begin());
    } else {
        nextInstance = currentMaxInstance;
        ++currentMaxInstance;
    }

    SWSS_LOG_DEBUG("Next Loopback Instance:%u", nextInstance);

    return nextInstance;
}

void SwitchVpp::markLoopbackInstanceDeleted (int instance)
{
    SWSS_LOG_ENTER();

    availableInstances.insert(instance);
}

bool SwitchVpp::vpp_intf_get_prefix_entry (const std::string &intf_name, std::string &ip_prefix)
{
    SWSS_LOG_ENTER();

    auto it = m_intf_prefix_map.find(intf_name);

    if (it == m_intf_prefix_map.end())
    {
        SWSS_LOG_NOTICE("failed to ip prefix entry for hostif device: %s", intf_name.c_str());

        return false;
    }
    SWSS_LOG_NOTICE("Found ip prefix %s for hostif device: %s", it->second.c_str(), intf_name.c_str());

    ip_prefix = it->second;

    return true;
}

void SwitchVpp::vpp_intf_remove_prefix_entry (const std::string& intf_name)
{
    SWSS_LOG_ENTER();

    auto it = m_intf_prefix_map.find(intf_name);

    if (it == m_intf_prefix_map.end())
    {
        SWSS_LOG_ERROR("failed to ip prefix entry for hostif device: %s", intf_name.c_str());

        return;
    }
    SWSS_LOG_NOTICE("Removing ip prefix %s for hostif device: %s", it->second.c_str(), intf_name.c_str());

    m_intf_prefix_map.erase(it);
}

bool SwitchVpp::vpp_get_hwif_name (
      _In_ sai_object_id_t object_id,
      _In_ uint32_t vlan_id,
      _Out_ std::string& ifname)
{
    SWSS_LOG_ENTER();

    const char *hwifname = nullptr;
    char hw_bondifname[32];

    if (objectTypeQuery(object_id) == SAI_OBJECT_TYPE_LAG) {
        platform_bond_info_t bond_info;
        sai_status_t status = get_lag_bond_info(object_id, bond_info);
        if (status != SAI_STATUS_SUCCESS)
        {
            return false;
        }
        format_bond_hwif(hw_bondifname, sizeof(hw_bondifname), bond_info.id, 0);
        hwifname = hw_bondifname;
    } else {
        std::string if_name;
        bool found = getTapNameFromPortId(object_id, if_name);

        if (found == false)
        {
            SWSS_LOG_ERROR("host interface for port id %s not found", sai_serialize_object_id(object_id).c_str());
            return false;
        }
        hwifname = tap_to_hwif_name(if_name.c_str());
    }

    if (!hwifname) return false;

    char hw_subifname[64];
    const char *hw_ifname;

    if (vlan_id) {
        snprintf(hw_subifname, sizeof(hw_subifname), "%s.%u", hwifname, vlan_id);
        hw_ifname = hw_subifname;
    } else {
        hw_ifname = hwifname;
    }
    ifname = std::string(hw_ifname);

    return true;
}

void SwitchVpp::vppProcessEvents ()
{
    SWSS_LOG_ENTER();

    const struct timespec req = {2, 0};
    vpp_event_info_t *evp;
    int ret;

    while(m_run_vpp_events_thread) {
        nanosleep(&req, NULL);
        ret = vpp_sync_for_events();
        SWSS_LOG_NOTICE("Checking for any VS events status %d", ret);
        while ((evp = vpp_ev_dequeue())) {
            if (evp->type == VPP_INTF_LINK_STATUS) {
                asyncIntfStateUpdate(evp->data.intf_status.hwif_name,
                                     evp->data.intf_status.link_up);
                SWSS_LOG_NOTICE("Received port link event for %s state %s",
                                evp->data.intf_status.hwif_name,
                                evp->data.intf_status.link_up ? "UP" : "DOWN");
            } else if (evp->type == VPP_BFD_STATE_CHANGE) {
                SWSS_LOG_NOTICE("Received bfd state change event, multihop:%d, "
                                "sw_idx:%d, state %d",
                                evp->data.bfd_notif.multihop,
                                evp->data.bfd_notif.sw_if_index,
                                evp->data.bfd_notif.state);
                asyncBfdStateUpdate(&evp->data.bfd_notif);
            }
            vpp_ev_free(evp);
        }
    }
}

sai_status_t SwitchVpp::asyncBfdStateUpdate(vpp_bfd_state_notif_t *bfd_notif)
{
    SWSS_LOG_ENTER();

    sai_bfd_session_state_t sai_state;
    vpp_bfd_info_t bfd_info;
    memset(&bfd_info, 0, sizeof(bfd_info));

    // Convert vpp state to sai state
    switch(bfd_notif->state) {
    case VPP_API_BFD_STATE_ADMIN_DOWN:
        sai_state = SAI_BFD_SESSION_STATE_ADMIN_DOWN;
        break;
    case VPP_API_BFD_STATE_DOWN:
        sai_state = SAI_BFD_SESSION_STATE_DOWN;
        break;
    case VPP_API_BFD_STATE_INIT:
        sai_state = SAI_BFD_SESSION_STATE_INIT;
        break;
    case VPP_API_BFD_STATE_UP:
        sai_state = SAI_BFD_SESSION_STATE_UP;
        break;
    default:
        sai_state = SAI_BFD_SESSION_STATE_DOWN;
        break;
    }

    bfd_info.multihop = bfd_notif->multihop;
    vpp_ip_addr_t_to_sai_ip_address_t(bfd_notif->local_addr, bfd_info.local_addr);
    vpp_ip_addr_t_to_sai_ip_address_t(bfd_notif->peer_addr, bfd_info.peer_addr);

    BFD_MUTEX;

    // Find the BFD session
    auto it = m_bfd_info_map.find(bfd_info);

    // Check if the key was found
    if (it != m_bfd_info_map.end()) {
        sai_object_id_t bfd_oid = it->second;
        sai_object_type_t obj_type = objectTypeQuery(bfd_oid);
       SWSS_LOG_NOTICE("Found existing bfd object %s, type %s",
            sai_serialize_object_id(bfd_oid).c_str(),
            sai_serialize_object_type(obj_type).c_str());
        send_bfd_state_change_notification(bfd_oid, sai_state, false);
    } else {
        SWSS_LOG_NOTICE("Existing bfd object not found");
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_dp_initialize()
{
    SWSS_LOG_ENTER();

    init_vpp_client();
    m_vpp_thread = std::make_shared<std::thread>(&SwitchVpp::vppProcessEvents, this);

    VppEventsThreadStarted = true;

    SWSS_LOG_NOTICE("VS DP initialized");

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::asyncIntfStateUpdate(const char *hwif_name, bool link_up)
{
    SWSS_LOG_ENTER();

    std::string tap_str;
    const char *tap;

    tap = hwif_to_tap_name(hwif_name);
    auto port_oid = getPortIdFromIfName(std::string(tap));

    if (port_oid == SAI_NULL_OBJECT_ID) {
        SWSS_LOG_NOTICE("Failed find port oid for tap interface %s. Ignore the update.", tap);
        return SAI_STATUS_SUCCESS;
    }

    auto state = link_up ? SAI_PORT_OPER_STATUS_UP : SAI_PORT_OPER_STATUS_DOWN;

    send_port_oper_status_notification(port_oid, state, false);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_set_interface_state (
        _In_ sai_object_id_t object_id,
        _In_ uint32_t vlan_id,
        _In_ bool is_up)
{
    SWSS_LOG_ENTER();

    if (is_ip_nbr_active() == false) {
        return SAI_STATUS_SUCCESS;
    }

    std::string ifname;

    if (vpp_get_hwif_name(object_id, vlan_id, ifname) == true) {
        const char *hwif_name = ifname.c_str();

        interface_set_state(hwif_name, is_up);
        SWSS_LOG_NOTICE("Updating router interface admin state %s %s", hwif_name,
                        (is_up ? "UP" : "DOWN"));
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_set_port_mtu (
        _In_ sai_object_id_t object_id,
        _In_ uint32_t vlan_id,
        _In_ uint32_t mtu)
{
    SWSS_LOG_ENTER();

    if (is_ip_nbr_active() == false) {
        return SAI_STATUS_SUCCESS;
    }

    std::string ifname;

    if (vpp_get_hwif_name(object_id, vlan_id, ifname) == true) {
        const char *hwif_name = ifname.c_str();

        hw_interface_set_mtu(hwif_name, mtu);
        SWSS_LOG_NOTICE("Updating router interface mtu %s to %u", hwif_name,
                        mtu);
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_set_interface_mtu (
        _In_ sai_object_id_t object_id,
        _In_ uint32_t vlan_id,
        _In_ uint32_t mtu)
{
    SWSS_LOG_ENTER();

    if (is_ip_nbr_active() == false) {
        return SAI_STATUS_SUCCESS;
    }

    std::string ifname;

    if (vpp_get_hwif_name(object_id, vlan_id, ifname) == true) {
        const char *hwif_name = ifname.c_str();

        sw_interface_set_mtu(hwif_name, mtu);
        SWSS_LOG_NOTICE("Updating router interface mtu %s to %u", hwif_name, mtu);
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::UpdatePort(
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto attr_type = sai_metadata_get_attr_by_id(SAI_PORT_ATTR_INGRESS_ACL, attr_count, attr_list);

    if (attr_type != NULL)
    {
        if (attr_type->value.oid == SAI_NULL_OBJECT_ID) {
            sai_attribute_t attr;

            attr.id = SAI_PORT_ATTR_INGRESS_ACL;
            if (get(SAI_OBJECT_TYPE_PORT, object_id, 1, &attr) != SAI_STATUS_SUCCESS) {
                aclBindUnbindPort(object_id, attr.value.oid, true, false);
            }
        } else {
            aclBindUnbindPort(object_id, attr_type->value.oid, true, true);
        }
    }

    attr_type = sai_metadata_get_attr_by_id(SAI_PORT_ATTR_EGRESS_ACL, attr_count, attr_list);

    if (attr_type != NULL)
    {
        if (attr_type->value.oid == SAI_NULL_OBJECT_ID) {
            sai_attribute_t attr;

            attr.id = SAI_PORT_ATTR_EGRESS_ACL;
            if (get(SAI_OBJECT_TYPE_PORT, object_id, 1, &attr) != SAI_STATUS_SUCCESS) {
                aclBindUnbindPort(object_id, attr.value.oid, false, false);
            }
        } else {
            aclBindUnbindPort(object_id, attr_type->value.oid, false, true);
        }
    }

    if (is_ip_nbr_active() == false) {
        return SAI_STATUS_SUCCESS;
    }

    attr_type = sai_metadata_get_attr_by_id(SAI_PORT_ATTR_ADMIN_STATE, attr_count, attr_list);

    if (attr_type != NULL)
    {
        vpp_set_interface_state(object_id, 0, attr_type->value.booldata);
    }

    attr_type = sai_metadata_get_attr_by_id(SAI_PORT_ATTR_MTU, attr_count, attr_list);

    if (attr_type != NULL)
    {
        vpp_set_port_mtu(object_id, 0, attr_type->value.u32);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_add_del_intf_ip_addr (
    _In_ sai_ip_prefix_t& ip_prefix,
    _In_ sai_object_id_t rif_id,
    _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    int32_t rif_type;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    sai_status_t status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_TYPE was not passed");

        return SAI_STATUS_FAILURE;
    }
    rif_type = attr.value.s32;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
    status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_PORT_ID was not passed");

        return SAI_STATUS_FAILURE;
    }

    sai_object_id_t obj_id = attr.value.oid;

    sai_object_type_t ot = objectTypeQuery(obj_id);

    if (ot == SAI_OBJECT_TYPE_VLAN)
    {
        SWSS_LOG_DEBUG("Skipping object type VLAN");
        return SAI_STATUS_SUCCESS;
    }

    if (ot != SAI_OBJECT_TYPE_PORT && ot != SAI_OBJECT_TYPE_LAG)
    {
        SWSS_LOG_ERROR("SAI_ROUTER_INTERFACE_ATTR_PORT_ID=%s expected to be PORT or LAG but is: %s",
                sai_serialize_object_id(obj_id).c_str(),
                sai_serialize_object_type(ot).c_str());

        return SAI_STATUS_FAILURE;
    }

    if (rif_type != SAI_ROUTER_INTERFACE_TYPE_SUB_PORT &&
        rif_type != SAI_ROUTER_INTERFACE_TYPE_PORT &&
        rif_type != SAI_ROUTER_INTERFACE_TYPE_LOOPBACK)
    {
        return SAI_STATUS_SUCCESS;
    }

    attr.id = SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID;
    status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr);

    uint16_t vlan_id = 0;
    if (status == SAI_STATUS_SUCCESS)
    {
        vlan_id = attr.value.u16;
    }

    /* Resolve host kernel interface name. For LAG, the team device is
     * "PortChannel<bond_id>" and the VPP-created host vlan sub-interface
     * (via lcp-auto-subint) is "PortChannel<bond_id>.<vlan_id>", which is
     * exactly where IntfMgr writes the IP. */
    std::string if_name;
    bool found = false;
    platform_bond_info_t bond_info;
    if (ot == SAI_OBJECT_TYPE_LAG) {
        sai_status_t lag_status = get_lag_bond_info(obj_id, bond_info);
        if (lag_status != SAI_STATUS_SUCCESS) {
            SWSS_LOG_ERROR("get_lag_bond_info failed for LAG %s",
                           sai_serialize_object_id(obj_id).c_str());
            return SAI_STATUS_FAILURE;
        }
        if_name = format_portchannel_name(bond_info.id);
        found = true;
    } else {
        found = getTapNameFromPortId(obj_id, if_name);
    }
    if (found == false)
    {
        SWSS_LOG_ERROR("host interface for port id %s not found", sai_serialize_object_id(obj_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    swss::IpPrefix intf_ip_prefix;
    char host_subifname[32];
    const char *linux_ifname;
    bool is_v6 = false;

    is_v6 = (ip_prefix.addr_family == SAI_IP_ADDR_FAMILY_IPV6) ? true : false;

    if (vlan_id) {
        snprintf(host_subifname, sizeof(host_subifname), "%s.%u", if_name.c_str(), vlan_id);
        linux_ifname = host_subifname;
    } else {
        linux_ifname= if_name.c_str();
    }
    std::string ip_prefix_key;
    std::string addr_family = ((is_v6) ? "v6" : "v4");

    ip_prefix_key = linux_ifname + addr_family + sai_serialize_ip_prefix(ip_prefix);

    if (is_add)
    {
        std::string ip_prefix_str;

        bool ret = vpp_get_intf_ip_address(linux_ifname, ip_prefix, is_v6, ip_prefix_str);
        if (ret == false)
        {
            SWSS_LOG_DEBUG("No ip address to add on router interface %s", linux_ifname);
            return SAI_STATUS_SUCCESS;
        }
        SWSS_LOG_NOTICE("Adding ip address on router interface %s", linux_ifname);

        intf_ip_prefix = swss::IpPrefix(ip_prefix_str.c_str());

        sai_ip_prefix_t saiIpPrefix;

        copy(saiIpPrefix, intf_ip_prefix);

        m_intf_prefix_map[ip_prefix_key] = sai_serialize_ip_prefix(saiIpPrefix);
    } else {
        sai_ip_prefix_t saiIpPrefix;

        std::string ip_prefix_str;

        if (vpp_intf_get_prefix_entry(ip_prefix_key, ip_prefix_str) == false)
        {
            SWSS_LOG_DEBUG("No ip address to remove on router interface %s", linux_ifname);
            return SAI_STATUS_SUCCESS;
        }
              SWSS_LOG_NOTICE("Removing ip address on router interface %s", linux_ifname);

        sai_deserialize_ip_prefix(ip_prefix_str, saiIpPrefix);

        intf_ip_prefix = getIpPrefixFromSaiPrefix(saiIpPrefix);

        vpp_intf_remove_prefix_entry(ip_prefix_key);
    }
    vpp_ip_route_t vpp_ip_prefix;
    swss::IpAddress m_ip = intf_ip_prefix.getIp();

    vpp_ip_prefix.prefix_len = intf_ip_prefix.getMaskLength();

    switch (m_ip.getIp().family)
    {
        case AF_INET:
        {
            struct sockaddr_in *sin =  &vpp_ip_prefix.prefix_addr.addr.ip4;

            vpp_ip_prefix.prefix_addr.sa_family = AF_INET;
            sin->sin_addr.s_addr = m_ip.getV4Addr();
            break;
        }
        case AF_INET6:
        {
            const uint8_t *prefix = m_ip.getV6Addr();
            struct sockaddr_in6 *sin6 =  &vpp_ip_prefix.prefix_addr.addr.ip6;

            vpp_ip_prefix.prefix_addr.sa_family = AF_INET6;
            memcpy(sin6->sin6_addr.s6_addr, prefix, sizeof(sin6->sin6_addr.s6_addr));
            break;
        }
        default:
        {
            throw std::logic_error("Invalid family");
        }
    }

    /* Resolve VPP hw interface name. For LAG sub-port, the parent in VPP is
     * BondEthernet<bond_id> (not the tap), so tap_to_hwif_name() does not
     * apply; we build BondEthernet<bond_id>[.<vlan>] directly from bond_info. */
    char hw_subifname[32];
    char hw_bondifname[32];
    const char *hw_ifname;

    if (ot == SAI_OBJECT_TYPE_LAG) {
        format_bond_hwif(hw_bondifname, sizeof(hw_bondifname),
                         bond_info.id, vlan_id);
        hw_ifname = hw_bondifname;
    } else {
        const char *hwifname = tap_to_hwif_name(if_name.c_str());
        if (vlan_id) {
            snprintf(hw_subifname, sizeof(hw_subifname), "%s.%u", hwifname, vlan_id);
            hw_ifname = hw_subifname;
        } else {
            hw_ifname = hwifname;
        }
    }

    int ret = interface_ip_address_add_del(hw_ifname, &vpp_ip_prefix, is_add);

    if (ret == 0)
    {
        if (is_add)
        {
            /*
             * Warm-arp here as well as in the PORT-next_hop branch
             * (vpp_add_del_intf_ip_addr_norif): on sub-port recreate
             * orchagent only re-emits the SUB_PORT-next_hop ROUTE_ENTRY,
             * so without this the kernel ARP cache stays empty and
             * DUT-originated traffic on the recreated sub-port is dropped.
             * The helper is idempotent/detached, so the extra probe on
             * initial create is harmless.
             */
            vpp_warm_arp_for_subnet_peers(std::string(linux_ifname),
                                          intf_ip_prefix);
        }
        return SAI_STATUS_SUCCESS;
    }
    else {
        return SAI_STATUS_FAILURE;
    }
}

static void get_intf_vlanid (std::string& sub_ifname, int *vlan_id, std::string& if_name)
{
    SWSS_LOG_ENTER();

    std::size_t pos = sub_ifname.find(".");

    if (pos == std::string::npos)
    {
        if_name = sub_ifname;
        *vlan_id = 0;
    } else {
        if_name = sub_ifname.substr(0, pos);
        std::string vlan = sub_ifname.substr(pos+1);
        *vlan_id = std::stoi(vlan);
    }
}
static void get_vlan_intf_vlanid(std::string& if_name, std::string& vlan_prefix, int* vlan_id)
{
    SWSS_LOG_ENTER();

    // Check if the if_name starts with vlan_prefix
    if (if_name.compare(0, vlan_prefix.length(), vlan_prefix) != 0) {
        // If it doesn't start with vlan_prefix, set vlan_id to 0
        *vlan_id = 0;
        return;
    }

    // Find the position of the first digit in the string
    size_t pos = if_name.find_first_of("0123456789");

    // Check if a digit is found
    if (pos == std::string::npos) {
        // If no digit is found, set vlan_id to 0
        *vlan_id = 0;
        return;
    }

    // Extract the numeric part using substr
    std::string numeric_part = if_name.substr(pos);

    // Convert the numeric part to an integer using stoi
    *vlan_id = std::stoi(numeric_part);
}
static void vpp_serialize_intf_data (std::string& k1, std::string& k2, std::string &serializedData)
{
    SWSS_LOG_ENTER();

    serializedData.append(k1);
    serializedData.append("@");
    serializedData.append(k2);
}

static void vpp_deserialize_intf_data (std::string &serializedData, std::string& k1, std::string& k2)
{
    SWSS_LOG_ENTER();

    std::size_t pos = serializedData.find("@");

    if (pos != std::string::npos)
    {
        k1 = serializedData.substr(0, pos);
        k2 = serializedData.substr(pos+1);
    } else {
        SWSS_LOG_WARN("String %s does not contain delimiter @", serializedData.c_str());
    }
}

sai_status_t SwitchVpp::vpp_add_del_intf_ip_addr_norif (
    _In_ const std::string& ip_prefix_key,
    _In_ sai_route_entry_t& route_entry,
    _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    bool is_v6 = false;

    is_v6 = (route_entry.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV6) ? true : false;

    // Check if this is an IPv6 link-local address (fe80::/10)
    if (is_v6) {
        const uint8_t* addr = route_entry.destination.addr.ip6;
        // Link-local addresses start with fe80::/10, so first byte is 0xfe and second byte is 0x80-0xbf
        if (addr[0] == 0xfe && (addr[1] & 0xc0) == 0x80) {
            SWSS_LOG_INFO("Skipping configuring interface IP: IPv6 link-local address");
            return SAI_STATUS_SUCCESS;
        }
    }

    std::string full_if_name;
    std::string ip_prefix_str;

    if (is_add)
    {
        bool found = vpp_get_intf_name_for_prefix(route_entry.destination, is_v6, full_if_name);
        if (found == false)
        {
            SWSS_LOG_ERROR("host interface for prefix not found");
            return SAI_STATUS_FAILURE;
        }
    } else {
        std::string intf_data;

        if (vpp_intf_get_prefix_entry(ip_prefix_key, intf_data) == false)
        {
            SWSS_LOG_DEBUG("No interface ip address found for %s", ip_prefix_key.c_str());
            return SAI_STATUS_SUCCESS;
        }
        vpp_deserialize_intf_data(intf_data, full_if_name, ip_prefix_str);
    }

    const char *linux_ifname;
    int vlan_id = 0;
    std::string if_name;

    std::string vlan_prefix = "Vlan";
    if (full_if_name.compare(0, vlan_prefix.length(), vlan_prefix) == 0)
    {
        get_vlan_intf_vlanid(full_if_name, vlan_prefix, &vlan_id);
        SWSS_LOG_NOTICE("It's Vlan interface. Vlan id: %d", vlan_id);
    } else {
        get_intf_vlanid(full_if_name, &vlan_id, if_name);
    }
    linux_ifname= full_if_name.c_str();

    std::string addr_family = ((is_v6) ? "v6" : "v4");

    swss::IpPrefix intf_ip_prefix;

    if (is_add)
    {
        bool ret = vpp_get_intf_ip_address(linux_ifname, route_entry.destination, is_v6, ip_prefix_str);
        if (ret == false)
        {
            SWSS_LOG_DEBUG("No ip address to add on router interface %s", linux_ifname);
            return SAI_STATUS_SUCCESS;
        }
        SWSS_LOG_NOTICE("Adding ip address on router interface %s", linux_ifname);

        intf_ip_prefix = swss::IpPrefix(ip_prefix_str.c_str());

        sai_ip_prefix_t saiIpPrefix;

        copy(saiIpPrefix, intf_ip_prefix);

        std::string intf_data;
        std::string sai_prefix;

        sai_prefix = sai_serialize_ip_prefix(saiIpPrefix);

        vpp_serialize_intf_data(full_if_name, sai_prefix, intf_data);

        m_intf_prefix_map[ip_prefix_key] = intf_data;
    } else {
        sai_ip_prefix_t saiIpPrefix;

              SWSS_LOG_NOTICE("Removing ip address on router interface %s", linux_ifname);

        sai_deserialize_ip_prefix(ip_prefix_str, saiIpPrefix);

        intf_ip_prefix = getIpPrefixFromSaiPrefix(saiIpPrefix);

        vpp_intf_remove_prefix_entry(ip_prefix_key);
    }

    vpp_ip_route_t vpp_ip_prefix;
    swss::IpAddress m_ip = intf_ip_prefix.getIp();

    vpp_ip_prefix.prefix_len = intf_ip_prefix.getMaskLength();

    switch (m_ip.getIp().family)
    {
        case AF_INET:
        {
            struct sockaddr_in *sin =  &vpp_ip_prefix.prefix_addr.addr.ip4;

            vpp_ip_prefix.prefix_addr.sa_family = AF_INET;
            sin->sin_addr.s_addr = m_ip.getV4Addr();
            break;
        }
        case AF_INET6:
        {
            const uint8_t *prefix = m_ip.getV6Addr();
            struct sockaddr_in6 *sin6 =  &vpp_ip_prefix.prefix_addr.addr.ip6;

            vpp_ip_prefix.prefix_addr.sa_family = AF_INET6;
            memcpy(sin6->sin6_addr.s6_addr, prefix, sizeof(sin6->sin6_addr.s6_addr));
            break;
        }
        default:
        {
            throw std::logic_error("Invalid family");
        }
    }

    const char *hwifname;
    char hw_subifname[32];
    char hw_bviifname[32];
    char hw_bondifname[32];
    const char *hw_ifname;
    if (full_if_name.compare(0, vlan_prefix.length(), vlan_prefix) == 0)
    {
       snprintf(hw_bviifname, sizeof(hw_bviifname), "%s%d","bvi",vlan_id);
       hw_ifname = hw_bviifname;
    } else if (full_if_name.compare(0, strlen(PORTCHANNEL_PREFIX), PORTCHANNEL_PREFIX) == 0) {
        /* "PortChannel<bond_id>" or "PortChannel<bond_id>.<vlan>" -> BondEthernet<bond_id>[.<vlan>]
         * Without the sub-id suffix below, LAG sub-port IPs end up on the
         * parent BondEthernet instead of on BondEthernet<id>.<vlan>, breaking
         * routed traffic for LAG sub-interfaces (PTF
         * test_packet_routed_with_valid_vlan[port_in_lag]). */
        std::string pc_suffix = full_if_name.substr(strlen(PORTCHANNEL_PREFIX));
        size_t dot_pos = pc_suffix.find('.');
        std::string bond_id_str = (dot_pos == std::string::npos) ? pc_suffix : pc_suffix.substr(0, dot_pos);
        uint32_t bond_id = std::stoi(bond_id_str);
        format_bond_hwif(hw_bondifname, sizeof(hw_bondifname),
                         bond_id, static_cast<uint32_t>(vlan_id));
        hw_ifname = hw_bondifname;
    } else {
       hwifname = tap_to_hwif_name(if_name.c_str());
       if (vlan_id) {
           snprintf(hw_subifname, sizeof(hw_subifname), "%s.%u", hwifname, vlan_id);
           hw_ifname = hw_subifname;
       } else {
           hw_ifname = hwifname;
       }
    }
    SWSS_LOG_NOTICE("Setting ip on hw_ifname %s", hw_ifname);

    int ret = interface_ip_address_add_del(hw_ifname, &vpp_ip_prefix, is_add);

    if (ret == 0)
    {
        if (is_add)
        {
            m_tunnel_mgr_ipip.retry_pending_unnumbered(vpp_ip_prefix.prefix_addr);
            /*
             * Trigger kernel ARP resolution toward peer hosts in this
             * /29..31 subnet so the first PTF probe doesn't get dropped
             * in VPP's ip4-glean. See helper above for full rationale.
             *
             * Also fired from vpp_add_del_intf_ip_addr() for the
             * SUB_PORT-next_hop recreate case; the duplicate probe on
             * initial create is at most one extra UDP packet per peer.
             */
            vpp_warm_arp_for_subnet_peers(std::string(linux_ifname), intf_ip_prefix);
        }
        return SAI_STATUS_SUCCESS;
    }
    else {
        return SAI_STATUS_FAILURE;
    }
}

enum class LpbOpType {
    NOP_LPB_IF = 0,
    ADD_IP_LPB_IF = 1,
    DEL_IP_LPB_IF = 2,
    ADD_LPB_IF = 3,
    DEL_LPB_IF = 4,
};

LpbOpType getLoopbackOperationType (
    _In_ bool is_add,
    _In_ const std::string vppIfName,
    _In_ sai_route_entry_t route_entr,
    _In_ const std::unordered_map<std::string, std::string>& lpbIpToIfMap)
{
    SWSS_LOG_ENTER();

    if (is_add) {
        if ((!vppIfName.empty()) && (vppIfName.find("loop") != std::string::npos)) {
            return LpbOpType::ADD_IP_LPB_IF;
        } else if (vppIfName.empty()) {
            return LpbOpType::ADD_LPB_IF;
        }
    } else {
        int count = 0;
        // Iterate over all elements in the unordered_map
        for (const auto& pair : lpbIpToIfMap) {
            if (pair.second == vppIfName) {
                ++count;
            }
        }
        if (count == 1) {
            // last IP then remove the loopback interface
            return LpbOpType::DEL_LPB_IF;
        } else {
            return LpbOpType::DEL_IP_LPB_IF;
        }
    }
    return LpbOpType::NOP_LPB_IF;
}

sai_status_t SwitchVpp::process_interface_loopback (
   _In_ const std::string &serializedObjectId,
   _In_ bool &isLoopback,
   _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    sai_route_entry_t route_entry;
    sai_deserialize_route_entry(serializedObjectId, route_entry);
    std::string destinationIP = extractDestinationIP(serializedObjectId);
    std::string hostIfName = "";

    if (is_add)
    {
        hostIfName = get_intf_name_for_prefix(route_entry);
    } else
    {
        hostIfName = lpbIpToHostIfMap[destinationIP];
    }

    isLoopback = (hostIfName.find("Loopback") != std::string::npos);
    SWSS_LOG_NOTICE("hostIfName:%s isLoopback:%u", hostIfName.c_str(), isLoopback);

    if (isLoopback) {
        std::string vppIfName = lpbHostIfToVppIfMap[hostIfName];
        LpbOpType lpbOp = getLoopbackOperationType(is_add, vppIfName, route_entry, lpbIpToIfMap);

        switch (lpbOp) {
            case LpbOpType::ADD_IP_LPB_IF:

                // interface exists - add ip to interface
                SWSS_LOG_NOTICE("hostIfName:%s exists new-ip:%s",
                    hostIfName.c_str(), destinationIP.c_str());

                // update interafce with ip add
                vpp_interface_ip_address_update(vppIfName.c_str(),
                    serializedObjectId, true);

                lpbIpToIfMap[destinationIP] = vppIfName;
                lpbIpToHostIfMap[destinationIP] = hostIfName;
                break;

            case LpbOpType::DEL_IP_LPB_IF:

                // interface exists - remove ip from interface
                SWSS_LOG_NOTICE("hostIfName:%s exists new-ip:%s",
                    hostIfName.c_str(), destinationIP.c_str());

                // update interafce with ip remove
                vpp_interface_ip_address_update(vppIfName.c_str(),
                    serializedObjectId, false);

                lpbIpToIfMap.erase(destinationIP);
                lpbIpToHostIfMap.erase(destinationIP);
                break;

            case LpbOpType::DEL_LPB_IF:

                // interface exists - delete interface
                vpp_del_lpb_intf_ip_addr(serializedObjectId);
                break;

            case LpbOpType::ADD_LPB_IF:

                // interface does not exist - create interface
                vpp_add_lpb_intf_ip_addr(serializedObjectId);
                break;

            default:

                SWSS_LOG_INFO("No matching loopback hostIfName:%s new-ip:%s",
                    hostIfName.c_str(), destinationIP.c_str());
                break;
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_interface_ip_address_update (
    _In_ const char *vppIfname,
    _In_ const std::string &serializedObjectId,
    _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    sai_route_entry_t route_entry;
    sai_deserialize_route_entry(serializedObjectId, route_entry);
    std::string destinationIP = extractDestinationIP(serializedObjectId);

    vpp_ip_route_t ip_route;
    create_route_prefix(&route_entry, &ip_route);

    if (route_entry.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        char prefixIp6Str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &(ip_route.prefix_addr.addr.ip6.sin6_addr),
            prefixIp6Str, INET6_ADDRSTRLEN);

    } else if (route_entry.destination.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        char prefixIp4Str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(ip_route.prefix_addr.addr.ip4.sin_addr),
            prefixIp4Str, INET_ADDRSTRLEN);
    } else {
        SWSS_LOG_ERROR("Could not determine IP address family!  destinationIP:%s",
            destinationIP.c_str());
    }

    int ret = interface_ip_address_add_del(vppIfname, &ip_route, is_add);
    if (ret != 0) {
        SWSS_LOG_ERROR("interface_ip_address_add returned error");
    } else if (is_add) {
        m_tunnel_mgr_ipip.retry_pending_unnumbered(ip_route.prefix_addr);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_add_lpb_intf_ip_addr (
    _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_route_entry_t route_entry;
    sai_deserialize_route_entry(serializedObjectId, route_entry);
    std::string destinationIP = extractDestinationIP(serializedObjectId);

    // Retrieve the current instance for the interface
    uint32_t instance = getNextLoopbackInstance();

    // Generate the loopback interface name
    std::string vppIfName = "loop" + std::to_string(instance);

    // Store the current instance interface pair
    lpbInstMap[vppIfName] = instance;

    SWSS_LOG_NOTICE("vpp_add_lpb vppIfName:%s instance:%u",
        vppIfName.c_str(), instance);

    // Get new list of physical interfaces from VS
    refresh_interfaces_list();

    // Create the loopback instance in vpp
    int ret = create_loopback_instance(vppIfName.c_str(), instance);
    if (ret != 0) {
        SWSS_LOG_ERROR("create_loopback_instance returned error: %d", ret);
    }

    // Get new list of physical interfaces from VS
    refresh_interfaces_list();

    //Set state up
    interface_set_state(vppIfName.c_str(), true /*is_up*/);

    const std::string hostIfname = get_intf_name_for_prefix(route_entry);
    SWSS_LOG_NOTICE("get_intf_name_for_prefix:%s", hostIfname.c_str());
    lpbHostIfToVppIfMap[hostIfname] = vppIfName;

    // create lcp tap between vpp and host
    {
        init_vpp_client();

        std::ostringstream tap_stream;
        tap_stream << "tap_" << hostIfname;
        std::string tap = tap_stream.str();

        SWSS_LOG_DEBUG("configure_lcp_interface vpp_name:%s sonic_name:%s",
            vppIfName.c_str(), hostIfname.c_str());
        configure_lcp_interface(vppIfName.c_str(), tap.c_str(), true);

        // add tc filter to redirect traffic from tap to Loopback
        CHECK_STATUS(add_tc_filter_redirect(tap, hostIfname));
    }

    // Store the ip/vppIfName pair
    lpbIpToIfMap[destinationIP] = vppIfName;
    lpbIpToHostIfMap[destinationIP] = hostIfname;

    // Update vpp interface ip address
    vpp_interface_ip_address_update(vppIfName.c_str(), serializedObjectId, true);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_del_lpb_intf_ip_addr (
    _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_route_entry_t route_entry;
    sai_deserialize_route_entry(serializedObjectId, route_entry);
    std::string destinationIP = extractDestinationIP(serializedObjectId);

    std::string vppIfName = lpbIpToIfMap[destinationIP];
    const std::string hostIfname = lpbIpToHostIfMap[destinationIP];
    uint32_t instance = lpbInstMap[vppIfName];

    // Update vpp interface ip address
    vpp_interface_ip_address_update(vppIfName.c_str(), serializedObjectId, false);

    // Delete the loopback instance
    delete_loopback(vppIfName.c_str(), instance);

    // refresh interfaces list as we have deleted the loopback interface
    refresh_interfaces_list();

    // Remove the IP/interface mappings from the maps
    lpbInstMap.erase(vppIfName);
    lpbIpToIfMap.erase(destinationIP);
    lpbIpToHostIfMap.erase(destinationIP);
    lpbHostIfToVppIfMap.erase(hostIfname);

    // Mark the loopback instance available
    markLoopbackInstanceDeleted(instance);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_get_router_intf_name (
    _In_ sai_ip_prefix_t& ip_prefix,
    _In_ sai_object_id_t rif_id,
    std::string& nexthop_ifname)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    int32_t rif_type;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    sai_status_t status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_TYPE was not passed");

        return SAI_STATUS_FAILURE;
    }
    rif_type = attr.value.s32;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
    status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_PORT_ID was not passed");

        return SAI_STATUS_FAILURE;
    }

    sai_object_id_t obj_id = attr.value.oid;

    sai_object_type_t ot = objectTypeQuery(obj_id);

    if (ot == SAI_OBJECT_TYPE_VLAN)
    {
        SWSS_LOG_DEBUG("Skipping object type VLAN");
        return SAI_STATUS_SUCCESS;
    }

    if (ot != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_ERROR("SAI_ROUTER_INTERFACE_ATTR_PORT_ID=%s expected to be PORT but is: %s",
                sai_serialize_object_id(obj_id).c_str(),
                sai_serialize_object_type(ot).c_str());

        return SAI_STATUS_FAILURE;
    }

    if (rif_type != SAI_ROUTER_INTERFACE_TYPE_SUB_PORT &&
        rif_type != SAI_ROUTER_INTERFACE_TYPE_PORT &&
    rif_type != SAI_ROUTER_INTERFACE_TYPE_LOOPBACK)
    {
        return SAI_STATUS_SUCCESS;
    }

    attr.id = SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID;
    status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr);

    uint16_t vlan_id = 0;
    if (status == SAI_STATUS_SUCCESS)
    {
        vlan_id = attr.value.u16;
    }

    std::string if_name;
    bool found = getTapNameFromPortId(obj_id, if_name);
    if (found == false)
    {
        SWSS_LOG_ERROR("host interface for port id %s not found", sai_serialize_object_id(obj_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    const char *hwifname = tap_to_hwif_name(if_name.c_str());
    char hw_subifname[32];
    const char *hw_ifname;

    if (vlan_id) {
        snprintf(hw_subifname, sizeof(hw_subifname), "%s.%u", hwifname, vlan_id);
        hw_ifname = hw_subifname;
    } else {
        hw_ifname = hwifname;
    }

    nexthop_ifname = std::string(hw_ifname);

    SWSS_LOG_NOTICE("Configuring ip address on router interface %s", nexthop_ifname.c_str());

    return SAI_STATUS_SUCCESS;
}

int SwitchVpp::vpp_add_ip_vrf (_In_ sai_object_id_t objectId, uint32_t vrf_id)
{
    SWSS_LOG_ENTER();

    auto it = vrf_objMap.find(objectId);

    if (it != vrf_objMap.end()) {
        auto sw = it->second;
        if (sw != nullptr) {
                 SWSS_LOG_NOTICE("VRF(%s) with id %u already exists", sai_serialize_object_id(objectId).c_str(), sw->m_vrf_id);
        } else {
            SWSS_LOG_ERROR("VRF(%s) object with null data", sai_serialize_object_id(objectId).c_str());
        }
        return 0;
    }

    std::string vrf_name = "vrf_" + vrf_id;

    if (!vrf_id || ip_vrf_add(vrf_id, vrf_name.c_str(), false) == 0) {
        SWSS_LOG_NOTICE("VRF(%s) with id %u created in VS", sai_serialize_object_id(objectId).c_str(), vrf_id);
        vrf_objMap[objectId] = std::make_shared<IpVrfInfo>(objectId, vrf_id, vrf_name, false);

        uint32_t hash_mask =  VPP_IP_API_FLOW_HASH_SRC_IP | VPP_IP_API_FLOW_HASH_DST_IP | \
            VPP_IP_API_FLOW_HASH_SRC_PORT | VPP_IP_API_FLOW_HASH_DST_PORT | \
            VPP_IP_API_FLOW_HASH_PROTO | VPP_IP_API_FLOW_HASH_PEEK_INNER;

        int ret = vpp_ip_flow_hash_set(vrf_id, hash_mask, AF_INET);
        SWSS_LOG_NOTICE("ip flow hash set for VRF %s with vrf_id %u in VS, status %d",
                        sai_serialize_object_id(objectId).c_str(), vrf_id, ret);
        ret = vpp_ip_flow_hash_set(vrf_id, hash_mask, AF_INET6);
        SWSS_LOG_NOTICE("ip6 flow hash set for VRF %s with vrf_id %u in VS, status %d",
                        sai_serialize_object_id(objectId).c_str(), vrf_id, ret);
    }

    return 0;
}

int SwitchVpp::vpp_del_ip_vrf (_In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    auto it = vrf_objMap.find(objectId);

    if (it != vrf_objMap.end()) {
        auto sw = it->second;
        if (sw != nullptr) {
                 SWSS_LOG_NOTICE("Deleting VRF(%s) with id %u", sai_serialize_object_id(objectId).c_str(), sw->m_vrf_id);
           ip_vrf_del(sw->m_vrf_id, sw->m_vrf_name.c_str(), sw->m_is_ipv6);
           vrf_objMap.erase(it);
        }
    }
    return 0;
}

std::shared_ptr<IpVrfInfo> SwitchVpp::vpp_get_ip_vrf (_In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    auto it = vrf_objMap.find(objectId);

    if (it != vrf_objMap.end()) {
        auto vrf = it->second;
        if (vrf == nullptr) {
            SWSS_LOG_NOTICE("No Vrf found with id %s", sai_serialize_object_id(objectId).c_str());
        }
        return vrf;
    }
    return nullptr;
}

/*
 * VS uses linux's vrf table id when linux_nl is active
 */
int SwitchVpp::vpp_get_vrf_id (const char *linux_ifname, uint32_t *vrf_id)
{
    SWSS_LOG_ENTER();

    std::stringstream cmd;
    std::string res;

    cmd << IP_CMD << " link show dev " << linux_ifname;
    int ret = swss::exec(cmd.str(), res);
    if (ret)
    {
        SWSS_LOG_ERROR("Command '%s' failed with rc %d", cmd.str().c_str(), ret);
        return -1;
    }

    std::stringstream table_cmd;

    table_cmd << IP_CMD << " -d link show dev " << linux_ifname << " | grep -o 'vrf_slave table [0-9]\\+' | cut -d' ' -f3";
    ret = swss::exec(table_cmd.str(), res);
    if (ret)
    {
        SWSS_LOG_ERROR("Command '%s' failed with rc %d", table_cmd.str().c_str(), ret);
        return -1;
    }

    if (res.length() != 0)
    {
        *vrf_id = std::stoi(res);
    } else {
        *vrf_id = 0;
    }

    return 0;
}

/*
 * Enable kernel arp_accept on a VPP-managed sub-port netdev so that
 * unsolicited ARP replies (sent by a remote peer in response to a
 * VPP-originated glean ARP request on the sub-interface) are accepted
 * by the kernel ARP table.
 *
 * Without this, VPP itself never learns the sub-port neighbor: the
 * vlan-tagged ARP reply enters the parent port and linux-cp punts it
 * to the LCP host tap before VPP's arp-reply / neighbor-learning path
 * has a chance to install the neighbor. The kernel then drops it as
 * an unsolicited reply (arp_accept=0 default), so lcp-sync has no
 * kernel entry to mirror to VPP, and subsequent packets keep hitting
 * ip4-glean and getting dropped.
 *
 * Setting arp_accept=1 makes the kernel accept the reply, populate
 * its ARP table, and lcp-sync mirrors it to the VPP neighbor table on
 * the sub-interface, enabling cross-port forwarding such as L3 plain
 * port -> sub-port and unaffected-by-removal scenarios.
 *
 * The kernel netdev may appear slightly after configure_lcp_interface
 * returns, so we briefly retry opening the sysctl path.
 */
static bool vpp_set_sub_port_arp_accept(const char *netdev)
{
    SWSS_LOG_ENTER();

    if (netdev == nullptr || netdev[0] == '\0')
    {
        SWSS_LOG_WARN("vpp_set_sub_port_arp_accept: empty netdev name");
        return false;
    }

    std::string path = std::string("/proc/sys/net/ipv4/conf/") + netdev + "/arp_accept";

    for (int attempt = 0; attempt < 10; ++attempt)
    {
        std::ofstream ofs(path);
        if (ofs.is_open())
        {
            ofs << "1";
            ofs.flush();
            if (ofs.good())
            {
                SWSS_LOG_NOTICE("Enabled arp_accept on sub-port netdev %s", netdev);
                return true;
            }
            SWSS_LOG_DEBUG("arp_accept write failed on %s (attempt %d), retrying",
                           path.c_str(), attempt);
        }
        usleep(50000); /* 50ms */
    }

    SWSS_LOG_ERROR("Failed to enable arp_accept on sub-port netdev %s (path=%s); "
                   "VPP-initiated ARP for this sub-port will not be learned",
                   netdev, path.c_str());
    return false;
}

/*
 * Pre-warm VPP's neighbor table on a router interface by triggering kernel
 * ARP resolution for the small set of peer IPs that share the same connected
 * /29 .. /31 subnet as the local interface IP.
 *
 * Why this is needed
 * ------------------
 * VPP's ip4-glean node *drops* the first packet that hits an unresolved
 * neighbor on a connected route. It sends an ARP request for the
 * destination but the original IP packet is not buffered. Vendor ASIC SDKs
 * typically queue the packet until ARP resolves, so SONiC PTF tests that
 * send a single probe frame and expect it to be delivered work fine on
 * Broadcom / Mellanox, but on the VPP virtual switch the first frame is
 * dropped and the test fails.
 *
 * Fix: when SAI installs a connected route on a small subnet (/29..31),
 * proactively send a UDP probe from the kernel netdev to each non-self
 * host IP in the subnet. The packet itself is harmless (port 9 / discard,
 * 1 byte payload) and almost always undelivered, but it forces the local
 * kernel to ARP-resolve the peer. That ARP exchange:
 *   - learns the peer MAC into VPP via the arp-input feature on the
 *     sub-interface, OR
 *   - lcp-sync mirrors the kernel ARP entry into the VPP neighbor table
 *     (combined with the arp_accept=1 helper above for sub-ports).
 *
 * The probe runs on a detached background thread so it never blocks the
 * SAI processing thread, and it sleeps briefly to let IntfMgr finish
 * adding the IP on the kernel netdev. It is intentionally limited to
 * small subnets to avoid broadcast-style probing on production /24's.
 */
static void vpp_warm_arp_for_subnet_peers(const std::string &netdev,
                                          const swss::IpPrefix &ip_prefix)
{
    SWSS_LOG_ENTER();

    if (netdev.empty())
    {
        return;
    }

    swss::IpAddress local_ip = ip_prefix.getIp();
    if (local_ip.getIp().family != AF_INET)
    {
        /* IPv6 handled separately via ND; sub-port v4 is the failing case. */
        return;
    }

    int prefix_len = ip_prefix.getMaskLength();
    if (prefix_len < 29 || prefix_len > 31)
    {
        /* Only do this for tiny subnets to avoid spraying the network. */
        return;
    }

    uint32_t self_h = ntohl(local_ip.getV4Addr());
    uint32_t mask_h = (prefix_len == 32)
                      ? 0xFFFFFFFFu
                      : ~((1u << (32 - prefix_len)) - 1);
    uint32_t network_h = self_h & mask_h;
    uint32_t broadcast_h = network_h | ~mask_h;

    std::thread([self_h, network_h, broadcast_h, prefix_len, netdev]() {
        /*
         * Wait for IntfMgr to write the IP on the kernel netdev before
         * any probe. Per-peer sendto would otherwise fail silently with
         * EADDRNOTAVAIL on a slow kernel/IntfMgr, leaving the first PTF
         * packet exposed to ip4-glean drop.
         *
         * Strategy: an initial 150ms sleep covers the common case where
         * IntfMgr is already done. Then probe readiness by binding a
         * dummy socket to the local IP; bind() returns EADDRNOTAVAIL
         * until the kernel has the address. Retry up to 10x at 50ms
         * intervals (500ms additional budget on slow systems).
         */
        static constexpr int kReadyMaxRetries = 10;
        static constexpr auto kReadyRetrySleep = std::chrono::milliseconds(50);

        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        bool ip_ready = false;
        for (int retry = 0; retry < kReadyMaxRetries; ++retry)
        {
            int tst = socket(AF_INET, SOCK_DGRAM, 0);
            if (tst < 0)
            {
                std::this_thread::sleep_for(kReadyRetrySleep);
                continue;
            }
            struct sockaddr_in src = {};
            src.sin_family = AF_INET;
            src.sin_addr.s_addr = htonl(self_h);
            int rv = bind(tst, (struct sockaddr *)&src, sizeof(src));
            int err = errno;
            close(tst);
            if (rv == 0)
            {
                ip_ready = true;
                break;
            }
            if (err != EADDRNOTAVAIL)
            {
                /* Anything else (EACCES, EAFNOSUPPORT, ...) will not
                 * clear with a retry. */
                break;
            }
            std::this_thread::sleep_for(kReadyRetrySleep);
        }

        char self_str[INET_ADDRSTRLEN] = {0};
        uint32_t self_n = htonl(self_h);
        inet_ntop(AF_INET, &self_n, self_str, sizeof(self_str));

        if (!ip_ready)
        {
            SWSS_LOG_WARN("vpp_warm_arp_for_subnet_peers: kernel IP %s/%d "
                          "not available on %s after ~650ms; skipping "
                          "warm-arp - first PTF packet may drop in VPP "
                          "ip4-glean",
                          self_str, prefix_len, netdev.c_str());
            return;
        }

        /* /31 has no network/broadcast convention; iterate both hosts.
         * /30 and /29 reserve network + broadcast; skip them. */
        uint32_t start_h = (prefix_len == 31) ? network_h : network_h + 1;
        uint32_t end_h   = (prefix_len == 31) ? broadcast_h : broadcast_h - 1;

        int attempts = 0;
        int sent = 0;
        for (uint32_t ipv_h = start_h; ipv_h <= end_h; ++ipv_h)
        {
            if (ipv_h == self_h)
            {
                continue;
            }

            int s = socket(AF_INET, SOCK_DGRAM, 0);
            if (s < 0)
            {
                continue;
            }

            /* Bind socket to the specific kernel netdev so the kernel uses
             * its IP/route/ARP cache rather than the global one. Requires
             * CAP_NET_RAW which syncd has by virtue of running as root. */
            if (setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE,
                           netdev.c_str(), (socklen_t)(netdev.size() + 1)) != 0)
            {
                close(s);
                continue;
            }

            struct sockaddr_in peer = {};
            peer.sin_family = AF_INET;
            peer.sin_port = htons(9); /* discard service */
            peer.sin_addr.s_addr = htonl(ipv_h);

            const char buf[1] = {0};
            ++attempts;
            ssize_t rv = sendto(s, buf, 1, MSG_DONTWAIT,
                                (struct sockaddr *)&peer, sizeof(peer));
            if (rv >= 0)
            {
                ++sent;
            }
            close(s);
        }

        if (sent < attempts)
        {
            SWSS_LOG_WARN("vpp_warm_arp_for_subnet_peers: only %d/%d UDP "
                          "probes succeeded from %s/%d on %s; first PTF "
                          "packet may still drop in VPP ip4-glean",
                          sent, attempts, self_str, prefix_len,
                          netdev.c_str());
        }
        else
        {
            SWSS_LOG_NOTICE("vpp_warm_arp_for_subnet_peers: sent %d UDP "
                            "probes from %s/%d on %s to pre-warm VPP "
                            "neighbors",
                            sent, self_str, prefix_len, netdev.c_str());
        }
    }).detach();
}

sai_status_t SwitchVpp::vpp_create_router_interface(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto attr_type = sai_metadata_get_attr_by_id(SAI_ROUTER_INTERFACE_ATTR_TYPE, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_TYPE was not passed");

        return SAI_STATUS_FAILURE;
    }
    if (attr_type->value.s32 == SAI_ROUTER_INTERFACE_TYPE_VLAN)
    {
        SWSS_LOG_NOTICE("Invoking BVI interface create for attr type %d", attr_type->value.s32);
        return vpp_create_bvi_interface(attr_count, attr_list);
    }
    if (attr_type->value.s32 != SAI_ROUTER_INTERFACE_TYPE_SUB_PORT &&
        attr_type->value.s32 != SAI_ROUTER_INTERFACE_TYPE_PORT)
    {
        SWSS_LOG_NOTICE("Skipping router interface create for attr type %d", attr_type->value.s32);

        return SAI_STATUS_SUCCESS;
    }

    auto attr_obj_id = sai_metadata_get_attr_by_id(SAI_ROUTER_INTERFACE_ATTR_PORT_ID, attr_count, attr_list);

    if (attr_obj_id == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_PORT_ID was not passed");

        return SAI_STATUS_SUCCESS;
    }

    sai_object_id_t obj_id = attr_obj_id->value.oid;

    sai_object_type_t ot = objectTypeQuery(obj_id);

    if (ot == SAI_OBJECT_TYPE_VLAN)
    {
        SWSS_LOG_DEBUG("Skipping tap creation for hostif with object type VLAN");
        return SAI_STATUS_SUCCESS;
    }

    if (ot != SAI_OBJECT_TYPE_PORT && ot != SAI_OBJECT_TYPE_LAG)
    {
        SWSS_LOG_ERROR("SAI_ROUTER_INTERFACE_ATTR_PORT_ID=%s expected to be PORT or LAG but is: %s",
                sai_serialize_object_id(obj_id).c_str(),
                sai_serialize_object_type(ot).c_str());

        return SAI_STATUS_FAILURE;
    }
    auto attr_vlan_id = sai_metadata_get_attr_by_id(SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID, attr_count, attr_list);

    uint16_t vlan_id = 0;
    if (attr_vlan_id == NULL) {
        if (attr_type->value.s32 == SAI_ROUTER_INTERFACE_TYPE_SUB_PORT)
        {
            SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID was not passed");

            return SAI_STATUS_FAILURE;
        }
    } else {
        vlan_id = attr_vlan_id->value.u16;
    }

    std::string if_name;
    bool found = false;
    platform_bond_info_t bond_info;
    if (ot == SAI_OBJECT_TYPE_LAG) {
        CHECK_STATUS(get_lag_bond_info(obj_id, bond_info));
        if_name = format_portchannel_name(bond_info.id);
        found = true;
    } else {
        found = getTapNameFromPortId(obj_id, if_name);
    }

    if (found == false)
    {
        SWSS_LOG_ERROR("host interface for port id %s not found", sai_serialize_object_id(obj_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    const char *dev = if_name.c_str();
    const char *linux_ifname;
    char host_subifname[32];
    char lcp_host_subifname[32];

    if (attr_type->value.s32 == SAI_ROUTER_INTERFACE_TYPE_SUB_PORT)
    {
        const char *parent_hwif;
        char hw_subif_parent[32];
        char hw_subifname[64];
        snprintf(host_subifname, sizeof(host_subifname), "%s.%u", dev, vlan_id);
        if (ot == SAI_OBJECT_TYPE_LAG) {
            format_bond_hwif(hw_subif_parent, sizeof(hw_subif_parent),
                             bond_info.id, 0);
            parent_hwif = hw_subif_parent;
        } else {
            parent_hwif = tap_to_hwif_name(dev);
        }
        snprintf(hw_subifname, sizeof(hw_subifname), "%s.%u", parent_hwif, vlan_id);

        /*
         * For LAG sub-ports the kernel-facing LCP host has to be the VPP-
         * created tap "be<bond_id>.<vlan>" (a vlan child on the parent tap
         * "be<bond_id>"), NOT the teamd vlan child "PortChannel<id>.<vlan>".
         *
         * Reason: pairing with PortChannel<id>.<vlan> wires the LCP punt to
         * a netdev that is not a VPP tap, so the kernel->VPP path cannot
         * use the linux-cp-xc-ip4/6 fast path. Kernel-originated replies
         * (e.g. ARP for a sub-port nexthop) then fall back to going through
         * teamd -> physical bond member -> bobm<N>, where ethernet-input
         * drops the vlan-tagged frame as "unknown vlan" because there is
         * no SAI sub-interface on the physical member port.
         *
         * Pairing with be<id>.<vlan> makes the kernel write packets into a
         * vlan netdev whose parent is a VPP tap; VPP ingests them via
         * tap-input -> ethernet-input -> tap<N>.<vlan> -> linux-cp-xc which
         * forwards directly to BondEthernet<id>.<vlan>. For PORT sub-ports
         * the existing host name dev.<vlan> already points at a vlan child
         * of a VPP tap, so behavior is unchanged.
         */
        const char *lcp_host;
        if (ot == SAI_OBJECT_TYPE_LAG) {
            format_be_lcp(lcp_host_subifname, sizeof(lcp_host_subifname),
                          bond_info.id, vlan_id);
            lcp_host = lcp_host_subifname;
        } else {
            lcp_host = host_subifname;
        }

        SWSS_LOG_NOTICE("Creating VPP sub interface host_if=%s hw_if=%s vlan=%u lcp_host=%s",
                        host_subifname, hw_subifname, vlan_id, lcp_host);
        int ret = create_sub_interface(parent_hwif, vlan_id, vlan_id);
        if (ret != 0)
        {
            SWSS_LOG_ERROR("Failed to create VPP sub interface host_if=%s hw_if=%s vlan=%u, ret=%d",
                           host_subifname, hw_subifname, vlan_id, ret);
            return SAI_STATUS_FAILURE;
        }

        /* Get new list of physical interfaces from VS */
        refresh_interfaces_list();

        /*
         * VPP no longer guarantees implicit LCP creation for sub-interfaces on all builds.
         * Explicitly configure host<->VPP pair so host interface state tracks VPP subif state.
         */
        SWSS_LOG_NOTICE("Configuring LCP pair for VPP sub interface lcp_host=%s hw_if=%s",
                        lcp_host, hw_subifname);
        ret = configure_lcp_interface(hw_subifname, lcp_host, true);
        if (ret != 0)
        {
            SWSS_LOG_ERROR("Failed to configure LCP pair for sub interface lcp_host=%s hw_if=%s, ret=%d; "
                           "rolling back VPP sub-interface",
                           lcp_host, hw_subifname, ret);
            /*
             * vpp_create_router_interface's return value is discarded by the
             * caller (see createRouterif: vslib/vpp/SwitchVppRif.cpp:2335).
             * Without an explicit rollback here the VPP sub-interface would
             * persist while the SAI RIF object is still created at the SAI
             * layer, producing a half-configured sub-port that responds to
             * SAI APIs but cannot punt/inject traffic.
             */
            delete_sub_interface(parent_hwif, vlan_id);
            return SAI_STATUS_FAILURE;
        }

        /*
         * For LAG sub-ports also bridge the LCP tap "be<id>.<vlan>" with the
         * teamd vlan child "PortChannel<id>.<vlan>" (where SONiC IntfMgr
         * writes the L3 IP). Bidirectional tc mirred redirect:
         *   - be<id>.<vlan> ingress -> PortChannel<id>.<vlan>: VPP-punted
         *     frames (e.g. ICMP destined to the sub-port IP) reach the
         *     SONiC-managed netdev that has the IP, so kernel handles
         *     ARP/ICMP/etc. normally and any daemon bound to PortChannel
         *     can see the traffic.
         *   - PortChannel<id>.<vlan> egress -> be<id>.<vlan>: kernel
         *     replies/originated frames bypass teamd's physical bond member
         *     path (which drops vlan frames) and instead enter VPP via the
         *     LCP tap, where the linux-cp-xc-ip4/6 fast path forwards them
         *     to BondEthernet<id>.<vlan>.
         * Mirrors the parent BondEthernet<id> <-> PortChannel<id> bridging
         * in SwitchVppFdb.cpp:addLagMember, but with both directions because
         * the vlan dispatch issue only affects sub-ports.
         *
         * Both redirects are mandatory: a one-direction-only bridge silently
         * breaks LAG sub-port ping. On failure of either direction roll back
         * all VPP state so the SAI RIF create fails cleanly.
         */
        if (ot == SAI_OBJECT_TYPE_LAG) {
            sai_status_t tc_status = add_tc_filter_redirect(
                    std::string(lcp_host), std::string(host_subifname));
            if (tc_status != SAI_STATUS_SUCCESS) {
                SWSS_LOG_ERROR("add_tc_filter_redirect ingress %s -> %s failed; "
                               "rolling back LCP pair and VPP sub-interface",
                               lcp_host, host_subifname);
                configure_lcp_interface(hw_subifname, lcp_host, false);
                delete_sub_interface(parent_hwif, vlan_id);
                return SAI_STATUS_FAILURE;
            }
            tc_status = add_tc_filter_redirect_egress(
                    std::string(host_subifname), std::string(lcp_host));
            if (tc_status != SAI_STATUS_SUCCESS) {
                SWSS_LOG_ERROR("add_tc_filter_redirect_egress %s -> %s failed; "
                               "rolling back ingress redirect, LCP pair, and VPP sub-interface",
                               host_subifname, lcp_host);
                del_tc_filter_redirect(std::string(lcp_host));
                configure_lcp_interface(hw_subifname, lcp_host, false);
                delete_sub_interface(parent_hwif, vlan_id);
                return SAI_STATUS_FAILURE;
            }
        }

        /*
         * Enable kernel arp_accept on the sub-port netdev(s) so the kernel
         * accepts unsolicited ARP replies that arrive in response to a
         * VPP-originated glean ARP request. This is required for cross-port
         * forwarding (e.g. L3 plain port -> sub-port) where VPP - not the
         * kernel - initiates ARP resolution. See helper above for full
         * rationale.
         *
         * For non-LAG sub-ports lcp_host == host_subifname, so the second
         * call is a harmless no-op. For LAG sub-ports we also set it on the
         * teamd vlan child PortChannel<id>.<vlan> where IntfMgr writes the
         * IP and where the kernel ARP table is consulted by lcp-sync.
         *
         * Failure is non-fatal to the RIF create (the sub-port still
         * carries traffic when both peers can ARP each other) but does
         * silently break cross-port-forward scenarios because VPP itself
         * never learns the peer. Surface the failure at WARN so the
         * operator has a hint when those flows misbehave.
         */
        if (!vpp_set_sub_port_arp_accept(lcp_host))
        {
            SWSS_LOG_WARN("Sub-port %s created WITHOUT kernel arp_accept=1; "
                          "VPP-initiated ARP (cross-port-forward to this "
                          "sub-port) will not be learned. Check "
                          "/proc/sys/net/ipv4/conf/%s/arp_accept permissions.",
                          lcp_host, lcp_host);
        }
        if (ot == SAI_OBJECT_TYPE_LAG)
        {
            if (!vpp_set_sub_port_arp_accept(host_subifname))
            {
                SWSS_LOG_WARN("LAG sub-port teamd child %s created WITHOUT "
                              "kernel arp_accept=1; lcp-sync ARP mirroring "
                              "will not populate the VPP neighbor table.",
                              host_subifname);
            }
        }

        /*
         * Keep linux_ifname pointing at PortChannel<id>.<vlan> for LAG
         * sub-ports so downstream vpp_get_vrf_id() (and any future
         * vpp_get_intf_ip_address() lookups) hit the SONiC-managed netdev
         * that IntfMgr actually writes VRF/IP attributes to. lcp-sync
         * mirrors the address to be<id>.<vlan>, but VRF/policy attributes
         * are not mirrored.
         */
        linux_ifname = host_subifname;
    } else {
        linux_ifname = dev;
    }

    sai_object_id_t vrf_obj_id = 0;

    auto attr_vrf_id = sai_metadata_get_attr_by_id(SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID, attr_count, attr_list);

    if (attr_vrf_id == NULL)
    {
        SWSS_LOG_NOTICE("attr SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID was not passed");
    } else {
        vrf_obj_id = attr_vrf_id->value.oid;
        SWSS_LOG_NOTICE("attr SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID %s is passed",
                        sai_serialize_object_id(vrf_obj_id).c_str());
    }

    uint32_t vrf_id;
    int ret = vpp_get_vrf_id(linux_ifname, &vrf_id);

    vpp_add_ip_vrf(vrf_obj_id, vrf_id);
    if (ret == 0 && vrf_id != 0) {
	const char *hwif_name;
	char hw_bondifname[32];
	if (ot == SAI_OBJECT_TYPE_LAG) {
	    format_bond_hwif(hw_bondifname, sizeof(hw_bondifname),
	                     bond_info.id, 0);
	    hwif_name = hw_bondifname;
	} else {
	    hwif_name = tap_to_hwif_name(dev);
	}
	SWSS_LOG_NOTICE("Setting interface vrf on hwif_name %s", hwif_name);
	set_interface_vrf(hwif_name, vlan_id, vrf_id, false);
    }
    auto attr_type_mtu = sai_metadata_get_attr_by_id(SAI_ROUTER_INTERFACE_ATTR_MTU, attr_count, attr_list);

    if (attr_type_mtu != NULL)
    {
        vpp_set_interface_mtu(obj_id, vlan_id, attr_type_mtu->value.u32);
    }

    bool v4_is_up = false, v6_is_up = false;

    auto attr_type_v4 = sai_metadata_get_attr_by_id(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE, attr_count, attr_list);

    if (attr_type_v4 != NULL)
    {
        v4_is_up = attr_type_v4->value.booldata;
    }
    auto attr_type_v6 = sai_metadata_get_attr_by_id(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V6_STATE, attr_count, attr_list);

    if (attr_type_v6 != NULL)
    {
        v6_is_up = attr_type_v6->value.booldata;
    }

    if (attr_type_v4 != NULL || attr_type_v6 != NULL)
    {
        return vpp_set_interface_state(obj_id, vlan_id, (v4_is_up || v6_is_up));
    } else {
        return SAI_STATUS_SUCCESS;
    }
}

sai_status_t SwitchVpp::vpp_update_router_interface(
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    int32_t rif_type;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    sai_status_t status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, object_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_TYPE was not passed");

        return SAI_STATUS_FAILURE;
    }
    rif_type = attr.value.s32;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
    status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, object_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_PORT_ID was not passed");

        return SAI_STATUS_FAILURE;
    }

    sai_object_id_t obj_id = attr.value.oid;

    sai_object_type_t ot = objectTypeQuery(obj_id);

    if (ot == SAI_OBJECT_TYPE_VLAN)
    {
        SWSS_LOG_DEBUG("Skipping tap creation for hostif with object type VLAN");
        return SAI_STATUS_SUCCESS;
    }

    if (ot != SAI_OBJECT_TYPE_PORT && ot != SAI_OBJECT_TYPE_LAG)
    {
        SWSS_LOG_ERROR("SAI_ROUTER_INTERFACE_ATTR_PORT_ID=%s expected to be PORT or LAG but is: %s",
                sai_serialize_object_id(obj_id).c_str(),
                sai_serialize_object_type(ot).c_str());

        return SAI_STATUS_FAILURE;
    }

    uint16_t vlan_id = 0;

    if (rif_type == SAI_ROUTER_INTERFACE_TYPE_SUB_PORT)
    {
        attr.id = SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID;
        status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, object_id, 1, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID was not passed");

            return SAI_STATUS_FAILURE;
        }

        vlan_id = attr.value.u16;
    }

    auto attr_type_mtu = sai_metadata_get_attr_by_id(SAI_ROUTER_INTERFACE_ATTR_MTU, attr_count, attr_list);

    if (attr_type_mtu != NULL)
    {
        vpp_set_interface_mtu(obj_id, vlan_id, attr_type_mtu->value.u32);
    }

    bool v4_is_up = false, v6_is_up = false;

    auto attr_type_v4 = sai_metadata_get_attr_by_id(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE, attr_count, attr_list);

    if (attr_type_v4 != NULL)
    {
        v4_is_up = attr_type_v4->value.booldata;
    }
    auto attr_type_v6 = sai_metadata_get_attr_by_id(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V6_STATE, attr_count, attr_list);

    if (attr_type_v6 != NULL)
    {
        v6_is_up = attr_type_v6->value.booldata;
    }

    if (attr_type_v4 != NULL || attr_type_v6 != NULL)
    {
        return vpp_set_interface_state(obj_id, vlan_id, (v4_is_up || v6_is_up));
    } else {
        return SAI_STATUS_SUCCESS;
    }
}

sai_status_t SwitchVpp::vpp_router_interface_remove_vrf(
     _In_ sai_object_id_t obj_id)
{
    SWSS_LOG_ENTER();

    std::string if_name;
    bool found = false;
    platform_bond_info_t bond_info;
    if (objectTypeQuery(obj_id) == SAI_OBJECT_TYPE_LAG) {
        CHECK_STATUS(get_lag_bond_info(obj_id, bond_info));
        if_name = format_portchannel_name(bond_info.id);
        found = true;
    } else {
        found = getTapNameFromPortId(obj_id, if_name);
    }

    if (found == false)
    {
        SWSS_LOG_ERROR("host interface for port id %s not found", sai_serialize_object_id(obj_id).c_str());
        return SAI_STATUS_FAILURE;
    }
    const char *linux_ifname;

    linux_ifname = if_name.c_str();

    const char *hwif_name;
    char hw_bondifname[32];
    if (objectTypeQuery(obj_id) == SAI_OBJECT_TYPE_LAG) {
        format_bond_hwif(hw_bondifname, sizeof(hw_bondifname),
                         bond_info.id, 0);
        hwif_name = hw_bondifname;
    } else {
        hwif_name = tap_to_hwif_name(if_name.c_str());
    }

    SWSS_LOG_NOTICE("Resetting to default vrf for interface %s, %s", linux_ifname, hwif_name);

    /* Remove all IP addresses before changing VRF.
     * VPP requires no addresses on the interface when rebinding to a new VRF table
     * (returns VNET_API_ERROR_ADDRESS_FOUND_FOR_INTERFACE / -114 otherwise). */
    interface_ip_address_del_all(hwif_name);

    uint32_t vrf_id = 0;
    /* For now support is only for ipv4 tables */
    set_interface_vrf(hwif_name, 0, vrf_id, false);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_remove_router_interface(sai_object_id_t rif_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    int32_t rif_type;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    sai_status_t status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_TYPE was not passed");

        return SAI_STATUS_FAILURE;
    }
    if (attr.value.s32 == SAI_ROUTER_INTERFACE_TYPE_VLAN)
    {
        SWSS_LOG_NOTICE("Invoking BVI interface create for attr type %d", attr.value.s32);
        return vpp_delete_bvi_interface(rif_id);
    }
    rif_type = attr.value.s32;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
    status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_PORT_ID was not passed");

        return SAI_STATUS_FAILURE;
    }

    sai_object_id_t obj_id = attr.value.oid;

    sai_object_type_t ot = objectTypeQuery(obj_id);

    if (ot == SAI_OBJECT_TYPE_VLAN)
    {
        SWSS_LOG_DEBUG("Skipping tap creation for hostif with object type VLAN");
        return SAI_STATUS_SUCCESS;
    }

    if (ot != SAI_OBJECT_TYPE_PORT && ot != SAI_OBJECT_TYPE_LAG)
    {
        SWSS_LOG_ERROR("SAI_ROUTER_INTERFACE_ATTR_PORT_ID=%s expected to be PORT or LAG but is: %s",
                sai_serialize_object_id(obj_id).c_str(),
                sai_serialize_object_type(ot).c_str());

        return SAI_STATUS_FAILURE;
    }

    if (rif_type != SAI_ROUTER_INTERFACE_TYPE_SUB_PORT)
    {
        vpp_router_interface_remove_vrf(obj_id);

        return SAI_STATUS_SUCCESS;
    }


    attr.id = SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID;
    status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID was not passed");

        return SAI_STATUS_FAILURE;
    }
    uint16_t vlan_id = attr.value.u16;

    /* Resolve parent hw interface name. Mirrors createRouterif (PORT/LAG
     * handling). For LAG sub-port the VPP parent is BondEthernet<bond_id>,
     * not a tap. tap_to_hwif_name() would return "Unknown" for the team
     * device, so the bond name must be built directly from bond_info. */
    std::string if_name;
    platform_bond_info_t bond_info;
    bool found;
    if (ot == SAI_OBJECT_TYPE_LAG) {
        status = get_lag_bond_info(obj_id, bond_info);
        if (status != SAI_STATUS_SUCCESS) {
            return status;
        }
        if_name = format_portchannel_name(bond_info.id);
        found = true;
    } else {
        found = getTapNameFromPortId(obj_id, if_name);
    }
    if (found == false)
    {
        SWSS_LOG_ERROR("host interface for port id %s not found", sai_serialize_object_id(obj_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    const char *dev = if_name.c_str();
    const char *parent_hwif;
    char hw_del_parent[32];
    if (ot == SAI_OBJECT_TYPE_LAG) {
        format_bond_hwif(hw_del_parent, sizeof(hw_del_parent),
                         bond_info.id, 0);
        parent_hwif = hw_del_parent;
    } else {
        parent_hwif = tap_to_hwif_name(dev);
    }

    char host_subifname[32], hw_subifname[64];
    char lcp_host_subifname[32];
    snprintf(host_subifname, sizeof(host_subifname), "%s.%u", dev, vlan_id);
    snprintf(hw_subifname, sizeof(hw_subifname), "%s.%u", parent_hwif, vlan_id);

    /*
     * Compute the LCP host name that matched vpp_create_router_interface:
     * for LAG sub-ports we pair with the VPP-created tap be<id>.<vlan>;
     * for PORT sub-ports it is just dev.<vlan> (host_subifname).
     */
    const char *lcp_host;
    if (ot == SAI_OBJECT_TYPE_LAG) {
        format_be_lcp(lcp_host_subifname, sizeof(lcp_host_subifname),
                      bond_info.id, vlan_id);
        lcp_host = lcp_host_subifname;

        /*
         * Tear down the bidirectional tc redirect first, before removing
         * the LCP pair (otherwise be<id>.<vlan> disappears and the qdisc
         * cleanup would silently no-op while the PortChannel<id>.<vlan>
         * clsact survives and could affect future runs).
         */
        SWSS_LOG_NOTICE("Removing tc filter bridge for LAG sub-port: %s <-> %s",
                        lcp_host, host_subifname);
        del_tc_filter_redirect_egress(std::string(host_subifname));
        del_tc_filter_redirect(std::string(lcp_host));
    } else {
        lcp_host = host_subifname;
    }

    SWSS_LOG_NOTICE("Removing LCP pair for VPP sub interface lcp_host=%s hw_if=%s",
                    lcp_host, hw_subifname);
    int lcp_ret = configure_lcp_interface(hw_subifname, lcp_host, false);
    if (lcp_ret != 0)
    {
        SWSS_LOG_WARN("Failed to remove LCP pair for sub interface lcp_host=%s hw_if=%s, ret=%d",
                      lcp_host, hw_subifname, lcp_ret);
    }

    delete_sub_interface(parent_hwif, vlan_id);
    /* Get new list of physical interfaces from VS */
    refresh_interfaces_list();

/*
    char host_subifname[32], hwif_name[32];
    snprintf(host_subifname, sizeof(host_subifname), "%s.%u", dev, vlan_id);
    snprintf(hwif_name, sizeof(hwif_name), "%s.%u", tap_to_hwif_name(dev), vlan_id);
    configure_lcp_interface(tap_to_hwif_name(dev), host_subifname);
*/

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::createRouterif(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (m_switchConfig->m_useTapDevice == true)
    {
        sai_attribute_t tattr;

        tattr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
        if (get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, object_id, 1, &tattr) == SAI_STATUS_ITEM_NOT_FOUND)
        {
            vpp_create_router_interface(attr_count, attr_list);
        } else {
            vpp_update_router_interface(object_id, attr_count, attr_list);
        }
    }

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sid, switch_id, attr_count, attr_list));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeRouterif(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (m_switchConfig->m_useTapDevice == true)
    {
        vpp_remove_router_interface(objectId);
    }

    auto sid = sai_serialize_object_id(objectId);

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sid));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeVrf(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (m_switchConfig->m_useTapDevice == true)
    {
        vpp_del_ip_vrf(objectId);
    }

    auto sid = sai_serialize_object_id(objectId);

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, sid));

    return SAI_STATUS_SUCCESS;
}
