/**
 * @file TestPortPhySerdesAttr.cpp
 * @brief Unit tests for PORT_SERDES_ATTR flex counter functionality
 *
 * Tests implementation according to UT Plan:
 * 1. sai_serialize_port_serdes_attr() function
 * 2. sai_deserialize_port_serdes_attr() function
 * 3. collectData() with mocked SAI and counters DB validation
 */

#include "FlexCounter.h"
#include "VirtualOidTranslator.h"
#include "DisabledRedisClient.h"
#include "lib/RedisVidIndexGenerator.h"
#include "lib/sairediscommon.h"
#include "sai_serialize.h"
#include "MockableSaiInterface.h"
#include "MockHelper.h"
#include "swss/table.h"
#include "swss/schema.h"
#include "syncd/SaiSwitch.h"
#include <atomic>
#include <string>
#include <gtest/gtest.h>
#include <memory>
#include <nlohmann/json.hpp>

using namespace saimeta;
using namespace sairedis;
using namespace syncd;
using namespace std;
using json = nlohmann::json;

static const std::string ATTR_TYPE_PORT_PHY_SERDES_ATTR = "Port Serdes Attributes";
static const uint32_t TEST_LANE_COUNT = 4;
static const uint32_t TEST_TAP_COUNT = 6;

template <typename T>
std::string toOid(T value)
{
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream << "oid:0x" << std::hex << value;
    return ostream.str();
}


class TestPortPhySerdesAttr : public ::testing::Test
{
protected:
    void SetUp() override
    {
        sai = std::make_shared<MockableSaiInterface>();

        sai->mock_switchIdQuery = [](sai_object_id_t) {
            return 0x21000000000000;
        };

        // syncd resolves the port of a port serdes with its VirtualOidTranslator.
        // DisabledRedisClient keeps the VID to RID mapping in memory only, so
        // the tests do not depend on VIDTORID entries in ASIC_DB.
        auto dbAsic = std::make_shared<swss::DBConnector>("ASIC_DB", 0);
        auto switchConfigContainer = std::make_shared<sairedis::SwitchConfigContainer>();
        auto redisVidIndexGenerator = std::make_shared<sairedis::RedisVidIndexGenerator>(dbAsic, REDIS_KEY_VIDCOUNTER);
        auto virtualObjectIdManager = std::make_shared<sairedis::VirtualObjectIdManager>(0, switchConfigContainer, redisVidIndexGenerator);

        translator = std::make_shared<VirtualOidTranslator>(
                std::make_shared<DisabledRedisClient>(),
                virtualObjectIdManager,
                sai);

        flexCounter = std::make_shared<FlexCounter>("TEST_PORT_SERDES_ATTR", sai, "COUNTERS_DB", false, translator);

        // Setup test port serdes OID and port OID
        // PORT_SERDES object type = 87 (0x57), so OID is 0x57000000000001
        testPortSerdesOid = 0x57000000000001;
        testPortSerdesRid = 0x57000000000001;
        testPortOid = 0x1000000000001;
        testPortRid = 0x1000000000001;
    }

    void TearDown() override
    {
        flexCounter.reset();
        translator.reset();
        sai.reset();

        swss::DBConnector db("COUNTERS_DB", 0);
        swss::Table portSerdesIdToPortIdTable(&db, "COUNTERS_PORT_SERDES_ID_TO_PORT_ID_MAP");
        swss::Table portPhyAttrTable(&db, PORT_PHY_ATTR_TABLE);

        for (const auto &ids : mappedSerdesPorts)
        {
            portSerdesIdToPortIdTable.hdel("", toOid(ids.first));
            portPhyAttrTable.del(toOid(ids.second));
        }
    }

    // Map the port serdes VID to its port VID in COUNTERS_DB, as orchagent
    // does. Remove data that is left for the port, so that each test starts
    // from a clean state. TearDown() removes both entries.
    void mapSerdesToPort(sai_object_id_t serdesVid, sai_object_id_t portVid)
    {
        SWSS_LOG_ENTER();

        swss::DBConnector db("COUNTERS_DB", 0);
        swss::Table portSerdesIdToPortIdTable(&db, "COUNTERS_PORT_SERDES_ID_TO_PORT_ID_MAP");
        swss::Table portPhyAttrTable(&db, PORT_PHY_ATTR_TABLE);

        portPhyAttrTable.del(toOid(portVid));
        portSerdesIdToPortIdTable.hset("", toOid(serdesVid), toOid(portVid));

        mappedSerdesPorts.emplace_back(serdesVid, portVid);
    }

