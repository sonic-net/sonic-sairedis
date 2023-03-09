#include <cstdint>

#include <memory>
#include <vector>
#include <array>

#include <gtest/gtest.h>

#include "ContextConfigContainer.h"
#include "VirtualSwitchSaiInterface.h"

using namespace saivs;

class VirtualSwitchSaiInterfaceTest : public ::testing::Test
{
public:
    VirtualSwitchSaiInterfaceTest() = default;
    virtual ~VirtualSwitchSaiInterfaceTest() = default;

public:
    virtual void SetUp() override
    {
        m_ccc = ContextConfigContainer::getDefault();
        m_cc = m_ccc->get(m_guid);
        m_scc = m_cc->m_scc;
        m_sc = m_scc->getConfig(m_scid);

        m_sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
        m_sc->m_switchType = SAI_VS_SWITCH_TYPE_MLNX2700;
        m_sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
        m_sc->m_useTapDevice = false;
        m_sc->m_laneMap = LaneMap::getDefaultLaneMap();
        m_sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

        m_vssai = std::make_shared<VirtualSwitchSaiInterface>(m_cc);

        sai_attribute_t attr;
        attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
        attr.value.booldata = true;

        auto status = m_vssai->create(SAI_OBJECT_TYPE_SWITCH, &m_swid, SAI_NULL_OBJECT_ID, 1, &attr);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

    virtual void TearDown() override
    {
        auto status = m_vssai->remove(SAI_OBJECT_TYPE_SWITCH, m_swid);
        ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

protected:
    std::shared_ptr<ContextConfigContainer> m_ccc;
    std::shared_ptr<ContextConfig> m_cc;
    std::shared_ptr<SwitchConfigContainer> m_scc;
    std::shared_ptr<SwitchConfig> m_sc;
    std::shared_ptr<VirtualSwitchSaiInterface> m_vssai;

    sai_object_id_t m_swid = SAI_NULL_OBJECT_ID;

    const std::uint32_t m_guid = 0; // default context config id
    const std::uint32_t m_scid = 0; // default switch config id
};

TEST_F(VirtualSwitchSaiInterfaceTest, portBulkAddRemove)
{
    const std::uint32_t portCount = 1;
    const std::uint32_t laneCount = 4;

    // Generate port config
    std::array<std::uint32_t, laneCount> laneList = { 0, 1, 2, 3 };

    sai_attribute_t attr;
    std::vector<sai_attribute_t> attrList;

    attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
    attr.value.u32list.count = static_cast<std::uint32_t>(laneList.size());
    attr.value.u32list.list = laneList.data();
    attrList.push_back(attr);

    attr.id = SAI_PORT_ATTR_SPEED;
    attr.value.u32 = 1000;
    attrList.push_back(attr);

    std::array<std::uint32_t, portCount> attrCountList = { static_cast<std::uint32_t>(attrList.size()) };
    std::array<const sai_attribute_t*, portCount> attrPtrList = { attrList.data() };

    std::array<sai_object_id_t, portCount> oidList = { SAI_NULL_OBJECT_ID };
    std::array<sai_status_t, portCount> statusList = { SAI_STATUS_SUCCESS };

    // Validate port bulk add
    auto status = m_vssai->bulkCreate(
        SAI_OBJECT_TYPE_PORT, m_swid, portCount, attrCountList.data(), attrPtrList.data(),
        SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
        oidList.data(), statusList.data()
    );
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (std::uint32_t i = 0; i < portCount; i++)
    {
        ASSERT_EQ(statusList.at(i), SAI_STATUS_SUCCESS);
    }

    // Validate port bulk remove
    status = m_vssai->bulkRemove(
        SAI_OBJECT_TYPE_PORT, portCount, oidList.data(),
        SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR,
        statusList.data()
    );
    ASSERT_EQ(status, SAI_STATUS_SUCCESS);

    for (std::uint32_t i = 0; i < portCount; i++)
    {
        ASSERT_EQ(statusList.at(i), SAI_STATUS_SUCCESS);
    }
}
