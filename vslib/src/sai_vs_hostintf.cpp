#include "sai_vs.h"
#include "sai_vs_internal.h"
#include "sai_vs_state.h"
#include "swss/selectableevent.h"
#include "swss/select.h"

#include "meta/sai_serialize.h"

#include <string.h>
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
#include <unistd.h>
#include <linux/if_packet.h>
#include <net/if_arp.h>
#include <linux/if_ether.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include "swss/json.hpp"
#pragma GCC diagnostic pop

using json = nlohmann::json;

class SelectableFd :
    public swss::Selectable
{
    public:
        SelectableFd(
                _In_ int fd)
        {
            SWSS_LOG_ENTER();

            m_fd = fd;
        }

        int getFd() override
        {
            SWSS_LOG_ENTER();

            return m_fd;
        }

        uint64_t readData() override
        {
            SWSS_LOG_ENTER();

            // empty
            return 0;
        }

    private:

        int m_fd;
};

typedef struct _hostif_info_t
{
    int tapfd;
    int packet_socket;

    std::shared_ptr<std::thread> e2t;
    std::shared_ptr<std::thread> t2e;

    swss::SelectableEvent e2tEvent;
    swss::SelectableEvent t2eEvent;

    sai_object_id_t hostif_vid;

    volatile bool run_thread;

    std::string name;

    sai_object_id_t portid;

    void veth2tap_fun();

    void tap2veth_fun();

    void process_packet_for_fdb_event(
            _In_ const uint8_t *buffer,
            _In_ size_t size) const;

    _hostif_info_t()
    {
        SWSS_LOG_ENTER();

        tapfd = -1;
        packet_socket = -1;
        run_thread = false;
    }

    ~_hostif_info_t()
    {
        SWSS_LOG_ENTER();

        run_thread = false;

        e2tEvent.notify();
        t2eEvent.notify();

        if (t2e)
            t2e->join();

        if (e2t)
            e2t->join();

        SWSS_LOG_NOTICE("joined threads for hostif: %s", name.c_str());

        // TODO close socket and tapfd here ?
    }

} hostif_info_t;

// TODO must be per switch when multiple switch support will be used
// since interface names can be the same in each switch
std::map<std::string, std::shared_ptr<hostif_info_t>> hostif_info_map;

std::set<fdb_info_t> g_fdb_info_set;

std::string sai_vs_serialize_fdb_info(
        _In_ const fdb_info_t& fi)
{
    SWSS_LOG_ENTER();

    json j;

    j["port_id"] = sai_serialize_object_id(fi.port_id);
    j["vlan_id"] = sai_serialize_vlan_id(fi.vlan_id);
    j["bridge_port_id"] = sai_serialize_object_id(fi.bridge_port_id);
    j["fdb_entry"] = sai_serialize_fdb_entry(fi.fdb_entry);
    j["timestamp"] = sai_serialize_number(fi.timestamp);

    SWSS_LOG_INFO("item: %s", j.dump().c_str());

    return j.dump();
}

void sai_vs_deserialize_fdb_info(
        _In_ const std::string& data,
        _Out_ fdb_info_t& fi)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("item: %s", data.c_str());

    const json& j = json::parse(data);

    memset(&fi, 0, sizeof(fi));

    sai_deserialize_object_id(j["port_id"], fi.port_id);
    sai_deserialize_vlan_id(j["vlan_id"], fi.vlan_id);
    sai_deserialize_object_id(j["bridge_port_id"], fi.bridge_port_id);
    sai_deserialize_fdb_entry(j["fdb_entry"], fi.fdb_entry);
    sai_deserialize_number(j["timestamp"], fi.timestamp);
}

void updateLocalDB(
        _In_ const sai_fdb_event_notification_data_t &data,
        _In_ sai_fdb_event_t fdb_event)
{
    SWSS_LOG_ENTER();

    sai_status_t status;

    switch (fdb_event)
    {
        case SAI_FDB_EVENT_LEARNED:

            {
                status = vs_generic_create_fdb_entry(&data.fdb_entry, data.attr_count, data.attr);

                if (status != SAI_STATUS_SUCCESS)
                {
                    SWSS_LOG_ERROR("failed to create fdb entry: %s",
                            sai_serialize_fdb_entry(data.fdb_entry).c_str());
                }
            }

            break;

        case SAI_FDB_EVENT_AGED:

            {
                status = vs_generic_remove_fdb_entry(&data.fdb_entry);

                if (status != SAI_STATUS_SUCCESS)
                {
                    SWSS_LOG_ERROR("failed to remove fdb entry %s",
                            sai_serialize_fdb_entry(data.fdb_entry).c_str());
                }
            }

            break;

        default:
            SWSS_LOG_ERROR("unsupported fdb event: %d", fdb_event);
            break;
    }
}

