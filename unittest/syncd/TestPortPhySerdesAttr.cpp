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
#include <functional>
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

    sai->mock_get = [](sai_object_type_t object_type,
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
                        attr_list[i].value.oid = 0x1000000000002;
                        break;

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

    // Register the serdes→port VID mapping in COUNTERS_DB
    swss::DBConnector db("COUNTERS_DB", 0);
    swss::Table portSerdesIdToPortIdTable(
        &db, "COUNTERS_PORT_SERDES_ID_TO_PORT_ID_MAP");
    portSerdesIdToPortIdTable.hset(
        "", toOid(partialSerdesOid), toOid(partialPortOid));

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

    flexCounter->removeCounter(partialSerdesOid);
}


// Exercise individual polls without enabling the background collector. The
// unittest target already uses -fno-access-control for FlexCounter internals.
class TestPortPhySerdesAttrResize : public TestPortPhySerdesAttr
{
protected:
    void SetUp() override
    {
        TestPortPhySerdesAttr::SetUp();
        flexCounter->endFlexCounterThread();

        // Use distinct VIDs and RIDs to check publication under the port VID.
        testPortSerdesOid = 0x570000000000f1;
        testPortSerdesRid = 0x570000000000b1;
        testPortOid = 0x10000000000f1;
        testPortRid = 0x10000000000b1;

        db.reset(new swss::DBConnector("COUNTERS_DB", 0));
        phyTable.reset(new swss::Table(db.get(), PORT_PHY_ATTR_TABLE));
        mappingTable.reset(new swss::Table(db.get(), "COUNTERS_PORT_SERDES_ID_TO_PORT_ID_MAP"));
        phyTable->del(toOid(testPortOid));
        phyTable->hset(toOid(testPortOid), "rx_vga", "previous-rx");
        phyTable->hset(toOid(testPortOid), "tx_fir_taps_list", "previous-taps");
        mappingTable->hset("", toOid(testPortSerdesOid), toOid(testPortOid));

        sai->mock_get = [this](sai_object_type_t type, sai_object_id_t rid,
                              uint32_t count, sai_attribute_t *attrs) -> sai_status_t
        {
            if (type == SAI_OBJECT_TYPE_PORT && rid == testPortRid &&
                count == 1 && attrs[0].id == SAI_PORT_ATTR_HW_LANE_LIST)
            {
                ++laneQueries;
                EXPECT_EQ(0u, attrs[0].value.u32list.count);
                EXPECT_EQ(nullptr, attrs[0].value.u32list.list);
                attrs[0].value.u32list.count = queriedLanes;
                return laneQueryStatus;
            }
            if (type == SAI_OBJECT_TYPE_PORT_SERDES && rid == testPortSerdesRid)
            {
                if (count == 1 && attrs[0].id == SAI_PORT_SERDES_ATTR_PORT_ID)
                {
                    attrs[0].value.oid = testPortRid;
                    return SAI_STATUS_SUCCESS;
                }
                if (count == 1 && attrs[0].id == SAI_PORT_SERDES_ATTR_TX_FIR_COUNT)
                {
                    ++tapQueries;
                    attrs[0].value.u32 = queriedTaps;
                    return tapQueryStatus;
                }
                ++collectionGets;
                if (onCollect)
                    return onCollect(count, attrs);
            }
            ADD_FAILURE() << "Unexpected GET: type=" << type << ", RID=" << toOid(rid);
            return SAI_STATUS_FAILURE;
        };
    }

    void TearDown() override
    {
        flexCounter.reset();
        if (phyTable)
        {
            phyTable->del(toOid(testPortOid));
            if (secondPortOid != SAI_NULL_OBJECT_ID)
                phyTable->del(toOid(secondPortOid));
        }
        if (mappingTable)
        {
            mappingTable->hdel("", toOid(testPortSerdesOid));
            if (secondSerdesOid != SAI_NULL_OBJECT_ID)
                mappingTable->hdel("", toOid(secondSerdesOid));
        }
        phyTable.reset();
        mappingTable.reset();
        db.reset();
        TestPortPhySerdesAttr::TearDown();
    }

