#include "sai_vs.h"
#include "sai_vs_state.h"

sai_object_id_t switch_object_id;

#define CHECK_STATUS(status) \
{\
    sai_status_t s = (status);\
    if (s != SAI_STATUS_SUCCESS) \
        return s;\
}

#define DEFAULT_VLAN_NUMBER 1

// TODO extra work may be needed on GET api if N on list will be > then actual

sai_status_t create_default_switch()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create internal switch object");

    return vs_generic_create(
            SAI_OBJECT_TYPE_SWITCH,
            &switch_object_id,
            0,
            NULL);
}

sai_status_t set_switch_mac_address()
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

    return vs_generic_set_switch(&attr);
}

sai_status_t create_default_vlan()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create default vlan");

    return vs_generic_create_vlan(DEFAULT_VLAN_NUMBER);
}

sai_status_t create_cpu_port()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create cpu port");

    sai_object_id_t cpu_port_id;

    sai_attribute_t attr;

    CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_PORT, &cpu_port_id, 0, &attr));

    // populate cpu port object on switch
    attr.id = SAI_SWITCH_ATTR_CPU_PORT;
    attr.value.oid = cpu_port_id;

    CHECK_STATUS(vs_generic_set_switch(&attr));

    // set type on cpu
    attr.id = SAI_PORT_ATTR_TYPE;
    attr.value.s32 = SAI_PORT_TYPE_CPU;

    return vs_generic_set(SAI_OBJECT_TYPE_PORT, cpu_port_id, &attr);
}

sai_status_t create_ports(std::vector<sai_object_id_t>& port_list)
{
    SWSS_LOG_ENTER();

    // TODO currently port information is hardcoded
    // but later on we can read this from config file

    const uint32_t port_count = 32;

    SWSS_LOG_INFO("create ports");

    sai_uint32_t lanes[] = {
        29,30,31,32,
        25,26,27,28,
        37,38,39,40,
        33,34,35,36,
        41,42,43,44,
        45,46,47,48,
        5,6,7,8,
        1,2,3,4,
        9,10,11,12,
        13,14,15,16,
        21,22,23,24,
        17,18,19,20,
        49,50,51,52,
        53,54,55,56,
        61,62,63,64,
        57,58,59,60,
        65,66,67,68,
        69,70,71,72,
        77,78,79,80,
        73,74,75,76,
        105,106,107,108,
        109,110,111,112,
        117,118,119,120,
        113,114,115,116,
        121,122,123,124,
        125,126,127,128,
        85,86,87,88,
        81,82,83,84,
        89,90,91,92,
        93,94,95,96,
        97,98,99,100,
        101,102,103,104
    };

    port_list.clear();

    for (uint32_t i = 0; i < port_count; i++)
    {
        SWSS_LOG_DEBUG("create port index %u", i);

        sai_object_id_t port_id;

        CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_PORT, &port_id, 0, NULL));

        port_list.push_back(port_id);

        sai_attribute_t attr;

        attr.id = SAI_PORT_ATTR_SPEED;
        attr.value.u32 = 10 * 1000;

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
        attr.value.u32list.count = 4;
        attr.value.u32list.list = &lanes[4 * i];

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_TYPE;
        attr.value.s32 = SAI_PORT_TYPE_LOGICAL;

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        // TODO populate other port attributes
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t create_port_list(std::vector<sai_object_id_t>& port_list)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create port list");

    // TODO this is static, when we start to "create/remove" ports
    // we need to update this list since it's dynamic

    sai_attribute_t attr;

    uint32_t port_count = (uint32_t)port_list.size();

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = port_count;
    attr.value.objlist.list = port_list.data();

    CHECK_STATUS(vs_generic_set_switch(&attr));

    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
    attr.value.u32 = port_count;

    return vs_generic_set_switch(&attr);
}

sai_status_t create_default_virtual_router()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create default virtual router");

    sai_object_id_t virtual_router_id;

    CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &virtual_router_id, 0, NULL));

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;
    attr.value.oid = virtual_router_id;

    return vs_generic_set_switch(&attr);
}

