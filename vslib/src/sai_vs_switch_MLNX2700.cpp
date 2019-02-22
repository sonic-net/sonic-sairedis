#include "sai_vs.h"
#include "sai_vs_state.h"

/*
 * We can use local variable here for initialization (init should be in class
 * constructor anyway, we can move it there later) because each switch init is
 * done under global lock.
 */

static std::shared_ptr<SwitchState> ss;

static std::vector<sai_object_id_t> port_list;
static std::vector<sai_object_id_t> bridge_port_list_port_based;

static sai_object_id_t default_vlan_id;
static sai_object_id_t default_bridge_port_1q_router;
static sai_object_id_t cpu_port_id;

static sai_status_t set_switch_mac_address()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create switch src mac address");

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;

    attr.value.mac[0] = 0x11;
    attr.value.mac[1] = 0x22;
    attr.value.mac[2] = 0x33;
    attr.value.mac[3] = 0x44;
    attr.value.mac[4] = 0x55;
    attr.value.mac[5] = 0x66;

    return vs_generic_set(SAI_OBJECT_TYPE_SWITCH, ss->getSwitchId(), &attr);
}

static sai_status_t set_switch_default_attributes()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create switch default attributes");

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;
    attr.value.ptr = NULL;

    CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_SWITCH, ss->getSwitchId(), &attr));

    attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;

    CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_SWITCH, ss->getSwitchId(), &attr));

    attr.id = SAI_SWITCH_ATTR_FDB_AGING_TIME;
    attr.value.u32 = 0;

    CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_SWITCH, ss->getSwitchId(), &attr));

    attr.id = SAI_SWITCH_ATTR_RESTART_WARM;
    attr.value.booldata = false;

    CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_SWITCH, ss->getSwitchId(), &attr));

    attr.id = SAI_SWITCH_ATTR_WARM_RECOVER;
    attr.value.booldata = false;

    return vs_generic_set(SAI_OBJECT_TYPE_SWITCH, ss->getSwitchId(), &attr);
}

static sai_status_t create_default_vlan()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create default vlan");

    sai_attribute_t attr;

    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = DEFAULT_VLAN_NUMBER;

    CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_VLAN, &default_vlan_id, ss->getSwitchId(), 1, &attr));

    /* set default vlan on switch */

    attr.id = SAI_SWITCH_ATTR_DEFAULT_VLAN_ID;
    attr.value.oid = default_vlan_id;

    return vs_generic_set(SAI_OBJECT_TYPE_SWITCH, ss->getSwitchId(), &attr);
}

static sai_status_t create_cpu_port()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create cpu port");

    sai_attribute_t attr;

    sai_object_id_t switch_id = ss->getSwitchId();

    CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_PORT, &cpu_port_id, switch_id, 0, &attr));

    // populate cpu port object on switch
    attr.id = SAI_SWITCH_ATTR_CPU_PORT;
    attr.value.oid = cpu_port_id;

    CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr));

    // set type on cpu
    attr.id = SAI_PORT_ATTR_TYPE;
    attr.value.s32 = SAI_PORT_TYPE_CPU;

    return vs_generic_set(SAI_OBJECT_TYPE_PORT, cpu_port_id, &attr);
}

static sai_status_t create_default_1q_bridge()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create default 1q bridge");

    sai_attribute_t attr;

    attr.id = SAI_BRIDGE_ATTR_TYPE;
    attr.value.s32 = SAI_BRIDGE_TYPE_1Q;

    sai_object_id_t switch_id = ss->getSwitchId();

    sai_object_id_t default_1q_bridge;

    CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_BRIDGE, &default_1q_bridge, switch_id, 1, &attr));

    attr.id = SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID;
    attr.value.oid = default_1q_bridge;

    return vs_generic_set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
}

