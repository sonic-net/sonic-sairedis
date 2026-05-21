#include "SwitchVpp.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include "vppxlate/SaiIntfStats.h"

using namespace saivs;

// TODO init vpp

SwitchVpp::SwitchVpp(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config):
    SwitchStateBase(switch_id, manager, config),
    m_object_db(this),
    m_tunnel_mgr(this),
    m_tunnel_mgr_srv6(this)
{
    SWSS_LOG_ENTER();

    vpp_dp_initialize();
}

SwitchVpp::SwitchVpp(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config,
        _In_ std::shared_ptr<WarmBootState> warmBootState):
    SwitchStateBase(switch_id, manager, config, warmBootState),
    m_object_db(this),
    m_tunnel_mgr(this),
    m_tunnel_mgr_srv6(this)
{
    SWSS_LOG_ENTER();

    vpp_dp_initialize();
}

SwitchVpp::~SwitchVpp()
{
    SWSS_LOG_ENTER();

    // Signal the vpp events thread to stop
    m_run_vpp_events_thread = false;

    // Wait for the thread to finish gracefully
    if (m_vpp_thread && m_vpp_thread->joinable()) {
        m_vpp_thread->join();
    }

    SWSS_LOG_NOTICE("SwitchVpp destructor completed");
}

sai_status_t SwitchVpp::create_qos_queues_per_port(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    // 10 in and 10 out queues per port
    const uint32_t port_qos_queues_count = 20;

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

sai_status_t SwitchVpp::create_cpu_qos_queues(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    // CPU queues are of type multicast queues
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

sai_status_t SwitchVpp::create_qos_queues()
{
    SWSS_LOG_ENTER();

    // XXX queues size may change when we will modify queue or ports

    SWSS_LOG_INFO("create qos queues");

    for (auto &port_id: m_port_list)
    {
        CHECK_STATUS(create_qos_queues_per_port(port_id));
    }

    CHECK_STATUS(create_cpu_qos_queues(m_cpu_port_id));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::create_port_serdes()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create port serdes for all ports");

    for (auto &port_id: m_port_list)
    {
        CHECK_STATUS(create_port_serdes_per_port(port_id));
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::create_port_serdes_per_port(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    sai_object_id_t port_serdes_id;

    sai_attribute_t attr;

    // create port serdes fir specific port

    attr.id = SAI_PORT_SERDES_ATTR_PORT_ID;
    attr.value.oid = port_id;

    CHECK_STATUS(create(SAI_OBJECT_TYPE_PORT_SERDES, &port_serdes_id, m_switch_id, 1, &attr));

    // set port serdes read only value

    attr.id = SAI_PORT_ATTR_PORT_SERDES_ID;
    attr.value.oid = port_serdes_id;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::create_scheduler_group_tree(
        _In_ const std::vector<sai_object_id_t>& sgs,
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attrq;

    std::vector<sai_object_id_t> queues;

    // in this implementation we have 20 queues per port
    // (10 in and 10 out), which will be assigned to schedulers
    uint32_t queues_count = 20;

    queues.resize(queues_count);

    attrq.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
    attrq.value.objlist.count = queues_count;
    attrq.value.objlist.list = queues.data();

    // NOTE it will do recalculate
    CHECK_STATUS(get(SAI_OBJECT_TYPE_PORT, port_id, 1, &attrq));

    // schedulers groups: 0 1 2 3 4 5 6 7 8 9 a b c

    // tree index
    // 0 = 1 2
    // 1 = 3 4 5 6 7 8 9 a
    // 2 = b c (bug on brcm)

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

        // now assign queues to level 1 scheduler groups,

        for (size_t i = 0; i < list.size(); ++i)
        {
            sai_object_id_t childs[2];

            childs[0] = queues[queue_index];    // first half are in queues
            childs[1] = queues[queue_index + queues_count/2]; // second half are out queues

            // for each scheduler set 2 queues
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

            // for each scheduler set 2 queues
            childs[0] = queues[queue_index];    // first half are in queues
            childs[1] = queues[queue_index + queues_count/2]; // second half are out queues

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

sai_status_t SwitchVpp::create_scheduler_groups_per_port(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    uint32_t port_sgs_count = 13; // brcm default

    // NOTE: this is only static data, to keep track of this
    // we would need to create actual objects and keep them
    // in respected objects, we need to move in to that
    // solution when we will start using different "profiles"
    // currently this is good enough

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_QOS_NUMBER_OF_SCHEDULER_GROUPS;
    attr.value.u32 = port_sgs_count;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    // scheduler groups per port

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

    // SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT // sched_groups + count
    // scheduler group are organized in tree and on the bottom there are queues
    // order matters in returning api

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::set_maximum_number_of_childs_per_scheduler_group()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create switch src mac address");

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_CHILDS_PER_SCHEDULER_GROUP;
    attr.value.u32 = 16;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchVpp::refresh_bridge_port_list(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t bridge_id)
{
    SWSS_LOG_ENTER();

    // XXX possible issues with vxlan and lag.

    auto &all_bridge_ports = m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT);

    sai_attribute_t attr;

    auto me_port_list = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE, SAI_BRIDGE_ATTR_PORT_LIST);
    auto m_port_id = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_PORT_ID);
    auto m_bridge_id = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_BRIDGE_ID);
    auto m_type = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_TYPE);

    /*
     * First get all port's that belong to this bridge id.
     */

    attr.id = SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID;

    CHECK_STATUS(get(SAI_OBJECT_TYPE_SWITCH, m_switch_id, 1, &attr));

    /*
     * Create bridge ports for regular ports.
     */

    sai_object_id_t default_1q_bridge_id = attr.value.oid;

    std::map<sai_object_id_t, SwitchState::AttrHash> bridge_port_list_on_bridge_id;

    // update default bridge port id's for bridge port if attr type is missing
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

        // this bridge port is type PORT, and it's missing BRIDGE_ID attr

        SWSS_LOG_NOTICE("setting default bridge id (%s) on bridge port %s",
                sai_serialize_object_id(default_1q_bridge_id).c_str(),
                bp.first.c_str());

        attr.id = SAI_BRIDGE_PORT_ATTR_BRIDGE_ID;
        attr.value.oid = default_1q_bridge_id;

        sai_object_id_t bridge_port;
        sai_deserialize_object_id(bp.first, bridge_port);

        CHECK_STATUS(set(SAI_OBJECT_TYPE_BRIDGE_PORT, bridge_port, &attr));
    }

    // will contain 1q router bridge port, which we want to skip?
    for (const auto &bp: all_bridge_ports)
    {
        auto it = bp.second.find(m_bridge_id->attridname);

        if (it == bp.second.end())
        {
            // fine on router 1q
            SWSS_LOG_NOTICE("not found %s on bridge port: %s", m_bridge_id->attridname, bp.first.c_str());
            continue;
        }

        if (bridge_id == it->second->getAttr()->value.oid)
        {
            /*
             * This bridge port belongs to currently processing bridge ID.
             */

            sai_object_id_t bridge_port;

            sai_deserialize_object_id(bp.first, bridge_port);

            bridge_port_list_on_bridge_id[bridge_port] = bp.second;
        }
    }

    /*
     * Now sort those bridge port id's by port id to be consistent.
     */

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

sai_status_t SwitchVpp::warm_update_queues()
{
    SWSS_LOG_ENTER();

    for (auto port: m_port_list)
    {
        sai_attribute_t attr;

        std::vector<sai_object_id_t> list(MAX_OBJLIST_LEN);

        // get all queues list on current port

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
                attr.value.u8 = index; // warn, we are guessing index here if it was not defined

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

bool SwitchVpp::port_to_hostif_list(
        _In_ sai_object_id_t port_id,
        _Inout_ std::string& if_name)
{
    SWSS_LOG_ENTER();

    // TODO to be removed and inlined

    //sai_object_id_t switch_id = switchIdQuery(port_id);
    //if (switch_id == SAI_NULL_OBJECT_ID) {
    //return false;
    //}
    //auto it = m_switchStateMap.find(switch_id);
    //if (it == m_switchStateMap.end()) {
    //return false;
    //}
    //auto sw = it->second;
    //if (sw == nullptr) {
    //return false;
    //}
    //return(
    return getTapNameFromPortId(port_id, if_name);
}

bool SwitchVpp::port_to_hwifname(
        _In_ sai_object_id_t port_id,
        _Inout_ std::string& if_name)
{
    SWSS_LOG_ENTER();

    // TODO to be removed and inlined

    // sai_object_id_t switch_id = switchIdQuery(port_id);
    // if (switch_id == SAI_NULL_OBJECT_ID) {
    // return false;
    // }
    // auto it = m_switchStateMap.find(switch_id);
    // if (it == m_switchStateMap.end()) {
    // return false;
    // }
    // auto sw = it->second;
    // if (sw == nullptr) {
    // return false;
    // }

    return vpp_get_hwif_name(port_id, 0, if_name);
}

void SwitchVpp::setPortStats(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    std::map<sai_stat_id_t, uint64_t> stats;

    std::string if_name;

    if (!port_to_hwifname(oid, if_name))
    {
        return;
    }

    vpp_interface_stats_t port_stats;

    if (vpp_intf_stats_query(if_name.c_str(), &port_stats) == 0)
    {
        stats[SAI_PORT_STAT_IF_IN_OCTETS] = port_stats.rx_bytes;
        stats[SAI_PORT_STAT_IF_IN_UCAST_PKTS] = port_stats.rx;
        stats[SAI_PORT_STAT_IF_IN_BROADCAST_PKTS] = port_stats.rx_broadcast;
        stats[SAI_PORT_STAT_IF_IN_MULTICAST_PKTS] = port_stats.rx_multicast;
        stats[SAI_PORT_STAT_IF_IN_DISCARDS] = port_stats.drops;
        stats[SAI_PORT_STAT_IF_OUT_OCTETS] = port_stats.tx_bytes;
        stats[SAI_PORT_STAT_IF_OUT_UCAST_PKTS] = port_stats.tx;
        stats[SAI_PORT_STAT_IF_OUT_BROADCAST_PKTS] = port_stats.tx_broadcast;
        stats[SAI_PORT_STAT_IF_OUT_MULTICAST_PKTS] = port_stats.tx_multicast;

        stats[SAI_PORT_STAT_IN_DROPPED_PKTS] = port_stats.rx_no_buf;
        stats[SAI_PORT_STAT_IF_IN_ERRORS] = port_stats.rx_error;
        stats[SAI_PORT_STAT_IF_OUT_ERRORS] = port_stats.tx_error;
        stats[SAI_PORT_STAT_IP_IN_RECEIVES] = port_stats.ip4;
        stats[SAI_PORT_STAT_IPV6_IN_RECEIVES] = port_stats.ip6;
    }

    debugSetStats(oid, stats);
}

sai_status_t SwitchVpp::queryAttributeCapability(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ sai_attr_id_t attr_id,
        _Out_ sai_attr_capability_t *capability)
{
    SWSS_LOG_ENTER();

    // TODO: We should generate this metadata for the virtual switch rather
    // than hard-coding it here.

    // in virtual switch by default all apis are implemented for all objects. SUCCESS for all attributes

    capability->create_implemented = true;
    capability->set_implemented    = true;
    capability->get_implemented    = true;

    return SAI_STATUS_SUCCESS;
}

uint64_t SwitchVpp::getObjectTypeAvailability(
        _In_ sai_object_type_t object_type)
{
    SWSS_LOG_ENTER();

    if (object_type == SAI_OBJECT_TYPE_MY_SID_ENTRY)
    {
        // Return available MY_SID entries (max - used)
        return static_cast<uint64_t>(m_maxMySidEntries - m_srv6_my_sid_count);
    }

    // Return 0 for unsupported types
    return 0;
}

sai_status_t SwitchVpp::getStatsExt(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    if (object_type == SAI_OBJECT_TYPE_PORT)
    {
        setPortStats(object_id);
    }

    return SwitchStateBase::getStatsExt(
            object_type,
            object_id,
            number_of_counters,
            counter_ids,
            mode,
            counters);
}

void SwitchVpp::processFdbEntriesForAging()
{
    SWSS_LOG_ENTER();

    return;
}

sai_status_t SwitchVpp::create(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (object_type == SAI_OBJECT_TYPE_DEBUG_COUNTER)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createDebugCounter(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_PORT)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createPort(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_HOSTIF)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createHostif(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_ROUTER_INTERFACE)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createRouterif(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_ROUTE_ENTRY)
    {
        return addIpRoute(serializedObjectId, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_MY_SID_ENTRY)
    {
        sai_status_t status = m_tunnel_mgr_srv6.create_my_sid_entry(serializedObjectId, switch_id, attr_count, attr_list);
        if (status == SAI_STATUS_SUCCESS)
        {
            m_srv6_my_sid_count++;
            SWSS_LOG_DEBUG("MY_SID entry created, count: %u", m_srv6_my_sid_count);
        }
        return status;
    }

    if (object_type == SAI_OBJECT_TYPE_SRV6_SIDLIST)
    {
        return  m_tunnel_mgr_srv6.create_sidlist(serializedObjectId, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_NEXT_HOP)
    {
        return createNexthop(serializedObjectId, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER)
    {
        return createNexthopGroupMember(serializedObjectId, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_NEIGHBOR_ENTRY)
    {
        return addIpNbr(serializedObjectId, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_ACL_ENTRY)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createAclEntry(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_ACL_TABLE)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return aclTableCreate(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createAclGrpMbr(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_MACSEC_PORT)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createMACsecPort(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_MACSEC_SC)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createMACsecSC(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_MACSEC_SA)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createMACsecSA(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_NEIGHBOR_ENTRY && m_system_port_list.size())
    {
        // Neighbor entry programming for VOQ systems
        return createVoqSystemNeighborEntry(serializedObjectId, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_VLAN_MEMBER)
    {
        sai_object_id_t object_id;
        sai_deserialize_object_id(serializedObjectId, object_id);
        return createVlanMember(object_id, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_FDB_ENTRY)
    {
        return FdbEntryadd(serializedObjectId, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_BFD_SESSION)
    {
        return bfd_session_add(serializedObjectId, switch_id, attr_count, attr_list);
    }

    if (object_type == SAI_OBJECT_TYPE_LAG )
    {
       sai_object_id_t object_id;
       sai_deserialize_object_id(serializedObjectId, object_id);
       return createLag(object_id, switch_id, attr_count, attr_list);
    }
    if (object_type == SAI_OBJECT_TYPE_LAG_MEMBER)
    {
       sai_object_id_t object_id;
       sai_deserialize_object_id(serializedObjectId, object_id);
       return createLagMember(object_id, switch_id, attr_count, attr_list);
    }

    return create_internal(object_type, serializedObjectId, switch_id, attr_count, attr_list);
}

sai_status_t SwitchVpp::create_internal(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto &objectHash = m_objectHash.at(object_type);

    if (m_switchConfig->m_resourceLimiter)
    {
        size_t limit = m_switchConfig->m_resourceLimiter->getObjectTypeLimit(object_type);

        if (objectHash.size() >= limit)
        {
            SWSS_LOG_ERROR("too many %s, created %zu is resource limit",
                    sai_serialize_object_type(object_type).c_str(),
                    limit);

            return SAI_STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    auto it = objectHash.find(serializedObjectId);

    if (object_type != SAI_OBJECT_TYPE_SWITCH)
    {
        /*
         * Switch is special, and object is already created by init.
         *
         * XXX revisit this.
         */

        if (it != objectHash.end())
        {
            SWSS_LOG_ERROR("create failed, object already exists, object type: %s: id: %s",
                    sai_serialize_object_type(object_type).c_str(),
                    serializedObjectId.c_str());

            return SAI_STATUS_ITEM_ALREADY_EXISTS;
        }
    }

    if (objectHash.find(serializedObjectId) == objectHash.end())
    {
        /*
         * Number of attributes may be zero, so see if actual entry was created
         * with empty hash.
         */

        objectHash[serializedObjectId] = {};
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        auto a = std::make_shared<SaiAttrWrap>(object_type, &attr_list[i]);

        objectHash[serializedObjectId][a->getAttrMetadata()->attridname] = a;
    }

    m_object_db.create_or_update(object_type, serializedObjectId, attr_count, attr_list, true /*is_create*/);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::createPort(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    UpdatePort(object_id, attr_count, attr_list);

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_PORT, sid, switch_id, attr_count, attr_list));

    return create_port_dependencies(object_id);
}


sai_status_t SwitchVpp::remove(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    if (object_type == SAI_OBJECT_TYPE_DEBUG_COUNTER)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeDebugCounter(objectId);
    }

    if (object_type == SAI_OBJECT_TYPE_PORT)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removePort(objectId);
    }

    if (object_type == SAI_OBJECT_TYPE_HOSTIF)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeHostif(objectId);
    }

    if (object_type == SAI_OBJECT_TYPE_ROUTER_INTERFACE)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeRouterif(objectId);
    }

    if (object_type == SAI_OBJECT_TYPE_VIRTUAL_ROUTER)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeVrf(objectId);
    }

    if (object_type == SAI_OBJECT_TYPE_ROUTE_ENTRY)
    {
        return removeIpRoute(serializedObjectId);
    }

    if (object_type == SAI_OBJECT_TYPE_MY_SID_ENTRY)
    {
        sai_status_t status = m_tunnel_mgr_srv6.remove_my_sid_entry(serializedObjectId);
        if (status == SAI_STATUS_SUCCESS)
        {
            if (m_srv6_my_sid_count > 0)
            {
                m_srv6_my_sid_count--;
            }
            SWSS_LOG_DEBUG("MY_SID entry removed, count: %u", m_srv6_my_sid_count);
        }
        return status;
    }

    if (object_type == SAI_OBJECT_TYPE_SRV6_SIDLIST)
    {
        return  m_tunnel_mgr_srv6.remove_sidlist(serializedObjectId);
    }

    if (object_type == SAI_OBJECT_TYPE_NEXT_HOP)
    {
        return removeNexthop(serializedObjectId);
    }

    if (object_type == SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER)
    {
        return removeNexthopGroupMember(serializedObjectId);
    }

    if (object_type == SAI_OBJECT_TYPE_NEIGHBOR_ENTRY)
    {
        return removeIpNbr(serializedObjectId);
    }

    if (object_type == SAI_OBJECT_TYPE_ACL_ENTRY)
    {
        return removeAclEntry(serializedObjectId);
    }

    if (object_type == SAI_OBJECT_TYPE_ACL_TABLE)
    {
        return aclTableRemove(serializedObjectId);
    }

    if (object_type == SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER)
    {
        return removeAclGrpMbr(serializedObjectId);
    }

    if (object_type == SAI_OBJECT_TYPE_ACL_TABLE_GROUP)
    {
        return removeAclGrp(serializedObjectId);
    }

    if (object_type == SAI_OBJECT_TYPE_MACSEC_PORT)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeMACsecPort(objectId);
    }
    else if (object_type == SAI_OBJECT_TYPE_MACSEC_SC)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeMACsecSC(objectId);
    }
    else if (object_type == SAI_OBJECT_TYPE_MACSEC_SA)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeMACsecSA(objectId);
    }

    if (object_type == SAI_OBJECT_TYPE_VLAN_MEMBER)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeVlanMember(objectId);
    }
    else if (object_type == SAI_OBJECT_TYPE_LAG)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeLag(objectId);
    }
    else if (object_type == SAI_OBJECT_TYPE_LAG_MEMBER)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return removeLagMember(objectId);
    }
    else if (object_type == SAI_OBJECT_TYPE_FDB_ENTRY)
    {
        return FdbEntrydel(serializedObjectId);
    }
    else if (object_type == SAI_OBJECT_TYPE_BFD_SESSION)
    {
        return bfd_session_del(serializedObjectId);
    }

    return remove_internal(object_type, serializedObjectId);
}

sai_status_t SwitchVpp::remove_internal(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("removing object: %s", serializedObjectId.c_str());

    m_object_db.remove(object_type, serializedObjectId);

    auto &objectHash = m_objectHash.at(object_type);

    auto it = objectHash.find(serializedObjectId);

    if (it == objectHash.end())
    {
        SWSS_LOG_ERROR("not found %s:%s",
                sai_serialize_object_type(object_type).c_str(),
                serializedObjectId.c_str());

        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    objectHash.erase(it);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::setPort(
        _In_ sai_object_id_t portId,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    UpdatePort(portId, 1, attr);

    auto sid = sai_serialize_object_id(portId);

    return set_internal(SAI_OBJECT_TYPE_PORT, sid, attr);
}

sai_status_t SwitchVpp::setLag(
        _In_ sai_object_id_t lagId,
        _In_ const sai_attribute_t* lagAttr)
{
    SWSS_LOG_ENTER();

    auto attr_type = sai_metadata_get_attr_by_id(SAI_LAG_ATTR_INGRESS_ACL, 1, lagAttr);

    if (attr_type != NULL)
    {
        if (attr_type->value.oid == SAI_NULL_OBJECT_ID) {
            sai_attribute_t attr;

            attr.id = SAI_LAG_ATTR_INGRESS_ACL;
            if (get(SAI_OBJECT_TYPE_LAG, lagId, 1, &attr) != SAI_STATUS_SUCCESS) {
                aclBindUnbindPort(lagId, attr.value.oid, true, false);
            }
        } else {
            aclBindUnbindPort(lagId, attr_type->value.oid, true, true);
        }
    }

    attr_type = sai_metadata_get_attr_by_id(SAI_LAG_ATTR_EGRESS_ACL, 1, lagAttr);

    if (attr_type != NULL)
    {
        if (attr_type->value.oid == SAI_NULL_OBJECT_ID) {
            sai_attribute_t attr;

            attr.id = SAI_LAG_ATTR_EGRESS_ACL;
            if (get(SAI_OBJECT_TYPE_LAG, lagId, 1, &attr) != SAI_STATUS_SUCCESS) {
                aclBindUnbindPort(lagId, attr.value.oid, false, false);
            }
        } else {
            aclBindUnbindPort(lagId, attr_type->value.oid, false, true);
        }
    }

    auto sid = sai_serialize_object_id(lagId);

    return set_internal(SAI_OBJECT_TYPE_LAG, sid, lagAttr);
}

sai_status_t SwitchVpp::setAclEntry(
        _In_ sai_object_id_t entry_id,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    if (attr && attr->id == SAI_ACL_ENTRY_ATTR_ACTION_MACSEC_FLOW)
    {
        return setAclEntryMACsecFlowActive(entry_id, attr);
    }

    auto sid = sai_serialize_object_id(entry_id);

    set_internal(SAI_OBJECT_TYPE_ACL_ENTRY, sid, attr);

    sai_object_id_t tbl_oid;

    if (getAclTableId(entry_id, &tbl_oid) != SAI_STATUS_SUCCESS)
    {
        return SAI_STATUS_FAILURE;
    }

    auto status = AclAddRemoveCheck(tbl_oid);

    SWSS_LOG_NOTICE("ACL entry %s set in table %s set status %d",
            sid.c_str(),
            sai_serialize_object_id(tbl_oid).c_str(),
            status);

    return status;
}

sai_status_t SwitchVpp::set(
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    if (objectType == SAI_OBJECT_TYPE_PORT)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return setPort(objectId, attr);
    }

    if (objectType == SAI_OBJECT_TYPE_ROUTER_INTERFACE)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return vpp_update_router_interface(objectId, 1, attr);
    }

    if (objectType == SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return setAclGrpMbr(objectId, attr);
    }

    if (objectType == SAI_OBJECT_TYPE_ROUTE_ENTRY)
    {
        return updateIpRoute(serializedObjectId, attr);
    }

    if (objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        switch(attr->id)
        {
            case SAI_SWITCH_ATTR_VXLAN_DEFAULT_ROUTER_MAC:
                {
                    m_tunnel_mgr.set_router_mac(attr);
                    break;
                }
            case SAI_SWITCH_ATTR_VXLAN_DEFAULT_PORT:
                {
                    m_tunnel_mgr.set_vxlan_port(attr);
                    break;
                }
        }
    }

    if (objectType == SAI_OBJECT_TYPE_ACL_ENTRY)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return setAclEntry(objectId, attr);
    }

    if (objectType == SAI_OBJECT_TYPE_MACSEC_SA)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return setMACsecSA(objectId, attr);
    }

    if (objectType == SAI_OBJECT_TYPE_LAG)
    {
        sai_object_id_t objectId;
        sai_deserialize_object_id(serializedObjectId, objectId);
        return setLag(objectId, attr);
    }

    return set_internal(objectType, serializedObjectId, attr);
}

sai_status_t SwitchVpp::set_internal(
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    //Update child-parent relationship before updating the attribute
    m_object_db.create_or_update(objectType, serializedObjectId, 1, attr, false /*is_create*/);

    auto it = m_objectHash.at(objectType).find(serializedObjectId);

    if (it == m_objectHash.at(objectType).end())
    {
        SWSS_LOG_ERROR("not found %s:%s",
                sai_serialize_object_type(objectType).c_str(),
                serializedObjectId.c_str());

        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    auto &attrHash = it->second;

    auto a = std::make_shared<SaiAttrWrap>(objectType, attr);

    // set have only one attribute
    attrHash[a->getAttrMetadata()->attridname] = a;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::get(
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (objectType == SAI_OBJECT_TYPE_ACL_COUNTER)
    {
        sai_object_id_t object_id;

        sai_deserialize_object_id(serializedObjectId, object_id);
        return getAclEntryStats(object_id, attr_count, attr_list);
    }

    const auto &objectHash = m_objectHash.at(objectType);

    auto it = objectHash.find(serializedObjectId);

    if (it == objectHash.end())
    {
        SWSS_LOG_ERROR("not found %s:%s",
                sai_serialize_object_type(objectType).c_str(),
                serializedObjectId.c_str());

        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    /*
     * We need reference here since we can potentially update attr hash for RO
     * object.
     */

    auto& attrHash = it->second;

    /*
     * Some of the list query maybe for length, so we can't do
     * normal serialize, maybe with count only.
     */

    sai_status_t final_status = SAI_STATUS_SUCCESS;

    for (uint32_t idx = 0; idx < attr_count; ++idx)
    {
        sai_attr_id_t id = attr_list[idx].id;

        auto meta = sai_metadata_get_attr_metadata(objectType, id);

        if (meta == NULL)
        {
            SWSS_LOG_ERROR("failed to find attribute %d for %s:%s", id,
                    sai_serialize_object_type(objectType).c_str(),
                    serializedObjectId.c_str());

            return SAI_STATUS_FAILURE;
        }

        sai_status_t status;

        if (SAI_HAS_FLAG_READ_ONLY(meta->flags))
        {
            /*
             * Read only attributes may require recalculation.
             * Metadata makes sure that non object id's can't have
             * read only attributes. So here is definitely OID.
             */

            sai_object_id_t oid;
            sai_deserialize_object_id(serializedObjectId, oid);

            status = refresh_read_only(meta, oid);

            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_INFO("%s read only not implemented on %s",
                        meta->attridname,
                        serializedObjectId.c_str());

                return status;
            }
        }

        auto ait = attrHash.find(meta->attridname);

        if (ait == attrHash.end())
        {
            return SAI_STATUS_ITEM_NOT_FOUND;

            SWSS_LOG_WARN("%s not implemented on %s",
                    meta->attridname,
                    serializedObjectId.c_str());

            return SAI_STATUS_NOT_IMPLEMENTED;
        }

        auto attr = ait->second->getAttr();

        status = transfer_attributes(objectType, 1, attr, &attr_list[idx], false);

        if (status == SAI_STATUS_BUFFER_OVERFLOW)
        {
            /*
             * This is considered partial success, since we get correct list
             * length.  Note that other items ARE processes on the list.
             */

            SWSS_LOG_NOTICE("BUFFER_OVERFLOW %s: %s",
                    serializedObjectId.c_str(),
                    meta->attridname);

            /*
             * We still continue processing other attributes for get as long as
             * we only will be getting buffer overflow error.
             */

            final_status = status;
            continue;
        }

        if (status != SAI_STATUS_SUCCESS)
        {
            // all other errors

            SWSS_LOG_ERROR("get failed %s: %s: %s",
                    serializedObjectId.c_str(),
                    meta->attridname,
                    sai_serialize_status(status).c_str());

            return status;
        }
    }

    return final_status;
}

sai_status_t SwitchVpp::bulkCreate(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    uint32_t object_count = (uint32_t) serialized_object_ids.size();

    if (!object_count || !attr_count || !attr_list || !object_statuses)
    {
        SWSS_LOG_ERROR("Invalid arguments");
        return SAI_STATUS_FAILURE;
    }

    sai_status_t status = SAI_STATUS_SUCCESS;
    uint32_t it;

    for (it = 0; it < object_count; it++)
    {
        object_statuses[it] = create_internal(object_type, serialized_object_ids[it], switch_id, attr_count[it], attr_list[it]);

        if (object_statuses[it] != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to create object with type = %u", object_type);

            status = SAI_STATUS_FAILURE;

            if (mode == SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR)
            {
                break;
            }
        }
    }

    while (++it < object_count)
    {
        object_statuses[it] = SAI_STATUS_NOT_EXECUTED;
    }

    return status;
}

sai_status_t SwitchVpp::bulkRemove(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    SWSS_LOG_ENTER();

    uint32_t object_count = (uint32_t) serialized_object_ids.size();

    if (!object_count || !object_statuses)
    {
        SWSS_LOG_ERROR("Invalid arguments");
        return SAI_STATUS_FAILURE;
    }

    sai_status_t status = SAI_STATUS_SUCCESS;
    uint32_t it;

    for (it = 0; it < object_count; it++)
    {
        object_statuses[it] = remove_internal(object_type, serialized_object_ids[it]);

        if (object_statuses[it] != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to remove object with type = %u", object_type);

            status = SAI_STATUS_FAILURE;

            if (mode == SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR)
            {
                break;
            }
        }
    }

    while (++it < object_count)
    {
        object_statuses[it] = SAI_STATUS_NOT_EXECUTED;
    }

    return status;
}

sai_status_t SwitchVpp::get_max(
        _In_ sai_object_type_t objectType,
        _In_ const std::string &serializedObjectId,
        _In_ const uint32_t  max_attr_count,
        _Out_ uint32_t *attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    *attr_count = 0;

    const auto &objectHash = m_objectHash.at(objectType);

    auto it = objectHash.find(serializedObjectId);

    if (it == objectHash.end())
    {
        SWSS_LOG_ERROR("not found %s:%s",
                sai_serialize_object_type(objectType).c_str(),
                serializedObjectId.c_str());

        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    /*
     * We need reference here since we can potentially update attr hash for RO
     * object.
     */

    auto& attrHash = it->second;

    /*
     * Some of the list query maybe for length, so we can't do
     * normal serialize, maybe with count only.
     */

    sai_status_t final_status = SAI_STATUS_SUCCESS, status;
    uint32_t idx = 0;
    sai_attribute_t *dst_attr;

    for (auto &kvp: attrHash)
    {
        auto attr = kvp.second->getAttr();

        dst_attr = &attr_list[idx];
        dst_attr->id = attr->id;

        status = transfer_attributes(objectType, 1, attr, dst_attr, false);

        if (status == SAI_STATUS_BUFFER_OVERFLOW)
        {
            /*
             * This is considered partial success, since we get correct list
             * length.  Note that other items ARE processes on the list.
             */

            SWSS_LOG_NOTICE("BUFFER_OVERFLOW %s: %d",
                    serializedObjectId.c_str(),
                    attr->id);

            /*
             * We still continue processing other attributes for get as long as
             * we only will be getting buffer overflow error.
             */

            final_status = status;
            continue;
        }

        if (status != SAI_STATUS_SUCCESS)
        {
            // all other errors

            SWSS_LOG_ERROR("get failed %s: %d: %s",
                    serializedObjectId.c_str(),
                    attr->id,
                    sai_serialize_status(status).c_str());

            return status;
        }

        ++idx;

        if (idx == max_attr_count)
            break;
    }

    *attr_count = idx;

    return final_status;
}

std::shared_ptr<SaiDBObject> SwitchVpp::get_sai_object(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    return m_object_db.get(object_type, serializedObjectId);
}

sai_status_t SwitchVpp::initialize_default_objects(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(set_switch_mac_address());
    CHECK_STATUS(create_cpu_port());
    CHECK_STATUS(create_default_vlan());
    CHECK_STATUS(create_default_virtual_router());
    CHECK_STATUS(create_default_stp_instance());
    CHECK_STATUS(create_default_1q_bridge());
    CHECK_STATUS(create_default_trap_group());
    CHECK_STATUS(create_ports());
    CHECK_STATUS(create_port_serdes());
    CHECK_STATUS(set_port_list());
    CHECK_STATUS(set_port_capabilities());
    CHECK_STATUS(create_bridge_ports());
    CHECK_STATUS(create_vlan_members());
    CHECK_STATUS(set_acl_entry_min_prio());
    CHECK_STATUS(set_acl_capabilities());
    CHECK_STATUS(create_ingress_priority_groups());
    CHECK_STATUS(create_qos_queues());
    CHECK_STATUS(set_maximum_number_of_childs_per_scheduler_group());
    CHECK_STATUS(set_number_of_ecmp_groups());
    CHECK_STATUS(set_switch_default_attributes());
    CHECK_STATUS(create_scheduler_groups());
    CHECK_STATUS(set_static_crm_values());

    // Initialize switch for VOQ attributes

    CHECK_STATUS(initialize_voq_switch_objects(attr_count, attr_list));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::queryHashNativeHashFieldListCapability(
    _Inout_ sai_s32_list_t *enum_values_capability)
{
    SWSS_LOG_ENTER();

    if (enum_values_capability->count < 5)
    {
        enum_values_capability->count = 5;
        return SAI_STATUS_BUFFER_OVERFLOW;
    }

    enum_values_capability->count = 5;
    enum_values_capability->list[0] = SAI_NATIVE_HASH_FIELD_IP_PROTOCOL;
    enum_values_capability->list[1] = SAI_NATIVE_HASH_FIELD_DST_IP;
    enum_values_capability->list[2] = SAI_NATIVE_HASH_FIELD_SRC_IP;
    enum_values_capability->list[3] = SAI_NATIVE_HASH_FIELD_L4_DST_PORT;
    enum_values_capability->list[4] = SAI_NATIVE_HASH_FIELD_L4_SRC_PORT;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::querySwitchHashAlgorithmCapability(
    _Inout_ sai_s32_list_t *enum_values_capability)
{
    SWSS_LOG_ENTER();

    if (enum_values_capability->count < 1)
    {
        enum_values_capability->count = 1;
        return SAI_STATUS_BUFFER_OVERFLOW;
    }

    enum_values_capability->count = 1;
    enum_values_capability->list[0] = SAI_HASH_ALGORITHM_CRC;

    return SAI_STATUS_SUCCESS;
}
