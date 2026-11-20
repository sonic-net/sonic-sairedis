#include "SwitchVppTestHarness.h"

#include "meta/sai_serialize.h"

using namespace saivs;

namespace saivs_test
{
    int vppCallIndex(
            const std::string& api,
            const std::string& name)
    {
        SWSS_LOG_ENTER();

        const auto& recorded = vppMock().calls;

        for (size_t i = 0; i < recorded.size(); i++)
        {
            if (recorded[i].api == api && recorded[i].name == name)
            {
                return (int)i;
            }
        }

        return -1;
    }

    std::vector<VppCall> vppCallsTo(
            const std::string& api)
    {
        SWSS_LOG_ENTER();

        std::vector<VppCall> calls;

        for (auto& call: vppMock().calls)
        {
            if (call.api == api)
            {
                calls.push_back(call);
            }
        }

        return calls;
    }

    void SwitchVppTest::SetUp()
    {
        SWSS_LOG_ENTER();

        vppMock().reset();

        auto sc = std::make_shared<SwitchConfig>(0, "");

        sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
        sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
        sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
        sc->m_useTapDevice = useTapDevice();
        sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
        sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

        auto scc = std::make_shared<SwitchConfigContainer>();

        scc->insert(sc);

        m_mgr = std::make_shared<RealObjectIdManager>(0, scc);
        m_sw = std::make_shared<SwitchVpp>(m_switchId, m_mgr, sc);

        ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_default_virtual_router());

        sai_attribute_t attr;

        attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;

        ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), 1, &attr));

        m_defaultVr = attr.value.oid;

        vppMock().calls.clear();
    }

    void SwitchVppTest::TearDown()
    {
        SWSS_LOG_ENTER();

        m_sw.reset();
    }

    bool SwitchVppTest::useTapDevice() const
    {
        SWSS_LOG_ENTER();

        return false;
    }

    sai_status_t SwitchVppTest::setSwitch(
            const sai_attribute_t& attr)
    {
        SWSS_LOG_ENTER();

        return m_sw->set(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), &attr);
    }
}
