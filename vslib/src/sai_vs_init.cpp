#include "sai_vs.h"
#include "sai_vs_state.h"

sai_object_id_t switch_object_id = (sai_object_id_t)SAI_OBJECT_TYPE_SWITCH << 48;

#define CHECK_STATUS(status) \
    if (status != SAI_STATUS_SUCCESS) \
        return status;

#define DEFAULT_VLAN_NUMBER 1

sai_status_t initialize_default_objects()
{
    SWSS_LOG_ENTER();

    sai_status_t status;

    SWSS_LOG_INFO("create internal switch object");

    {
        sai_attribute_t attr;

        status = vs_generic_create(
                SAI_OBJECT_TYPE_SWITCH,
                &switch_object_id,
                0,
                &attr);

        CHECK_STATUS(status);
    }

    SWSS_LOG_INFO("create switch src mac address");

    {
        sai_attribute_t attr;

        attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;
        attr.value.mac[0] = 0x11;
        attr.value.mac[1] = 0x22;
        attr.value.mac[2] = 0x33;
        attr.value.mac[3] = 0x44;
        attr.value.mac[4] = 0x55;
        attr.value.mac[5] = 0x66;

        status = vs_generic_set_switch(&attr);

        CHECK_STATUS(status);
    }

    SWSS_LOG_INFO("create default vlan");

    {
        status = vs_generic_create_vlan(DEFAULT_VLAN_NUMBER);

        CHECK_STATUS(status);
    }

    SWSS_LOG_INFO("create cpu port");

    {
        sai_object_id_t cpu_port_id;

        sai_attribute_t attr;

        // currently we don't have attributes on cpu port
        status = vs_generic_create(SAI_OBJECT_TYPE_PORT, &cpu_port_id, 0, &attr);

        CHECK_STATUS(status);


        // populate cpu port object on switch

        attr.id = SAI_SWITCH_ATTR_CPU_PORT;
        attr.value.oid = cpu_port_id;

        status = vs_generic_set_switch(&attr);

        CHECK_STATUS(status);

        //attr.id = SAI_PORT_ATTR_TYPE;
        //attr.value.s32 = SAI_PORT_TYPE_CPU;

        //status = vs_generic_set(SAI_OBJECT_TYPE_PORT, cpu_port_id, &attr);

        //CHECK_STATUS(status);
    }


    // TODO currently port information is hardcoded
    // but later on we can read this from config file

    const uint32_t port_count = 32;

    std::vector<sai_object_id_t> port_list;

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

    // create ports
    {
        for (uint32_t i = 0; i < port_count; i++)
        {
            SWSS_LOG_INFO("create port index %u", i);

            sai_object_id_t port_id;

            sai_attribute_t attr;

            status = vs_generic_create(SAI_OBJECT_TYPE_PORT, &port_id, 0, &attr);

            CHECK_STATUS(status);

            port_list.push_back(port_id);

            // populate port object on switch

            attr.id = SAI_PORT_ATTR_SPEED;
            attr.value.u32 = 10 * 1000;

            status = vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr);

            CHECK_STATUS(status);

            // populate lanes for each port

            attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
            attr.value.u32list.count = 4;
            attr.value.u32list.list = &lanes[4 * i];

            status = vs_generic_set(SAI_OBJECT_TYPE_PORT, port_id, &attr);

            CHECK_STATUS(status);

            // TODO populate ports attributes
            // TODO populate vlan members
        }
    }

    SWSS_LOG_INFO("create port list");

    {
        sai_attribute_t attr;

        attr.id = SAI_SWITCH_ATTR_PORT_LIST;
        attr.value.objlist.count = port_count;
        attr.value.objlist.list = port_list.data();

        status = vs_generic_set_switch(&attr);

        CHECK_STATUS(status);

        attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;
        attr.value.u32 = port_count;

        status = vs_generic_set_switch(&attr);

        CHECK_STATUS(status);
    }

    SWSS_LOG_INFO("create default virtual router");

    {
        sai_object_id_t virtual_router_id;

        sai_attribute_t attr;

        status = vs_generic_create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, &virtual_router_id, 0, &attr);

        CHECK_STATUS(status);

        attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;
        attr.value.oid = virtual_router_id;

        status = vs_generic_set_switch(&attr);

        CHECK_STATUS(status);
    }

    SWSS_LOG_INFO("create vlan members for all ports");

    std::vector<sai_object_id_t> vlan_member_list;

    {
        for (auto &portId : port_list)
        {
            std::vector<sai_attribute_t> attrs;

            sai_attribute_t attr_vlan_id;

            attr_vlan_id.id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
            attr_vlan_id.value.u16 = DEFAULT_VLAN_NUMBER;
            attrs.push_back(attr_vlan_id);

            sai_attribute_t attr_port_id;

            attr_port_id.id = SAI_VLAN_MEMBER_ATTR_PORT_ID;
            attr_port_id.value.oid = portId;
            attrs.push_back(attr_port_id);

            sai_object_id_t vlan_member_id;
            status = vs_generic_create(SAI_OBJECT_TYPE_VLAN_MEMBER, &vlan_member_id, (uint32_t)attrs.size() , attrs.data());

            CHECK_STATUS(status);

            vlan_member_list.push_back(vlan_member_id);
        }
    }

    SWSS_LOG_INFO("create vlan member list");

    {
        // TODO this list should be updated each time we modify any VLAN member
        // and it probably should be handled inside create/remove vlan member api
        // and not here

        sai_attribute_t attr;

        attr.id = SAI_VLAN_ATTR_MEMBER_LIST;
        attr.value.objlist.count = (uint32_t)vlan_member_list.size();
        attr.value.objlist.list = vlan_member_list.data();

        status = vs_generic_set_vlan(DEFAULT_VLAN_NUMBER, &attr);

        CHECK_STATUS(status);
    }

    return status;
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
