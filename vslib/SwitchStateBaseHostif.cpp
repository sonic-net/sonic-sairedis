#include "SwitchStateBase.h"
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

using namespace saivs;

// XXX set must also be supported when we change operational status up/down and
// probably also generate notification then

#define ETH_FRAME_BUFFER_SIZE (0x4000)

#define MAX_INTERFACE_NAME_LEN (IFNAMSIZ-1)

#define SAI_VS_VETH_PREFIX   "v"

int SwitchStateBase::vs_create_tap_device(
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

    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = (short int)flags;  // IFF_TUN or IFF_TAP, IFF_NO_PI

    strncpy(ifr.ifr_name, dev, MAX_INTERFACE_NAME_LEN);

    int err = ioctl(fd, TUNSETIFF, (void *) &ifr);

    if (err < 0)
    {
        SWSS_LOG_ERROR("ioctl TUNSETIFF on fd %d %s failed, err %d", fd, dev, err);

        close(fd);

        return err;
    }

    return fd;
}

int SwitchStateBase::vs_set_dev_mac_address(
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

void SwitchStateBase::update_port_oper_status(
        _In_ sai_object_id_t port_id,
        _In_ sai_port_oper_status_t port_oper_status)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_OPER_STATUS;
    attr.value.s32 = port_oper_status;

    sai_status_t status = set(SAI_OBJECT_TYPE_PORT, port_id, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to update port status %s: %s",
                sai_serialize_object_id(port_id).c_str(),
                sai_serialize_port_oper_status(port_oper_status).c_str());
    }
}

void SwitchStateBase::send_port_oper_status_notification(
        _In_ sai_object_id_t portId,
        _In_ sai_port_oper_status_t status,
        _In_ bool force)
{
    SWSS_LOG_ENTER();

    sai_port_oper_status_notification_t data;

    data.port_id = portId;
    data.port_state = status;

    auto meta = getMeta();

    if (meta)
    {
        meta->meta_sai_on_port_state_change(1, &data);
    }

    auto objectType = objectTypeQuery(portId); // can be port, bridge port, lag

    if (objectType != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_ERROR("object type %s not supported on portId %s",
                sai_serialize_object_type(objectType).c_str(),
                sai_serialize_object_id(portId).c_str());
        return;
    }

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_OPER_STATUS;

    if (get(objectType, portId, 1, &attr) != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get port attribute SAI_PORT_ATTR_OPER_STATUS");
    }
    else
    {
        if (force)
        {
            SWSS_LOG_NOTICE("explicitly send SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY for port %s: %s (port was UP)",
                    sai_serialize_object_id(data.port_id).c_str(),
                    sai_serialize_port_oper_status(data.port_state).c_str());

        }
        else if ((sai_port_oper_status_t)attr.value.s32 == data.port_state)
        {
            SWSS_LOG_INFO("port oper status didn't changed, will not send notification");
            return;
        }
    }

    attr.id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;

    if (get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &attr) != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY for switch %s",
                sai_serialize_object_id(m_switch_id).c_str());
        return;
    }

    if (attr.value.ptr == NULL)
    {
        SWSS_LOG_INFO("SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY callback is NULL");
        return;
    }

    sai_switch_notifications_t sn = { };

    sn.on_port_state_change = (sai_port_state_change_notification_fn)attr.value.ptr;

    attr.id = SAI_PORT_ATTR_OPER_STATUS;

    update_port_oper_status(portId, data.port_state);

    SWSS_LOG_NOTICE("send event SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY for port %s: %s",
            sai_serialize_object_id(data.port_id).c_str(),
            sai_serialize_port_oper_status(data.port_state).c_str());

    auto str = sai_serialize_port_oper_status_ntf(1, &data);

    auto ntf = std::make_shared<sairedis::NotificationPortStateChange>(str);

    auto payload = std::make_shared<EventPayloadNotification>(ntf, sn);

    m_switchConfig->m_eventQueue->enqueue(std::make_shared<Event>(EVENT_TYPE_NOTIFICATION, payload));
}

