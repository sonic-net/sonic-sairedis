#include "SwitchVppTestHarness.h"

#include "meta/sai_serialize.h"

#include <set>
#include <string>
#include <utility>
#include <vector>
#include <unistd.h>

using namespace saivs;
using namespace saivs_test;

namespace
{
    // Every SAI reason VPP counts, with its /sonic-ext/drop-reason/ leaf.
    const std::vector<std::pair<int32_t, std::string>> g_countedReasons = {
        { SAI_IN_DROP_REASON_SIP_LINK_LOCAL, "SIP_LINK_LOCAL" },
        { SAI_IN_DROP_REASON_DIP_LINK_LOCAL, "DIP_LINK_LOCAL" },
        { SAI_IN_DROP_REASON_SIP_LOOPBACK, "SIP_LOOPBACK" },
        { SAI_IN_DROP_REASON_SIP_MC, "SIP_MC" },
        { SAI_IN_DROP_REASON_SIP_CLASS_E, "SIP_CLASS_E" },
        { SAI_IN_DROP_REASON_SIP_UNSPECIFIED, "SIP_UNSPECIFIED" },
        { SAI_IN_DROP_REASON_DIP_LOOPBACK, "DIP_LOOPBACK" },
        { SAI_IN_DROP_REASON_UC_DIP_MC_DMAC, "UC_DIP_MC_DMAC" },
    };
}

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

            vppMock().enabledFeatures.insert("drop-reason-stats");

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

        void setReasons(
                sai_object_id_t counter,
                std::vector<int32_t> reasons)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = SAI_DEBUG_COUNTER_ATTR_IN_DROP_REASON_LIST;
            attr.value.s32list.count = static_cast<uint32_t>(reasons.size());
            attr.value.s32list.list = reasons.data();

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->set(SAI_OBJECT_TYPE_DEBUG_COUNTER, sai_serialize_object_id(counter), &attr));
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

    std::set<int32_t> expected;

    for (const auto& reason: g_countedReasons)
    {
        expected.insert(reason.first);
    }

    EXPECT_EQ(expected, std::set<int32_t>(in.begin(), in.end()));

    auto types = enumCapability(SAI_DEBUG_COUNTER_ATTR_TYPE);

    EXPECT_EQ((std::set<int32_t>{ SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS, SAI_DEBUG_COUNTER_TYPE_SWITCH_IN_DROP_REASONS }),
            std::set<int32_t>(types.begin(), types.end()));

    // No egress reasons, so orchagent publishes no egress counter types.
    EXPECT_TRUE(enumCapability(SAI_DEBUG_COUNTER_ATTR_OUT_DROP_REASON_LIST).empty());
}

/*
 * sonicExtFeatureEnabled() keeps VPP's first answer for the rest of the
 * process, so the "off" case runs in a process of its own.
 */
TEST_F(SwitchVppDebugCounterTest, advertisesNothingWhenVppFeatureIsOff)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";

    ASSERT_EXIT({
        vppMock().enabledFeatures.erase("drop-reason-stats");

        bool none = enumCapability(SAI_DEBUG_COUNTER_ATTR_IN_DROP_REASON_LIST).empty() &&
            enumCapability(SAI_DEBUG_COUNTER_ATTR_OUT_DROP_REASON_LIST).empty() &&
            enumCapability(SAI_DEBUG_COUNTER_ATTR_TYPE).empty();

        _exit(none && vppMock().queriedFeature == "drop-reason-stats" ? 0 : 1);
    }, ::testing::ExitedWithCode(0), "");
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

TEST_F(SwitchVppDebugCounterTest, everyReasonReadsItsOwnVppCounter)
{
    std::vector<sai_object_id_t> counters;

    for (const auto& reason: g_countedReasons)
    {
        counters.push_back(createCounter(SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS, { reason.first }));
    }

    for (size_t i = 0; i < g_countedReasons.size(); i++)
    {
        setDrops(g_countedReasons[i].second, PORT0_IDX, { static_cast<uint64_t>(i + 1) });
    }

    for (size_t i = 0; i < g_countedReasons.size(); i++)
    {
        EXPECT_EQ(static_cast<uint64_t>(i + 1), portValue(m_port0, counters[i])) << g_countedReasons[i].second;
    }
}

TEST_F(SwitchVppDebugCounterTest, counterSumsAllOfItsReasons)
{
    auto counter = createCounter(SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS,
            { SAI_IN_DROP_REASON_SIP_LINK_LOCAL, SAI_IN_DROP_REASON_DIP_LINK_LOCAL });

    setDrops("SIP_LINK_LOCAL", PORT0_IDX, { 4 });
    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 6 });

    EXPECT_EQ(10u, portValue(m_port0, counter));
}