void processFdbInfo(
        _In_ const fdb_info_t &fi,
        _In_ sai_fdb_event_t fdb_event)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
    attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;

    attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = fi.bridge_port_id;

    sai_fdb_event_notification_data_t data;

    data.event_type = fdb_event;

    data.fdb_entry = fi.fdb_entry;

    data.attr_count = 2;
    data.attr = attrs;

    // update metadata DB
    meta_sai_on_fdb_event(1, &data);

    // update local DB
    updateLocalDB(data, fdb_event);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;

    sai_status_t status = vs_generic_get(SAI_OBJECT_TYPE_SWITCH, data.fdb_entry.switch_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get fdb event notify from switch %s",
                sai_serialize_object_id(data.fdb_entry.switch_id).c_str());
        return;
    }

    std::string s = sai_serialize_fdb_event_ntf(1, &data);

    SWSS_LOG_DEBUG("calling user fdb event callback: %s", s.c_str());

    sai_fdb_event_notification_fn ntf = (sai_fdb_event_notification_fn)attr.value.ptr;

    if (ntf != NULL)
    {
        ntf(1, &data);
    }
}

bool getLagFromPort(
        _In_ sai_object_id_t port_id,
        _Inout_ sai_object_id_t& lag_id);

void findBridgeVlanForPortVlan(
        _In_ sai_object_id_t port_id,
        _In_ sai_vlan_id_t vlan_id,
        _Inout_ sai_object_id_t &bv_id,
        _Inout_ sai_object_id_t &bridge_port_id)
{
    SWSS_LOG_ENTER();

    bv_id = SAI_NULL_OBJECT_ID;
    bridge_port_id = SAI_NULL_OBJECT_ID;

    sai_object_id_t bridge_id;

    /*
     * The bridge port lookup process is two steps:
     *
     * - use (vlan_id, physical port_id) to match any .1D bridge port created.
     *   If there is match, then quit, found=true
     *
     * - use (physical port_id) to match any .1Q bridge created. if there is a
     *   match, the quite, found=true.
     *
     * If found==true, generate fdb learn event on the .1D or .1Q bridge port.
     * If not found, then do not generate fdb event. It means the packet is not
     * received on the bridge port.
     *
     * XXX: this is not whats happening here, we are just looking for any
     * bridge id (as in our case this is shortcut, we will remove all bridge ports
     * when we will use router interface based port/lag and no bridge
     * will be found.
     */

    sai_object_id_t switch_id = sai_switch_id_query(port_id);

    auto &objectHash = g_switch_state_map.at(switch_id)->objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT);

    // iterate via all bridge ports to find match on port id

    sai_object_id_t lag_id = SAI_NULL_OBJECT_ID;

    if (getLagFromPort(port_id,lag_id))
    {
        SWSS_LOG_INFO("got lag %s for port %s",
                sai_serialize_object_id(lag_id).c_str(),
                sai_serialize_object_id(port_id).c_str());
    }

    bool bv_id_set = false;

    for (auto it = objectHash.begin(); it != objectHash.end(); ++it)
    {
        sai_object_id_t bpid;

        sai_deserialize_object_id(it->first, bpid);

        sai_attribute_t attrs[2];

        attrs[0].id = SAI_BRIDGE_PORT_ATTR_PORT_ID;
        attrs[1].id = SAI_BRIDGE_PORT_ATTR_TYPE;

        sai_status_t status = vs_generic_get(SAI_OBJECT_TYPE_BRIDGE_PORT, bpid, (uint32_t)(sizeof(attrs)/sizeof(attrs[0])), attrs);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_WARN("failed to get attr PORT_ID and TYPE for bridge port %s",
                    sai_serialize_object_id(bpid).c_str());
            continue;
        }

        if (lag_id != SAI_NULL_OBJECT_ID)
        {
            // if port is member of lag, we should check if port_id is that LAG

            if (port_id == attrs[0].value.oid)
            {
                // there should be no case that the same port is lag member and has bridge port object on it

                SWSS_LOG_ERROR("port %s is member of lag %s, and also has bridge port created: %s",
                        sai_serialize_object_id(port_id).c_str(),
                        sai_serialize_object_id(lag_id).c_str(),
                        sai_serialize_object_id(attrs[0].value.oid).c_str());
                continue;
            }

            if (lag_id != attrs[0].value.oid)
            {
                // this is not expected port
                continue;
            }
        }
        else if (port_id != attrs[0].value.oid)
        {
            // this is not expected port
            continue;
        }

        bridge_port_id = bpid;

        // get the 1D bridge id if the bridge port type is subport
        auto bp_type = attrs[1].value.s32;

        SWSS_LOG_DEBUG("found bridge port %s of type %d",
                sai_serialize_object_id(bridge_port_id).c_str(),
                bp_type);

        if (bp_type == SAI_BRIDGE_PORT_TYPE_SUB_PORT)
        {
            sai_attribute_t attr;
            attr.id = SAI_BRIDGE_PORT_ATTR_BRIDGE_ID;

            status = vs_generic_get(SAI_OBJECT_TYPE_BRIDGE_PORT, bridge_port_id, 1, &attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                break;
            }

            bridge_id = attr.value.oid;

            SWSS_LOG_DEBUG("found bridge %s for port %s",
                    sai_serialize_object_id(bridge_id).c_str(),
                    sai_serialize_object_id(port_id).c_str());

            attr.id = SAI_BRIDGE_ATTR_TYPE;

            status = vs_generic_get(SAI_OBJECT_TYPE_BRIDGE, bridge_id, 1, &attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                break;
            }

            SWSS_LOG_DEBUG("bridge %s type is %d",
                    sai_serialize_object_id(bridge_id).c_str(),
                    attr.value.s32);
            bv_id = bridge_id;
            bv_id_set = true;
        }
        else
        {
            auto &objectHash2 = g_switch_state_map.at(switch_id)->objectHash.at(SAI_OBJECT_TYPE_VLAN);

            // iterate via all vlans to find match on vlan id

            for (auto it2 = objectHash2.begin(); it2 != objectHash2.end(); ++it2)
            {
                sai_object_id_t vlan_oid;

                sai_deserialize_object_id(it2->first, vlan_oid);

                sai_attribute_t attr;
                attr.id = SAI_VLAN_ATTR_VLAN_ID;

                status = vs_generic_get(SAI_OBJECT_TYPE_VLAN, vlan_oid, 1, &attr);

                if (status != SAI_STATUS_SUCCESS)
                {
                    continue;
                }

                if (vlan_id == attr.value.u16)
                {
                    bv_id = vlan_oid;
                    bv_id_set = true;
                    break;
                }
            }
        }

        break;
    }

    if (!bv_id_set)
    {
        // if port is lag member, then we didn't found bridge_port for that lag (expected for rif lag)
        SWSS_LOG_WARN("failed to find bv_id for vlan %d and port_id %s",
                vlan_id,
                sai_serialize_object_id(port_id).c_str());
    }
}

