#include "SwitchMicas48CON2SFP.h"

#include "HostInterfaceInfo.h"
#include "EventPayloadNotification.h"

#include "swss/logger.h"
#include "swss/exec.h"
#include "meta/sai_serialize.h"
#include "meta/NotificationPortStateChange.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/if.h>
#include <fstream>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <thread>
#include <sstream>

#define ETH_FRAME_BUFFER_SIZE (9888)
#define MAX_INTERFACE_NAME_LEN (IFNAMSIZ-1)

using namespace saivs;

namespace
{
    std::string getBackingInterfaceName(int portNum)
    {
        SWSS_LOG_ENTER();

        return "etp" + std::to_string(portNum);
    }

    std::string joinLanes(const std::vector<uint32_t>& lanes)
    {
        SWSS_LOG_ENTER();

        std::ostringstream os;

        for (size_t i = 0; i < lanes.size(); ++i)
        {
            if (i > 0)
            {
                os << ",";
            }

            os << lanes[i];
        }

        return os.str();
    }
}

SwitchMicas48CON2SFP::SwitchMicas48CON2SFP(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config):
    SwitchStateBase(switch_id, manager, config),
    m_shutdown(std::make_shared<std::atomic<bool>>(false))
{
    SWSS_LOG_ENTER();
}

SwitchMicas48CON2SFP::SwitchMicas48CON2SFP(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config,
        _In_ std::shared_ptr<WarmBootState> warmBootState):
    SwitchStateBase(switch_id, manager, config, warmBootState),
    m_shutdown(std::make_shared<std::atomic<bool>>(false))
{
    SWSS_LOG_ENTER();
}

SwitchMicas48CON2SFP::~SwitchMicas48CON2SFP()
{
    SWSS_LOG_ENTER();

    *m_shutdown = true;

    restoreBackingInterfaces();
}

