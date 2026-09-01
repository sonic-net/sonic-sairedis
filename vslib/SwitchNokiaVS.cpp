#include "SwitchNokiaVS.h"
#include "HostInterfaceInfo.h"
#include "EventPayloadNotification.h"

#include "swss/logger.h"
#include "meta/sai_serialize.h"
#include "meta/NotificationPortStateChange.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <net/ethernet.h>
#include <linux/if.h>

#include "swss/exec.h"

#include <thread>
#include <fstream>

#define ETH_FRAME_BUFFER_SIZE (9888)
#define MAX_INTERFACE_NAME_LEN (IFNAMSIZ-1)

using namespace saivs;

SwitchNokiaVS::SwitchNokiaVS(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config):
    SwitchStateBase(switch_id, manager, config)
{
    SWSS_LOG_ENTER();
}

SwitchNokiaVS::SwitchNokiaVS(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config,
        _In_ std::shared_ptr<WarmBootState> warmBootState):
    SwitchStateBase(switch_id, manager, config, warmBootState)
{
    SWSS_LOG_ENTER();
}

SwitchNokiaVS::~SwitchNokiaVS()
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("NOKIA_VSLIB: destructor - restoring eth interfaces");

    restoreEthInterfaces();
}

bool SwitchNokiaVS::renameNetIntf(
        _In_ const std::string &srcName,
        _In_ const std::string &dstName)
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("NOKIA_VSLIB: renaming %s -> %s", srcName.c_str(), dstName.c_str());

    std::string res;
    swss::exec("ip addr flush dev " + srcName + " scope global", res);
    SWSS_LOG_NOTICE("NOKIA_VSLIB: flushed addresses on %s", srcName.c_str());

    int s = socket(AF_INET, SOCK_DGRAM, 0);

    if (s < 0)
    {
        SWSS_LOG_WARN("NOKIA_VSLIB: socket() failed (errno %d: %s)",
                errno, strerror(errno));
        return false;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, srcName.c_str(), IFNAMSIZ - 1);

    if (ioctl(s, SIOCGIFFLAGS, &ifr) == 0 && (ifr.ifr_flags & IFF_UP))
    {
        ifr.ifr_flags &= ~IFF_UP;
        ioctl(s, SIOCSIFFLAGS, &ifr);
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, srcName.c_str(), IFNAMSIZ - 1);
    strncpy(ifr.ifr_newname, dstName.c_str(), IFNAMSIZ - 1);

    bool ok = true;

    if (ioctl(s, SIOCSIFNAME, &ifr) < 0)
    {
        SWSS_LOG_WARN("NOKIA_VSLIB: rename %s -> %s failed (errno %d: %s)",
                srcName.c_str(), dstName.c_str(), errno, strerror(errno));
        ok = false;
    }
    else
    {
        SWSS_LOG_NOTICE("NOKIA_VSLIB: renamed %s -> %s successfully",
                srcName.c_str(), dstName.c_str());
    }

    close(s);
    return ok;
}

void SwitchNokiaVS::restoreEthInterfaces()
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("NOKIA_VSLIB: checking for EthernetX interfaces to rename back to ethX");

    struct if_nameindex *if_ni = if_nameindex();

    if (!if_ni)
    {
        SWSS_LOG_WARN("NOKIA_VSLIB: if_nameindex() failed (errno %d: %s)",
                errno, strerror(errno));
        return;
    }

    for (struct if_nameindex *p = if_ni; p->if_index != 0 && p->if_name != nullptr; p++)
    {
        int portNum = -1;

        if (sscanf(p->if_name, "Ethernet%d", &portNum) != 1 || portNum < 0)
        {
            continue;
        }

        std::string ethName = "eth" + std::to_string(portNum + 1);

        renameNetIntf(p->if_name, ethName);
    }

    if_freenameindex(if_ni);
}

bool SwitchNokiaVS::createEthernetInterface(
        _In_ const std::string &ethName,
        _In_ const std::string &ethernetName)
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("NOKIA_VSLIB: creating Ethernet interface %s from %s",
            ethernetName.c_str(), ethName.c_str());

    return renameNetIntf(ethName, ethernetName);
}