bool getLagFromPort(
        _In_ sai_object_id_t port_id,
        _Inout_ sai_object_id_t& lag_id)
{
    SWSS_LOG_ENTER();

    lag_id = SAI_NULL_OBJECT_ID;

    sai_object_id_t switch_id = sai_switch_id_query(port_id);

    auto &objectHash = g_switch_state_map.at(switch_id)->objectHash.at(SAI_OBJECT_TYPE_LAG_MEMBER);

    // iterate via all lag members to find match on port id

    for (auto it = objectHash.begin(); it != objectHash.end(); ++it)
    {
        sai_object_id_t lag_member_id;

        sai_deserialize_object_id(it->first, lag_member_id);

        sai_attribute_t attr;

        attr.id = SAI_LAG_MEMBER_ATTR_PORT_ID;

        sai_status_t status = vs_generic_get(SAI_OBJECT_TYPE_LAG_MEMBER, lag_member_id, 1, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to get port id from leg member %s",
                    sai_serialize_object_id(lag_member_id).c_str());
            continue;
        }

        if (port_id != attr.value.oid)
        {
            // this is not the port we are looking for
            continue;
        }

        attr.id = SAI_LAG_MEMBER_ATTR_LAG_ID;

        status = vs_generic_get(SAI_OBJECT_TYPE_LAG_MEMBER, lag_member_id, 1, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to get lag id from lag member %s",
                    sai_serialize_object_id(lag_member_id).c_str());
            continue;
        }

        lag_id = attr.value.oid;

        return true;
    }

    // this port does not belong to any lag

    return false;
}

bool isLagOrPortRifBased(
        _In_ sai_object_id_t lag_or_port_id)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_id = sai_switch_id_query(lag_or_port_id);

    auto &objectHash = g_switch_state_map.at(switch_id)->objectHash.at(SAI_OBJECT_TYPE_ROUTER_INTERFACE);

    // iterate via all lag members to find match on port id

    for (auto it = objectHash.begin(); it != objectHash.end(); ++it)
    {
        sai_object_id_t rif_id;

        sai_deserialize_object_id(it->first, rif_id);

        sai_attribute_t attr;

        attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;

        sai_status_t status = vs_generic_get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to get rif type from rif %s",
                    sai_serialize_object_id(rif_id).c_str());
            continue;
        }

        switch (attr.value.s32)
        {
            case SAI_ROUTER_INTERFACE_TYPE_PORT:
            case SAI_ROUTER_INTERFACE_TYPE_SUB_PORT:
                break;

            default:
                continue;
        }

        attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;

        status = vs_generic_get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to get rif port id from rif %s",
                    sai_serialize_object_id(rif_id).c_str());
            continue;
        }

        if (attr.value.oid == lag_or_port_id)
        {
            return true;
        }
    }

    return false;
}