static sai_status_t create_ports()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create ports");

    std::vector<std::vector<uint32_t>> laneMap;

    getPortLaneMap(laneMap);

    uint32_t port_count = (uint32_t)laneMap.size();

    port_list.clear();

    sai_object_id_t switch_id = ss->getSwitchId();

    for (uint32_t i = 0; i < port_count; i++)
    {
        SWSS_LOG_DEBUG("create port index %u", i);

        sai_object_id_t port_id;

        CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_PORT, &port_id, switch_id, 0, NULL));

        port_list.push_back(port_id);

        sai_attribute_t attr;

        attr.id = SAI_PORT_ATTR_ADMIN_STATE;
        attr.value.booldata = false;     /* default admin state is down as defined in SAI */

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_MTU;
        attr.value.u32 = 1514;     /* default MTU is 1514 as defined in SAI */

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_SPEED;
        attr.value.u32 = 40 * 1000;     /* TODO from config */

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        std::vector<uint32_t> lanes = laneMap.at(i);

        attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
        attr.value.u32list.count = (uint32_t)lanes.size();
        attr.value.u32list.list = lanes.data();

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_TYPE;
        attr.value.s32 = SAI_PORT_TYPE_LOGICAL;

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_OPER_STATUS;
        attr.value.s32 = SAI_PORT_OPER_STATUS_DOWN;

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_PORT_VLAN_ID;
        attr.value.u32 = DEFAULT_VLAN_NUMBER;

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));
    }

    return SAI_STATUS_SUCCESS;
}

static sai_status_t create_port_list()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create port list");

    /*
     * TODO this is static, when we start to "create/remove" ports we need to
     * update this list since it can change depends on profile.ini
     */

    sai_attribute_t attr;

    sai_object_id_t switch_id = ss->getSwitchId();

    uint32_t port_count = (uint32_t)port_list.size();

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = port_count;
    attr.value.objlist.list = port_list.data();

    CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr));

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    attr.value.u32 = port_count;

    return vs_generic_set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
}

static sai_status_t create_default_virtual_router()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create default virtual router");

    sai_object_id_t switch_id = ss->getSwitchId();

    sai_object_id_t virtual_router_id;

    CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &virtual_router_id, switch_id, 0, NULL));

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;
    attr.value.oid = virtual_router_id;

    return vs_generic_set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
}

static sai_status_t create_default_stp_instance()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create default stp instance");

    sai_object_id_t stp_instance_id;

    sai_object_id_t switch_id = ss->getSwitchId();

    CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_STP, &stp_instance_id, switch_id, 0, NULL));

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_DEFAULT_STP_INST_ID;
    attr.value.oid = stp_instance_id;

    return vs_generic_set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
}

/*
static sai_status_t create_vlan_members_for_default_vlan(
        std::vector<sai_object_id_t>& bridge_port_list,
        std::vector<sai_object_id_t>& vlan_member_list)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create vlan members for all ports");

    sai_object_id_t switch_id = ss->getSwitchId();

    vlan_member_list.clear();

    ss->vlan_members_map[ss->default_vlan_id] = {};

    for (auto &bridge_port_id : bridge_port_list)
    {
        sai_object_id_t vlan_member_id;

        std::vector<sai_attribute_t> attrs;

        sai_attribute_t attr_vlan_id;

        attr_vlan_id.id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
        attr_vlan_id.value.u16 = DEFAULT_VLAN_NUMBER;

        attrs.push_back(attr_vlan_id);

        sai_attribute_t attr_port_id;

        // TODO first we need to create bridge and bridge ports ?

        attr_port_id.id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
        attr_port_id.value.oid = bridge_port_id;

        attrs.push_back(attr_port_id);

        CHECK_STATUS(vs_generic_create(
                    SAI_OBJECT_TYPE_VLAN_MEMBER,
                    &vlan_member_id,
                    switch_id,
                    (uint32_t)attrs.size(),
                    attrs.data()));

        vlan_member_list.push_back(vlan_member_id);

        ss->vlan_members_map[ss->default_vlan_id].insert(vlan_member_id);
    }

    return SAI_STATUS_SUCCESS;
}
*/

