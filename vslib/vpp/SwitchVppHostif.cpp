#include "SwitchVpp.h"
#include "HostInterfaceInfo.h"
#include "EventPayloadNotification.h"

#include "meta/sai_serialize.h"
#include "meta/NotificationPortStateChange.h"

#include "swss/logger.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <unistd.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <netlink/route/link.h>
#include <netlink/route/addr.h>
#include <linux/if.h>

#include <algorithm>
#include <fstream>

#include "vppxlate/SaiVppXlate.h"

using namespace saivs;

// XXX set must also be supported when we change operational status up/down and
// probably also generate notification then

#define ETH_FRAME_BUFFER_SIZE (0x4000)

#define MAX_INTERFACE_NAME_LEN (IFNAMSIZ-1)

#define SAI_VS_VETH_PREFIX   "v"

int SwitchVpp::vs_create_tap_device(
        _In_ const char *dev,
        _In_ int flags)
{
    SWSS_LOG_ENTER();

    const char *tundev = "/dev/net/tun";

    int fd = open(tundev, O_RDWR);

    if (fd < 0)
    {
        SWSS_LOG_ERROR("failed to open %s", tundev);

        return -1;
    }

    return fd;
}

int SwitchVpp::vs_set_dev_mac_address(
        _In_ const char *dev,
        _In_ const sai_mac_t& mac)
{
    SWSS_LOG_ENTER();

    int s = socket(AF_INET, SOCK_DGRAM, 0);

    if (s < 0)
    {
        SWSS_LOG_ERROR("failed to create socket, errno: %d", errno);

        return -1;
    }

    struct ifreq ifr;

    strncpy(ifr.ifr_name, dev, MAX_INTERFACE_NAME_LEN);

    memcpy(ifr.ifr_hwaddr.sa_data, mac, 6);

    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;

    int err = ioctl(s, SIOCSIFHWADDR, &ifr);

    if (err < 0)
    {
        SWSS_LOG_ERROR("ioctl SIOCSIFHWADDR on socket %d %s failed, err %d", s, dev, err);
    }

    close(s);

    return err;
}

int SwitchVpp::promisc(
        _In_ const char *dev)
{
    SWSS_LOG_ENTER();

    return 0;
}

bool SwitchVpp::hostif_create_tap_veth_forwarding(
        _In_ const std::string &tapname,
        _In_ int tapfd,
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    // we assume here that veth devices were added by user before creating this
    // host interface, vEthernetX will be used for packet transfer between ip
    // namespaces or ethernet device name used in lane map if provided

    std::string vethname = vs_get_veth_name(tapname, port_id);

    int packet_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    if (packet_socket < 0)
    {
        SWSS_LOG_ERROR("failed to open packet socket, errno: %d", errno);

        return false;
    }

    int val = 1;
    if (setsockopt(packet_socket, SOL_PACKET, PACKET_AUXDATA, &val, sizeof(val)) < 0)
    {
        SWSS_LOG_ERROR("setsockopt() set PACKET_AUXDATA failed: %s", strerror(errno));
        return false;
    }

    // bind to device

    struct sockaddr_ll sock_address;

    memset(&sock_address, 0, sizeof(sock_address));

    sock_address.sll_family = PF_PACKET;
    sock_address.sll_protocol = htons(ETH_P_ALL);
    sock_address.sll_ifindex = if_nametoindex(vethname.c_str());

    if (sock_address.sll_ifindex == 0)
    {
        SWSS_LOG_ERROR("failed to get interface index for %s", vethname.c_str());

        close(packet_socket);

        return false;
    }

    SWSS_LOG_NOTICE("interface index = %d, %s\n", sock_address.sll_ifindex, vethname.c_str());

    if (promisc(vethname.c_str()))
    {
        SWSS_LOG_ERROR("promisc failed on %s", vethname.c_str());

        close(packet_socket);

        return false;
    }

    if (bind(packet_socket, (struct sockaddr*) &sock_address, sizeof(sock_address)) < 0)
    {
        SWSS_LOG_ERROR("bind failed on %s", vethname.c_str());

        close(packet_socket);

        return false;
    }

    m_hostif_info_map[tapname] =
        std::make_shared<HostInterfaceInfo>(
                sock_address.sll_ifindex,
                packet_socket,
                tapfd,
                tapname,
                port_id,
                m_switchConfig->m_eventQueue);

    // NOTE: threads are not run

    SWSS_LOG_NOTICE("setup forward rule for %s succeeded", tapname.c_str());

    return true;
}