int SwitchStateBase::ifup(
        _In_ const char *dev,
        _In_ sai_object_id_t port_id,
        _In_ bool up,
        _In_ bool explicitNotification)
{
    SWSS_LOG_ENTER();

    int s = socket(AF_INET, SOCK_DGRAM, 0);

    if (s < 0)
    {
        SWSS_LOG_ERROR("failed to open socket: %d", s);

        return -1;
    }

    struct ifreq ifr;

    memset(&ifr, 0, sizeof ifr);

    strncpy(ifr.ifr_name, dev , MAX_INTERFACE_NAME_LEN);

    int err = ioctl(s, SIOCGIFFLAGS, &ifr);

    if (err < 0)
    {
        SWSS_LOG_ERROR("ioctl SIOCGIFFLAGS on socket %d %s failed, err %d", s, dev, err);

        close(s);

        return err;
    }

    if (up && explicitNotification && (ifr.ifr_flags & IFF_UP))
    {
        close(s);

        // interface status didn't changed, we need to send manual notification
        // that interface status is UP but that notification would need to be
        // sent after actual interface creation, since user may receive that
        // notification before hostif create function will actually return,
        // this can happen when syncd will be operating in synchronous mode

        send_port_oper_status_notification(port_id, SAI_PORT_OPER_STATUS_UP, true);

        return 0;
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
        SWSS_LOG_ERROR("ioctl SIOCSIFFLAGS on socket %d %s failed, err %d", s, dev, err);
    }

    close(s);

    return err;
}

int SwitchStateBase::promisc(
        _In_ const char *dev)
{
    SWSS_LOG_ENTER();

    int s = socket(AF_INET, SOCK_DGRAM, 0);

    if (s < 0)
    {
        SWSS_LOG_ERROR("failed to open socket: %d", s);

        return -1;
    }

    struct ifreq ifr;

    memset(&ifr, 0, sizeof ifr);

    strncpy(ifr.ifr_name, dev , MAX_INTERFACE_NAME_LEN);

    int err = ioctl(s, SIOCGIFFLAGS, &ifr);

    if (err < 0)
    {
        SWSS_LOG_ERROR("ioctl SIOCGIFFLAGS on socket %d %s failed, err %d", s, dev, err);

        close(s);

        return err;
    }

    if (ifr.ifr_flags & IFF_PROMISC)
    {
        close(s);

        return 0;
    }

    ifr.ifr_flags |= IFF_PROMISC;

    err = ioctl(s, SIOCSIFFLAGS, &ifr);

    if (err < 0)
    {
        SWSS_LOG_ERROR("ioctl SIOCSIFFLAGS on socket %d %s failed, err %d", s, dev, err);
    }

    close(s);

    return err;
}

int SwitchStateBase::vs_set_dev_mtu(
        _In_ const char*name,
        _In_ int mtu)
{
    SWSS_LOG_ENTER();

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

    if (sock < 0)
    {
        SWSS_LOG_ERROR("create socket failed: %s", strerror(errno));

        return sock;
    }

    struct ifreq ifr;

    strncpy(ifr.ifr_name, name, MAX_INTERFACE_NAME_LEN);

    ifr.ifr_mtu = mtu;

    int err = ioctl(sock, SIOCSIFMTU, &ifr);

    if (err == 0)
    {
        SWSS_LOG_INFO("success set mtu on %s to %d", name, mtu);
        return 0;
    }

    SWSS_LOG_WARN("failed to set mtu on %s to %d: %s", name, mtu, strerror(errno));
    return err;
}