    void registerCounter(bool rx = true, bool taps = true)
    {
        std::string ids;
        if (rx)
            ids = "SAI_PORT_SERDES_ATTR_RX_VGA";
        if (taps)
        {
            if (!ids.empty())
                ids += ",";
            ids += "SAI_PORT_SERDES_ATTR_TX_FIR_TAPS_LIST";
        }
        test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT_SERDES);
        flexCounter->addCounter(testPortSerdesOid, testPortSerdesRid,
                {{PORT_PHY_SERDES_ATTR_ID_LIST, ids}});
        EXPECT_EQ(0u, collectionGets);
        EXPECT_EQ(1u, laneQueries);
        EXPECT_EQ(1u, tapQueries);
    }

    void poll()
    {
        swss::Table countersTable(db.get(), "COUNTERS");
        flexCounter->collectCounters(countersTable);
    }

    void expectCapacity(uint32_t count, sai_attribute_t *attrs,
                        uint32_t lanes, uint32_t taps)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (attrs[i].id == SAI_PORT_SERDES_ATTR_RX_VGA)
            {
                EXPECT_EQ(lanes, attrs[i].value.u32list.count);
                EXPECT_NE(nullptr, attrs[i].value.u32list.list);
            }
            else
            {
                ASSERT_EQ(SAI_PORT_SERDES_ATTR_TX_FIR_TAPS_LIST, attrs[i].id);
                auto &list = attrs[i].value.portserdestaps;
                ASSERT_EQ(taps, list.count);
                ASSERT_NE(nullptr, list.list);
                for (uint32_t tap = 0; tap < list.count; ++tap)
                {
                    EXPECT_EQ(lanes, list.list[tap].count) << "tap " << tap;
                    EXPECT_NE(nullptr, list.list[tap].list) << "tap " << tap;
                }
            }
        }
    }

    // Fill only a successful GET. Guard every write by the capacity supplied
    // by the caller, so a regression fails the test instead of overrunning it.
    sai_status_t completeGet(uint32_t count, sai_attribute_t *attrs,
                             uint32_t lanes, uint32_t taps)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (attrs[i].id == SAI_PORT_SERDES_ATTR_RX_VGA)
            {
                auto &list = attrs[i].value.u32list;
                if (!list.list || list.count < lanes)
                {
                    ADD_FAILURE() << "RX_VGA buffer was not resized";
                    return SAI_STATUS_FAILURE;
                }
                for (uint32_t lane = 0; lane < lanes; ++lane)
                    list.list[lane] = 100 + lane;
                list.count = lanes;
            }
            else if (attrs[i].id == SAI_PORT_SERDES_ATTR_TX_FIR_TAPS_LIST)
            {
                auto &list = attrs[i].value.portserdestaps;
                if (!list.list || list.count < taps)
                {
                    ADD_FAILURE() << "Outer tap buffer was not resized";
                    return SAI_STATUS_FAILURE;
                }
                for (uint32_t tap = 0; tap < taps; ++tap)
                {
                    auto &inner = list.list[tap];
                    if (!inner.list || inner.count < lanes)
                    {
                        ADD_FAILURE() << "Lane buffer was not resized for tap " << tap;
                        return SAI_STATUS_FAILURE;
                    }
                    for (uint32_t lane = 0; lane < lanes; ++lane)
                        inner.list[lane] = -100 + 10 * static_cast<int32_t>(tap) + static_cast<int32_t>(lane);
                    inner.count = lanes;
                }
                list.count = taps;
            }
            else
            {
                ADD_FAILURE() << "Unexpected collection attribute " << attrs[i].id;
                return SAI_STATUS_FAILURE;
            }
        }
        return SAI_STATUS_SUCCESS;
    }

    void expectPublished(uint32_t lanes, uint32_t taps, bool rx = true, bool tx = true)
    {
        json expectedRx = json::object();
        json expectedTx = json::object();
        for (uint32_t lane = 0; lane < lanes; ++lane)
        {
            const auto key = std::to_string(lane);
            expectedRx[key] = 100 + lane;
            expectedTx[key] = json::array();
            for (uint32_t tap = 0; tap < taps; ++tap)
            {
                const int32_t value = -100 + 10 * static_cast<int32_t>(tap) + static_cast<int32_t>(lane);
                expectedTx[key].push_back({{"tap" + std::to_string(tap), value}});
            }
        }
        std::string value;
        if (rx)
        {
            ASSERT_TRUE(phyTable->hget(toOid(testPortOid), "rx_vga", value));
            EXPECT_EQ(expectedRx, json::parse(value));
        }
        if (tx)
        {
            ASSERT_TRUE(phyTable->hget(toOid(testPortOid), "tx_fir_taps_list", value));
            EXPECT_EQ(expectedTx, json::parse(value));
        }
    }

    void expectUnchanged()
    {
        std::string value;
        ASSERT_TRUE(phyTable->hget(toOid(testPortOid), "rx_vga", value));
        EXPECT_EQ("previous-rx", value);
        ASSERT_TRUE(phyTable->hget(toOid(testPortOid), "tx_fir_taps_list", value));
        EXPECT_EQ("previous-taps", value);
    }

    std::unique_ptr<swss::DBConnector> db;
    std::unique_ptr<swss::Table> phyTable;
    std::unique_ptr<swss::Table> mappingTable;
    std::function<sai_status_t(uint32_t, sai_attribute_t *)> onCollect;
    uint32_t queriedLanes = 2;
    uint32_t queriedTaps = 3;
    uint32_t laneQueries = 0;
    uint32_t tapQueries = 0;
    uint32_t collectionGets = 0;
    sai_status_t laneQueryStatus = SAI_STATUS_BUFFER_OVERFLOW;
    sai_status_t tapQueryStatus = SAI_STATUS_SUCCESS;
    sai_object_id_t secondPortOid = SAI_NULL_OBJECT_ID;
    sai_object_id_t secondSerdesOid = SAI_NULL_OBJECT_ID;
};

