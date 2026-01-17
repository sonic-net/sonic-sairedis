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
#include <regex>
#include <fstream>

using namespace saivs;

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
            SWSS_LOG_ERROR("host interface for prefix not found: %s", prefix_str.c_str());
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

    if (objectTypeQuery(object_id) == SAI_OBJECT_TYPE_LAG) {
        platform_bond_info_t bond_info;
        sai_status_t status = get_lag_bond_info(object_id, bond_info);
        if (status != SAI_STATUS_SUCCESS)
        {
            return false;
        }
        ifname = std::string(BONDETHERNET_PREFIX) + std::to_string(bond_info.id);
        return true;
    }

    std::string if_name;
    bool found = getTapNameFromPortId(object_id, if_name);

    if (found == false)
    {
        SWSS_LOG_ERROR("host interface for port id %s not found", sai_serialize_object_id(object_id).c_str());
        return false;
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
        SWSS_LOG_NOTICE("Failed find port oid for tap interface %s", tap);
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

    const char *hwifname = tap_to_hwif_name(if_name.c_str());
    char hw_subifname[32];
    const char *hw_ifname;

    if (vlan_id) {
        snprintf(hw_subifname, sizeof(hw_subifname), "%s.%u", hwifname, vlan_id);
        hw_ifname = hw_subifname;
    } else {
        hw_ifname = hwifname;
    }

    int ret = interface_ip_address_add_del(hw_ifname, &vpp_ip_prefix, is_add);

    if (ret == 0)
    {
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
        uint32_t bond_id = std::stoi(full_if_name.substr(strlen(PORTCHANNEL_PREFIX)));
        snprintf(hw_bondifname, sizeof(hw_bondifname), "%s%d", BONDETHERNET_PREFIX, bond_id);
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
            VPP_IP_API_FLOW_HASH_PROTO;

        int ret = vpp_ip_flow_hash_set(vrf_id, hash_mask, AF_INET);
        SWSS_LOG_NOTICE("ip flow hash set for VRF %s with vrf_id %u in VS, status %d",
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
        if_name = std::string(PORTCHANNEL_PREFIX) + std::to_string(bond_info.id);
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

    if (attr_type->value.s32 == SAI_ROUTER_INTERFACE_TYPE_SUB_PORT)
    {
        snprintf(host_subifname, sizeof(host_subifname), "%s.%u", dev, vlan_id);

        /* The host(tap) subinterface is also created as part of the vpp subinterface creation */
        create_sub_interface(tap_to_hwif_name(dev), vlan_id, vlan_id);

        /* Get new list of physical interfaces from VS */
        refresh_interfaces_list();

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
	    snprintf(hw_bondifname, sizeof(hw_bondifname), "%s%d", BONDETHERNET_PREFIX, bond_info.id);
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

    if (rif_type != SAI_ROUTER_INTERFACE_TYPE_SUB_PORT)
    {
        vpp_router_interface_remove_vrf(obj_id);

        return SAI_STATUS_SUCCESS;
    }


    attr.id = SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID;
    status = get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, object_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("attr SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID was not passed");

        return SAI_STATUS_FAILURE;
    }

    uint16_t vlan_id = attr.value.u16;

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
        if_name = std::string(PORTCHANNEL_PREFIX) + std::to_string(bond_info.id);
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
        snprintf(hw_bondifname, sizeof(hw_bondifname), "%s%d", BONDETHERNET_PREFIX, bond_info.id);
        hwif_name = hw_bondifname;
    } else {
        hwif_name = tap_to_hwif_name(if_name.c_str());
    }

    SWSS_LOG_NOTICE("Resetting to default vrf for interface %s, %s", linux_ifname, hwif_name);

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

    std::string if_name;
    bool found = getTapNameFromPortId(obj_id, if_name);
    if (found == false)
    {
        SWSS_LOG_ERROR("host interface for port id %s not found", sai_serialize_object_id(obj_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    const char *dev = if_name.c_str();

    delete_sub_interface(tap_to_hwif_name(dev), vlan_id);
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