std::string SwitchStateBase::vs_get_veth_name(
        _In_ const std::string& tapname,
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    std::string vethname = SAI_VS_VETH_PREFIX + tapname;

    // check if user override interface names

    sai_attribute_t attr;

    uint32_t lanes[8];

    attr.id = SAI_PORT_ATTR_HW_LANE_LIST;

    attr.value.u32list.count = 8;
    attr.value.u32list.list = lanes;

    if (get(SAI_OBJECT_TYPE_PORT, port_id, 1, &attr) != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_WARN("failed to get port %s lanes, using veth: %s",
                sai_serialize_object_id(port_id).c_str(),
                vethname.c_str());
    }
    else
    {
        if (m_switchConfig->m_laneMap)
        {
            auto ifname = m_switchConfig->m_laneMap->getInterfaceFromLaneNumber(lanes[0]);

            if (ifname == "")
            {
                SWSS_LOG_WARN("failed to get ifname from lane number %u", lanes[0]);
            }
            else
            {
                SWSS_LOG_NOTICE("using %s instead of %s", ifname.c_str(), vethname.c_str());

                vethname = ifname;
            }
        }
        else
        {
            SWSS_LOG_WARN("laneMap is NULL for switch %s, index: %u",
                    sai_serialize_object_id(m_switch_id).c_str(),
                    m_switchConfig->m_switchIndex);
        }
    }

    return vethname;
}

bool SwitchStateBase::hostif_create_tap_veth_forwarding(
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

    SWSS_LOG_NOTICE("setup forward rule for %s succeeded", tapname.c_str());

    return true;
}