sai_status_t create_vlan_members_for_default_vlan(
        std::vector<sai_object_id_t>& port_list,
        std::vector<sai_object_id_t>& vlan_member_list)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create vlan members for all ports");

    vlan_member_list.clear();

    vlan_members_map[DEFAULT_VLAN_NUMBER] = {};

    for (auto &port_id : port_list)
    {
        sai_object_id_t vlan_member_id;

        std::vector<sai_attribute_t> attrs;

        sai_attribute_t attr_vlan_id;

        attr_vlan_id.id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
        attr_vlan_id.value.u16 = DEFAULT_VLAN_NUMBER;

        attrs.push_back(attr_vlan_id);

        sai_attribute_t attr_port_id;

        attr_port_id.id = SAI_VLAN_MEMBER_ATTR_PORT_ID;
        attr_port_id.value.oid = port_id;

        attrs.push_back(attr_port_id);

        CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_VLAN_MEMBER, &vlan_member_id, (uint32_t)attrs.size(), attrs.data()));

        vlan_member_list.push_back(vlan_member_id);

        vlan_members_map[DEFAULT_VLAN_NUMBER].insert(vlan_member_id);
    }

    update_vlan_member_list_on_vlan(DEFAULT_VLAN_NUMBER);

    return SAI_STATUS_SUCCESS;
}

/*
sai_status_t create_vlan_members_list_for_default_vlan(std::vector<sai_object_id_t>& vlan_member_list)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create vlan members list for default vlan");

    // TODO this list must be modified when we add/remove vlan member

    sai_attribute_t attr;

    uint32_t members_count = (uint32_t)vlan_member_list.size();

    attr.id = SAI_VLAN_ATTR_MEMBER_LIST;
    attr.value.objlist.count = members_count;
    attr.value.objlist.list = vlan_member_list.data();

    return vs_generic_set_vlan(DEFAULT_VLAN_NUMBER, &attr);
}
*/
/*
sai_status_t creae_vlan_member_list(std::vector<sai_object_id_t>& vlan_member_list)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create vlan member list");

    // TODO this list must be updated every time vlan member is created/removed

    sai_attribute_t attr;

    uint32_t vlan_member_list_count = (uint32_t)vlan_member_list.size();

    attr.id = SAI_VLAN_ATTR_MEMBER_LIST;
    attr.value.objlist.count = vlan_member_list_count;
    attr.value.objlist.list = vlan_member_list.data();

    return vs_generic_set_vlan(DEFAULT_VLAN_NUMBER, &attr);
}
*/

sai_status_t create_default_trap_group()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("create default trap group");

    sai_object_id_t trap_group_id;

    CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_TRAP_GROUP, &trap_group_id, 0, NULL));

    sai_attribute_t attr;

    // populate trap group on switch
    attr.id = SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP;
    attr.value.oid = trap_group_id;

    return vs_generic_set_switch(&attr);
}