    // Vendor SAI for the RX_VGA tests. Only portRid has a hardware lane list.
    // SAI_PORT_SERDES_ATTR_PORT_ID fails with SAI_STATUS_OBJECT_IN_USE (-17),
    // the error that the SDK returned while the port came up.
    void mockVendorSai(sai_object_id_t portRid)
    {
        SWSS_LOG_ENTER();

        sai->mock_get = [this, portRid](sai_object_type_t object_type,
                                        sai_object_id_t object_id,
                                        uint32_t attr_count,
                                        sai_attribute_t *attr_list) -> sai_status_t
        {
            if (object_type == SAI_OBJECT_TYPE_PORT)
            {
                if (attr_count != 1 || attr_list[0].id != SAI_PORT_ATTR_HW_LANE_LIST)
                {
                    return SAI_STATUS_NOT_SUPPORTED;
                }

                laneListQueries++;

                if (object_id != portRid)
                {
                    return SAI_STATUS_INVALID_OBJECT_ID;
                }

                attr_list[0].value.u32list.count = TEST_LANE_COUNT;
                return SAI_STATUS_BUFFER_OVERFLOW;
            }

            if (object_type != SAI_OBJECT_TYPE_PORT_SERDES)
            {
                return SAI_STATUS_INVALID_PARAMETER;
            }

            for (uint32_t i = 0; i < attr_count; i++)
            {
                switch (attr_list[i].id)
                {
                    case SAI_PORT_SERDES_ATTR_PORT_ID:
                        portIdQueries++;
                        return SAI_STATUS_OBJECT_IN_USE;

                    case SAI_PORT_SERDES_ATTR_TX_FIR_COUNT:
                        attr_list[i].value.u32 = TEST_TAP_COUNT;
                        break;

                    case SAI_PORT_SERDES_ATTR_RX_VGA:
                        for (uint32_t lane = 0; lane < attr_list[i].value.u32list.count; lane++)
                        {
                            attr_list[i].value.u32list.list[lane] = 100 + lane;
                        }
                        break;

                    default:
                        return SAI_STATUS_NOT_SUPPORTED;
                }
            }

            return SAI_STATUS_SUCCESS;
        };
    }

    // Enroll the port serdes for RX_VGA, enable polling and wait for one poll.
    void pollRxVga(
            std::shared_ptr<FlexCounter> fc,
            sai_object_id_t serdesVid,
            sai_object_id_t serdesRid)
    {
        SWSS_LOG_ENTER();

        vector<swss::FieldValueTuple> portSerdesAttrValues;
        portSerdesAttrValues.emplace_back(PORT_PHY_SERDES_ATTR_ID_LIST, "SAI_PORT_SERDES_ATTR_RX_VGA");

        test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT_SERDES);
        fc->addCounter(serdesVid, serdesRid, portSerdesAttrValues);

        vector<swss::FieldValueTuple> pluginValues;
        pluginValues.emplace_back(POLL_INTERVAL_FIELD, "1000");
        pluginValues.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
        pluginValues.emplace_back(STATS_MODE_FIELD, STATS_MODE_READ);
        fc->addCounterPlugin(pluginValues);

        usleep(1000 * 1050); // 1.05 seconds - one poll cycle
    }

    bool getRxVga(sai_object_id_t portVid, std::string &value)
    {
        SWSS_LOG_ENTER();

        swss::DBConnector db("COUNTERS_DB", 0);
        swss::Table portPhyAttrTable(&db, PORT_PHY_ATTR_TABLE);

        return portPhyAttrTable.hget(toOid(portVid), "rx_vga", value);
    }

    std::shared_ptr<MockableSaiInterface> sai;
    std::shared_ptr<VirtualOidTranslator> translator;
    std::shared_ptr<FlexCounter> flexCounter;
    sai_object_id_t testPortSerdesOid;
    sai_object_id_t testPortSerdesRid;
    sai_object_id_t testPortOid;
    sai_object_id_t testPortRid;
    std::vector<std::pair<sai_object_id_t, sai_object_id_t>> mappedSerdesPorts;
    std::atomic<uint32_t> portIdQueries{0};
    std::atomic<uint32_t> laneListQueries{0};
};