sai_status_t SwitchStateBase::vs_create_hostif_tap_interface(
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

    int tapfd = vs_create_tap_device(name.c_str(), IFF_TAP | IFF_MULTI_QUEUE | IFF_NO_PI);

    if (tapfd < 0)
    {
        SWSS_LOG_ERROR("failed to create TAP device for %s", name.c_str());

        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_INFO("created TAP device for %s, fd: %d", name.c_str(), tapfd);

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

    std::string vname = vs_get_veth_name(name, obj_id);

    int mtu = ETH_FRAME_BUFFER_SIZE;

    sai_attribute_t attrmtu;

    attrmtu.id = SAI_PORT_ATTR_MTU;

    if (get(SAI_OBJECT_TYPE_PORT, obj_id, 1, &attrmtu) == SAI_STATUS_SUCCESS)
    {
        mtu = attrmtu.value.u32;

        SWSS_LOG_INFO("setting new MTU: %d on %s", mtu, vname.c_str());
    }

    vs_set_dev_mtu(vname.c_str(), mtu);

    attr.id = SAI_PORT_ATTR_ADMIN_STATE;

    status = get(SAI_OBJECT_TYPE_PORT, obj_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get admin state for port %s",
                sai_serialize_object_id(obj_id).c_str());

        return status;
    }

    if (ifup(vname.c_str(), obj_id, attr.value.booldata, false))
    {
        SWSS_LOG_ERROR("ifup failed on %s", vname.c_str());

        return SAI_STATUS_FAILURE;
    }

    if (!hostif_create_tap_veth_forwarding(name, tapfd, obj_id))
    {
        SWSS_LOG_ERROR("forwarding rule on %s was not added", name.c_str());
    }

    SWSS_LOG_INFO("mapping interface %s to port id %s",
            vname.c_str(),
            sai_serialize_object_id(obj_id).c_str());

    setIfNameToPortId(vname, obj_id);
    setPortIdToTapName(obj_id, name);

    SWSS_LOG_INFO("created tap interface %s", name.c_str());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::vs_recreate_hostif_tap_interfaces()
{
    SWSS_LOG_ENTER();

    if (m_switchConfig->m_useTapDevice == false)
    {
        return SAI_STATUS_SUCCESS;
    }

    auto &objectHash = m_objectHash.at(SAI_OBJECT_TYPE_HOSTIF);

    SWSS_LOG_NOTICE("attempt to recreate %zu tap devices for host interfaces", objectHash.size());

    for (auto okvp: objectHash)
    {
        std::vector<sai_attribute_t> attrs;

        for (auto akvp: okvp.second)
        {
            attrs.push_back(*akvp.second->getAttr());
        }

        vs_create_hostif_tap_interface((uint32_t)attrs.size(), attrs.data());
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::vs_remove_hostif_tap_interface(
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

    auto it = m_hostif_info_map.find(name);

    if (it == m_hostif_info_map.end())
    {
        SWSS_LOG_ERROR("failed to find host info entry for tap device: %s", name.c_str());

        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_NOTICE("attempting to remove tap device: %s", name.c_str());

    auto info = it->second; // destructor will stop threads

    // remove host info entry from map

    m_hostif_info_map.erase(it);

    // remove interface mapping

    std::string vname = vs_get_veth_name(name, info->m_portId);

    removeIfNameToPortId(vname);
    removePortIdToTapName(info->m_portId);

    SWSS_LOG_NOTICE("successfully removed hostif tap device: %s", name.c_str());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::createHostif(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (m_switchConfig->m_useTapDevice == true)
    {
        auto status = vs_create_hostif_tap_interface(attr_count, attr_list);

        CHECK_STATUS(status);
    }

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_HOSTIF, sid, switch_id, attr_count, attr_list));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchStateBase::removeHostif(
        _In_ sai_object_id_t objectId)
{
    SWSS_LOG_ENTER();

    if (m_switchConfig->m_useTapDevice == true)
    {
        auto status = vs_remove_hostif_tap_interface(objectId);

        CHECK_STATUS(status);
    }

    auto sid = sai_serialize_object_id(objectId);

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_HOSTIF, sid));

    return SAI_STATUS_SUCCESS;
}

bool SwitchStateBase::hasIfIndex(
        _In_ int ifindex) const
{
    SWSS_LOG_ENTER();

    for (auto& kvp: m_hostif_info_map)
    {
        if (kvp.second->m_ifindex == ifindex)
        {
            return true;
        }
    }

    return false;
}

bool SwitchStateBase::vs_get_oper_speed(
        _In_ sai_object_id_t port_id,
        _Out_ uint32_t& speed)

{
    SWSS_LOG_ENTER();

    auto info = findHostInterfaceInfoByPort(port_id);

    if (!info)
    {
        SWSS_LOG_ERROR("Port %s don't exists",
            sai_serialize_object_id(port_id).c_str());

        return false;
    }

    auto veth_name = vs_get_veth_name(info->m_name, port_id);
    std::string veth_speed_filename = "/sys/class/net/";

    veth_speed_filename += veth_name;
    veth_speed_filename += "/speed";

    std::ifstream ifs(veth_speed_filename);

    if (!ifs.is_open())
    {
        SWSS_LOG_ERROR("Failed to open %s", veth_speed_filename.c_str());

        return false;
    }

    ifs >> speed;
    ifs.close();

    return true;
}

void SwitchStateBase::syncOnLinkMsg(
        _In_ std::shared_ptr<EventPayloadNetLinkMsg> payload)
{
    SWSS_LOG_ENTER();

    // XXX if during switch shutdown lane map changed (port added/removed)
    // then loaded lane map will point to wrong mapping

    auto ifindex = payload->getIfIndex();
    auto ifflags = payload->getIfFlags();
    auto ifname = payload->getIfName();

    auto msgtype = payload->getNlmsgType();

    if (msgtype != RTM_NEWLINK)
    {
        // ignore delete message
        return;
    }

    if (!hasIfIndex(ifindex))
    {
        // since we will receive messages from all indexes and all switches
        // then we only need to check messages addressed for us and since
        // ifindex is unique across system, then we will use that

        return;
    }

    auto map = m_switchConfig->m_laneMap;

    if (!map)
    {
        SWSS_LOG_ERROR("lane map for switch %s don't exists",
                sai_serialize_object_id(m_switch_id).c_str());
        return;
    }

    if (strncmp(ifname.c_str(), SAI_VS_VETH_PREFIX, sizeof(SAI_VS_VETH_PREFIX) - 1) != 0 &&
            !map->hasInterface(ifname))
    {
        SWSS_LOG_ERROR("skipping newlink for %s, name not found in map", ifname.c_str());
        return;
    }

    SWSS_LOG_NOTICE("newlink: ifindex: %d, ifflags: 0x%x, ifname: %s",
            ifindex,
            ifflags,
            ifname.c_str());

    auto state = (ifflags & IFF_LOWER_UP) ? SAI_PORT_OPER_STATUS_UP : SAI_PORT_OPER_STATUS_DOWN;

    auto portId = getPortIdFromIfName(ifname);

    send_port_oper_status_notification(portId, state, false);
}
