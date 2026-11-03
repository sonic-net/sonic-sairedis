#include "SwitchVppTestHarness.h"

#include "meta/sai_serialize.h"

#include <set>
#include <string>
#include <vector>

using namespace saivs;
using namespace saivs_test;

/*
 * SAI debug counters (SONiC configurable drop counters) on VPP. VPP exports
 * drops per reason and sw_if_index in /sonic-ext/drop-reason/<REASON>, which
 * vppMock().dropReasonStats stands in for; SwitchVpp sums a counter's reasons
 * and reports them relative to the values when the counter was created.
 */
class SwitchVppDebugCounterTest : public SwitchVppTest
{
    protected:

        static constexpr uint32_t PORT0_IDX = 5;
        static constexpr uint32_t PORT1_IDX = 6;

        void SetUp() override
        {
            SWSS_LOG_ENTER();

            SwitchVppTest::SetUp();

            m_port0 = addPort("bobm0", PORT0_IDX);
            m_port1 = addPort("bobm1", PORT1_IDX);
        }

        sai_object_id_t addPort(
                const char *hwif,
                uint32_t swIfIndex)
        {
            SWSS_LOG_ENTER();

            auto port = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, m_switchId);

            EXPECT_NE(nullptr, m_sw->m_ifaceRegistry.addPhysicalPort(hwif, std::string("os_") + hwif, port));

            vppMock().swIfIndexByName[hwif] = swIfIndex;

            return port;
        }

        static void setDrops(
                const std::string& reason,
                uint32_t swIfIndex,
                std::vector<uint64_t> perThread)
        {
            SWSS_LOG_ENTER();

            vppMock().dropReasonStats[reason][swIfIndex] = perThread;
        }

        sai_object_id_t createCounter(
                sai_debug_counter_type_t type,
                std::vector<int32_t> reasons)
        {
            SWSS_LOG_ENTER();

            auto counter = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_DEBUG_COUNTER, m_switchId);

            sai_attribute_t attrs[2];

            attrs[0].id = SAI_DEBUG_COUNTER_ATTR_TYPE;
            attrs[0].value.s32 = type;
            attrs[1].id = SAI_DEBUG_COUNTER_ATTR_IN_DROP_REASON_LIST;
            attrs[1].value.s32list.count = static_cast<uint32_t>(reasons.size());
            attrs[1].value.s32list.list = reasons.data();

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create(SAI_OBJECT_TYPE_DEBUG_COUNTER, sai_serialize_object_id(counter), m_switchId, 2, attrs));

            return counter;
        }

        uint32_t counterIndex(
                sai_object_id_t counter)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = SAI_DEBUG_COUNTER_ATTR_INDEX;

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_DEBUG_COUNTER, counter, 1, &attr));

            return attr.value.u32;
        }

        uint64_t readStat(
                sai_object_type_t type,
                sai_object_id_t object,
                sai_stat_id_t stat,
                sai_stats_mode_t mode = SAI_STATS_MODE_READ)
        {
            SWSS_LOG_ENTER();

            uint64_t value = UINT64_MAX;

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->getStatsExt(type, object, 1, &stat, mode, &value));

            return value;
        }

        uint64_t portValue(
                sai_object_id_t port,
                sai_object_id_t counter,
                sai_stats_mode_t mode = SAI_STATS_MODE_READ)
        {
            SWSS_LOG_ENTER();

            // Defeat the short-lived dump cache: every read sees the latest mock values.
            m_sw->m_dropReasonStatsValid = false;

            return readStat(SAI_OBJECT_TYPE_PORT, port,
                    static_cast<sai_stat_id_t>(SAI_PORT_STAT_IN_DROP_REASON_RANGE_BASE + counterIndex(counter)), mode);
        }

        uint64_t switchValue(
                sai_object_id_t counter,
                sai_stats_mode_t mode = SAI_STATS_MODE_READ)
        {
            SWSS_LOG_ENTER();

            m_sw->m_dropReasonStatsValid = false;

            return readStat(SAI_OBJECT_TYPE_SWITCH, m_switchId,
                    static_cast<sai_stat_id_t>(SAI_SWITCH_STAT_IN_DROP_REASON_RANGE_BASE + counterIndex(counter)), mode);
        }

        std::vector<int32_t> enumCapability(
                sai_attr_id_t attrId)
        {
            SWSS_LOG_ENTER();

            sai_s32_list_t list = { .count = 0, .list = nullptr };

            sai_status_t status = m_sw->queryAttrEnumValuesCapability(
                    m_switchId, SAI_OBJECT_TYPE_DEBUG_COUNTER, attrId, &list);

            if (status == SAI_STATUS_SUCCESS)
            {
                EXPECT_EQ(0u, list.count);
                return {};
            }

            EXPECT_EQ(SAI_STATUS_BUFFER_OVERFLOW, status);

            std::vector<int32_t> values(list.count);

            list.list = values.data();

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->queryAttrEnumValuesCapability(
                        m_switchId, SAI_OBJECT_TYPE_DEBUG_COUNTER, attrId, &list));

            values.resize(list.count);

            return values;
        }

        sai_object_id_t m_port0 = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_port1 = SAI_NULL_OBJECT_ID;
};