TEST_F(TestPortPhySerdesAttr, SerializePortSerdesAttr)
{
    sai_port_serdes_attr_t attr = SAI_PORT_SERDES_ATTR_RX_VGA;
    std::string result = sai_serialize_port_serdes_attr(attr);
    EXPECT_EQ(result, "SAI_PORT_SERDES_ATTR_RX_VGA");

    attr = SAI_PORT_SERDES_ATTR_TX_FIR_TAPS_LIST;
    result = sai_serialize_port_serdes_attr(attr);
    EXPECT_EQ(result, "SAI_PORT_SERDES_ATTR_TX_FIR_TAPS_LIST");

    attr = SAI_PORT_SERDES_ATTR_TX_FIR_COUNT;
    result = sai_serialize_port_serdes_attr(attr);
    EXPECT_EQ(result, "SAI_PORT_SERDES_ATTR_TX_FIR_COUNT");
}

TEST_F(TestPortPhySerdesAttr, DeserializePortSerdesAttr)
{
    sai_port_serdes_attr_t attr_out;

    std::string input = "SAI_PORT_SERDES_ATTR_RX_VGA";
    sai_deserialize_port_serdes_attr(input, attr_out);
    EXPECT_EQ(attr_out, SAI_PORT_SERDES_ATTR_RX_VGA);

    input = "SAI_PORT_SERDES_ATTR_TX_FIR_TAPS_LIST";
    sai_deserialize_port_serdes_attr(input, attr_out);
    EXPECT_EQ(attr_out, SAI_PORT_SERDES_ATTR_TX_FIR_TAPS_LIST);

    input = "SAI_PORT_SERDES_ATTR_TX_FIR_COUNT";
    sai_deserialize_port_serdes_attr(input, attr_out);
    EXPECT_EQ(attr_out, SAI_PORT_SERDES_ATTR_TX_FIR_COUNT);
}

/**
 * Test collectData() with mocked SAI and COUNTERS_DB validation
 * This test verifies the complete data collection workflow:
 * 1. Mock SAI interface returns realistic PORT_SERDES attribute data
 * 2. FlexCounter collects the data via collectData()
 * 3. Verify collected data is properly written to COUNTERS_DB with friendly aliases
 *
 * This test validates the complete PORT_SERDES_ATTR collection workflow
 * including RX_VGA and TX_FIR_TAPS_LIST attributes.
 */