static sai_status_t create_default_trap_group()
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_id = ss->getSwitchId();

    SWSS_LOG_INFO("create default trap group");

    sai_object_id_t trap_group_id;

    CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP, &trap_group_id, switch_id, 0, NULL));

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP;
    attr.value.oid = trap_group_id;

    return vs_generic_set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);
}

static sai_status_t create_qos_queues()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create qos queues");

    sai_object_id_t switch_id = ss->getSwitchId();

    // 8 in and 8 out queues per port
    const uint32_t port_qos_queues_count = 16;

    std::vector<sai_object_id_t> copy = port_list;

    copy.push_back(cpu_port_id);

    for (auto &port_id : copy)
    {
        std::vector<sai_object_id_t> queues;

        for (uint32_t i = 0; i < port_qos_queues_count; ++i)
        {
            sai_object_id_t queue_id;

            sai_attribute_t attr[2];

            attr[0].id = SAI_QUEUE_ATTR_INDEX;
            attr[0].value.u8 = (uint8_t)i;

            attr[1].id = SAI_QUEUE_ATTR_PORT;
            attr[1].value.oid = port_id;

            CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_QUEUE, &queue_id, switch_id, 2, attr));

            queues.push_back(queue_id);
        }

        sai_attribute_t attr;

        attr.id = SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES;
        attr.value.u32 = port_qos_queues_count;

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
        attr.value.objlist.count = port_qos_queues_count;
        attr.value.objlist.list = queues.data();

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));
    }

    return SAI_STATUS_SUCCESS;
}

static sai_status_t create_ingress_priority_groups()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create ingress priority groups");

    sai_object_id_t switch_id = ss->getSwitchId();

    const uint32_t port_pgs_count = 8;

    for (auto &port_id : port_list)
    {
        std::vector<sai_object_id_t> pgs;

        for (uint32_t i = 0; i < port_pgs_count; ++i)
        {
            sai_object_id_t pg_id;

            sai_attribute_t attr[3];

            attr[0].id = SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE;
            attr[0].value.oid = SAI_NULL_OBJECT_ID;

            /*
             * not in headers yet
             *
             * attr[1].id = SAI_INGRESS_PRIORITY_GROUP_ATTR_PORT;
             * attr[1].value.oid = port_id;

             * attr[2].id = SAI_INGRESS_PRIORITY_GROUP_ATTR_INDEX;
             * attr[2].value.oid = i;

             * fix number of attributes
             */

            CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP, &pg_id, switch_id, 1, attr));

            pgs.push_back(pg_id);
        }

        sai_attribute_t attr;

        attr.id = SAI_PORT_ATTR_NUMBER_OF_INGRESS_PRIORITY_GROUPS;
        attr.value.u32 = port_pgs_count;

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST;
        attr.value.objlist.count = port_pgs_count;
        attr.value.objlist.list = pgs.data();

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));
    }

    return SAI_STATUS_SUCCESS;
}

static sai_status_t create_scheduler_group_tree(
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

    CHECK_STATUS(vs_generic_get(SAI_OBJECT_TYPE_PORT, port_id, 1, &attrq));

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
        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_0, &attr));


        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT;
        attr.value.u32 = 8;

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_0, &attr));

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

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg_0, &attr));
    }

    for (int i = 1; i < 8; ++i)
    {
        // 1..7 schedulers are empty

        sai_object_id_t sg = sgs.at(i);

        sai_attribute_t attr;

        attr.id = SAI_SCHEDULER_GROUP_ATTR_PORT_ID;
        attr.value.oid = port_id;
        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg, &attr));

        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT;
        attr.value.u32 = 0;

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg, &attr));

        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST;
        attr.value.objlist.count = 0;
        attr.value.objlist.list = NULL;

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg, &attr));
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

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg, &attr));

        attr.id = SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT;
        attr.value.u32 = 2;

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_SCHEDULER_GROUP, sg, &attr));
    }


    return SAI_STATUS_SUCCESS;
}