TEST_F(SwitchVppDebugCounterTest, advertisesOnlyReasonsVppCounts)
{
    auto in = enumCapability(SAI_DEBUG_COUNTER_ATTR_IN_DROP_REASON_LIST);

    EXPECT_EQ((std::set<int32_t>{ SAI_IN_DROP_REASON_SIP_LINK_LOCAL, SAI_IN_DROP_REASON_DIP_LINK_LOCAL }),
            std::set<int32_t>(in.begin(), in.end()));

    auto types = enumCapability(SAI_DEBUG_COUNTER_ATTR_TYPE);

    EXPECT_EQ((std::set<int32_t>{ SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS, SAI_DEBUG_COUNTER_TYPE_SWITCH_IN_DROP_REASONS }),
            std::set<int32_t>(types.begin(), types.end()));

    // No egress reasons, so orchagent publishes no egress counter types.
    EXPECT_TRUE(enumCapability(SAI_DEBUG_COUNTER_ATTR_OUT_DROP_REASON_LIST).empty());
}

TEST_F(SwitchVppDebugCounterTest, portStatsCapabilityListsDebugCounterStats)
{
    sai_stat_capability_list_t list = { .count = 0, .list = nullptr };

    ASSERT_EQ(SAI_STATUS_BUFFER_OVERFLOW, m_sw->queryPortStatsCapability(&list));

    std::vector<sai_stat_capability_t> entries(list.count);

    list.list = entries.data();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->queryPortStatsCapability(&list));

    std::set<sai_stat_id_t> stats;

    for (const auto& entry: entries)
    {
        stats.insert(entry.stat_enum);
    }

    // The standard port stats stay, and every debug counter index has its port
    // stat, so syncd polls it for the drop monitor's flex counter group.
    EXPECT_EQ(1u, stats.count(SAI_PORT_STAT_IF_IN_DISCARDS));

    for (int i = 0; i < SwitchStateBase::maxDebugCounters; i++)
    {
        EXPECT_EQ(1u, stats.count(static_cast<sai_stat_id_t>(SAI_PORT_STAT_IN_DROP_REASON_RANGE_BASE + i))) << i;
    }
}

TEST_F(SwitchVppDebugCounterTest, portCounterStartsAtZeroAndCountsItsPortOnly)
{
    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 40, 10 });

    auto counter = createCounter(SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS, { SAI_IN_DROP_REASON_DIP_LINK_LOCAL });

    EXPECT_EQ(0u, portValue(m_port0, counter));

    // Summed over VPP threads, relative to the values at creation.
    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 45, 17 });
    setDrops("DIP_LINK_LOCAL", PORT1_IDX, { 3 });
    setDrops("SIP_LINK_LOCAL", PORT0_IDX, { 100 });

    EXPECT_EQ(12u, portValue(m_port0, counter));
    EXPECT_EQ(3u, portValue(m_port1, counter));
}

TEST_F(SwitchVppDebugCounterTest, counterSumsAllOfItsReasons)
{
    auto counter = createCounter(SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS,
            { SAI_IN_DROP_REASON_SIP_LINK_LOCAL, SAI_IN_DROP_REASON_DIP_LINK_LOCAL });

    setDrops("SIP_LINK_LOCAL", PORT0_IDX, { 4 });
    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 6 });

    EXPECT_EQ(10u, portValue(m_port0, counter));
}

