#include "SwitchVpp.h"
#include "HostInterfaceInfo.h"
#include "EventPayloadNotification.h"
#include "SwitchVppUtils.h"

#include "meta/sai_serialize.h"
#include "meta/NotificationPortStateChange.h"

#include "swss/exec.h"
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
#include <cctype>
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

int SwitchVpp::vs_set_dev_admin_up(
        _In_ const char *dev,
        _In_ bool up)
{
    SWSS_LOG_ENTER();

    int s = socket(AF_INET, SOCK_DGRAM, 0);

    if (s < 0)
    {
        SWSS_LOG_ERROR("failed to create socket, errno: %d", errno);

        return -1;
    }

    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));

    strncpy(ifr.ifr_name, dev, MAX_INTERFACE_NAME_LEN);

    int err = ioctl(s, SIOCGIFFLAGS, &ifr);

    if (err < 0)
    {
        SWSS_LOG_ERROR("ioctl SIOCGIFFLAGS on %s failed, err %d", dev, err);

        close(s);

        return err;
    }

    if (up)
    {
        ifr.ifr_flags |= IFF_UP;
    }
    else
    {
        ifr.ifr_flags &= ~IFF_UP;
    }

    err = ioctl(s, SIOCSIFFLAGS, &ifr);

    if (err < 0)
    {
        SWSS_LOG_ERROR("ioctl SIOCSIFFLAGS %s on %s failed, err %d",
                (up ? "UP" : "DOWN"), dev, err);
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

sai_status_t SwitchVpp::add_tc_filter_redirect(
        _In_ const std::string& tap,
        _In_ const std::string& hostIfname)
{
    SWSS_LOG_ENTER();

    int ret;
    std::stringstream cmd;
    std::string res;

    cmd << "tc qdisc add dev " << tap << " ingress";
    ret = swss::exec(cmd.str(), res);
    if (ret) {
        SWSS_LOG_ERROR("Command '%s' failed with rc %d", cmd.str().c_str(), ret);
        // Not returning here, as qdisc may already exist
    }

    cmd.str("");
    cmd.clear();
    cmd << "tc filter add dev " << tap << " parent ffff: protocol all prio 2 u32 match u32 0 0 flowid 1:1"
        << " action mirred ingress redirect dev " << hostIfname;
    ret = swss::exec(cmd.str(), res);
    if (ret) {
        SWSS_LOG_ERROR("Command '%s' failed with rc %d", cmd.str().c_str(), ret);
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
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

bool SwitchVpp::register_hostif_info(
        _In_ const std::string &tapname,
        _In_ int tapfd,
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    // VPP uses the TAP directly through Linux CP. No packet socket or veth
    // forwarding is needed here; syncOnLinkMsg() only needs this ifindex
    // to accept runtime RTM_NEWLINK events for this hostif.
    int ifindex = if_nametoindex(tapname.c_str());
    if (ifindex == 0)
    {
        SWSS_LOG_ERROR("failed to get interface index for %s", tapname.c_str());

        return false;
    }

    m_hostif_info_map[tapname] =
        std::make_shared<HostInterfaceInfo>(
                ifindex,
                -1,
                tapfd,
                tapname,
                port_id,
                m_switchConfig->m_eventQueue);

    SWSS_LOG_INFO(
            "registered hostif info for %s, ifindex %d",
            tapname.c_str(),
            ifindex);

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
    const std::string hwif = m_ifaceRegistry.resolveHwIfByOsIf(name);

    if (hwif.empty())
    {
        /*
         * Every use of hwif_name below either calls into VPP or formats it with
         * "%s", so there is nothing useful to do without it. This used to run on
         * with the literal "Unknown" and fail later at sw_interface_set_mac();
         * failing here reports the actual cause.
         */
        SWSS_LOG_ERROR("no hwif mapping for hostif %s, cannot attach it to VPP", dev);

        close(tapfd);

        return SAI_STATUS_FAILURE;
    }

    const char *hwif_name = hwif.c_str();

    configure_lcp_interface(hwif_name, dev, true);
    interface_set_promiscuous(hwif_name, true);

    /*
     * Re-apply the port's configured admin state to the freshly created VPP host
     * interface. The SAI port admin state may have been set (e.g. during port
     * bring-up) before this host interface existed; at that point
     * vpp_set_interface_state() could not resolve the hwif name (no tap yet) and
     * the admin-up was silently dropped. Without re-applying it here the VPP
     * host-interface stays admin-down, which keeps the paired kernel netdev (and
     * therefore the PTF-side veth carrier) down, causing ENETDOWN on transmit.
     */
    {
        sai_attribute_t admin_attr;
        admin_attr.id = SAI_PORT_ATTR_ADMIN_STATE;
        if (get(SAI_OBJECT_TYPE_PORT, obj_id, 1, &admin_attr) == SAI_STATUS_SUCCESS &&
            admin_attr.value.booldata)
        {
            interface_set_state(hwif_name, true);
            SWSS_LOG_NOTICE("Applied admin-up to VPP host interface %s for port %s",
                    hwif_name, sai_serialize_object_id(obj_id).c_str());
        }
    }

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

    if (!register_hostif_info(name, tapfd, obj_id))
    {
        SWSS_LOG_ERROR("failed to register hostif info for %s", name.c_str());
        close(tapfd);

        return SAI_STATUS_FAILURE;
    }

    /*
     * Write-only from the VPP backend's point of view: nothing under vslib/vpp/
     * reads these two maps any more, every VPP lookup goes through
     * m_ifaceRegistry instead. They are kept in step purely so the inherited
     * SwitchState/SwitchStateBase paths that still consult them -- getPortStat()
     * being the one VPP actually reaches -- see the same state they always did.
     */
    setIfNameToPortId(name, obj_id);
    setPortIdToTapName(obj_id, name);

    /*
     * The port record is created by the lane based resolution, which may not
     * have run yet for this port -- so force it once here before attaching the
     * tap. VPP is definitely up at this point, we have just been calling it.
     */
    std::string reg_hwif_name = hwif_name;
    //@todo: can we use lane to report hwif_name?
    if (!m_ifaceRegistry.findByHwif(reg_hwif_name))
    {
        std::string resolved;

        if (getPortHwifNameFromLane(obj_id, resolved))
        {
            reg_hwif_name = resolved;
        }
    }

    m_ifaceRegistry.setTapName(reg_hwif_name, name);

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
     * Only needed to undo the compatibility writes in
     * vs_create_hostif_tap_interface(). Resolved through the registry rather
     * than getPortIdFromIfName(), and taken before clearTapName() below drops
     * the tap index.
     */
    auto rec = m_ifaceRegistry.findByTap(name);

    sai_object_id_t port_id = rec ? rec->getOid() : SAI_NULL_OBJECT_ID;

    auto it = m_hostif_info_map.find(name);

    if (it != m_hostif_info_map.end())
    {
        SWSS_LOG_INFO("attempting to remove host info entry for tap device: %s", name.c_str());

        port_id = it->second->m_portId;
        m_hostif_info_map.erase(it);
    }

    /*
     * Tear down the VPP state that vs_create_hostif_tap_interface() set up, so a
     * later re-create of the same host interface in the SAME saiserver process
     * starts from a clean slate. Without this the leftover linux-cp pair and
     * enabled IPv6 make the next create fail (config_lcp_hostif / ip6-enable
     * return VALUE_EXIST), which previously required a full backend restart per
     * test to avoid. Both calls are idempotent (vpp_normalize_ret tolerates
     * NO_SUCH_ENTRY on delete), so removing a partially-created hostif is safe.
     */
    const std::string hwif_name = m_ifaceRegistry.resolveHwIfByOsIf(name);

    if (!hwif_name.empty())
    {
        sw_interface_ip6_enable_disable(hwif_name.c_str(), false);
        configure_lcp_interface(hwif_name.c_str(), name.c_str(), false);

        /*
         * Only the tap goes away. The port itself still exists in VPP and is
         * still reachable through the lane based lookup, so the record must
         * survive -- clearTapName(), never remove().
         */
        m_ifaceRegistry.clearTapName(hwif_name);
    }

    /* Compatibility only, see vs_create_hostif_tap_interface(). */
    removeIfNameToPortId(name);

    if (port_id != SAI_NULL_OBJECT_ID)
    {
        removePortIdToTapName(port_id);
    }

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