static sai_status_t create_scheduler_groups()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create scheduler groups");

    uint32_t port_sgs_count = 16; // mlnx default

    for (const auto &port_id : port_list)
    {
        sai_attribute_t attr;

        attr.id = SAI_PORT_ATTR_QOS_NUMBER_OF_SCHEDULER_GROUPS;
        attr.value.u32 = port_sgs_count;

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        // scheduler groups per port

        std::vector<sai_object_id_t> sgs;

        for (uint32_t i = 0; i < port_sgs_count; ++i)
        {
            sai_object_id_t sg_id;

            CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_SCHEDULER_GROUP, &sg_id, ss->getSwitchId(), 0, NULL));

            sgs.push_back(sg_id);
        }

        attr.id = SAI_PORT_ATTR_QOS_SCHEDULER_GROUP_LIST;
        attr.value.objlist.count = port_sgs_count;
        attr.value.objlist.list = sgs.data();

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        CHECK_STATUS(create_scheduler_group_tree(sgs, port_id));
    }

    // SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT // sched_groups + count
    // scheduler group are organized in tree and on the bottom there are queues
    // order matters in returning api
    return SAI_STATUS_SUCCESS;
}

static sai_status_t set_maximum_number_of_childs_per_scheduler_group()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("set maximum number of childs per SG");

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_CHILDS_PER_SCHEDULER_GROUP;
    attr.value.u32 = 8;

    return vs_generic_set(SAI_OBJECT_TYPE_SWITCH, ss->getSwitchId(), &attr);
}

static sai_status_t create_bridge_ports()
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_id = ss->getSwitchId();

    /*
     * Create bridge port for 1q router.
     */

    sai_attribute_t attr;

    attr.id = SAI_BRIDGE_PORT_ATTR_TYPE;
    attr.value.s32 = SAI_BRIDGE_PORT_TYPE_1Q_ROUTER;

    CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_BRIDGE_PORT, &default_bridge_port_1q_router, ss->getSwitchId(), 1, &attr));

    attr.id = SAI_BRIDGE_PORT_ATTR_PORT_ID;
    attr.value.oid = SAI_NULL_OBJECT_ID;

    CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_BRIDGE_PORT, default_bridge_port_1q_router, &attr));

    attr.id = SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID;

    CHECK_STATUS(vs_generic_get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr));

    /*
     * Create bridge ports for regular ports.
     */

    sai_object_id_t default_1q_bridge_id = attr.value.oid;

    bridge_port_list_port_based.clear();

    for (const auto &port_id: port_list)
    {
        SWSS_LOG_DEBUG("create bridge port for port %s", sai_serialize_object_id(port_id).c_str());

        sai_attribute_t attrs[4];

        attrs[0].id = SAI_BRIDGE_PORT_ATTR_BRIDGE_ID;
        attrs[0].value.oid = default_1q_bridge_id;

        attrs[1].id = SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE;
        attrs[1].value.s32 = SAI_BRIDGE_PORT_FDB_LEARNING_MODE_HW;

        attrs[2].id = SAI_BRIDGE_PORT_ATTR_PORT_ID;
        attrs[2].value.oid = port_id;

        attrs[3].id = SAI_BRIDGE_PORT_ATTR_TYPE;
        attrs[3].value.s32 = SAI_BRIDGE_PORT_TYPE_PORT;

        sai_object_id_t bridge_port_id;

        CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_BRIDGE_PORT, &bridge_port_id, switch_id, 4, attrs));

        bridge_port_list_port_based.push_back(bridge_port_id);
    }

    return SAI_STATUS_SUCCESS;
}

static sai_status_t create_vlan_members()
{
    SWSS_LOG_ENTER();

    sai_object_id_t switch_id = ss->getSwitchId();

    /*
     * Crete vlan members for bridge ports.
     */

    for (auto bridge_port_id: bridge_port_list_port_based)
    {
        SWSS_LOG_DEBUG("create vlan member for bridge port %s",
                sai_serialize_object_id(bridge_port_id).c_str());

        sai_attribute_t attrs[3];

        attrs[0].id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
        attrs[0].value.oid = bridge_port_id;

        attrs[1].id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
        attrs[1].value.oid = default_vlan_id;

        attrs[2].id = SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE;
        attrs[2].value.s32 = SAI_VLAN_TAGGING_MODE_UNTAGGED;

        sai_object_id_t vlan_member_id;

        CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_VLAN_MEMBER, &vlan_member_id, switch_id, 3, attrs));
    }

    return SAI_STATUS_SUCCESS;
}