TEST_F(SwitchVppDebugCounterTest, reasonChangeRestartsFromZero)
{
    auto counter = createCounter(SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS, { SAI_IN_DROP_REASON_DIP_LINK_LOCAL });

    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 9 });
    setDrops("SIP_LINK_LOCAL", PORT0_IDX, { 5 });

    ASSERT_EQ(9u, portValue(m_port0, counter));

    std::vector<int32_t> reasons = { SAI_IN_DROP_REASON_SIP_LINK_LOCAL, SAI_IN_DROP_REASON_DIP_LINK_LOCAL };
    sai_attribute_t attr;

    attr.id = SAI_DEBUG_COUNTER_ATTR_IN_DROP_REASON_LIST;
    attr.value.s32list.count = static_cast<uint32_t>(reasons.size());
    attr.value.s32list.list = reasons.data();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set(SAI_OBJECT_TYPE_DEBUG_COUNTER, sai_serialize_object_id(counter), &attr));

    EXPECT_EQ(0u, portValue(m_port0, counter));

    setDrops("SIP_LINK_LOCAL", PORT0_IDX, { 7 });

    EXPECT_EQ(2u, portValue(m_port0, counter));
}

TEST_F(SwitchVppDebugCounterTest, countsNeverGoBackwards)
{
    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 20 });

    auto counter = createCounter(SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS, { SAI_IN_DROP_REASON_DIP_LINK_LOCAL });

    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 30 });
    ASSERT_EQ(10u, portValue(m_port0, counter));

    // VPP restarted or its counters were cleared: start over from the new value.
    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 4 });
    EXPECT_EQ(0u, portValue(m_port0, counter));

    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 6 });
    EXPECT_EQ(2u, portValue(m_port0, counter));
}

TEST_F(SwitchVppDebugCounterTest, switchCounterSumsEveryPortAndUnattributedDrops)
{
    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 1 });

    auto counter = createCounter(SAI_DEBUG_COUNTER_TYPE_SWITCH_IN_DROP_REASONS, { SAI_IN_DROP_REASON_DIP_LINK_LOCAL });

    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 3, 1 });
    setDrops("DIP_LINK_LOCAL", PORT1_IDX, { 5 });
    // Index 0 is where VPP counts drops it cannot charge to a port.
    setDrops("DIP_LINK_LOCAL", 0, { 2 });

    EXPECT_EQ(10u, switchValue(counter));

    // A port counter ignores the switch-level index.
    auto portCounter = createCounter(SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS, { SAI_IN_DROP_REASON_DIP_LINK_LOCAL });

    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 4, 1 });

    EXPECT_EQ(1u, portValue(m_port0, portCounter));
}

TEST_F(SwitchVppDebugCounterTest, readAndClearRestartsThePortOnly)
{
    auto counter = createCounter(SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS, { SAI_IN_DROP_REASON_DIP_LINK_LOCAL });

    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 8 });
    setDrops("DIP_LINK_LOCAL", PORT1_IDX, { 3 });

    EXPECT_EQ(8u, portValue(m_port0, counter, SAI_STATS_MODE_READ_AND_CLEAR));
    EXPECT_EQ(0u, portValue(m_port0, counter));
    EXPECT_EQ(3u, portValue(m_port1, counter));

    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 11 });

    EXPECT_EQ(3u, portValue(m_port0, counter));
}

TEST_F(SwitchVppDebugCounterTest, readAndClearRestartsTheSwitchCounter)
{
    auto counter = createCounter(SAI_DEBUG_COUNTER_TYPE_SWITCH_IN_DROP_REASONS, { SAI_IN_DROP_REASON_DIP_LINK_LOCAL });

    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 8 });
    setDrops("DIP_LINK_LOCAL", PORT1_IDX, { 3 });

    EXPECT_EQ(11u, switchValue(counter, SAI_STATS_MODE_READ_AND_CLEAR));
    EXPECT_EQ(0u, switchValue(counter));

    setDrops("DIP_LINK_LOCAL", PORT1_IDX, { 4 });

    EXPECT_EQ(1u, switchValue(counter));
}

TEST_F(SwitchVppDebugCounterTest, unavailableStatsReadAsZero)
{
    vppMock().dropReasonDumpResult = -1;

    auto counter = createCounter(SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS, { SAI_IN_DROP_REASON_DIP_LINK_LOCAL });

    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 5 });

    EXPECT_EQ(0u, portValue(m_port0, counter));
}

TEST_F(SwitchVppDebugCounterTest, removeForgetsTheCounterAndFreesItsIndex)
{
    auto first = createCounter(SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS, { SAI_IN_DROP_REASON_DIP_LINK_LOCAL });
    auto index = counterIndex(first);

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_DEBUG_COUNTER, sai_serialize_object_id(first)));

    EXPECT_TRUE(m_sw->m_debugCounters.empty());

    auto second = createCounter(SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS, { SAI_IN_DROP_REASON_SIP_LINK_LOCAL });

    EXPECT_EQ(index, counterIndex(second));
    EXPECT_EQ(1u, m_sw->m_debugCounters.size());
}