TEST_F(TestPortPhySerdesAttr, CollectDataAndValidateCountersDB)
{
    // Setup mock for PORT_SERDES attributes with realistic data
    sai->mock_get = [this](sai_object_type_t object_type,
                      sai_object_id_t object_id,
                      uint32_t attr_count,
                      sai_attribute_t *attr_list) -> sai_status_t
    {
        if (object_type == SAI_OBJECT_TYPE_PORT_SERDES) {
            for (uint32_t i = 0; i < attr_count; i++) {
                switch (attr_list[i].id) {
                    case SAI_PORT_SERDES_ATTR_PORT_ID:
                        // The port comes from the translator. Fail the vendor
                        // query as the SDK did (-17), to detect any use of it.
                        portIdQueries++;
                        return SAI_STATUS_OBJECT_IN_USE;

                    case SAI_PORT_SERDES_ATTR_TX_FIR_COUNT:
                        // Return tap count
                        attr_list[i].value.u32 = TEST_TAP_COUNT;
                        break;

                    case SAI_PORT_SERDES_ATTR_RX_VGA:
                        if (attr_list[i].value.u32list.list == nullptr) {
                            // First call: return count needed
                            attr_list[i].value.u32list.count = TEST_LANE_COUNT;
                            return SAI_STATUS_BUFFER_OVERFLOW;
                        } else {
                            // Second call: fill actual data
                            uint32_t count = attr_list[i].value.u32list.count;
                            for (uint32_t lane = 0; lane < count && lane < TEST_LANE_COUNT; lane++) {
                                attr_list[i].value.u32list.list[lane] = 177 + lane; // VGA values: 177, 178, 179, 180
                            }
                            attr_list[i].value.u32list.count = std::min(count, TEST_LANE_COUNT);
                        }
                        break;

                    case SAI_PORT_SERDES_ATTR_TX_FIR_TAPS_LIST:
                        if (attr_list[i].value.portserdestaps.list == nullptr) {
                            // First call: return tap count needed
                            attr_list[i].value.portserdestaps.count = TEST_TAP_COUNT;
                            return SAI_STATUS_BUFFER_OVERFLOW;
                        } else {
                            // Second call: fill actual data
                            uint32_t tap_count = attr_list[i].value.portserdestaps.count;
                            for (uint32_t tap_idx = 0; tap_idx < tap_count && tap_idx < TEST_TAP_COUNT; tap_idx++) {
                                if (attr_list[i].value.portserdestaps.list[tap_idx].list == nullptr) {
                                    // Query for lane count for this tap
                                    attr_list[i].value.portserdestaps.list[tap_idx].count = TEST_LANE_COUNT;
                                } else {
                                    // Fill lane values for this tap
                                    uint32_t lane_count = attr_list[i].value.portserdestaps.list[tap_idx].count;
                                    for (uint32_t lane = 0; lane < lane_count && lane < TEST_LANE_COUNT; lane++) {
                                        // Generate tap values: tap0: -23,-22,-21,-20, tap1: -13,-12,-11,-10, etc.
                                        int32_t base_value = -23 + (tap_idx * 10);
                                        attr_list[i].value.portserdestaps.list[tap_idx].list[lane] = base_value + lane;
                                    }
                                    attr_list[i].value.portserdestaps.list[tap_idx].count = std::min(lane_count, TEST_LANE_COUNT);
                                }
                            }
                            attr_list[i].value.portserdestaps.count = std::min(tap_count, TEST_TAP_COUNT);
                        }
                        break;

                    default:
                        return SAI_STATUS_NOT_SUPPORTED;
                }
            }
            return SAI_STATUS_SUCCESS;
        } else if (object_type == SAI_OBJECT_TYPE_PORT) {
            for (uint32_t i = 0; i < attr_count; i++) {
                if (attr_list[i].id == SAI_PORT_ATTR_HW_LANE_LIST) {
                    if (attr_list[i].value.u32list.list == nullptr) {
                        // Return lane count
                        attr_list[i].value.u32list.count = TEST_LANE_COUNT;
                        return SAI_STATUS_BUFFER_OVERFLOW;
                    } else {
                        // Fill lane list
                        uint32_t count = attr_list[i].value.u32list.count;
                        for (uint32_t lane = 0; lane < count && lane < TEST_LANE_COUNT; lane++) {
                            attr_list[i].value.u32list.list[lane] = lane;
                        }
                        attr_list[i].value.u32list.count = std::min(count, TEST_LANE_COUNT);
                        return SAI_STATUS_SUCCESS;
                    }
                }
            }
        }
        return SAI_STATUS_INVALID_PARAMETER;
    };

    // Map the port serdes to its port: port serdes VID -> port VID in
    // COUNTERS_DB, and port VID -> port RID in the translator
    translator->insertRidAndVid(testPortRid, testPortOid);
    mapSerdesToPort(testPortSerdesOid, testPortOid);

    swss::DBConnector db("COUNTERS_DB", 0);

    vector<swss::FieldValueTuple> portSerdesAttrValues;

    std::string attrIds = "SAI_PORT_SERDES_ATTR_RX_VGA,SAI_PORT_SERDES_ATTR_TX_FIR_TAPS_LIST";

    portSerdesAttrValues.emplace_back(PORT_PHY_SERDES_ATTR_ID_LIST, attrIds);

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT_SERDES);

    flexCounter->addCounter(testPortSerdesOid, testPortSerdesRid, portSerdesAttrValues);

    vector<swss::FieldValueTuple> pluginValues;
    pluginValues.emplace_back(POLL_INTERVAL_FIELD, "1000");
    pluginValues.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    pluginValues.emplace_back(STATS_MODE_FIELD, STATS_MODE_READ);
    flexCounter->addCounterPlugin(pluginValues);

    usleep(1000 * 1050); // 1.05 seconds to ensure at least one poll cycle

    // Connect to COUNTERS_DB and verify entries in PORT_PHY_ATTR_TABLE
    swss::RedisPipeline pipeline(&db);
    swss::Table portPhyAttrTable(&pipeline, PORT_PHY_ATTR_TABLE, false);

    // The key should be the port VID (not port serdes VID)
    std::string expectedKey = toOid(testPortOid);

    // Validate RX_VGA with friendly alias "rx_vga"
    std::string rxVgaValue;
    bool found = portPhyAttrTable.hget(expectedKey, "rx_vga", rxVgaValue);
    EXPECT_TRUE(found) << "rx_vga not found in COUNTERS_DB PORT_PHY_ATTR_TABLE table";

    // Parse JSON and validate structure
    json rxVgaJson;
    try {
        rxVgaJson = json::parse(rxVgaValue);
    } catch (const json::parse_error& e) {
        FAIL() << "Failed to parse rx_vga as JSON: " << e.what()
               << "\nValue: " << rxVgaValue;
    }

    // Verify it's a JSON object
    EXPECT_TRUE(rxVgaJson.is_object())
        << "rx_vga should be a JSON object\nActual: " << rxVgaValue;

    // Verify we have exactly TEST_LANE_COUNT lanes
    EXPECT_EQ(rxVgaJson.size(), TEST_LANE_COUNT)
        << "rx_vga should have " << TEST_LANE_COUNT << " lanes";

    // Verify lane keys are exactly "0", "1", "2", "3"
    for (uint32_t lane = 0; lane < TEST_LANE_COUNT; lane++) {
        std::string lane_key = std::to_string(lane);
        EXPECT_TRUE(rxVgaJson.contains(lane_key))
            << "Missing lane key '" << lane_key << "' in rx_vga JSON";
    }

    // Verify each lane's value
    for (uint32_t lane = 0; lane < TEST_LANE_COUNT; lane++) {
        std::string lane_key = std::to_string(lane);
        uint32_t expected_vga = 177 + lane;

        ASSERT_TRUE(rxVgaJson.contains(lane_key))
            << "Missing lane " << lane << " in rx_vga";

        ASSERT_TRUE(rxVgaJson[lane_key].is_number_integer())
            << "Lane " << lane << " value should be an integer";

        uint32_t actual_vga = rxVgaJson[lane_key].get<uint32_t>();
        EXPECT_EQ(actual_vga, expected_vga)
            << "Lane " << lane << " VGA should be " << expected_vga
            << " but got " << actual_vga;
    }

    // Validate TX_FIR_TAPS_LIST with friendly alias "tx_fir_taps_list"
    std::string txFirTapsValue;
    found = portPhyAttrTable.hget(expectedKey, "tx_fir_taps_list", txFirTapsValue);
    EXPECT_TRUE(found) << "tx_fir_taps_list not found in COUNTERS_DB PORT_SERDES_ATTR table";

    // Parse JSON and validate structure
    json txFirTapsJson;
    try {
        txFirTapsJson = json::parse(txFirTapsValue);
    } catch (const json::parse_error& e) {
        FAIL() << "Failed to parse tx_fir_taps_list as JSON: " << e.what()
               << "\nValue: " << txFirTapsValue;
    }

    // Verify it's a JSON object
    EXPECT_TRUE(txFirTapsJson.is_object())
        << "tx_fir_taps_list should be a JSON object\nActual: " << txFirTapsValue;

    // Verify we have exactly TEST_LANE_COUNT lanes
    EXPECT_EQ(txFirTapsJson.size(), TEST_LANE_COUNT)
        << "Should have " << TEST_LANE_COUNT << " lanes";

    // Verify that lane keys are exactly "0", "1", "2", ... "TEST_LANE_COUNT-1"
    for (uint32_t lane = 0; lane < TEST_LANE_COUNT; lane++) {
        std::string lane_key = std::to_string(lane);
        EXPECT_TRUE(txFirTapsJson.contains(lane_key))
            << "Missing lane key '" << lane_key << "' in JSON";
    }

    // Verify each lane's tap values with sequential tap numbering per lane
    for (uint32_t lane = 0; lane < TEST_LANE_COUNT; lane++) {
        std::string lane_key = std::to_string(lane);

        ASSERT_TRUE(txFirTapsJson.contains(lane_key))
            << "Missing lane " << lane << " in JSON";

        ASSERT_TRUE(txFirTapsJson[lane_key].is_array())
            << "Lane " << lane << " should be an array";

        const json& lane_taps = txFirTapsJson[lane_key];
        EXPECT_EQ(lane_taps.size(), TEST_TAP_COUNT)
            << "Lane " << lane << " should have " << TEST_TAP_COUNT << " taps";

        // Verify each tap in this lane (with sequential numbering: tap0, tap1, tap2...)
        for (uint32_t tap_idx = 0; tap_idx < TEST_TAP_COUNT; tap_idx++) {
            ASSERT_LT(tap_idx, lane_taps.size())
                << "Lane " << lane << " missing tap at index " << tap_idx;

            const json& tap_obj = lane_taps[tap_idx];
            ASSERT_TRUE(tap_obj.is_object())
                << "Lane " << lane << " tap " << tap_idx << " should be an object";

            std::string tap_key = "tap" + std::to_string(tap_idx);
            ASSERT_TRUE(tap_obj.contains(tap_key))
                << "Lane " << lane << " tap object at index " << tap_idx
                << " should have key '" << tap_key << "'";

            // Calculate expected value
            // tap_idx=0, base=-23: lane0=-23, lane1=-22, lane2=-21, lane3=-20
            // tap_idx=1, base=-13: lane0=-13, lane1=-12, lane2=-11, lane3=-10
            int32_t base_value = -23 + (tap_idx * 10);
            int32_t expected_value = base_value + lane;

            int32_t actual_value = tap_obj[tap_key].get<int32_t>();
            EXPECT_EQ(actual_value, expected_value)
                << "Lane " << lane << " " << tap_key << " should be " << expected_value
                << " but got " << actual_value;
        }
    }

    EXPECT_EQ(portIdQueries.load(), 0u)
        << "SAI_PORT_SERDES_ATTR_PORT_ID must not be queried from the vendor SAI";

    flexCounter->removeCounter(testPortSerdesOid);
}