sai_status_t SwitchNokiaVS::create_qos_queues_per_port(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    const uint32_t port_qos_queues_count = m_unicastQueueNumber + m_multicastQueueNumber;

    std::vector<sai_object_id_t> queues;

    for (uint32_t i = 0; i < port_qos_queues_count; ++i)
    {
        sai_object_id_t queue_id;

        CHECK_STATUS(create(SAI_OBJECT_TYPE_QUEUE, &queue_id, m_switch_id, 0, NULL));

        queues.push_back(queue_id);

        attr.id = SAI_QUEUE_ATTR_TYPE;
        attr.value.s32 = (i < port_qos_queues_count / 2) ?  SAI_QUEUE_TYPE_UNICAST : SAI_QUEUE_TYPE_MULTICAST;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_QUEUE, queue_id, &attr));

        attr.id = SAI_QUEUE_ATTR_INDEX;
        attr.value.u8 = (uint8_t)i;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_QUEUE, queue_id, &attr));

        attr.id = SAI_QUEUE_ATTR_PORT;
        attr.value.oid = port_id;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_QUEUE, queue_id, &attr));
    }

    attr.id = SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES;
    attr.value.u32 = port_qos_queues_count;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    attr.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
    attr.value.objlist.count = port_qos_queues_count;
    attr.value.objlist.list = queues.data();

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchNokiaVS::create_cpu_qos_queues(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    const uint32_t port_qos_queues_count = 32;

    std::vector<sai_object_id_t> queues;

    for (uint32_t i = 0; i < port_qos_queues_count; ++i)
    {
        sai_object_id_t queue_id;

        CHECK_STATUS(create(SAI_OBJECT_TYPE_QUEUE, &queue_id, m_switch_id, 0, NULL));

        queues.push_back(queue_id);

        attr.id = SAI_QUEUE_ATTR_TYPE;
        attr.value.s32 = SAI_QUEUE_TYPE_MULTICAST;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_QUEUE, queue_id, &attr));

        attr.id = SAI_QUEUE_ATTR_INDEX;
        attr.value.u8 = (uint8_t)i;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_QUEUE, queue_id, &attr));

        attr.id = SAI_QUEUE_ATTR_PORT;
        attr.value.oid = port_id;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_QUEUE, queue_id, &attr));
    }

    attr.id = SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES;
    attr.value.u32 = port_qos_queues_count;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    attr.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
    attr.value.objlist.count = port_qos_queues_count;
    attr.value.objlist.list = queues.data();

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchNokiaVS::create_qos_queues()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create qos queues");

    for (auto &port_id: m_port_list)
    {
        CHECK_STATUS(create_qos_queues_per_port(port_id));
    }

    CHECK_STATUS(create_cpu_qos_queues(m_cpu_port_id));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchNokiaVS::set_number_of_queues()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("set number of unicast queues");

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_UNICAST_QUEUES;
    attr.value.u32 = m_unicastQueueNumber;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    SWSS_LOG_INFO("set number of multicast queues");

    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_MULTICAST_QUEUES;
    attr.value.u32 = m_multicastQueueNumber;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    SWSS_LOG_INFO("set number of queues");

    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_QUEUES;
    attr.value.u32 = m_unicastQueueNumber + m_multicastQueueNumber;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchNokiaVS::create_port_serdes()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create port serdes for all ports");

    for (auto &port_id: m_port_list)
    {
        CHECK_STATUS(create_port_serdes_per_port(port_id));
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchNokiaVS::create_port_serdes_per_port(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    sai_object_id_t port_serdes_id;

    sai_attribute_t attr;

    attr.id = SAI_PORT_SERDES_ATTR_PORT_ID;
    attr.value.oid = port_id;

    CHECK_STATUS(create(SAI_OBJECT_TYPE_PORT_SERDES, &port_serdes_id, m_switch_id, 1, &attr));

    attr.id = SAI_PORT_ATTR_PORT_SERDES_ID;
    attr.value.oid = port_serdes_id;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchNokiaVS::create_scheduler_group_tree(
        _In_ const std::vector<sai_object_id_t>& sgs,
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attrq;

    std::vector<sai_object_id_t> queues;

    uint32_t queues_count = 20;

    queues.resize(queues_count);

    attrq.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
    attrq.value.objlist.count = queues_count;
    attrq.value.objlist.list = queues.data();

    CHECK_STATUS(get(SAI_OBJECT_TYPE_PORT, port_id, 1, &attrq));

    // schedulers groups: 0 1 2 3 4 5 6 7 8 9 a b c

    // tree index
    // 0 = 1 2
    // 1 = 3 4 5 6 7 8 9 a
    // 2 = b c

    // 3..c - have both QUEUES, each one 2

    // scheduler group 0 (2 groups)
    {
        sai_object_id_t sg_0 = sgs.at(0);

        sai_attribute_t attr;

        attr.id = SAI_SCHEDULER_GROUP_ATTR_PORT_ID;
        attr.value.oid = port_id;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_0, &attr));

        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT;
        attr.value.u32 = 2;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_0, &attr));

        uint32_t list_count = 2;
        std::vector<sai_object_id_t> list;

        list.push_back(sgs.at(1));
        list.push_back(sgs.at(2));

        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST;
        attr.value.objlist.count = list_count;
        attr.value.objlist.list = list.data();

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_0, &attr));
    }

    uint32_t queue_index = 0;

    // scheduler group 1 (8 groups)
    {
        sai_object_id_t sg_1 = sgs.at(1);

        sai_attribute_t attr;

        attr.id = SAI_SCHEDULER_GROUP_ATTR_PORT_ID;
        attr.value.oid = port_id;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_1, &attr));

        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT;
        attr.value.u32 = 8;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_1, &attr));

        uint32_t list_count = 8;
        std::vector<sai_object_id_t> list;

        list.push_back(sgs.at(3));
        list.push_back(sgs.at(4));
        list.push_back(sgs.at(5));
        list.push_back(sgs.at(6));
        list.push_back(sgs.at(7));
        list.push_back(sgs.at(8));
        list.push_back(sgs.at(9));
        list.push_back(sgs.at(0xa));

        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST;
        attr.value.objlist.count = list_count;
        attr.value.objlist.list = list.data();

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_1, &attr));

        for (size_t i = 0; i < list.size(); ++i)
        {
            sai_object_id_t childs[2];

            childs[0] = queues[queue_index];
            childs[1] = queues[queue_index + queues_count/2];

            attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST;
            attr.value.objlist.count = 2;
            attr.value.objlist.list = childs;

            queue_index++;

            CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, list.at(i), &attr));

            attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT;
            attr.value.u32 = 2;

            CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, list.at(i), &attr));

            attr.id = SAI_SCHEDULER_GROUP_ATTR_PORT_ID;
            attr.value.oid = port_id;

            CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, list.at(i), &attr));
        }
    }

    // scheduler group 2 (2 groups)
    {
        sai_object_id_t sg_2 = sgs.at(2);

        sai_attribute_t attr;

        attr.id = SAI_SCHEDULER_GROUP_ATTR_PORT_ID;
        attr.value.oid = port_id;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_2, &attr));

        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT;
        attr.value.u32 = 2;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_2, &attr));

        uint32_t list_count = 2;
        std::vector<sai_object_id_t> list;

        list.push_back(sgs.at(0xb));
        list.push_back(sgs.at(0xc));

        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST;
        attr.value.objlist.count = list_count;
        attr.value.objlist.list = list.data();

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_2, &attr));

        for (size_t i = 0; i < list.size(); ++i)
        {
            sai_object_id_t childs[2];

            childs[0] = queues[queue_index];
            childs[1] = queues[queue_index + queues_count/2];

            attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST;
            attr.value.objlist.count = 2;
            attr.value.objlist.list = childs;

            queue_index++;

            CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, list.at(i), &attr));

            attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT;
            attr.value.u32 = 2;

            CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, list.at(i), &attr));

            attr.id = SAI_SCHEDULER_GROUP_ATTR_PORT_ID;
            attr.value.oid = port_id;

            CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, list.at(i), &attr));
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchNokiaVS::create_scheduler_groups_per_port(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    uint32_t port_sgs_count = 13;

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_QOS_NUMBER_OF_SCHEDULER_GROUPS;
    attr.value.u32 = port_sgs_count;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    std::vector<sai_object_id_t> sgs;

    for (uint32_t i = 0; i < port_sgs_count; ++i)
    {
        sai_object_id_t sg_id;

        CHECK_STATUS(create(SAI_OBJECT_TYPE_SCHEDULER_GROUP, &sg_id, m_switch_id, 0, NULL));

        sgs.push_back(sg_id);
    }

    attr.id = SAI_PORT_ATTR_QOS_SCHEDULER_GROUP_LIST;
    attr.value.objlist.count = port_sgs_count;
    attr.value.objlist.list = sgs.data();

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    CHECK_STATUS(create_scheduler_group_tree(sgs, port_id));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchNokiaVS::set_maximum_number_of_childs_per_scheduler_group()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_CHILDS_PER_SCHEDULER_GROUP;
    attr.value.u32 = 16;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchNokiaVS::refresh_bridge_port_list(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t bridge_id)
{
    SWSS_LOG_ENTER();

    auto &all_bridge_ports = m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT);

    sai_attribute_t attr;

    auto me_port_list = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE, SAI_BRIDGE_ATTR_PORT_LIST);
    auto m_port_id = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_PORT_ID);
    auto m_bridge_id = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_BRIDGE_ID);
    auto m_type = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_TYPE);

    attr.id = SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID;

    CHECK_STATUS(get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &attr));

    sai_object_id_t default_1q_bridge_id = attr.value.oid;

    std::map<sai_object_id_t, SwitchState::AttrHash> bridge_port_list_on_bridge_id;

    for (const auto &bp: all_bridge_ports)
    {
        auto it = bp.second.find(m_type->attridname);

        if (it == bp.second.end())
            continue;

        if (it->second->getAttr()->value.s32 != SAI_BRIDGE_PORT_TYPE_PORT)
            continue;

        it = bp.second.find(m_bridge_id->attridname);

        if (it != bp.second.end())
            continue;

        SWSS_LOG_NOTICE("setting default bridge id (%s) on bridge port %s",
                sai_serialize_object_id(default_1q_bridge_id).c_str(),
                bp.first.c_str());

        attr.id = SAI_BRIDGE_PORT_ATTR_BRIDGE_ID;
        attr.value.oid = default_1q_bridge_id;

        sai_object_id_t bridge_port;
        sai_deserialize_object_id(bp.first, bridge_port);

        CHECK_STATUS(set(SAI_OBJECT_TYPE_BRIDGE_PORT, bridge_port, &attr));
    }

    for (const auto &bp: all_bridge_ports)
    {
        auto it = bp.second.find(m_bridge_id->attridname);

        if (it == bp.second.end())
        {
            SWSS_LOG_NOTICE("not found %s on bridge port: %s", m_bridge_id->attridname, bp.first.c_str());
            continue;
        }

        if (bridge_id == it->second->getAttr()->value.oid)
        {
            sai_object_id_t bridge_port;

            sai_deserialize_object_id(bp.first, bridge_port);

            bridge_port_list_on_bridge_id[bridge_port] = bp.second;
        }
    }

    std::vector<sai_object_id_t> bridge_port_list;

    for (const auto &p: m_port_list)
    {
        for (const auto &bp: bridge_port_list_on_bridge_id)
        {
            auto it = bp.second.find(m_port_id->attridname);

            if (it == bp.second.end())
            {
                SWSS_LOG_THROW("bridge port is missing %s, not supported yet, FIXME", m_port_id->attridname);
            }

            if (p == it->second->getAttr()->value.oid)
            {
                bridge_port_list.push_back(bp.first);
            }
        }
    }

    if (bridge_port_list_on_bridge_id.size() != bridge_port_list.size())
    {
        SWSS_LOG_THROW("filter by port id failed size on lists is different: %zu vs %zu",
                bridge_port_list_on_bridge_id.size(),
                bridge_port_list.size());
    }

    uint32_t bridge_port_list_count = (uint32_t)bridge_port_list.size();

    SWSS_LOG_NOTICE("recalculated %s: %u", me_port_list->attridname, bridge_port_list_count);

    attr.id = SAI_BRIDGE_ATTR_PORT_LIST;
    attr.value.objlist.count = bridge_port_list_count;
    attr.value.objlist.list = bridge_port_list.data();

    return set(SAI_OBJECT_TYPE_BRIDGE, bridge_id, &attr);
}