sai_status_t SwitchMicas48CON2SFP::create_cpu_qos_queues(
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

sai_status_t SwitchMicas48CON2SFP::create_qos_queues_per_port(
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
        attr.value.s32 = (i < port_qos_queues_count / 2) ? SAI_QUEUE_TYPE_UNICAST : SAI_QUEUE_TYPE_MULTICAST;
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

sai_status_t SwitchMicas48CON2SFP::create_qos_queues()
{
    SWSS_LOG_ENTER();

    for (auto &port_id: m_port_list)
    {
        CHECK_STATUS(create_qos_queues_per_port(port_id));
    }

    CHECK_STATUS(create_cpu_qos_queues(m_cpu_port_id));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchMicas48CON2SFP::set_number_of_queues()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_UNICAST_QUEUES;
    attr.value.u32 = m_unicastQueueNumber;
    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_MULTICAST_QUEUES;
    attr.value.u32 = m_multicastQueueNumber;
    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_NUMBER_OF_QUEUES;
    attr.value.u32 = m_unicastQueueNumber + m_multicastQueueNumber;
    CHECK_STATUS(set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchMicas48CON2SFP::create_port_serdes()
{
    SWSS_LOG_ENTER();

    for (auto &port_id: m_port_list)
    {
        CHECK_STATUS(create_port_serdes_per_port(port_id));
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchMicas48CON2SFP::create_port_serdes_per_port(
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

sai_status_t SwitchMicas48CON2SFP::create_scheduler_group_tree(
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

sai_status_t SwitchMicas48CON2SFP::create_scheduler_groups_per_port(
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

sai_status_t SwitchMicas48CON2SFP::set_maximum_number_of_childs_per_scheduler_group()
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_CHILDS_PER_SCHEDULER_GROUP;
    attr.value.u32 = 16;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchMicas48CON2SFP::refresh_bridge_port_list(
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

sai_status_t SwitchMicas48CON2SFP::warm_update_queues()
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
                attr.value.s32 = (index < port_qos_queues_count / 2) ? SAI_QUEUE_TYPE_UNICAST : SAI_QUEUE_TYPE_MULTICAST;
                CHECK_STATUS(set(SAI_OBJECT_TYPE_QUEUE, queue, &attr));
            }

            index++;
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchMicas48CON2SFP::create_ports()
{
    SWSS_LOG_ENTER();

    auto map = m_switchConfig->m_laneMap;

    if (!map)
    {
        SWSS_LOG_ERROR("lane map for switch %s is NULL",
                sai_serialize_object_id(m_switch_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    auto lanesVector = map->getLaneVector();

    if (m_switchConfig->m_useTapDevice)
    {
        auto lanes = lanesVector.begin();

        while (lanes != lanesVector.end())
        {
            bool available_lane = false;

            for (auto lane: *lanes)
            {
                std::string ifname = map->getInterfaceFromLaneNumber(lane);
                std::string path = std::string("/sys/class/net/") + ifname + "/operstate";

                if (access(path.c_str(), F_OK) == 0)
                {
                    available_lane = true;
                    continue;
                }

                int portNum = -1;

                if (sscanf(ifname.c_str(), "Ethernet%d", &portNum) == 1 && portNum > 0)
                {
                    std::string backingName = getBackingInterfaceName(portNum);
                    path = std::string("/sys/class/net/") + backingName + "/operstate";

                    if (access(path.c_str(), F_OK) == 0)
                    {
                        available_lane = true;
                        continue;
                    }
                }

                available_lane = false;
                break;
            }

            if (!available_lane)
            {
                std::string ifname = lanes->empty() ? "" : map->getInterfaceFromLaneNumber(lanes->at(0));

                SWSS_LOG_WARN("MICAS_VSLIB: skip port %s lanes %s because backing netdev is not available",
                        ifname.c_str(), joinLanes(*lanes).c_str());

                lanes = lanesVector.erase(lanes);
            }
            else
            {
                ++lanes;
            }
        }
    }

    uint32_t port_count = (uint32_t)lanesVector.size();
    m_port_list.clear();

    for (uint32_t i = 0; i < port_count; i++)
    {
        sai_object_id_t port_id;
        CHECK_STATUS(create(SAI_OBJECT_TYPE_PORT, &port_id, m_switch_id, 0, NULL));
        m_port_list.push_back(port_id);

        std::vector<uint32_t> lanes = lanesVector.at(i);

        sai_attribute_t attr;

        attr.id = SAI_PORT_ATTR_ADMIN_STATE;
        attr.value.booldata = false;
        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_MTU;
        attr.value.u32 = 1514;
        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_SPEED;
        attr.value.u32 = 40 * 1000;
        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
        attr.value.u32list.count = (uint32_t)lanes.size();
        attr.value.u32list.list = lanes.data();
        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_TYPE;
        attr.value.s32 = SAI_PORT_TYPE_LOGICAL;
        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_OPER_STATUS;
        attr.value.s32 = SAI_PORT_OPER_STATUS_DOWN;
        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_PORT_VLAN_ID;
        attr.value.u32 = DEFAULT_VLAN_NUMBER;
        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_HOST_TX_READY_STATUS;
        attr.value.u32 = SAI_PORT_HOST_TX_READY_STATUS_READY;
        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_AUTO_NEG_MODE;
        attr.value.booldata = true;
        CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));
    }

    return SAI_STATUS_SUCCESS;
}

bool SwitchMicas48CON2SFP::renameNetIntf(
        _In_ const std::string &srcName,
        _In_ const std::string &dstName)
{
    SWSS_LOG_ENTER();

    std::string res;
    swss::exec("ip addr flush dev " + srcName + " scope global", res);

    int s = socket(AF_INET, SOCK_DGRAM, 0);

    if (s < 0)
    {
        SWSS_LOG_WARN("MICAS_VSLIB: socket() failed for %s -> %s (errno %d: %s)",
                srcName.c_str(), dstName.c_str(), errno, strerror(errno));
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
        SWSS_LOG_WARN("MICAS_VSLIB: rename %s -> %s failed (errno %d: %s)",
                srcName.c_str(), dstName.c_str(), errno, strerror(errno));
        ok = false;
    }

    close(s);

    return ok;
}

void SwitchMicas48CON2SFP::restoreBackingInterfaces()
{
    SWSS_LOG_ENTER();

    struct if_nameindex *if_ni = if_nameindex();

    if (!if_ni)
    {
        SWSS_LOG_WARN("MICAS_VSLIB: if_nameindex() failed (errno %d: %s)", errno, strerror(errno));
        return;
    }

    for (struct if_nameindex *p = if_ni; p->if_index != 0 && p->if_name != nullptr; p++)
    {
        int portNum = -1;

        if (sscanf(p->if_name, "Ethernet%d", &portNum) != 1 || portNum <= 0)
        {
            continue;
        }

        std::string backingName = getBackingInterfaceName(portNum);
        renameNetIntf(p->if_name, backingName);
    }

    if_freenameindex(if_ni);
}

bool SwitchMicas48CON2SFP::createEthernetInterface(
        _In_ const std::string &backingName,
        _In_ const std::string &ethernetName)
{
    SWSS_LOG_ENTER();

    return renameNetIntf(backingName, ethernetName);
}

bool SwitchMicas48CON2SFP::hostif_create_tap_veth_forwarding(
        _In_ const std::string &tapname,
        _In_ int tapfd,
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    (void)tapname;
    (void)tapfd;
    (void)port_id;

    return true;
}

sai_status_t SwitchMicas48CON2SFP::vs_create_hostif_tap_interface(
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
    int ifindex = (int)if_nametoindex(name.c_str());

    if (ifindex == 0)
    {
        int portNum = -1;

        if (sscanf(name.c_str(), "Ethernet%d", &portNum) != 1 || portNum <= 0)
        {
            SWSS_LOG_ERROR("MICAS_VSLIB: cannot parse port number from %s", name.c_str());
            return SAI_STATUS_FAILURE;
        }

        std::string backingName = getBackingInterfaceName(portNum);

        if ((int)if_nametoindex(backingName.c_str()) == 0)
        {
            SWSS_LOG_ERROR("MICAS_VSLIB: source interface %s not found (errno %d: %s)",
                    backingName.c_str(), errno, strerror(errno));
            return SAI_STATUS_FAILURE;
        }

        if (!createEthernetInterface(backingName, name))
        {
            return SAI_STATUS_FAILURE;
        }

        ifindex = (int)if_nametoindex(name.c_str());

        if (ifindex == 0)
        {
            SWSS_LOG_ERROR("MICAS_VSLIB: %s not found after rename", name.c_str());
            return SAI_STATUS_FAILURE;
        }
    }

    int mtu = ETH_FRAME_BUFFER_SIZE;
    sai_attribute_t attrmtu;
    attrmtu.id = SAI_PORT_ATTR_MTU;

    if (get(SAI_OBJECT_TYPE_PORT, obj_id, 1, &attrmtu) == SAI_STATUS_SUCCESS)
    {
        mtu = attrmtu.value.u32;
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

    setIfNameToPortId(name, obj_id);
    setPortIdToTapName(obj_id, name);

    sai_attribute_t attr;
    attr.id = SAI_PORT_ATTR_ADMIN_STATE;
    sai_status_t status = get(SAI_OBJECT_TYPE_PORT, obj_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (ifup(name.c_str(), obj_id, attr.value.booldata, true))
    {
        SWSS_LOG_ERROR("MICAS_VSLIB: ifup failed on %s", name.c_str());
        return SAI_STATUS_FAILURE;
    }

    if (attr.value.booldata)
    {
        scheduleDeferredPortUpNotification(obj_id);
    }

    for (const auto &family : { "ipv4", "ipv6" })
    {
        std::string path = std::string("/proc/sys/net/") + family + "/conf/" + name + "/forwarding";
        std::ofstream ofs(path);
        if (ofs.is_open())
        {
            ofs << "1";
        }
    }

    return SAI_STATUS_SUCCESS;
}

void SwitchMicas48CON2SFP::scheduleDeferredPortUpNotification(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t cbattr;
    cbattr.id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;

    if (get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &cbattr) != SAI_STATUS_SUCCESS
            || cbattr.value.ptr == NULL)
    {
        SWSS_LOG_WARN("MICAS_VSLIB: cannot schedule deferred notification for port %s - callback not registered yet",
                sai_serialize_object_id(port_id).c_str());
        return;
    }

    sai_port_oper_status_notification_t deferredData = {};
    deferredData.port_id = port_id;
    deferredData.port_state = SAI_PORT_OPER_STATUS_UP;

    auto str = sai_serialize_port_oper_status_ntf(1, &deferredData);
    auto ntf = std::make_shared<sairedis::NotificationPortStateChange>(str);

    sai_switch_notifications_t sn = {};
    sn.on_port_state_change = (sai_port_state_change_notification_fn)cbattr.value.ptr;

    std::string tapName;

    if (!getTapNameFromPortId(port_id, tapName))
    {
        SWSS_LOG_WARN("MICAS_VSLIB: cannot schedule deferred notification for port %s - tap name not found",
                sai_serialize_object_id(port_id).c_str());
        return;
    }

    auto payload = std::make_shared<EventPayloadNotification>(ntf, sn);
    auto event = std::make_shared<Event>(EVENT_TYPE_NOTIFICATION, payload);
    auto eventQueue = m_switchConfig->m_eventQueue;
    auto shutdown = m_shutdown;

    std::thread([event, eventQueue, port_id, tapName, shutdown]() {
        sleep(15);

        if (*shutdown)
        {
            SWSS_LOG_NOTICE("MICAS_VSLIB: skipping deferred port state UP for %s (switch destroyed)",
                    sai_serialize_object_id(port_id).c_str());
            return;
        }

        std::string operstatePath = "/sys/class/net/" + tapName + "/operstate";
        std::ifstream operstateFile(operstatePath);
        std::string operstate;

        if (!operstateFile.is_open() || !(operstateFile >> operstate) || operstate != "up")
        {
            SWSS_LOG_NOTICE("MICAS_VSLIB: skipping deferred port state UP for %s (kernel operstate=%s)",
                    sai_serialize_object_id(port_id).c_str(), operstate.c_str());
            return;
        }

        SWSS_LOG_NOTICE("MICAS_VSLIB: sending deferred port state notification for port %s",
                sai_serialize_object_id(port_id).c_str());

        eventQueue->enqueue(event);
    }).detach();
}

sai_status_t SwitchMicas48CON2SFP::vs_remove_hostif_tap_interface(
        _In_ sai_object_id_t hostif_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_HOSTIF_ATTR_TYPE;
    sai_status_t status = get(SAI_OBJECT_TYPE_HOSTIF, hostif_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (attr.value.s32 == SAI_HOSTIF_TYPE_GENETLINK)
    {
        return SAI_STATUS_SUCCESS;
    }

    attr.id = SAI_HOSTIF_ATTR_OBJ_ID;
    status = get(SAI_OBJECT_TYPE_HOSTIF, hostif_id, 1, &attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (objectTypeQuery(attr.value.oid) == SAI_OBJECT_TYPE_VLAN)
    {
        return SAI_STATUS_SUCCESS;
    }

    attr.id = SAI_HOSTIF_ATTR_NAME;
    status = get(SAI_OBJECT_TYPE_HOSTIF, hostif_id, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (strnlen(attr.value.chardata, sizeof(attr.value.chardata)) >= MAX_INTERFACE_NAME_LEN)
    {
        return SAI_STATUS_FAILURE;
    }

    std::string name = std::string(attr.value.chardata);
    auto it = m_hostif_info_map.find(name);

    if (it == m_hostif_info_map.end())
    {
        return SAI_STATUS_FAILURE;
    }

    auto info = it->second;

    m_hostif_info_map.erase(it);
    removeIfNameToPortId(name);
    removePortIdToTapName(info->m_portId);

    int portNum = -1;

    if (sscanf(name.c_str(), "Ethernet%d", &portNum) == 1 && portNum > 0)
    {
        std::string backingName = getBackingInterfaceName(portNum);
        renameNetIntf(name, backingName);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchMicas48CON2SFP::setPort(
        _In_ sai_object_id_t portId,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    if (attr && attr->id == SAI_PORT_ATTR_ADMIN_STATE)
    {
        bool up = attr->value.booldata;
        std::string name;

        if (getTapNameFromPortId(portId, name))
        {
            if (ifup(name.c_str(), portId, up, false))
            {
                SWSS_LOG_ERROR("MICAS_VSLIB: if admin %s failed on %s: %s",
                        (up ? "UP" : "DOWN"), name.c_str(), strerror(errno));
                return SAI_STATUS_FAILURE;
            }

        }

        return set_internal(SAI_OBJECT_TYPE_PORT, sai_serialize_object_id(portId), attr);
    }

    return SwitchStateBase::setPort(portId, attr);
}