/**
 * Test that collectData() collects only the successfully initialized serdes
 * attributes when one attribute fails initAttrData(), leaving others intact.
 *
 * Scenario: SAI returns failure for SAI_PORT_SERDES_ATTR_TX_FIR_COUNT so
 * m_portSerdesTapsCountMap has no entry for this RID, causing initAttrData
 * for TX_FIR_TAPS_LIST to return false. RX_VGA is fully supported.
 *
 * Expected behaviour after the fix:
 *   - tx_fir_taps_list is NOT written to COUNTERS_DB
 *   - rx_vga IS written to COUNTERS_DB with correct data
 */
TEST_F(TestPortPhySerdesAttr, CollectDataPartialSuccess)
{
    sai_object_id_t partialSerdesOid = 0x57000000000002;
    sai_object_id_t partialSerdesRid = 0x57000000000002;
    sai_object_id_t partialPortOid   = 0x1000000000002;
    sai_object_id_t partialPortRid   = 0x1000000000002;

    sai->mock_get = [this](sai_object_type_t object_type,
                      sai_object_id_t /*object_id*/,
                      uint32_t attr_count,
                      sai_attribute_t *attr_list) -> sai_status_t
    {
        if (object_type == SAI_OBJECT_TYPE_PORT_SERDES)
        {
            for (uint32_t i = 0; i < attr_count; i++)
            {
                switch (attr_list[i].id)
                {
                    case SAI_PORT_SERDES_ATTR_PORT_ID:
                        // The port comes from the translator. Fail the vendor
                        // query as the SDK did (-17), to detect any use of it.
                        portIdQueries++;
                        return SAI_STATUS_OBJECT_IN_USE;

                    case SAI_PORT_SERDES_ATTR_TX_FIR_COUNT:
                        // Simulate SDK busy / not ready: fail the count query
                        // so m_portSerdesTapsCountMap is never populated and
                        // initAttrData for TX_FIR_TAPS_LIST returns false.
                        return SAI_STATUS_OBJECT_IN_USE;

                    case SAI_PORT_SERDES_ATTR_RX_VGA:
                        if (attr_list[i].value.u32list.list == nullptr)
                        {
                            attr_list[i].value.u32list.count = TEST_LANE_COUNT;
                            return SAI_STATUS_BUFFER_OVERFLOW;
                        }
                        for (uint32_t lane = 0;
                             lane < attr_list[i].value.u32list.count
                             && lane < TEST_LANE_COUNT; lane++)
                        {
                            attr_list[i].value.u32list.list[lane] = 200 + lane;
                        }
                        attr_list[i].value.u32list.count =
                            std::min(attr_list[i].value.u32list.count,
                                     TEST_LANE_COUNT);
                        break;

                    default:
                        return SAI_STATUS_NOT_SUPPORTED;
                }
            }
            return SAI_STATUS_SUCCESS;
        }
        else if (object_type == SAI_OBJECT_TYPE_PORT)
        {
            for (uint32_t i = 0; i < attr_count; i++)
            {
                if (attr_list[i].id == SAI_PORT_ATTR_HW_LANE_LIST)
                {
                    if (attr_list[i].value.u32list.list == nullptr)
                    {
                        attr_list[i].value.u32list.count = TEST_LANE_COUNT;
                        return SAI_STATUS_BUFFER_OVERFLOW;
                    }
                    for (uint32_t lane = 0;
                         lane < attr_list[i].value.u32list.count
                         && lane < TEST_LANE_COUNT; lane++)
                    {
                        attr_list[i].value.u32list.list[lane] = lane;
                    }
                    attr_list[i].value.u32list.count =
                        std::min(attr_list[i].value.u32list.count,
                                 TEST_LANE_COUNT);
                    return SAI_STATUS_SUCCESS;
                }
            }
        }
        return SAI_STATUS_INVALID_PARAMETER;
    };

    // Map the port serdes to its port: port serdes VID -> port VID in
    // COUNTERS_DB, and port VID -> port RID in the translator
    translator->insertRidAndVid(partialPortRid, partialPortOid);
    mapSerdesToPort(partialSerdesOid, partialPortOid);

    swss::DBConnector db("COUNTERS_DB", 0);

    vector<swss::FieldValueTuple> portSerdesAttrValues;
    std::string attrIds =
        "SAI_PORT_SERDES_ATTR_RX_VGA,SAI_PORT_SERDES_ATTR_TX_FIR_TAPS_LIST";
    portSerdesAttrValues.emplace_back(PORT_PHY_SERDES_ATTR_ID_LIST, attrIds);

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT_SERDES);
    flexCounter->addCounter(
        partialSerdesOid, partialSerdesRid, portSerdesAttrValues);

    vector<swss::FieldValueTuple> pluginValues;
    pluginValues.emplace_back(POLL_INTERVAL_FIELD, "1000");
    pluginValues.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    pluginValues.emplace_back(STATS_MODE_FIELD, STATS_MODE_READ);
    flexCounter->addCounterPlugin(pluginValues);

    usleep(1000 * 1050); // 1.05 seconds - one poll cycle

    swss::RedisPipeline pipeline(&db);
    swss::Table portPhyAttrTable(&pipeline, PORT_PHY_ATTR_TABLE, false);

    std::string expectedKey = toOid(partialPortOid);

    // TX_FIR_TAPS_LIST failed initAttrData → must NOT appear in DB
    std::string txFirTapsValue;
    bool found = portPhyAttrTable.hget(
        expectedKey, "tx_fir_taps_list", txFirTapsValue);
    EXPECT_FALSE(found)
        << "tx_fir_taps_list should NOT be in COUNTERS_DB "
           "when TX_FIR_COUNT query fails";

    // RX_VGA succeeded → MUST appear in DB with correct lane values
    std::string rxVgaValue;
    found = portPhyAttrTable.hget(expectedKey, "rx_vga", rxVgaValue);
    EXPECT_TRUE(found)
        << "rx_vga MUST be in COUNTERS_DB "
           "even when TX_FIR_TAPS_LIST initAttrData failed";

    if (found)
    {
        json rxVgaJson;
        ASSERT_NO_THROW(rxVgaJson = json::parse(rxVgaValue))
            << "rx_vga is not valid JSON: " << rxVgaValue;

        EXPECT_EQ(rxVgaJson.size(), TEST_LANE_COUNT)
            << "rx_vga should have " << TEST_LANE_COUNT << " lanes";

        for (uint32_t lane = 0; lane < TEST_LANE_COUNT; lane++)
        {
            std::string lane_key = std::to_string(lane);
            ASSERT_TRUE(rxVgaJson.contains(lane_key))
                << "Missing lane " << lane << " in rx_vga";
            EXPECT_EQ(rxVgaJson[lane_key].get<uint32_t>(), 200 + lane)
                << "Lane " << lane << " VGA mismatch";
        }
    }

    EXPECT_EQ(portIdQueries.load(), 0u)
        << "SAI_PORT_SERDES_ATTR_PORT_ID must not be queried from the vendor SAI";

    flexCounter->removeCounter(partialSerdesOid);
}