TEST_F(TestPortPhySerdesAttrResize, TapCountGrowthRetriesAndCachesCapacity)
{
    registerCounter(false, true);
    onCollect = [this](uint32_t count, sai_attribute_t *attrs) -> sai_status_t
    {
        EXPECT_EQ(1u, count);
        expectCapacity(count, attrs, 2, collectionGets == 1 ? 3 : 6);
        if (collectionGets == 1)
        {
            // Only the required outer count changes. There are still just
            // three allocated entries; the overflow handler must not read six.
            attrs[0].value.portserdestaps.count = 6;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        if (collectionGets == 2)
            expectUnchanged();
        return completeGet(count, attrs, 2, 6);
    };

    poll();
    ASSERT_EQ(2u, collectionGets);
    expectPublished(2, 6, false, true);
    poll();
    EXPECT_EQ(3u, collectionGets);
    expectPublished(2, 6, false, true);
    // Explicit queries still report 3 taps: returned list counts take priority.
    EXPECT_EQ(1u, laneQueries);
    EXPECT_EQ(1u, tapQueries);
}

TEST_F(TestPortPhySerdesAttrResize, RxVgaLaneGrowthRetriesAndCachesCapacity)
{
    registerCounter(true, false);
    onCollect = [this](uint32_t count, sai_attribute_t *attrs) -> sai_status_t
    {
        EXPECT_EQ(1u, count);
        expectCapacity(count, attrs, collectionGets == 1 ? 2 : 4, 3);
        if (collectionGets == 1)
        {
            attrs[0].value.u32list.count = 4;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        return completeGet(count, attrs, 4, 3);
    };

    poll();
    ASSERT_EQ(2u, collectionGets);
    expectPublished(4, 3, true, false);
    poll();
    EXPECT_EQ(3u, collectionGets);
    EXPECT_EQ(1u, laneQueries);
    EXPECT_EQ(1u, tapQueries);
}

TEST_F(TestPortPhySerdesAttrResize, InnerLaneGrowthUsesLargestReturnedCount)
{
    registerCounter(false, true);
    onCollect = [this](uint32_t count, sai_attribute_t *attrs) -> sai_status_t
    {
        expectCapacity(count, attrs, collectionGets == 1 ? 2 : 8, 3);
        if (collectionGets == 1)
        {
            auto &list = attrs[0].value.portserdestaps;
            list.list[0].count = 4;
            list.list[2].count = 8;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        return completeGet(count, attrs, 8, 3);
    };

    poll();
    EXPECT_EQ(2u, collectionGets);
    expectPublished(8, 3, false, true);
    EXPECT_EQ(1u, laneQueries);
    EXPECT_EQ(1u, tapQueries);
}

TEST_F(TestPortPhySerdesAttrResize, TapThenLaneOverflowUsesThirdAttempt)
{
    registerCounter(false, true);
    onCollect = [this](uint32_t count, sai_attribute_t *attrs) -> sai_status_t
    {
        expectCapacity(count, attrs, collectionGets < 3 ? 2 : 4,
                       collectionGets == 1 ? 3 : 6);
        if (collectionGets == 1)
        {
            attrs[0].value.portserdestaps.count = 6;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        if (collectionGets == 2)
        {
            attrs[0].value.portserdestaps.list[5].count = 4;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        return completeGet(count, attrs, 4, 6);
    };

    poll();
    ASSERT_EQ(3u, collectionGets);
    expectPublished(4, 6, false, true);
    poll();
    EXPECT_EQ(4u, collectionGets);
    EXPECT_EQ(1u, laneQueries);
    EXPECT_EQ(1u, tapQueries);
}

TEST_F(TestPortPhySerdesAttrResize, SimultaneousGrowthRebuildsBothAttributeBuffers)
{
    registerCounter();
    onCollect = [this](uint32_t count, sai_attribute_t *attrs) -> sai_status_t
    {
        EXPECT_EQ(2u, count);
        expectCapacity(count, attrs, collectionGets == 1 ? 2 : 4,
                       collectionGets == 1 ? 3 : 6);
        if (collectionGets == 1)
        {
            for (uint32_t i = 0; i < count; ++i)
            {
                if (attrs[i].id == SAI_PORT_SERDES_ATTR_RX_VGA)
                    attrs[i].value.u32list.count = 4;
                else
                {
                    attrs[i].value.portserdestaps.list[0].count = 4;
                    attrs[i].value.portserdestaps.count = 6;
                }
            }
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        return completeGet(count, attrs, 4, 6);
    };

    poll();
    EXPECT_EQ(2u, collectionGets);
    expectPublished(4, 6);
    EXPECT_EQ(1u, laneQueries);
    EXPECT_EQ(1u, tapQueries);
}

struct SerdesFallbackCounts
{
    uint32_t initialLanes;
    uint32_t initialTaps;
    uint32_t reportedLanes;
    uint32_t reportedTaps;
    uint32_t expectedLaneCapacity;
    uint32_t expectedTapCapacity;
};

class TestPortPhySerdesAttrFallback : public TestPortPhySerdesAttrResize,
                                    public ::testing::WithParamInterface<SerdesFallbackCounts>
{
};

TEST_P(TestPortPhySerdesAttrFallback, QueriesGrowBuffersWithoutShrinkingOtherDimension)
{
    const auto sizes = GetParam();
    queriedLanes = sizes.initialLanes;
    queriedTaps = sizes.initialTaps;
    registerCounter();
    queriedLanes = sizes.reportedLanes;
    queriedTaps = sizes.reportedTaps;
    onCollect = [this, sizes](uint32_t count, sai_attribute_t *attrs) -> sai_status_t
    {
        expectCapacity(count, attrs,
                collectionGets == 1 ? sizes.initialLanes : sizes.expectedLaneCapacity,
                collectionGets == 1 ? sizes.initialTaps : sizes.expectedTapCapacity);
        if (collectionGets == 1)
            return SAI_STATUS_BUFFER_OVERFLOW; // Vendor leaves all counts unchanged.
        return completeGet(count, attrs, sizes.reportedLanes, sizes.reportedTaps);
    };

    poll();
    ASSERT_EQ(2u, collectionGets);
    expectPublished(sizes.reportedLanes, sizes.reportedTaps);
    EXPECT_EQ(2u, laneQueries);
    EXPECT_EQ(2u, tapQueries);
    poll();
    EXPECT_EQ(3u, collectionGets);
    EXPECT_EQ(2u, laneQueries);
    EXPECT_EQ(2u, tapQueries);
}

INSTANTIATE_TEST_SUITE_P(CountChanges, TestPortPhySerdesAttrFallback,
        ::testing::Values(
            SerdesFallbackCounts{2, 3, 4, 3, 4, 3},
            SerdesFallbackCounts{2, 3, 2, 6, 2, 6},
            SerdesFallbackCounts{2, 3, 4, 6, 4, 6},
            SerdesFallbackCounts{4, 6, 2, 8, 4, 8},
            SerdesFallbackCounts{4, 6, 8, 3, 8, 6}));

TEST_F(TestPortPhySerdesAttrResize, OverflowWithoutLargerCountsStops)
{
    registerCounter();
    onCollect = [this](uint32_t count, sai_attribute_t *attrs) -> sai_status_t
    {
        expectCapacity(count, attrs, 2, 3);
        return SAI_STATUS_BUFFER_OVERFLOW;
    };

    poll();
    EXPECT_EQ(1u, collectionGets);
    EXPECT_EQ(2u, laneQueries);
    EXPECT_EQ(2u, tapQueries);
    expectUnchanged();
}

TEST_F(TestPortPhySerdesAttrResize, FailedCountQueriesStopWithoutRetry)
{
    registerCounter();
    laneQueryStatus = SAI_STATUS_FAILURE;
    tapQueryStatus = SAI_STATUS_FAILURE;
    // Failed queries must not corrupt cached capacities, even if they write data.
    queriedLanes = 9;
    queriedTaps = 9;
    onCollect = [](uint32_t, sai_attribute_t *) -> sai_status_t
    {
        return SAI_STATUS_BUFFER_OVERFLOW;
    };

    poll();
    ASSERT_EQ(1u, collectionGets);
    EXPECT_EQ(2u, laneQueries);
    EXPECT_EQ(2u, tapQueries);
    expectUnchanged();
    onCollect = [this](uint32_t count, sai_attribute_t *attrs) -> sai_status_t
    {
        expectCapacity(count, attrs, 2, 3);
        return completeGet(count, attrs, 2, 3);
    };
    poll();
    EXPECT_EQ(2u, collectionGets);
    expectPublished(2, 3);
}

TEST_F(TestPortPhySerdesAttrResize, RetryLimitPreservesRequirementsForNextPoll)
{
    registerCounter(false, true);
    onCollect = [this](uint32_t count, sai_attribute_t *attrs) -> sai_status_t
    {
        expectCapacity(count, attrs, 2, 2 + collectionGets);
        ++attrs[0].value.portserdestaps.count;
        return SAI_STATUS_BUFFER_OVERFLOW;
    };

    poll();
    ASSERT_EQ(3u, collectionGets);
    expectUnchanged();
    onCollect = [this](uint32_t count, sai_attribute_t *attrs) -> sai_status_t
    {
        expectCapacity(count, attrs, 2, 6);
        return completeGet(count, attrs, 2, 6);
    };
    poll();
    EXPECT_EQ(4u, collectionGets);
    expectPublished(2, 6, false, true);
    EXPECT_EQ(1u, laneQueries);
    EXPECT_EQ(1u, tapQueries);
}

TEST_F(TestPortPhySerdesAttrResize, SmallerSuccessfulListsDoNotShrinkCapacity)
{
    queriedLanes = 4;
    queriedTaps = 6;
    registerCounter();
    onCollect = [this](uint32_t count, sai_attribute_t *attrs) -> sai_status_t
    {
        expectCapacity(count, attrs, 4, 6);
        return completeGet(count, attrs, 2, 3);
    };

    poll();
    expectPublished(2, 3);
    poll();
    EXPECT_EQ(2u, collectionGets);
    expectPublished(2, 3);
    EXPECT_EQ(1u, laneQueries);
    EXPECT_EQ(1u, tapQueries);
}

class TestPortPhySerdesAttrGetFailure : public TestPortPhySerdesAttrResize,
                                      public ::testing::WithParamInterface<bool>
{
};

TEST_P(TestPortPhySerdesAttrGetFailure, OrdinaryErrorDoesNotRetryOrPublish)
{
    const bool failAfterResize = GetParam();
    registerCounter(false, true);
    onCollect = [this, failAfterResize](uint32_t count, sai_attribute_t *attrs) -> sai_status_t
    {
        expectCapacity(count, attrs, 2, collectionGets == 1 ? 3 : 6);
        if (failAfterResize && collectionGets == 1)
        {
            attrs[0].value.portserdestaps.count = 6;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        return SAI_STATUS_FAILURE;
    };

    poll();
    EXPECT_EQ(failAfterResize ? 2u : 1u, collectionGets);
    EXPECT_EQ(1u, laneQueries);
    EXPECT_EQ(1u, tapQueries);
    expectUnchanged();
}

INSTANTIATE_TEST_SUITE_P(InitialAndRetry, TestPortPhySerdesAttrGetFailure,
        ::testing::Bool());

TEST_F(TestPortPhySerdesAttrResize, PartialInitializationStillAllowsResizeRetry)
{
    tapQueryStatus = SAI_STATUS_FAILURE;
    phyTable->hdel(toOid(testPortOid), "tx_fir_taps_list");
    registerCounter();
    onCollect = [this](uint32_t count, sai_attribute_t *attrs) -> sai_status_t
    {
        EXPECT_EQ(1u, count);
        EXPECT_EQ(SAI_PORT_SERDES_ATTR_RX_VGA, attrs[0].id);
        expectCapacity(count, attrs, collectionGets == 1 ? 2 : 4, 0);
        if (collectionGets == 1)
        {
            attrs[0].value.u32list.count = 4;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        return completeGet(count, attrs, 4, 0);
    };

    poll();
    EXPECT_EQ(2u, collectionGets);
    expectPublished(4, 0, true, false);
    std::string value;
    EXPECT_FALSE(phyTable->hget(toOid(testPortOid), "tx_fir_taps_list", value));
    EXPECT_EQ(1u, laneQueries);
    EXPECT_EQ(1u, tapQueries);
}

TEST_F(TestPortPhySerdesAttrResize, NoInitializedAttributesSkipsCollectionGet)
{
    laneQueryStatus = SAI_STATUS_FAILURE;
    registerCounter();
    poll();
    EXPECT_EQ(0u, collectionGets);
    EXPECT_EQ(1u, laneQueries);
    EXPECT_EQ(1u, tapQueries);
    expectUnchanged();
}

TEST_F(TestPortPhySerdesAttrResize, FailedObjectDoesNotBlockOtherObjects)
{
    registerCounter();
    onCollect = [](uint32_t, sai_attribute_t *) -> sai_status_t
    {
        return SAI_STATUS_BUFFER_OVERFLOW; // No progress after the fallback queries.
    };

    secondSerdesOid = testPortSerdesOid + 1;
    secondPortOid = testPortOid + 1;
    const sai_object_id_t secondSerdesRid = testPortSerdesRid + 1;
    const sai_object_id_t secondPortRid = testPortRid + 1;
    mappingTable->hset("", toOid(secondSerdesOid), toOid(secondPortOid));
    phyTable->del(toOid(secondPortOid));
    uint32_t secondGets = 0;
    const auto firstGet = sai->mock_get;
    sai->mock_get = [&, firstGet](sai_object_type_t type, sai_object_id_t rid,
                                uint32_t count, sai_attribute_t *attrs) -> sai_status_t
    {
        if (type == SAI_OBJECT_TYPE_PORT && rid == secondPortRid && count == 1 &&
            attrs[0].id == SAI_PORT_ATTR_HW_LANE_LIST)
        {
            attrs[0].value.u32list.count = 2;
            return SAI_STATUS_BUFFER_OVERFLOW;
        }
        if (type == SAI_OBJECT_TYPE_PORT_SERDES && rid == secondSerdesRid)
        {
            if (count == 1 && attrs[0].id == SAI_PORT_SERDES_ATTR_PORT_ID)
            {
                attrs[0].value.oid = secondPortRid;
                return SAI_STATUS_SUCCESS;
            }
            if (count == 1 && attrs[0].id == SAI_PORT_SERDES_ATTR_TX_FIR_COUNT)
            {
                attrs[0].value.u32 = 3;
                return SAI_STATUS_SUCCESS;
            }
            ++secondGets;
            return completeGet(count, attrs, 2, 3);
        }
        return firstGet(type, rid, count, attrs);
    };
    flexCounter->addCounter(secondSerdesOid, secondSerdesRid,
            {{PORT_PHY_SERDES_ATTR_ID_LIST, "SAI_PORT_SERDES_ATTR_RX_VGA"}});

    poll();
    EXPECT_EQ(1u, collectionGets);
    EXPECT_EQ(1u, secondGets);
    expectUnchanged();
    std::string value;
    ASSERT_TRUE(phyTable->hget(toOid(secondPortOid), "rx_vga", value));
    EXPECT_EQ((json{{"0", 100}, {"1", 101}}), json::parse(value));
}