void hostif_info_t::process_packet_for_fdb_event(
        _In_ const uint8_t *buffer,
        _In_ size_t size) const
{
    MUTEX();

    SWSS_LOG_ENTER();

    uint32_t frametime = (uint32_t)time(NULL);

    /*
     * We add +2 in case if frame contains 1Q VLAN tag.
     */

    if (size < (sizeof(ethhdr) + 2))
    {
        SWSS_LOG_WARN("ethernet frame is too small: %zu", size);
        return;
    }

    const ethhdr *eh = (const ethhdr*)buffer;

    uint16_t proto = htons(eh->h_proto);

    uint16_t vlan_id = DEFAULT_VLAN_NUMBER;

    bool tagged = (proto == ETH_P_8021Q);

    if (tagged)
    {
        // this is tagged frame, get vlan id from frame

        uint16_t tci = htons(((const uint16_t*)&eh->h_proto)[1]); // tag is after h_proto field

        vlan_id = tci & 0xfff;

        if (vlan_id == 0xfff)
        {
            SWSS_LOG_WARN("invalid vlan id %u in ethernet frame on %s", vlan_id, name.c_str());
            return;
        }

        if (vlan_id == 0)
        {
            // priority packet, frame should be treated as non tagged
            tagged = false;
        }
    }

    if (tagged == false)
    {
        // untagged ethernet frame

        sai_attribute_t attr;

#ifdef SAI_LAG_ATTR_PORT_VLAN_ID

        sai_object_id_t lag_id;

        if (getLagFromPort(portid, lag_id))
        {
            // if port belongs to lag we need to get SAI_LAG_ATTR_PORT_VLAN_ID

            attr.id = SAI_LAG_ATTR_PORT_VLAN_ID

            sai_status_t status = vs_generic_get(SAI_OBJECT_TYPE_LAG, lag_id, 1, &attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_WARN("failed to get lag vlan id from lag %s",
                        sai_serialize_object_id(lag_id).c_str());
                return;
            }

            vlan_id = attr.value.u16;

            if (isLagOrPortRifBased(lag_id))
            {
                // this lag is router interface based, skip mac learning
                return;
            }
        }
        else
#endif
        {
            attr.id = SAI_PORT_ATTR_PORT_VLAN_ID;

            sai_status_t status = vs_generic_get(SAI_OBJECT_TYPE_PORT,portid, 1, &attr);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_WARN("failed to get port vlan id from port %s",
                        sai_serialize_object_id(portid).c_str());
                return;
            }

            // untagged port vlan (default is 1, but may change setting port attr)
            vlan_id = attr.value.u16;
        }
    }

    sai_object_id_t lag_id;
    if (getLagFromPort(portid, lag_id) && isLagOrPortRifBased(lag_id))
    {
        SWSS_LOG_DEBUG("lag %s is rif based, skip mac learning for port %s",
                sai_serialize_object_id(lag_id).c_str(),
                sai_serialize_object_id(portid).c_str());
        return;
    }

    if (isLagOrPortRifBased(portid))
    {
        SWSS_LOG_DEBUG("port %s is rif based, skip mac learning",
                sai_serialize_object_id(portid).c_str());
        return;
    }

    // we have vlan and mac address which is KEY, so just see if that is already defined

    fdb_info_t fi;

    fi.port_id = (lag_id != SAI_NULL_OBJECT_ID) ? lag_id : portid;
    fi.vlan_id = vlan_id;

    memcpy(fi.fdb_entry.mac_address, eh->h_source, sizeof(sai_mac_t));

    std::set<fdb_info_t>::iterator it = g_fdb_info_set.find(fi);

    if (it != g_fdb_info_set.end())
    {
        // this key was found, update timestamp
        // and since iterator is const we need to reinsert

        fi = *it;

        fi.timestamp = frametime;

        g_fdb_info_set.insert(fi);

        return;
    }

    // key was not found, get additional information

    fi.timestamp = frametime;
    fi.fdb_entry.switch_id = sai_switch_id_query(portid);

    findBridgeVlanForPortVlan(portid, vlan_id, fi.fdb_entry.bv_id, fi.bridge_port_id);

    if (fi.fdb_entry.bv_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_WARN("skipping mac learn for %s, since BV_ID was not found for mac",
                sai_serialize_fdb_entry(fi.fdb_entry).c_str());

        // bridge was not found, skip mac learning
        return;
    }

    SWSS_LOG_INFO("inserting to fdb_info set: %s, vid: %d",
            sai_serialize_fdb_entry(fi.fdb_entry).c_str(),
            fi.vlan_id);

    g_fdb_info_set.insert(fi);

    processFdbInfo(fi, SAI_FDB_EVENT_LEARNED);
}