/**
 * Test that the port of a port serdes is resolved without the vendor SAI.
 *
 * COUNTERS_DB maps the port serdes VID to the port VID, and the translator
 * maps the port VID to the port RID. The vendor SAI fails
 * SAI_PORT_SERDES_ATTR_PORT_ID with SAI_STATUS_OBJECT_IN_USE (-17), as the
 * SDK did while the port came up.
 *
 * Expected behaviour:
 *   - SAI_PORT_SERDES_ATTR_PORT_ID is not queried
 *   - the hardware lane count is read from the translated port RID
 *   - rx_vga is written to PORT_PHY_ATTR_TABLE with the port VID as key
 */
TEST_F(TestPortPhySerdesAttr, ResolvePortWithTranslatorNotVendorSai)
{
    const sai_object_id_t serdesVid = 0x57000000000003;
    const sai_object_id_t serdesRid = 0x57000000000103;
    const sai_object_id_t portVid   = 0x1000000000003;
    const sai_object_id_t portRid   = 0x1000000000103;

    translator->insertRidAndVid(portRid, portVid);
    mapSerdesToPort(serdesVid, portVid);
    mockVendorSai(portRid);

    pollRxVga(flexCounter, serdesVid, serdesRid);

    EXPECT_EQ(portIdQueries.load(), 0u)
        << "SAI_PORT_SERDES_ATTR_PORT_ID must not be queried from the vendor SAI";
    EXPECT_EQ(laneListQueries.load(), 1u)
        << "The lane count must be read once, from the translated port RID";

    std::string rxVgaValue;
    ASSERT_TRUE(getRxVga(portVid, rxVgaValue))
        << "rx_vga must be in COUNTERS_DB for the port VID";

    json rxVgaJson;
    ASSERT_NO_THROW(rxVgaJson = json::parse(rxVgaValue))
        << "rx_vga is not valid JSON: " << rxVgaValue;
    ASSERT_EQ(rxVgaJson.size(), TEST_LANE_COUNT);

    for (uint32_t lane = 0; lane < TEST_LANE_COUNT; lane++)
    {
        EXPECT_EQ(rxVgaJson[std::to_string(lane)].get<uint32_t>(), 100 + lane)
            << "Lane " << lane << " VGA mismatch";
    }

    std::string unexpected;
    EXPECT_FALSE(getRxVga(portRid, unexpected))
        << "rx_vga must not use the port RID as key";

    flexCounter->removeCounter(serdesVid);
}