/*
 * dropstat shows a counter less the value it saved at sonic-clear
 * dropcounters, so a counter whose reasons change must not go backwards.
 */
TEST_F(SwitchVppDebugCounterTest, reasonChangeKeepsTheCount)
{
    setDrops("SIP_LINK_LOCAL", PORT0_IDX, { 500 });
    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 1500, 500 });

    auto counter = createCounter(SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS, { SAI_IN_DROP_REASON_SIP_LINK_LOCAL });

    setDrops("SIP_LINK_LOCAL", PORT0_IDX, { 600 });

    ASSERT_EQ(100u, portValue(m_port0, counter));

    // An added reason counts from now, not the drops VPP counted before.
    setReasons(counter, { SAI_IN_DROP_REASON_SIP_LINK_LOCAL, SAI_IN_DROP_REASON_DIP_LINK_LOCAL });

    EXPECT_EQ(100u, portValue(m_port0, counter));

    setDrops("SIP_LINK_LOCAL", PORT0_IDX, { 630 });
    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 1505, 505 });

    EXPECT_EQ(140u, portValue(m_port0, counter));

    // A removed reason keeps what it counted, and counts no more.
    setReasons(counter, { SAI_IN_DROP_REASON_SIP_LINK_LOCAL });

    EXPECT_EQ(140u, portValue(m_port0, counter));

    setDrops("SIP_LINK_LOCAL", PORT0_IDX, { 631 });
    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 1550, 505 });

    EXPECT_EQ(141u, portValue(m_port0, counter));

    // Added back, it counts from its value then.
    setReasons(counter, { SAI_IN_DROP_REASON_SIP_LINK_LOCAL, SAI_IN_DROP_REASON_DIP_LINK_LOCAL });

    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 1555, 505 });

    EXPECT_EQ(146u, portValue(m_port0, counter));
    EXPECT_EQ(0u, portValue(m_port1, counter));
}

TEST_F(SwitchVppDebugCounterTest, reasonChangeKeepsTheSwitchCount)
{
    setDrops("SIP_LINK_LOCAL", PORT0_IDX, { 5 });
    setDrops("DIP_LINK_LOCAL", PORT1_IDX, { 50 });
    setDrops("DIP_LINK_LOCAL", 0, { 7 });

    auto counter = createCounter(SAI_DEBUG_COUNTER_TYPE_SWITCH_IN_DROP_REASONS, { SAI_IN_DROP_REASON_SIP_LINK_LOCAL });

    setDrops("SIP_LINK_LOCAL", PORT0_IDX, { 8 });

    ASSERT_EQ(3u, switchValue(counter));

    setReasons(counter, { SAI_IN_DROP_REASON_SIP_LINK_LOCAL, SAI_IN_DROP_REASON_DIP_LINK_LOCAL });

    EXPECT_EQ(3u, switchValue(counter));

    setDrops("DIP_LINK_LOCAL", PORT1_IDX, { 52 });
    setDrops("DIP_LINK_LOCAL", 0, { 8 });

    EXPECT_EQ(6u, switchValue(counter));

    setReasons(counter, { SAI_IN_DROP_REASON_DIP_LINK_LOCAL });

    EXPECT_EQ(6u, switchValue(counter));

    setDrops("SIP_LINK_LOCAL", PORT0_IDX, { 20 });

    EXPECT_EQ(6u, switchValue(counter));
    EXPECT_EQ(6u, switchValue(counter, SAI_STATS_MODE_READ_AND_CLEAR));
    EXPECT_EQ(0u, switchValue(counter));
}

TEST_F(SwitchVppDebugCounterTest, readAndClearClearsWhatRemovedReasonsCounted)
{
    auto counter = createCounter(SAI_DEBUG_COUNTER_TYPE_PORT_IN_DROP_REASONS,
            { SAI_IN_DROP_REASON_SIP_LINK_LOCAL, SAI_IN_DROP_REASON_DIP_LINK_LOCAL });

    setDrops("SIP_LINK_LOCAL", PORT0_IDX, { 4 });
    setDrops("DIP_LINK_LOCAL", PORT0_IDX, { 6 });
    setDrops("DIP_LINK_LOCAL", PORT1_IDX, { 2 });

    setReasons(counter, { SAI_IN_DROP_REASON_SIP_LINK_LOCAL });

    EXPECT_EQ(10u, portValue(m_port0, counter, SAI_STATS_MODE_READ_AND_CLEAR));
    EXPECT_EQ(0u, portValue(m_port0, counter));
    EXPECT_EQ(2u, portValue(m_port1, counter));

    setDrops("SIP_LINK_LOCAL", PORT0_IDX, { 5 });

    EXPECT_EQ(1u, portValue(m_port0, counter));
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
