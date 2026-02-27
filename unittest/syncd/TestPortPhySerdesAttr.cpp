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
#include "sai_serialize.h"
#include "MockableSaiInterface.h"
#include "MockHelper.h"
#include "swss/table.h"
#include "swss/schema.h"
#include "syncd/SaiSwitch.h"
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

        flexCounter = std::make_shared<FlexCounter>("TEST_PORT_SERDES_ATTR", sai, "COUNTERS_DB");

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
        sai.reset();
    }

    std::shared_ptr<MockableSaiInterface> sai;
    std::shared_ptr<FlexCounter> flexCounter;
    sai_object_id_t testPortSerdesOid;
    sai_object_id_t testPortSerdesRid;
    sai_object_id_t testPortOid;
    sai_object_id_t testPortRid;
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
    sai->mock_get = [](sai_object_type_t object_type,
                      sai_object_id_t object_id,
                      uint32_t attr_count,
                      sai_attribute_t *attr_list) -> sai_status_t
    {
        if (object_type == SAI_OBJECT_TYPE_PORT_SERDES) {
            for (uint32_t i = 0; i < attr_count; i++) {
                switch (attr_list[i].id) {
                    case SAI_PORT_SERDES_ATTR_PORT_ID:
                        // Return the associated port OID
                        attr_list[i].value.oid = 0x1000000000001;
                        break;

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

    // Setup COUNTERS_PORT_SERDES_ID_TO_PORT_ID_MAP in COUNTERS_DB
    swss::DBConnector db("COUNTERS_DB", 0);
    swss::Table portSerdesIdToPortIdTable(&db, "COUNTERS_PORT_SERDES_ID_TO_PORT_ID_MAP");
    portSerdesIdToPortIdTable.hset("", toOid(testPortSerdesOid), toOid(testPortOid));

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

    flexCounter->removeCounter(testPortSerdesOid);
}