/**
 * Test that a port serdes is not collected when the translator has no RID
 * for its port.
 *
 * Expected behaviour:
 *   - SAI_PORT_SERDES_ATTR_PORT_ID is not queried as a fallback
 *   - the hardware lane count is not queried
 *   - rx_vga is not written to PORT_PHY_ATTR_TABLE for the port
 */
TEST_F(TestPortPhySerdesAttr, UnresolvedPortIsNotCollected)
{
    const sai_object_id_t serdesVid = 0x57000000000004;
    const sai_object_id_t serdesRid = 0x57000000000104;
    const sai_object_id_t portVid   = 0x1000000000004;
    const sai_object_id_t portRid   = 0x1000000000104;

    // The translator has no RID for portVid
    mapSerdesToPort(serdesVid, portVid);
    mockVendorSai(portRid);

    pollRxVga(flexCounter, serdesVid, serdesRid);

    EXPECT_EQ(portIdQueries.load(), 0u)
        << "SAI_PORT_SERDES_ATTR_PORT_ID must not be queried from the vendor SAI";
    EXPECT_EQ(laneListQueries.load(), 0u)
        << "The lane count must not be read for an unresolved port";

    std::string rxVgaValue;
    EXPECT_FALSE(getRxVga(portVid, rxVgaValue))
        << "rx_vga must not be in COUNTERS_DB for an unresolved port";

    flexCounter->removeCounter(serdesVid);
}

