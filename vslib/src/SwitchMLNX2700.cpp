#include "SwitchMLNX2700.h"

#include "swss/logger.h"
#include "meta/sai_serialize.h"

using namespace saivs;

SwitchMLNX2700::SwitchMLNX2700(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config):
    SwitchStateBase(switch_id, manager, config)
{
    SWSS_LOG_ENTER();

    // empty
}

SwitchMLNX2700::SwitchMLNX2700(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<RealObjectIdManager> manager,
        _In_ std::shared_ptr<SwitchConfig> config,
        _In_ std::shared_ptr<WarmBootState> warmBootState):
    SwitchStateBase(switch_id, manager, config, warmBootState)
{
    SWSS_LOG_ENTER();

    // empty
}

sai_status_t SwitchMLNX2700::create_qos_queues_per_port(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    // 8 in and 8 out queues per port
    const uint32_t port_qos_queues_count = 16;
    std::vector<sai_object_id_t> queues;

    for (uint32_t i = 0; i < port_qos_queues_count; ++i)
    {
        sai_object_id_t queue_id;

        sai_attribute_t attr[2];

        attr[0].id = SAI_QUEUE_ATTR_INDEX;
        attr[0].value.u8 = (uint8_t)i;
        attr[1].id = SAI_QUEUE_ATTR_PORT;
        attr[1].value.oid = port_id;

        CHECK_STATUS(create(SAI_OBJECT_TYPE_QUEUE, &queue_id, m_switch_id, 2, attr));

        queues.push_back(queue_id);
    }

    sai_attribute_t attr;

    attr.id = SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES;
    attr.value.u32 = port_qos_queues_count;

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    attr.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
    attr.value.objlist.count = port_qos_queues_count;
    attr.value.objlist.list = queues.data();

    CHECK_STATUS(set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchMLNX2700::create_qos_queues()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create qos queues");

    std::vector<sai_object_id_t> copy = m_port_list;

    copy.push_back(m_cpu_port_id);

    for (auto &port_id: copy)
    {
        create_qos_queues_per_port(port_id);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchMLNX2700::create_scheduler_group_tree(
        _In_ const std::vector<sai_object_id_t>& sgs,
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attrq;

    std::vector<sai_object_id_t> queues;

    // we have 16 queues per port, and 16 queues (8 in, 8 out)

    uint32_t queues_count = 16;

    queues.resize(queues_count);

    attrq.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
    attrq.value.objlist.count = queues_count;
    attrq.value.objlist.list = queues.data();

    CHECK_STATUS(get(SAI_OBJECT_TYPE_PORT, port_id, 1, &attrq));

    // schedulers groups indexes on list: 0 1 2 3 4 5 6 7 8 9 a b c d e f

    // tree level (2 levels)
    // 0 = 9 8 a b d e f
    // 1 =

    // 2.. - have both QUEUES, each one 2

    // scheduler group 0 (8 childs)
    {
        sai_object_id_t sg_0 = sgs.at(0);

        sai_attribute_t attr;

        attr.id = SAI_SCHEDULER_GROUP_ATTR_PORT_ID;
        attr.value.oid = port_id;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_0, &attr));


        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT;
        attr.value.u32 = 8;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_0, &attr));

        uint32_t list_count = 8;
        std::vector<sai_object_id_t> list;

        list.push_back(sgs.at(0x8));
        list.push_back(sgs.at(0x9));
        list.push_back(sgs.at(0xa));
        list.push_back(sgs.at(0xb));
        list.push_back(sgs.at(0xc));
        list.push_back(sgs.at(0xd));
        list.push_back(sgs.at(0xe));
        list.push_back(sgs.at(0xf));

        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST;
        attr.value.objlist.count = list_count;
        attr.value.objlist.list = list.data();

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_0, &attr));
    }

    for (int i = 1; i < 8; ++i)
    {
        // 1..7 schedulers are empty

        sai_object_id_t sg = sgs.at(i);

        sai_attribute_t attr;

        attr.id = SAI_SCHEDULER_GROUP_ATTR_PORT_ID;
        attr.value.oid = port_id;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg, &attr));

        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT;
        attr.value.u32 = 0;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg, &attr));

        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST;
        attr.value.objlist.count = 0;
        attr.value.objlist.list = NULL;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg, &attr));
    }

    // 8..f have for 2 queues

    int queue_index = 0;

    for (int i = 8; i < 0x10; ++i)
    {
        sai_object_id_t sg = sgs.at(i);

        sai_object_id_t childs[2];

        sai_attribute_t attr;

        // for each scheduler set 2 queues
        childs[0] = queues[queue_index];    // first half are in queues
        childs[1] = queues[queue_index + queues_count/2]; // second half are out queues

        queue_index++;

        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST;
        attr.value.objlist.count = 2;
        attr.value.objlist.list = childs;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg, &attr));

        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT;
        attr.value.u32 = 2;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg, &attr));

        attr.id = SAI_SCHEDULER_GROUP_ATTR_PORT_ID;
        attr.value.oid = port_id;

        CHECK_STATUS(set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg, &attr));
    }


    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchMLNX2700::create_scheduler_groups_per_port(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    uint32_t port_sgs_count = 16; // mlnx default

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

sai_status_t SwitchMLNX2700::set_maximum_number_of_childs_per_scheduler_group()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("set maximum number of childs per SG");

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_CHILDS_PER_SCHEDULER_GROUP;
    attr.value.u32 = 8;

    return set(SAI_OBJECT_TYPE_SWITCH, m_switch_id, &attr);
}

sai_status_t SwitchMLNX2700::refresh_bridge_port_list(
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

    /*
     * First get all port's that belong to this bridge id.
     */

    std::map<sai_object_id_t, SwitchState::AttrHash> bridge_port_list_on_bridge_id;

    for (const auto &bp: all_bridge_ports)
    {
        auto it = bp.second.find(m_bridge_id->attridname);

        if (it == bp.second.end())
        {
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

    /* default 1q router at the end */

    bridge_port_list.push_back(m_default_bridge_port_1q_router);

    /*
        SAI_BRIDGE_PORT_ATTR_BRIDGE_ID: oid:0x100100000039
        SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE: SAI_BRIDGE_PORT_FDB_LEARNING_MODE_HW
        SAI_BRIDGE_PORT_ATTR_PORT_ID: oid:0x1010000000001
        SAI_BRIDGE_PORT_ATTR_TYPE: SAI_BRIDGE_PORT_TYPE_PORT
    */

    uint32_t bridge_port_list_count = (uint32_t)bridge_port_list.size();

    SWSS_LOG_NOTICE("recalculated %s: %u", me_port_list->attridname, bridge_port_list_count);

    attr.id = SAI_BRIDGE_ATTR_PORT_LIST;
    attr.value.objlist.count = bridge_port_list_count;
    attr.value.objlist.list = bridge_port_list.data();

    return set(SAI_OBJECT_TYPE_BRIDGE, bridge_id, &attr);
}