sai_status_t create_qos_queues(std::vector<sai_object_id_t>& port_list)
{
    SWSS_LOG_ENTER();

    // TODO queues size may change when we will modify queue or ports

    SWSS_LOG_INFO("create qos queues");

    // 10 in and 10 out queues per port
    const uint32_t port_qos_queues_count = 20;

    for (auto &port_id : port_list)
    {
        std::vector<sai_object_id_t> queues;

        for (uint32_t i = 0; i < port_qos_queues_count; ++i)
        {
            sai_object_id_t queue_id;

            CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_QUEUE, &queue_id, 0, NULL));

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

sai_status_t create_priority_groups(std::vector<sai_object_id_t>& port_list)
{
    SWSS_LOG_ENTER();

    // TODO prioirity groups size may change when we will modify pg or ports

    SWSS_LOG_INFO("create priority groups");

    //
    const uint32_t port_pgs_count = 8;

    for (auto &port_id : port_list)
    {
        std::vector<sai_object_id_t> pgs;

        for (uint32_t i = 0; i < port_pgs_count; ++i)
        {
            sai_object_id_t pg_id;

            CHECK_STATUS(vs_generic_create(SAI_OBJECT_TYPE_PRIORITY_GROUP, &pg_id, 0, NULL));

            pgs.push_back(pg_id);
        }

        sai_attribute_t attr;

        attr.id = SAI_PORT_ATTR_NUMBER_OF_PRIORITY_GROUPS;
        attr.value.u32 = port_pgs_count;

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));

        attr.id = SAI_PORT_ATTR_PRIORITY_GROUP_LIST;
        attr.value.objlist.count = port_pgs_count;
        attr.value.objlist.list = pgs.data();

        CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr));
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t create_hostif_traps()
{
    SWSS_LOG_ENTER();

    std::vector<sai_hostif_trap_id_t> traps = {
        SAI_HOSTIF_TRAP_ID_STP,
        SAI_HOSTIF_TRAP_ID_LACP,
        SAI_HOSTIF_TRAP_ID_EAPOL,
        SAI_HOSTIF_TRAP_ID_LLDP,
        SAI_HOSTIF_TRAP_ID_PVRST,
        SAI_HOSTIF_TRAP_ID_IGMP_TYPE_QUERY,
        SAI_HOSTIF_TRAP_ID_IGMP_TYPE_LEAVE,
        SAI_HOSTIF_TRAP_ID_IGMP_TYPE_V1_REPORT,
        SAI_HOSTIF_TRAP_ID_IGMP_TYPE_V2_REPORT,
        SAI_HOSTIF_TRAP_ID_IGMP_TYPE_V3_REPORT,
        SAI_HOSTIF_TRAP_ID_SAMPLEPACKET,
        SAI_HOSTIF_TRAP_ID_ARP_REQUEST,
        SAI_HOSTIF_TRAP_ID_ARP_RESPONSE,
        SAI_HOSTIF_TRAP_ID_DHCP,
        SAI_HOSTIF_TRAP_ID_OSPF,
        SAI_HOSTIF_TRAP_ID_PIM,
        SAI_HOSTIF_TRAP_ID_VRRP,
        SAI_HOSTIF_TRAP_ID_BGP,
        SAI_HOSTIF_TRAP_ID_DHCPV6,
        SAI_HOSTIF_TRAP_ID_OSPFV6,
        SAI_HOSTIF_TRAP_ID_VRRPV6,
        SAI_HOSTIF_TRAP_ID_BGPV6,
        SAI_HOSTIF_TRAP_ID_IPV6_NEIGHBOR_DISCOVERY,
        SAI_HOSTIF_TRAP_ID_IPV6_MLD_V1_V2,
        SAI_HOSTIF_TRAP_ID_IPV6_MLD_V1_REPORT,
        SAI_HOSTIF_TRAP_ID_IPV6_MLD_V1_DONE,
        SAI_HOSTIF_TRAP_ID_MLD_V2_REPORT,
        SAI_HOSTIF_TRAP_ID_IP2ME,
        SAI_HOSTIF_TRAP_ID_SSH,
        SAI_HOSTIF_TRAP_ID_SNMP,
        SAI_HOSTIF_TRAP_ID_L3_MTU_ERROR,
        SAI_HOSTIF_TRAP_ID_TTL_ERROR
    };

    for (auto trap: traps)
    {
        CHECK_STATUS(vs_generic_create_trap(trap, 0, NULL));
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t initialize_default_objects()
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(create_default_switch());
    CHECK_STATUS(set_switch_mac_address());
    CHECK_STATUS(create_default_vlan());
    CHECK_STATUS(create_cpu_port());

    std::vector<sai_object_id_t> port_list;

    CHECK_STATUS(create_ports(port_list));
    CHECK_STATUS(create_port_list(port_list));

    CHECK_STATUS(create_default_virtual_router());

    std::vector<sai_object_id_t> vlan_member_list;

    CHECK_STATUS(create_vlan_members_for_default_vlan(port_list, vlan_member_list));
 //   CHECK_STATUS(create_vlan_members_list_for_default_vlan(vlan_member_list));

    CHECK_STATUS(create_default_trap_group());

    CHECK_STATUS(create_qos_queues(port_list));
    CHECK_STATUS(create_priority_groups(port_list));

    CHECK_STATUS(create_hostif_traps());

    return SAI_STATUS_SUCCESS;
}

sai_status_t init_switch()
{
    sai_status_t status = meta_init_db();

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to init meta db FIXME");

        return status;
    }

    reset_id_counter();

    // remove all objects

    g_objectHash.clear();

    return initialize_default_objects();
}