#define MAX_INTERFACE_NAME_LEN IFNAMSIZ

sai_status_t vs_recv_hostif_packet(
        _In_ sai_object_id_t hif_id,
        _Inout_ sai_size_t *buffer_size,
        _Out_ void *buffer,
        _Inout_ uint32_t *attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_send_hostif_packet(
        _In_ sai_object_id_t hif_id,
        _In_ sai_size_t buffer_size,
        _In_ const void *buffer,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

int vs_create_tap_device(
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

    strncpy(ifr.ifr_name, dev, IFNAMSIZ);

    int err = ioctl(fd, TUNSETIFF, (void *) &ifr);

    if (err < 0)
    {
        SWSS_LOG_ERROR("ioctl TUNSETIFF on fd %d %s failed, err %d", fd, dev, err);

        close(fd);

        return err;
    }

    return fd;
}

int vs_set_dev_mac_address(
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

    strncpy(ifr.ifr_name, dev, IFNAMSIZ);

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

void send_port_up_notification(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_id = sai_switch_id_query(port_id);

    if (switch_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("failed to get switch OID from port id %s",
                sai_serialize_object_id(port_id).c_str());
        return;
    }

    std::shared_ptr<SwitchState> sw = vs_get_switch_state(switch_id);

    if (sw == nullptr)
    {
        SWSS_LOG_ERROR("failed to get switch state for switch id %s",
                sai_serialize_object_id(switch_id).c_str());
        return;
    }

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;

    if (vs_switch_api.get_switch_attribute(switch_id, 1, &attr) != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY for switch %s",
                sai_serialize_object_id(switch_id).c_str());
        return;
    }

    sai_port_state_change_notification_fn callback =
        (sai_port_state_change_notification_fn)attr.value.ptr;

    if (callback == NULL)
    {
        SWSS_LOG_INFO("SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY callback is NULL");
        return;
    }

    sai_port_oper_status_notification_t data;

    data.port_id = port_id;
    data.port_state = SAI_PORT_OPER_STATUS_UP;

    attr.id = SAI_PORT_ATTR_OPER_STATUS;

    update_port_oper_status(port_id, data.port_state);

    SWSS_LOG_NOTICE("explicitly send SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY for port %s: %s (port was UP)",
            sai_serialize_object_id(data.port_id).c_str(),
            sai_serialize_port_oper_status(data.port_state).c_str());

    // NOTE this callback should be executed from separate non blocking thread

    if (callback)
        callback(1, &data);
}

int ifup(
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

    strncpy(ifr.ifr_name, dev , IFNAMSIZ);

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

        send_port_up_notification(port_id);

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

int promisc(const char *dev)
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

    strncpy(ifr.ifr_name, dev , IFNAMSIZ);

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

int vs_set_dev_mtu(
        _In_ const char*name,
        _In_ int mtu)
{
    SWSS_LOG_ENTER();

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

    struct ifreq ifr;

    strncpy(ifr.ifr_name, name, IFNAMSIZ);

    ifr.ifr_mtu = mtu;

    int err = ioctl(sock, SIOCSIFMTU, &ifr);

    if (err == 0)
    {
        SWSS_LOG_INFO("success set mtu on %s to %d", name, mtu);
        return 0;
    }

    SWSS_LOG_WARN("failed to set mtu on %s to %d", name, mtu);
    return err;
}


#define ETH_FRAME_BUFFER_SIZE (0x4000)
#define CONTROL_MESSAGE_BUFFER_SIZE (0x1000)
#define IEEE_8021Q_ETHER_TYPE (0x8100)
#define MAC_ADDRESS_SIZE (6)
#define VLAN_TAG_SIZE (4)

void hostif_info_t::veth2tap_fun()
{
    SWSS_LOG_ENTER();

    unsigned char buffer[ETH_FRAME_BUFFER_SIZE];

    swss::Select s;
    SelectableFd fd(packet_socket);

    s.addSelectable(&e2tEvent);
    s.addSelectable(&fd);

    while (run_thread)
    {
        struct msghdr  msg;
        memset(&msg, 0, sizeof(struct msghdr));

        struct sockaddr_storage src_addr;

        struct iovec iov[1];

        iov[0].iov_base = buffer;       // buffer for message
        iov[0].iov_len = sizeof(buffer);

        char control[CONTROL_MESSAGE_BUFFER_SIZE];   // buffer for control messages

        msg.msg_name = &src_addr;
        msg.msg_namelen = sizeof(src_addr);
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;
        msg.msg_control = control;
        msg.msg_controllen = sizeof(control);

        swss::Selectable *sel = NULL;

        int result = s.select(&sel);

        if (result != swss::Select::OBJECT)
        {
            SWSS_LOG_ERROR("selectable failed: %d, ending thread for %s", result, name.c_str());
            return;
        }

        if (sel == &e2tEvent) // thread end event
            break;

        ssize_t size = recvmsg(packet_socket, &msg, 0);

        if (size < 0)
        {
            if (errno != ENETDOWN)
            {
                SWSS_LOG_ERROR("failed to read from socket fd %d, errno(%d): %s",
                        packet_socket, errno, strerror(errno));
            }

            continue;
        }

        if (size < (ssize_t)sizeof(ethhdr))
        {
            SWSS_LOG_ERROR("invalid ethernet frame length: %zu", msg.msg_controllen);
            continue;
        }

        struct cmsghdr *cmsg;

        for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg))
        {
            if (cmsg->cmsg_level != SOL_PACKET || cmsg->cmsg_type != PACKET_AUXDATA)
                continue;

            struct tpacket_auxdata* aux = (struct tpacket_auxdata*)CMSG_DATA(cmsg);

            if ((aux->tp_status & TP_STATUS_VLAN_VALID) &&
                    (aux->tp_status & TP_STATUS_VLAN_TPID_VALID))
            {
                SWSS_LOG_DEBUG("got vlan tci: 0x%x, vlanid: %d", aux->tp_vlan_tci, aux->tp_vlan_tci & 0xFFF);

                // inject vlan tag into frame

                // for overlapping buffers
                memmove(buffer + 2 * MAC_ADDRESS_SIZE + VLAN_TAG_SIZE,
                        buffer + 2 * MAC_ADDRESS_SIZE,
                        size - (2 * MAC_ADDRESS_SIZE));

                uint16_t tci = htons(aux->tp_vlan_tci);
                uint16_t tpid = htons(IEEE_8021Q_ETHER_TYPE);

                uint8_t* pvlan =  (uint8_t *)(buffer + 2 * MAC_ADDRESS_SIZE);
                memcpy(pvlan, &tpid, sizeof(uint16_t));
                memcpy(pvlan + sizeof(uint16_t), &tci, sizeof(uint16_t));

                size += VLAN_TAG_SIZE;

                break;
            }
        }

        process_packet_for_fdb_event(buffer, size);

        if (write(tapfd, buffer, size) < 0)
        {
            /*
             * We filter out EIO because of this patch:
             * https://github.com/torvalds/linux/commit/1bd4978a88ac2589f3105f599b1d404a312fb7f6
             */

            if (errno != ENETDOWN && errno != EIO)
            {
                SWSS_LOG_ERROR("failed to write to tap device fd %d, errno(%d): %s",
                        tapfd, errno, strerror(errno));
            }

            if (errno == EBADF)
            {
                // bad file descriptor, just end thread
                SWSS_LOG_NOTICE("ending thread for tap fd %d", tapfd);
                return;
            }

            continue;
        }
    }

    SWSS_LOG_NOTICE("ending thread proc for %s", name.c_str());
}

void hostif_info_t::tap2veth_fun()
{
    SWSS_LOG_ENTER();

    unsigned char buffer[ETH_FRAME_BUFFER_SIZE];

    swss::Select s;
    SelectableFd fd(tapfd);

    s.addSelectable(&t2eEvent);
    s.addSelectable(&fd);

    while (run_thread)
    {
        swss::Selectable *sel = NULL;

        int result = s.select(&sel);

        if (result != swss::Select::OBJECT)
        {
            SWSS_LOG_ERROR("selectable failed: %d, ending thread for %s", result, name.c_str());
            return;
        }

        if (sel == &t2eEvent) // thread end event
            break;

        ssize_t size = read(tapfd, buffer, sizeof(buffer));

        if (size < 0)
        {
            SWSS_LOG_ERROR("failed to read from tapfd fd %d, errno(%d): %s",
                    tapfd, errno, strerror(errno));

            if (errno == EBADF)
            {
                // bad file descriptor, just close the thread
                SWSS_LOG_NOTICE("ending thread for tap fd %d",tapfd);
                return;
            }

            continue;
        }

        if (write(packet_socket, buffer, (int)size) < 0)
        {
            if (errno != ENETDOWN)
            {
                SWSS_LOG_ERROR("failed to write to socket fd %d, errno(%d): %s",
                        packet_socket, errno, strerror(errno));
            }

            continue;
        }
    }

    SWSS_LOG_NOTICE("ending thread proc for %s", name.c_str());
}

std::string vs_get_veth_name(
        _In_ const std::string& tapname,
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    std::string vethname = SAI_VS_VETH_PREFIX + tapname;

    // check if user override interface names

    sai_attribute_t attr;

    uint32_t lanes[4];

    attr.id = SAI_PORT_ATTR_HW_LANE_LIST;

    attr.value.u32list.count = 4;
    attr.value.u32list.list = lanes;

    if (vs_generic_get(SAI_OBJECT_TYPE_PORT, port_id, 1, &attr) != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_WARN("failed to get port %s lanes, using veth: %s",
                sai_serialize_object_id(port_id).c_str(),
                vethname.c_str());
    }
    else
    {
        auto it = g_lane_to_ifname.find(lanes[0]);

        if (it == g_lane_to_ifname.end())
        {
            SWSS_LOG_WARN("failed to get ifname from lane number %u", lanes[0]);
        }
        else
        {
            SWSS_LOG_NOTICE("using %s instead of %s", it->second.c_str(), vethname.c_str());

            vethname = it->second;
        }
    }

    return vethname;
}

bool hostif_create_tap_veth_forwarding(
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

    SWSS_LOG_INFO("interface index = %d %s\n", sock_address.sll_ifindex, vethname.c_str());

    if (ifup(vethname.c_str(), port_id, true, true))
    {
        SWSS_LOG_ERROR("ifup failed on %s", vethname.c_str());

        close(packet_socket);

        return false;
    }

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

    std::shared_ptr<hostif_info_t> info = std::make_shared<hostif_info_t>();

    hostif_info_map[tapname] = info;

    // TODO move to constructor
    info->packet_socket = packet_socket;
    info->tapfd         = tapfd;
    info->run_thread    = true;
    info->e2t           = std::make_shared<std::thread>(&hostif_info_t::veth2tap_fun, info.get());
    info->t2e           = std::make_shared<std::thread>(&hostif_info_t::tap2veth_fun, info.get());
    info->name          = tapname;
    info->portid        = port_id;

    SWSS_LOG_NOTICE("setup forward rule for %s succeeded", tapname.c_str());

    return true;
}

sai_status_t vs_create_hostif_tap_interface(
        _In_ sai_object_id_t switch_id,
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

    sai_object_type_t ot = sai_object_type_query(obj_id);

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

    // TODO currently tapfd is ignored, it should be closed on hostif_remove
    // and we should use it to read/write packets from that interface

    sai_attribute_t attr;

    memset(&attr, 0, sizeof(attr));

    attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;

    sai_status_t status = vs_generic_get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get SAI_SWITCH_ATTR_SRC_MAC_ADDRESS on switch %s: %s",
                sai_serialize_object_id(switch_id).c_str(),
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

    vs_set_dev_mtu(name.c_str(), ETH_FRAME_BUFFER_SIZE);

    if (!hostif_create_tap_veth_forwarding(name, tapfd, obj_id))
    {
        SWSS_LOG_ERROR("forwarding rule on %s was not added", name.c_str());
    }

    std::string vname = vs_get_veth_name(name, obj_id);

    SWSS_LOG_INFO("mapping interface %s to port id %s",
            vname.c_str(),
            sai_serialize_object_id(obj_id).c_str());

    g_switch_state_map.at(switch_id)->setIfNameToPortId(vname, obj_id);
    g_switch_state_map.at(switch_id)->setPortIdToTapName(obj_id, name);

    // TODO what about FDB entries notifications, they also should
    // be generated if new mac address will show up on the interface/arp table

    // TODO IP address should be assigned when router interface is created

    SWSS_LOG_INFO("created tap interface %s", name.c_str());

    return SAI_STATUS_SUCCESS;
}

sai_status_t vs_recreate_hostif_tap_interfaces(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    if (g_vs_hostif_use_tap_device == false)
    {
        return SAI_STATUS_SUCCESS;
    }

    if (g_switch_state_map.find(switch_id) == g_switch_state_map.end())
    {
        SWSS_LOG_ERROR("failed to find switch %s in switch state map", sai_serialize_object_id(switch_id).c_str());

        return SAI_STATUS_FAILURE;
    }

    auto &objectHash = g_switch_state_map.at(switch_id)->objectHash.at(SAI_OBJECT_TYPE_HOSTIF);

    SWSS_LOG_NOTICE("attempt to recreate %zu tap devices for host interfaces", objectHash.size());

    for (auto okvp: objectHash)
    {
        std::vector<sai_attribute_t> attrs;

        for (auto akvp: okvp.second)
        {
            attrs.push_back(*akvp.second->getAttr());
        }

        vs_create_hostif_tap_interface(switch_id, (uint32_t)attrs.size(), attrs.data());
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t vs_remove_hostif_tap_interface(
        _In_ sai_object_id_t hostif_id)
{
    SWSS_LOG_ENTER();

    // get tap interface name

    sai_object_id_t switch_id = sai_switch_id_query(hostif_id);

    if (switch_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("failed to obtain switch_id from hostif_id %s",
                sai_serialize_object_id(hostif_id).c_str());

        return SAI_STATUS_FAILURE;
    }

    sai_attribute_t attr;

    attr.id = SAI_HOSTIF_ATTR_NAME;

    sai_status_t status = vs_generic_get(SAI_OBJECT_TYPE_HOSTIF, hostif_id, 1, &attr);

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

    std::string name = std::string(attr.value.chardata);

    auto it = hostif_info_map.find(name);

    if (it == hostif_info_map.end())
    {
        SWSS_LOG_ERROR("failed to find host info entry for tap device: %s", name.c_str());

        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_NOTICE("attempting to remove tap device: %s", name.c_str());

    // TODO add hostif id into hostif info entry and search by this but id is
    // created after creating tap device

    auto info = it->second;

    // remove host info entry from map

    hostif_info_map.erase(it); // will stop threads

    // remove tap device

    int err = close(info->tapfd);

    if (err)
    {
        SWSS_LOG_ERROR("failed to remove tap device: %s, err: %d", name.c_str(), err);
    }

    // remove interface mapping

    std::string vname = vs_get_veth_name(name, info->portid);

    g_switch_state_map.at(switch_id)->removeIfNameToPortId(vname);
    g_switch_state_map.at(switch_id)->removePortIdToTapName(info->portid);

    SWSS_LOG_NOTICE("successfully removed hostif tap device: %s", name.c_str());

    return SAI_STATUS_SUCCESS;
}

sai_status_t vs_create_hostif_int(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t *hostif_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (g_vs_hostif_use_tap_device == true)
    {
        sai_status_t status = vs_create_hostif_tap_interface(
                switch_id,
                attr_count,
                attr_list);

        if (status != SAI_STATUS_SUCCESS)
        {
            return status;
        }
    }

    return vs_generic_create(
            object_type,
            hostif_id,
            switch_id,
            attr_count,
            attr_list);
}

sai_status_t vs_create_hostif(
        _Out_ sai_object_id_t *hostif_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_HOSTIF,
            hostif_id,
            switch_id,
            attr_count,
            attr_list,
            &vs_create_hostif_int);
}

sai_status_t vs_set_admin_state(
            _In_ sai_object_id_t portId,
            _In_ bool up)
{
    SWSS_LOG_ENTER();

    // find corresponding host if interface and bring it down !
    for (auto& kvp: hostif_info_map)
    {
        auto tapname = kvp.first;

        if (kvp.second->portid == portId)
        {
            std::string vethname = vs_get_veth_name(tapname, portId);

            if (ifup(vethname.c_str(), portId, up, false))
            {
                SWSS_LOG_ERROR("if admin %s failed on %s failed: %s", (up ? "UP" : "DOWN"), vethname.c_str(),  strerror(errno));

                return SAI_STATUS_FAILURE;
            }

            SWSS_LOG_NOTICE("if admin %s success on %s", (up ? "UP" : "DOWN"), vethname.c_str());

            break;
        }
    }

    return SAI_STATUS_SUCCESS;
}

// TODO set must also be supported when we change operational status up/down
// and probably also generate notification then

sai_status_t vs_remove_hostif_int(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t hostif_id)
{
    SWSS_LOG_ENTER();

    if (g_vs_hostif_use_tap_device == true)
    {
        sai_status_t status = vs_remove_hostif_tap_interface(
                hostif_id);

        if (status != SAI_STATUS_SUCCESS)
        {
            return status;
        }
    }

    return vs_generic_remove(
            SAI_OBJECT_TYPE_HOSTIF,
            hostif_id);
}

sai_status_t vs_remove_hostif(
        _In_ sai_object_id_t hostif_id)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_HOSTIF,
            hostif_id,
            &vs_remove_hostif_int);
}

VS_SET(HOSTIF,hostif);
VS_GET(HOSTIF,hostif);

VS_GENERIC_QUAD(HOSTIF_TABLE_ENTRY,hostif_table_entry);
VS_GENERIC_QUAD(HOSTIF_TRAP_GROUP,hostif_trap_group);
VS_GENERIC_QUAD(HOSTIF_TRAP,hostif_trap);
VS_GENERIC_QUAD(HOSTIF_USER_DEFINED_TRAP,hostif_user_defined_trap);

const sai_hostif_api_t vs_hostif_api = {

    VS_GENERIC_QUAD_API(hostif)
    VS_GENERIC_QUAD_API(hostif_table_entry)
    VS_GENERIC_QUAD_API(hostif_trap_group)
    VS_GENERIC_QUAD_API(hostif_trap)
    VS_GENERIC_QUAD_API(hostif_user_defined_trap)

    vs_recv_hostif_packet,
    vs_send_hostif_packet,
};