sai_status_t SwitchNokiaVS::warm_update_queues()
{
    SWSS_LOG_ENTER();

    for (auto port: m_port_list)
    {
        sai_attribute_t attr;

        std::vector<sai_object_id_t> list(MAX_OBJLIST_LEN);

        attr.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;

        attr.value.objlist.count = MAX_OBJLIST_LEN;
        attr.value.objlist.list = list.data();

        CHECK_STATUS(get(SAI_OBJECT_TYPE_PORT, port , 1, &attr));

        list.resize(attr.value.objlist.count);

        uint8_t index = 0;

        size_t port_qos_queues_count = list.size();

        for (auto queue: list)
        {
            attr.id = SAI_QUEUE_ATTR_PORT;

            if (get(SAI_OBJECT_TYPE_QUEUE, queue, 1, &attr) != SAI_STATUS_SUCCESS)
            {
                attr.value.oid = port;

                CHECK_STATUS(set(SAI_OBJECT_TYPE_QUEUE, queue, &attr));
            }

            attr.id = SAI_QUEUE_ATTR_INDEX;

            if (get(SAI_OBJECT_TYPE_QUEUE, queue, 1, &attr) != SAI_STATUS_SUCCESS)
            {
                attr.value.u8 = index;

                CHECK_STATUS(set(SAI_OBJECT_TYPE_QUEUE, queue, &attr));
            }

            attr.id = SAI_QUEUE_ATTR_TYPE;

            if (get(SAI_OBJECT_TYPE_QUEUE, queue, 1, &attr) != SAI_STATUS_SUCCESS)
            {
                attr.value.s32 = (index < port_qos_queues_count / 2) ?  SAI_QUEUE_TYPE_UNICAST : SAI_QUEUE_TYPE_MULTICAST;

                CHECK_STATUS(set(SAI_OBJECT_TYPE_QUEUE, queue, &attr));
            }

            index++;
        }
    }

    return SAI_STATUS_SUCCESS;
}