sai_status_t SwitchVpp::vs_create_hostif_tap_interface(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // validate SAI_HOSTIF_ATTR_TYPE

    auto attr_type = sai_metadata_get_attr_by_id(SAI_HOSTIF_ATTR_TYPE, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_HOSTIF_ATTR_TYPE was not passed");

        return SAI_STATUS_FAILURE;
    }

    /* The genetlink host interface is created to associate trap group to genetlink family and multicast group
     * created by driver. It does not create any netdev interface. Hence skipping tap interface creation
     */
    if (attr_type->value.s32 == SAI_HOSTIF_TYPE_GENETLINK)
    {
        SWSS_LOG_DEBUG("Skipping tap create for hostif type genetlink");

        return SAI_STATUS_SUCCESS;
    }

    if (attr_type->value.s32 != SAI_HOSTIF_TYPE_NETDEV)
    {
        SWSS_LOG_ERROR("only SAI_HOSTIF_TYPE_NETDEV is supported");

        return SAI_STATUS_FAILURE;
    }

    // validate SAI_HOSTIF_ATTR_OBJ_ID

    auto attr_obj_id = sai_metadata_get_attr_by_id(SAI_HOSTIF_ATTR_OBJ_ID, attr_count, attr_list);

    if (attr_obj_id == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_HOSTIF_ATTR_OBJ_ID was not passed");

        return SAI_STATUS_FAILURE;
    }

    sai_object_id_t obj_id = attr_obj_id->value.oid;

    sai_object_type_t ot = objectTypeQuery(obj_id);

    if (ot == SAI_OBJECT_TYPE_VLAN)
    {
        SWSS_LOG_DEBUG("Skipping tap creation for hostif with object type VLAN");
        return SAI_STATUS_SUCCESS;
    }

    if (ot != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_ERROR("SAI_HOSTIF_ATTR_OBJ_ID=%s expected to be PORT but is: %s",
                sai_serialize_object_id(obj_id).c_str(),
                sai_serialize_object_type(ot).c_str());

        return SAI_STATUS_FAILURE;
    }

    // validate SAI_HOSTIF_ATTR_NAME

    auto attr_name = sai_metadata_get_attr_by_id(SAI_HOSTIF_ATTR_NAME, attr_count, attr_list);

    if (attr_name == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_HOSTIF_ATTR_NAME was not passed");

        return SAI_STATUS_FAILURE;
    }

    if (strnlen(attr_name->value.chardata, sizeof(attr_name->value.chardata)) >= MAX_INTERFACE_NAME_LEN)
    {
        SWSS_LOG_ERROR("interface name is too long: %.*s", MAX_INTERFACE_NAME_LEN, attr_name->value.chardata);

        return SAI_STATUS_FAILURE;
    }

    std::string name = std::string(attr_name->value.chardata);

    // create TAP device

    SWSS_LOG_INFO("creating hostif %s", name.c_str());

    int tapfd;

    tapfd = vs_create_tap_device(name.c_str(), IFF_TAP | IFF_MULTI_QUEUE | IFF_NO_PI | IFF_VNET_HDR);

    if (tapfd < 0)
    {
        SWSS_LOG_ERROR("failed to create TAP device for %s", name.c_str());

        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_NOTICE("created TAP device for %s, fd: %d", name.c_str(), tapfd);
    const char *dev = name.c_str();
    const char *hwif_name = tap_to_hwif_name(dev);

    configure_lcp_interface(hwif_name, dev, true);

    {
        bool link_up = false;

        interface_get_state(hwif_name, &link_up);

        auto state = link_up ? SAI_PORT_OPER_STATUS_UP : SAI_PORT_OPER_STATUS_DOWN;

        send_port_oper_status_notification(obj_id, state, true);

        SWSS_LOG_NOTICE("VPP interface %s(%s) oper state %s", hwif_name, dev,
                (link_up ? "UP" : "DOWN"));
    }

    sai_attribute_t attr;

    memset(&attr, 0, sizeof(attr));

    attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;

    sai_status_t status = get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get SAI_SWITCH_ATTR_SRC_MAC_ADDRESS on switch %s: %s",
                sai_serialize_object_id(m_switch_id).c_str(),
                sai_serialize_status(status).c_str());
    }

    int err = vs_set_dev_mac_address(name.c_str(), attr.value.mac);

    if (err < 0)
    {
        SWSS_LOG_ERROR("failed to set MAC address %s for %s",
                sai_serialize_mac(attr.value.mac).c_str(),
                name.c_str());

        close(tapfd);

        return SAI_STATUS_FAILURE;
    }

    err = sw_interface_set_mac(hwif_name, attr.value.mac);

    if (err < 0)
    {
        SWSS_LOG_ERROR("failed to set MAC address %s for %s",
                sai_serialize_mac(attr.value.mac).c_str(),
                hwif_name);

        close(tapfd);

        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_INFO("Successfully set mac to %s for %s", sai_serialize_mac(attr.value.mac).c_str(), name.c_str());

    // enable ipv6, which will set link local address based on mac. ipv4 can be enabled
    // when ip is configured.
    err = sw_interface_ip6_enable_disable(hwif_name, true);
    if (err < 0)
    {
        SWSS_LOG_ERROR("failed to enable ipv6 for %s", hwif_name);
        close(tapfd);
        return SAI_STATUS_FAILURE;
    }

    setIfNameToPortId(name, obj_id);
    setPortIdToTapName(obj_id, name);

    SWSS_LOG_INFO("created tap interface %s", name.c_str());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vs_remove_hostif_tap_interface(
        _In_ sai_object_id_t hostif_id)
{
    SWSS_LOG_ENTER();

    // get tap interface name

    sai_attribute_t attr;


    attr.id = SAI_HOSTIF_ATTR_TYPE;
    sai_status_t status = get(SAI_OBJECT_TYPE_HOSTIF, hostif_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get attr type for hostif %s",
                sai_serialize_object_id(hostif_id).c_str());
        return status;
    }


    /* The genetlink host interface is created to associate trap group to genetlink family and multicast group
     * created by driver. It does not create any netdev interface. Hence skipping tap interface deletion
     */
    if (attr.value.s32 == SAI_HOSTIF_TYPE_GENETLINK)
    {
        SWSS_LOG_DEBUG("Skipping tap delete for hostif type genetlink");
        return SAI_STATUS_SUCCESS;
    }

    attr.id = SAI_HOSTIF_ATTR_OBJ_ID;
    status = get(SAI_OBJECT_TYPE_HOSTIF, hostif_id, 1, &attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to get object ID for hostif %s", sai_serialize_object_id(hostif_id).c_str());
        return status;
    }
    if (objectTypeQuery(attr.value.oid) == SAI_OBJECT_TYPE_VLAN)
    {
        SWSS_LOG_DEBUG("Skipping tap deletion for hostif with object type VLAN");
        return SAI_STATUS_SUCCESS;
    }

    attr.id = SAI_HOSTIF_ATTR_NAME;

    status = get(SAI_OBJECT_TYPE_HOSTIF, hostif_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get attr name for hostif %s",
                sai_serialize_object_id(hostif_id).c_str());

        return status;
    }

    if (strnlen(attr.value.chardata, sizeof(attr.value.chardata)) >= MAX_INTERFACE_NAME_LEN)
    {
        SWSS_LOG_ERROR("interface name is too long: %.*s", MAX_INTERFACE_NAME_LEN, attr.value.chardata);

        return SAI_STATUS_FAILURE;
    }

    // TODO this should be hosif_id or if index ?
    std::string name = std::string(attr.value.chardata);

    /*
       auto it = m_hostif_info_map.find(name);

       if (it == m_hostif_info_map.end())
       {
       SWSS_LOG_ERROR("failed to find host info entry for tap device: %s", name.c_str());

       return SAI_STATUS_FAILURE;
       }

       SWSS_LOG_NOTICE("attempting to remove tap device: %s", name.c_str());

       auto info = it->second; // destructor will stop threads
       */
    // remove host info entry from map

    // m_hostif_info_map.erase(it);

    // remove interface mapping

    // std::string vname = vpp_get_veth_name(name, info->m_portId);

    sai_object_id_t port_id = getPortIdFromIfName(name);

    removeIfNameToPortId(name);
    removePortIdToTapName(port_id);

    SWSS_LOG_NOTICE("successfully removed hostif tap device: %s", name.c_str());

    return SAI_STATUS_SUCCESS;
}