static sai_status_t create_acl_entry_min_prio()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create acl entry min prio");

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_ACL_ENTRY_MINIMUM_PRIORITY;
    attr.value.u32 = 1;

    CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_SWITCH, ss->getSwitchId(), &attr));

    attr.id = SAI_SWITCH_ATTR_ACL_ENTRY_MAXIMUM_PRIORITY;
    attr.value.u32 = 16000;

    return vs_generic_set(SAI_OBJECT_TYPE_SWITCH, ss->getSwitchId(), &attr);
}

static sai_status_t initialize_default_objects()
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
    CHECK_STATUS(create_port_list());
    CHECK_STATUS(create_bridge_ports());
    CHECK_STATUS(create_vlan_members());
    CHECK_STATUS(create_acl_entry_min_prio());
    CHECK_STATUS(create_ingress_priority_groups());
    CHECK_STATUS(create_qos_queues());
    CHECK_STATUS(set_maximum_number_of_childs_per_scheduler_group());
    CHECK_STATUS(set_switch_default_attributes());
    CHECK_STATUS(create_scheduler_groups());

    return SAI_STATUS_SUCCESS;
}

static sai_status_t warm_boot_initialize_objects()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("warm boot init objects");

    /*
     * We need to bring back previous state in case user will get some read
     * only attributes and recalculation will need to be done.
     *
     * We need to refresh:
     * - ports
     * - default bridge port 1q router
     */

    sai_object_id_t switch_id = ss->getSwitchId();

    port_list.resize(SAI_VS_MAX_PORTS);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;

    attr.value.objlist.count = SAI_VS_MAX_PORTS;
    attr.value.objlist.list = port_list.data();

    CHECK_STATUS(vs_generic_get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr));

    port_list.resize(attr.value.objlist.count);

    SWSS_LOG_NOTICE("port list size: %zu", port_list.size());

    attr.id = SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID;

    CHECK_STATUS(vs_generic_get(SAI_OBJECT_TYPE_SWITCH, switch_id, 1, &attr));

    default_bridge_port_1q_router = attr.value.oid;

    SWSS_LOG_NOTICE("default bridge port 1q router: %s",
            sai_serialize_object_id(default_bridge_port_1q_router).c_str());

    return SAI_STATUS_SUCCESS;
}

void init_switch_MLNX2700(
        _In_ sai_object_id_t switch_id,
        _In_ std::shared_ptr<SwitchState> warmBootState)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_TIMER("init");

    if (switch_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_THROW("init switch with NULL switch id is not allowed");
    }

    if (warmBootState != nullptr)
    {
        ss = g_switch_state_map[switch_id] = warmBootState;

        warm_boot_initialize_objects();

        SWSS_LOG_NOTICE("initialized switch %s in WARM boot mode", sai_serialize_object_id(switch_id).c_str());

        return;
    }

    if (g_switch_state_map.find(switch_id) != g_switch_state_map.end())
    {
        SWSS_LOG_THROW("switch already exists %s", sai_serialize_object_id(switch_id).c_str());
    }

    ss = g_switch_state_map[switch_id] = std::make_shared<SwitchState>(switch_id);

    sai_status_t status = initialize_default_objects();

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_THROW("unable to init switch %s", sai_serialize_status(status).c_str());
    }

    SWSS_LOG_NOTICE("initialized switch %s", sai_serialize_object_id(switch_id).c_str());
}

void uninit_switch_MLNX2700(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    if (g_switch_state_map.find(switch_id) == g_switch_state_map.end())
    {
        SWSS_LOG_THROW("switch doesn't exist 0x%lx", switch_id);
    }

    SWSS_LOG_NOTICE("remove switch 0x%lx", switch_id);

    g_switch_state_map.erase(switch_id);
}