/**
 * Test that a FlexCounter without a translator does not resolve the port of
 * a port serdes, and does not query the vendor SAI for it.
 */
TEST_F(TestPortPhySerdesAttr, NoTranslatorDoesNotResolvePort)
{
    const sai_object_id_t serdesVid = 0x57000000000005;
    const sai_object_id_t serdesRid = 0x57000000000105;
    const sai_object_id_t portVid   = 0x1000000000005;
    const sai_object_id_t portRid   = 0x1000000000105;

    translator->insertRidAndVid(portRid, portVid);
    mapSerdesToPort(serdesVid, portVid);
    mockVendorSai(portRid);

    auto fc = std::make_shared<FlexCounter>("TEST_PORT_SERDES_ATTR_NO_TRANSLATOR", sai, "COUNTERS_DB");

    pollRxVga(fc, serdesVid, serdesRid);

    EXPECT_EQ(portIdQueries.load(), 0u)
        << "SAI_PORT_SERDES_ATTR_PORT_ID must not be queried from the vendor SAI";
    EXPECT_EQ(laneListQueries.load(), 0u)
        << "The lane count must not be read for an unresolved port";

    std::string rxVgaValue;
    EXPECT_FALSE(getRxVga(portVid, rxVgaValue))
        << "rx_vga must not be in COUNTERS_DB without a translator";

    fc->removeCounter(serdesVid);
}