bool SwitchVpp::hasIfIndex(
        _In_ int ifindex) const
{
    SWSS_LOG_ENTER();

    if (m_hostif_info_map.size() == 0)
    {
        return false;
    }

    for (auto& kvp: m_hostif_info_map)
    {
        if (kvp.second->m_ifindex == ifindex)
        {
            return true;
        }
    }

    return false;
}

// VPP

// TODO to config
static const char *sonic_vpp_ifmap = "/usr/share/sonic/hwsku/sonic_vpp_ifmap.ini";

void SwitchVpp::populate_if_mapping()
{
    SWSS_LOG_ENTER();

    if (mapping_init)
    {
        return;
    }

    FILE *fp;
    char sonic_name[64], vpp_name[64];

    fp = fopen(sonic_vpp_ifmap, "r");

    if (!fp)
    {
        return;
    }

    while (fscanf(fp, "%s %s", sonic_name, vpp_name) != EOF)
    {
        std::string tap_name, hwif_name;

        tap_name = std::string(sonic_name);
        hwif_name = std::string(vpp_name);

        m_hostif_hwif_map[tap_name] = hwif_name;
        m_hwif_hostif_map[hwif_name] = tap_name;
    }

    mapping_init = 1;

    fclose(fp);
}

const char* SwitchVpp::tap_to_hwif_name(
        _In_ const char *name)
{
    SWSS_LOG_ENTER();

    populate_if_mapping();

    std::string tap_name = std::string(name);

    auto it = m_hostif_hwif_map.find(tap_name);

    if (it == m_hostif_hwif_map.end())
    {
        SWSS_LOG_ERROR("failed to find hwif info entry for hostif device: %s", tap_name.c_str());

        return "Unknown";
    }

    SWSS_LOG_DEBUG("Found hwif %s info entry for hostif device: %s", it->second.c_str(), tap_name.c_str());

    return it->second.c_str();
}

const char* SwitchVpp::hwif_to_tap_name(
        _In_ const char *name)
{
    SWSS_LOG_ENTER();

    populate_if_mapping();

    std::string tap_name = std::string(name);

    auto it = m_hwif_hostif_map.find(tap_name);

    if (it == m_hwif_hostif_map.end())
    {
        SWSS_LOG_ERROR("failed to find hostif info entry for hwif device: %s", tap_name.c_str());

        return "Unknown";
    }

    SWSS_LOG_DEBUG("Found  hostif %s info entry for hwif device: %s", it->second.c_str(), tap_name.c_str());

    return it->second.c_str();
}