/*
 * TODO develop a way to filter by oid attribute.
 */

static sai_status_t refresh_bridge_port_list(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t bridge_id,
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    /*
     * TODO possible issues with vxlan and lag.
     */

    auto &all_bridge_ports = g_switch_state_map.at(switch_id)->objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT);

    sai_attribute_t attr;

    auto m_port_list = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE, SAI_BRIDGE_ATTR_PORT_LIST);
    auto m_port_id = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_PORT_ID);
    auto m_bridge_id = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_BRIDGE_PORT, SAI_BRIDGE_PORT_ATTR_BRIDGE_ID);

    /*
     * First get all port's that belong to this bridge id.
     */

    std::map<sai_object_id_t, AttrHash> bridge_port_list_on_bridge_id;

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

    for (const auto &p: port_list)
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

    bridge_port_list.push_back(default_bridge_port_1q_router);

    /*
        SAI_BRIDGE_PORT_ATTR_BRIDGE_ID: oid:0x100100000039
        SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE: SAI_BRIDGE_PORT_FDB_LEARNING_MODE_HW
        SAI_BRIDGE_PORT_ATTR_PORT_ID: oid:0x1010000000001
        SAI_BRIDGE_PORT_ATTR_TYPE: SAI_BRIDGE_PORT_TYPE_PORT
    */

    uint32_t bridge_port_list_count = (uint32_t)bridge_port_list.size();

    SWSS_LOG_NOTICE("recalculated %s: %u", m_port_list->attridname, bridge_port_list_count);

    attr.id = SAI_BRIDGE_ATTR_PORT_LIST;
    attr.value.objlist.count = bridge_port_list_count;
    attr.value.objlist.list = bridge_port_list.data();

    return vs_generic_set(SAI_OBJECT_TYPE_BRIDGE, bridge_id, &attr);
}

static sai_status_t refresh_vlan_member_list(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t vlan_id,
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    auto &all_vlan_members = g_switch_state_map.at(switch_id)->objectHash.at(SAI_OBJECT_TYPE_VLAN_MEMBER);

    auto m_member_list = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_VLAN, SAI_VLAN_ATTR_MEMBER_LIST);
    auto md_vlan_id = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_VLAN_MEMBER, SAI_VLAN_MEMBER_ATTR_VLAN_ID);
    //auto md_brportid = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_VLAN_MEMBER, SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID);

    std::vector<sai_object_id_t> vlan_member_list;

    /*
     * We want order as bridge port order (so port order)
     */

    sai_attribute_t attr;

    auto me = g_switch_state_map.at(switch_id)->objectHash.at(SAI_OBJECT_TYPE_VLAN).at(sai_serialize_object_id(vlan_id));

    for (auto vm: all_vlan_members)
    {
        if (vm.second.at(md_vlan_id->attridname)->getAttr()->value.oid != vlan_id)
        {
            /*
             * Only interested in our vlan
             */

            continue;
        }

        // TODO we need order as bridge ports, but we need bridge id!

        {
            sai_object_id_t vlan_member_id;

            sai_deserialize_object_id(vm.first, vlan_member_id);

            vlan_member_list.push_back(vlan_member_id);
        }
    }

    uint32_t vlan_member_list_count = (uint32_t)vlan_member_list.size();

    SWSS_LOG_NOTICE("recalculated %s: %u", m_member_list->attridname, vlan_member_list_count);

    attr.id = SAI_VLAN_ATTR_MEMBER_LIST;
    attr.value.objlist.count = vlan_member_list_count;
    attr.value.objlist.list = vlan_member_list.data();

    return vs_generic_set(SAI_OBJECT_TYPE_VLAN, vlan_id, &attr);
}

static sai_status_t refresh_ingress_priority_group(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t port_id,
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    /*
     * TODO Currently we don't have index in groups, so we don't know how to
     * sort.  Returning success, since assuming that we will not create more
     * ingress priority groups.
     */

    return SAI_STATUS_SUCCESS;
}

