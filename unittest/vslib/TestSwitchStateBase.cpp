#include "SwitchStateBase.h"
#include "MACsecAttr.h"

#include <gtest/gtest.h>

#include <vector>

using namespace saivs;

//Test the following function:
//sai_status_t initialize_voq_switch_objects(
//             _In_ uint32_t attr_count,
//             _In_ const sai_attribute_t *attr_list);

TEST(SwitchStateBase, initialize_voq_switch_objects)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto scc = std::make_shared<SwitchConfigContainer>();

    SwitchStateBase ss(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_TYPE;
    attr.value.u32 = SAI_SWITCH_TYPE_FABRIC;
    sc->m_fabricLaneMap = LaneMap::getDefaultLaneMap(0);
    // Check the result of the initialize_voq_switch_objects
    EXPECT_EQ(SAI_STATUS_SUCCESS,
              ss.initialize_voq_switch_objects(1, &attr));
}

TEST(SwitchStateBase, initialize_voq_switch)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto scc = std::make_shared<SwitchConfigContainer>();

    SwitchStateBase ss(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    std::vector<sai_attribute_t> attrs;
    sai_attribute_t attr;
    const u_int32_t numSysPorts = 2;
    sai_system_port_config_t sysports[ numSysPorts ] = {
       { .port_id = 0, .attached_switch_id = 2, .attached_core_index = 0,
         .attached_core_port_index = 0, .speed=40000, .num_voq = 8 },
       { .port_id = 1, .attached_switch_id = 2, .attached_core_index = 0,
         .attached_core_port_index = 1, .speed=40000, .num_voq = 8 }
    };

    u_int32_t switchId = 2;
    u_int32_t maxSystemCores = 16;

    attr.id = SAI_SWITCH_ATTR_TYPE;
    attr.value.u32 = SAI_SWITCH_TYPE_VOQ;
    attrs.push_back(attr);

    attr.id = SAI_SWITCH_ATTR_SWITCH_ID;
    attr.value.u32 = switchId;
    attrs.push_back(attr);

    attr.id = SAI_SWITCH_ATTR_MAX_SYSTEM_CORES;
    attr.value.u32 = maxSystemCores;
    attrs.push_back(attr);

    attr.id = SAI_SWITCH_ATTR_SYSTEM_PORT_CONFIG_LIST;
    attr.value.sysportconfiglist.count = numSysPorts;
    attr.value.sysportconfiglist.list = sysports;
    attrs.push_back(attr);

    // Check the result of the initialize_voq_switch_objects
    EXPECT_EQ(SAI_STATUS_SUCCESS,
              ss.initialize_voq_switch_objects((uint32_t)attrs.size(), attrs.data()));
}