bool SwitchNokiaVS::hostif_create_tap_veth_forwarding(
        _In_ const std::string &tapname,
        _In_ int tapfd,
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("NOKIA_VSLIB: tap-veth forwarding disabled for %s, using kernel interface directly",
            tapname.c_str());

    return true;
}

sai_status_t SwitchNokiaVS::vs_create_hostif_tap_interface(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto attr_type = sai_metadata_get_attr_by_id(SAI_HOSTIF_ATTR_TYPE, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attr SAI_HOSTIF_ATTR_TYPE was not passed");

        return SAI_STATUS_FAILURE;
    }

    if (attr_type->value.s32 == SAI_HOSTIF_TYPE_GENETLINK)
    {
        SWSS_LOG_DEBUG("Skipping hostif create for type genetlink");

        return SAI_STATUS_SUCCESS;
    }

    if (attr_type->value.s32 != SAI_HOSTIF_TYPE_NETDEV)
    {
        SWSS_LOG_ERROR("only SAI_HOSTIF_TYPE_NETDEV is supported");

        return SAI_STATUS_FAILURE;
    }

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
        SWSS_LOG_DEBUG("Skipping hostif create for object type VLAN");
        return SAI_STATUS_SUCCESS;
    }

    if (ot != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_ERROR("SAI_HOSTIF_ATTR_OBJ_ID=%s expected to be PORT but is: %s",
                sai_serialize_object_id(obj_id).c_str(),
                sai_serialize_object_type(ot).c_str());

        return SAI_STATUS_FAILURE;
    }

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

    SWSS_LOG_NOTICE("NOKIA_VSLIB: creating hostif %s for port %s",
            name.c_str(),
            sai_serialize_object_id(obj_id).c_str());

    int ifindex = (int)if_nametoindex(name.c_str());

    if (ifindex == 0)
    {
        int portNum = -1;

        if (sscanf(name.c_str(), "Ethernet%d", &portNum) != 1 || portNum < 0)
        {
            SWSS_LOG_ERROR("NOKIA_VSLIB: cannot parse port number from %s", name.c_str());
            return SAI_STATUS_FAILURE;
        }

        std::string ethName = "eth" + std::to_string(portNum + 1);

        if ((int)if_nametoindex(ethName.c_str()) == 0)
        {
            SWSS_LOG_ERROR("NOKIA_VSLIB: source interface %s not found (errno %d: %s)",
                    ethName.c_str(), errno, strerror(errno));
            return SAI_STATUS_FAILURE;
        }

        if (!createEthernetInterface(ethName, name))
        {
            return SAI_STATUS_FAILURE;
        }

        ifindex = (int)if_nametoindex(name.c_str());

        if (ifindex == 0)
        {
            SWSS_LOG_ERROR("NOKIA_VSLIB: %s not found after rename", name.c_str());
            return SAI_STATUS_FAILURE;
        }
    }

    SWSS_LOG_NOTICE("NOKIA_VSLIB: interface %s ready, ifindex %d", name.c_str(), ifindex);

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
    else
    {
        int err = vs_set_dev_mac_address(name.c_str(), attr.value.mac);

        if (err < 0)
        {
            SWSS_LOG_WARN("NOKIA_VSLIB: failed to set MAC address %s for %s (non-fatal)",
                    sai_serialize_mac(attr.value.mac).c_str(),
                    name.c_str());
        }
    }

    int mtu = ETH_FRAME_BUFFER_SIZE;

    sai_attribute_t attrmtu;

    attrmtu.id = SAI_PORT_ATTR_MTU;

    if (get(SAI_OBJECT_TYPE_PORT, obj_id, 1, &attrmtu) == SAI_STATUS_SUCCESS)
    {
        mtu = attrmtu.value.u32;

        SWSS_LOG_NOTICE("NOKIA_VSLIB: setting MTU %d on %s", mtu, name.c_str());
    }

    vs_set_dev_mtu(name.c_str(), mtu);

    m_hostif_info_map[name] =
        std::make_shared<HostInterfaceInfo>(
                ifindex,
                -1,
                -1,
                name,
                obj_id,
                m_switchConfig->m_eventQueue);

    SWSS_LOG_NOTICE("NOKIA_VSLIB: stored hostif info: name=%s ifindex=%d port=%s",
            name.c_str(), ifindex,
            sai_serialize_object_id(obj_id).c_str());

    setIfNameToPortId(name, obj_id);
    setPortIdToTapName(obj_id, name);

    SWSS_LOG_NOTICE("NOKIA_VSLIB: mapped %s <-> port %s",
            name.c_str(),
            sai_serialize_object_id(obj_id).c_str());

    attr.id = SAI_PORT_ATTR_ADMIN_STATE;

    status = get(SAI_OBJECT_TYPE_PORT, obj_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("NOKIA_VSLIB: failed to get admin state for port %s",
                sai_serialize_object_id(obj_id).c_str());

        return status;
    }

    SWSS_LOG_NOTICE("NOKIA_VSLIB: admin state for %s port %s is %s",
            name.c_str(),
            sai_serialize_object_id(obj_id).c_str(),
            attr.value.booldata ? "UP" : "DOWN");

    if (ifup(name.c_str(), obj_id, attr.value.booldata, true))
    {
        SWSS_LOG_ERROR("NOKIA_VSLIB: ifup failed on %s", name.c_str());

        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_NOTICE("NOKIA_VSLIB: hostif setup complete for %s port %s",
            name.c_str(),
            sai_serialize_object_id(obj_id).c_str());

    for (const auto &family : { "ipv4", "ipv6" })
    {
        std::string path = std::string("/proc/sys/net/") + family
                         + "/conf/" + name + "/forwarding";

        std::ofstream ofs(path);

        if (ofs.is_open())
        {
            ofs << "1";
            SWSS_LOG_NOTICE("NOKIA_VSLIB: enabled %s forwarding on %s", family, name.c_str());
        }
        else
        {
            SWSS_LOG_WARN("NOKIA_VSLIB: failed to open %s to enable %s forwarding",
                    path.c_str(), family);
        }
    }

    if (attr.value.booldata)
    {
        sai_attribute_t cbattr;
        cbattr.id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;

        if (get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &cbattr) == SAI_STATUS_SUCCESS
                && cbattr.value.ptr != NULL)
        {
            sai_port_oper_status_notification_t deferredData = {};
            deferredData.port_id = obj_id;
            deferredData.port_state = SAI_PORT_OPER_STATUS_UP;

            auto str = sai_serialize_port_oper_status_ntf(1, &deferredData);
            auto ntf = std::make_shared<sairedis::NotificationPortStateChange>(str);

            sai_switch_notifications_t sn = {};
            sn.on_port_state_change = (sai_port_state_change_notification_fn)cbattr.value.ptr;

            auto payload = std::make_shared<EventPayloadNotification>(ntf, sn);
            auto event = std::make_shared<Event>(EVENT_TYPE_NOTIFICATION, payload);
            auto eventQueue = m_switchConfig->m_eventQueue;

            SWSS_LOG_NOTICE("NOKIA_VSLIB: scheduling deferred port UP notification for %s port %s",
                    name.c_str(), sai_serialize_object_id(obj_id).c_str());

            std::thread([event, eventQueue, obj_id]() {
                sleep(15);

                SWSS_LOG_NOTICE("NOKIA_VSLIB: sending deferred port state notification for port %s",
                        sai_serialize_object_id(obj_id).c_str());

                eventQueue->enqueue(event);
            }).detach();
        }
        else
        {
            SWSS_LOG_WARN("NOKIA_VSLIB: cannot schedule deferred notification - callback not registered yet");
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchNokiaVS::vs_remove_hostif_tap_interface(
        _In_ sai_object_id_t hostif_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_HOSTIF_ATTR_TYPE;
    sai_status_t status = get(SAI_OBJECT_TYPE_HOSTIF, hostif_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to get attr type for hostif %s",
                sai_serialize_object_id(hostif_id).c_str());
        return status;
    }

    if (attr.value.s32 == SAI_HOSTIF_TYPE_GENETLINK)
    {
        SWSS_LOG_DEBUG("Skipping hostif remove for type genetlink");
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
        SWSS_LOG_DEBUG("Skipping hostif remove for object type VLAN");
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

    std::string name = std::string(attr.value.chardata);

    auto it = m_hostif_info_map.find(name);

    if (it == m_hostif_info_map.end())
    {
        SWSS_LOG_ERROR("NOKIA_VSLIB: no hostif info for %s", name.c_str());

        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_NOTICE("NOKIA_VSLIB: removing hostif mapping for %s port %s",
            name.c_str(),
            sai_serialize_object_id(it->second->m_portId).c_str());

    auto info = it->second;

    m_hostif_info_map.erase(it);

    removeIfNameToPortId(name);
    removePortIdToTapName(info->m_portId);

    int portNum = -1;

    if (sscanf(name.c_str(), "Ethernet%d", &portNum) == 1 && portNum >= 0)
    {
        std::string ethName = "eth" + std::to_string(portNum + 1);

        renameNetIntf(name, ethName);
    }

    SWSS_LOG_NOTICE("NOKIA_VSLIB: hostif removed for %s", name.c_str());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchNokiaVS::setPort(
        _In_ sai_object_id_t portId,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    if (attr && attr->id == SAI_PORT_ATTR_AUTO_NEG_MODE)
    {
        std::string tapName;

        if (getTapNameFromPortId(portId, tapName))
        {
            int portNum = -1;

            if (sscanf(tapName.c_str(), "Ethernet%d", &portNum) == 1 && portNum >= 0 && portNum <= 1)
            {
                std::string path = "/sys/kernel/cn9130_led/bypass_an_port" + std::to_string(portNum);
                int sysfs_val = attr->value.booldata ? 0 : 1;

                std::ofstream ofs(path);
                if (ofs.is_open())
                {
                    ofs << sysfs_val;
                    ofs.close();
                    SWSS_LOG_NOTICE("NOKIA_VSLIB: wrote %d to %s for %s (AN %s)",
                            sysfs_val, path.c_str(), tapName.c_str(),
                            attr->value.booldata ? "enabled" : "disabled");
                }
                else
                {
                    SWSS_LOG_WARN("NOKIA_VSLIB: failed to open %s", path.c_str());
                }
            }
        }
    }

    return SwitchStateBase::setPort(portId, attr);
}