static sai_status_t refresh_qos_queues(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t port_id,
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    /*
     * TODO Currently we don't have index in groups, so we don't know how to
     * sort.  Returning success, since assuming that we will not create more
     * ingress priority groups.
     */

    return SAI_STATUS_SUCCESS;
}

static sai_status_t refresh_scheduler_groups(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t port_id,
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    /*
     * TODO Currently we don't have index in groups, so we don't know how to
     * sort.  Returning success, since assuming that we will not create more
     * ingress priority groups.
     */

    return SAI_STATUS_SUCCESS;
}

/*
 * NOTE For recalculation we can add flag on create/remove specific object type
 * so we can deduce whether actually need to perform recalculation, as
 * optimization.
 */

sai_status_t refresh_read_only_MLNX2700(
        _In_ const sai_attr_metadata_t *meta,
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    if (meta->objecttype == SAI_OBJECT_TYPE_SWITCH)
    {
        switch (meta->attrid)
        {
            case SAI_SWITCH_ATTR_PORT_NUMBER:
                return SAI_STATUS_SUCCESS;

            case SAI_SWITCH_ATTR_CPU_PORT:
            case SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID:
            case SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP:
            case SAI_SWITCH_ATTR_DEFAULT_VLAN_ID:
            case SAI_SWITCH_ATTR_DEFAULT_STP_INST_ID:
            case SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID:
                return SAI_STATUS_SUCCESS;

            case SAI_SWITCH_ATTR_ACL_ENTRY_MINIMUM_PRIORITY:
            case SAI_SWITCH_ATTR_ACL_ENTRY_MAXIMUM_PRIORITY:
                return SAI_STATUS_SUCCESS;

                /*
                 * We don't need to recalculate port list, since now we assume
                 * that port list will not change.
                 */

            case SAI_SWITCH_ATTR_PORT_LIST:
                return SAI_STATUS_SUCCESS;

            case SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_CHILDS_PER_SCHEDULER_GROUP:
                return SAI_STATUS_SUCCESS;
        }
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_PORT)
    {
        switch (meta->attrid)
        {
            case SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES:
            case SAI_PORT_ATTR_QOS_QUEUE_LIST:
                return refresh_qos_queues(meta, object_id, switch_id);

            case SAI_PORT_ATTR_NUMBER_OF_INGRESS_PRIORITY_GROUPS:
            case SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST:
                return refresh_ingress_priority_group(meta, object_id, switch_id);

            case SAI_PORT_ATTR_QOS_NUMBER_OF_SCHEDULER_GROUPS:
            case SAI_PORT_ATTR_QOS_SCHEDULER_GROUP_LIST:
                return refresh_scheduler_groups(meta, object_id, switch_id);

                /*
                 * This status is based on hostif vEthernetX status.
                 */

            case SAI_PORT_ATTR_OPER_STATUS:
                return SAI_STATUS_SUCCESS;
        }
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_SCHEDULER_GROUP)
    {
        switch (meta->attrid)
        {
            case SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT:
            case SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST:
                return refresh_scheduler_groups(meta, object_id, switch_id);
        }
    }


    if (meta->objecttype == SAI_OBJECT_TYPE_BRIDGE && meta->attrid == SAI_BRIDGE_ATTR_PORT_LIST)
    {
        return refresh_bridge_port_list(meta, object_id, switch_id);
    }

    if (meta->objecttype == SAI_OBJECT_TYPE_VLAN && meta->attrid == SAI_VLAN_ATTR_MEMBER_LIST)
    {
        return refresh_vlan_member_list(meta, object_id, switch_id);
    }

    if (meta_unittests_enabled())
    {
        SWSS_LOG_NOTICE("unittests enabled, SET could be performed on %s, not recalculating", meta->attridname);

        return SAI_STATUS_SUCCESS;
    }

    SWSS_LOG_WARN("need to recalculate RO: %s", meta->attridname);

    return SAI_STATUS_NOT_IMPLEMENTED;
}
