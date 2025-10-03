/**
 * @file TestPortSerdesAttr.cpp
 * @brief Unit tests for PORT_SERDES_ATTR flex counter functionality
 *
 * Tests implementation according to UT Plan:
 * 1. sai_serialize_port_attr() function
 * 2. sai_deserialize_port_attr() function
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

using namespace saimeta;
using namespace sairedis;
using namespace syncd;
using namespace std;

static const std::string ATTR_TYPE_PORT_SERDES = "Port Physical Link Attributes";

template <typename T>
std::string toOid(T value)
{
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream << "oid:0x" << std::hex << value;
    return ostream.str();
}


class TestPortSerdesAttr : public ::testing::Test
{
protected:
    void SetUp() override
    {
        sai = std::make_shared<MockableSaiInterface>();

        sai->mock_switchIdQuery = [](sai_object_id_t) {
            return 0x21000000000000;
        };

        flexCounter = std::make_shared<FlexCounter>("TEST_PORT_SERDES_ATTR", sai, "COUNTERS_DB");

        // Setup test port OID
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
    sai_object_id_t testPortOid;
    sai_object_id_t testPortRid;
};

TEST_F(TestPortSerdesAttr, SerializePortAttr)
{
    sai_port_attr_t attr = SAI_PORT_ATTR_RX_SIGNAL_DETECT;
    std::string result = sai_serialize_port_attr(attr);
    EXPECT_EQ(result, "SAI_PORT_ATTR_RX_SIGNAL_DETECT");

    attr = SAI_PORT_ATTR_FEC_ALIGNMENT_LOCK;
    result = sai_serialize_port_attr(attr);
    EXPECT_EQ(result, "SAI_PORT_ATTR_FEC_ALIGNMENT_LOCK");

    attr = SAI_PORT_ATTR_RX_SNR;
    result = sai_serialize_port_attr(attr);
    EXPECT_EQ(result, "SAI_PORT_ATTR_RX_SNR");
}

TEST_F(TestPortSerdesAttr, DeserializePortAttr)
{
    sai_port_attr_t attr_out;

    std::string input = "SAI_PORT_ATTR_RX_SIGNAL_DETECT";
    sai_deserialize_port_attr(input, attr_out);
    EXPECT_EQ(attr_out, SAI_PORT_ATTR_RX_SIGNAL_DETECT);

    input = "SAI_PORT_ATTR_FEC_ALIGNMENT_LOCK";
    sai_deserialize_port_attr(input, attr_out);
    EXPECT_EQ(attr_out, SAI_PORT_ATTR_FEC_ALIGNMENT_LOCK);

    input = "SAI_PORT_ATTR_RX_SNR";
    sai_deserialize_port_attr(input, attr_out);
    EXPECT_EQ(attr_out, SAI_PORT_ATTR_RX_SNR);
}

/**
 * Test collectData() with mocked SAI and COUNTERS_DB validation
 * This test verifies the complete data collection workflow:
 * 1. Mock SAI interface returns realistic SERDES attribute data
 * 2. FlexCounter collects the data via collectData()
 * 3. Verify collected data is properly written to COUNTERS_DB
 *
 * This test validates the complete PORT_SERDES_ATTR collection workflow
 * including RX_SIGNAL_DETECT, FEC_ALIGNMENT_LOCK, and RX_SNR attributes.
 */
TEST_F(TestPortSerdesAttr, CollectDataAndValidateCountersDB)
{
    // Setup mock for SERDES attributes with realistic data
    sai->mock_get = [](sai_object_type_t object_type,
                      sai_object_id_t object_id,
                      uint32_t attr_count,
                      sai_attribute_t *attr_list) -> sai_status_t
    {
        if (object_type != SAI_OBJECT_TYPE_PORT) {
            return SAI_STATUS_INVALID_PARAMETER;
        }

        for (uint32_t i = 0; i < attr_count; i++) {
            switch (attr_list[i].id) {
                case SAI_PORT_ATTR_RX_SIGNAL_DETECT:
                    if (attr_list[i].value.portlanelatchstatuslist.list != nullptr) {
                        uint32_t count = attr_list[i].value.portlanelatchstatuslist.count;
                        for (uint32_t lane = 0; lane < count && lane < MAX_LANES_PER_PORT; lane++) {
                            attr_list[i].value.portlanelatchstatuslist.list[lane].lane = lane;
                            attr_list[i].value.portlanelatchstatuslist.list[lane].value.changed = true;
                            attr_list[i].value.portlanelatchstatuslist.list[lane].value.current_status = (lane % 2 == 0);
                        }
                        attr_list[i].value.portlanelatchstatuslist.count = std::min(count, static_cast<uint32_t>(MAX_LANES_PER_PORT));
                    }
                    break;
                case SAI_PORT_ATTR_FEC_ALIGNMENT_LOCK:
                    if (attr_list[i].value.portlanelatchstatuslist.list != nullptr) {
                        uint32_t count = attr_list[i].value.portlanelatchstatuslist.count;
                        for (uint32_t lane = 0; lane < count && lane < MAX_LANES_PER_PORT; lane++) {
                            attr_list[i].value.portlanelatchstatuslist.list[lane].lane = lane;
                            attr_list[i].value.portlanelatchstatuslist.list[lane].value.changed = (lane % 2 == 0);
                            attr_list[i].value.portlanelatchstatuslist.list[lane].value.current_status = false;
                        }
                        attr_list[i].value.portlanelatchstatuslist.count = std::min(count, static_cast<uint32_t>(MAX_LANES_PER_PORT));
                    }
                    break;
                case SAI_PORT_ATTR_RX_SNR:
                    if (attr_list[i].value.portsnrlist.list != nullptr) {
                        uint32_t count = attr_list[i].value.portsnrlist.count;
                        for (uint32_t lane = 0; lane < count && lane < MAX_LANES_PER_PORT; lane++) {
                            attr_list[i].value.portsnrlist.list[lane].lane = lane;
                            attr_list[i].value.portsnrlist.list[lane].snr = static_cast<sai_uint16_t>(145 + (lane * 5));
                        }
                        attr_list[i].value.portsnrlist.count = std::min(count, static_cast<uint32_t>(MAX_LANES_PER_PORT));
                    }
                    break;
                default:
                    return SAI_STATUS_NOT_SUPPORTED;
            }
        }
        return SAI_STATUS_SUCCESS;
    };

    vector<swss::FieldValueTuple> portSerdesValues;

    std::string attrIds = "SAI_PORT_ATTR_RX_SIGNAL_DETECT,SAI_PORT_ATTR_FEC_ALIGNMENT_LOCK,SAI_PORT_ATTR_RX_SNR";

    portSerdesValues.emplace_back(PORT_SERDES_ATTR_ID_LIST, attrIds);

    test_syncd::mockVidManagerObjectTypeQuery(SAI_OBJECT_TYPE_PORT);

    flexCounter->addCounter(testPortOid, testPortRid, portSerdesValues);

    vector<swss::FieldValueTuple> pluginValues;
    pluginValues.emplace_back(POLL_INTERVAL_FIELD, "1000");
    pluginValues.emplace_back(FLEX_COUNTER_STATUS_FIELD, "enable");
    pluginValues.emplace_back(STATS_MODE_FIELD, STATS_MODE_READ);
    flexCounter->addCounterPlugin(pluginValues);

    usleep(1000 * 1050); // 1.05 seconds to ensure at least one poll cycle

    // Connect to COUNTERS_DB and verify entries
    swss::DBConnector db("COUNTERS_DB", 0);
    swss::RedisPipeline pipeline(&db);
    swss::Table countersTable(&pipeline, COUNTERS_TABLE, false);

    std::string expectedKey = toOid(testPortOid);

    // Validate actual values against mocked data
    std::string rxSignalDetectValue;
    bool found = countersTable.hget(expectedKey, "SAI_PORT_ATTR_RX_SIGNAL_DETECT", rxSignalDetectValue);
    EXPECT_TRUE(found) << "SAI_PORT_ATTR_RX_SIGNAL_DETECT not found in COUNTERS_DB";

    EXPECT_TRUE(rxSignalDetectValue.find("lane") != std::string::npos) << "Serialized data missing lane information";
    EXPECT_TRUE(rxSignalDetectValue.find("count") != std::string::npos) << "Serialized data missing count field";

    for (uint32_t lane = 0; lane < MAX_LANES_PER_PORT; lane++) {
        std::ostringstream expected_json;
        //expected pattern : {"lane":<lane_no>,"value":"<changed>:<current_status>"}
        expected_json << "{\"lane\":" << lane << ",\"value\":\""
                     << "true:" << ((lane % 2 == 0) ? "true" : "false")
                     << "\"}";
        EXPECT_TRUE(rxSignalDetectValue.find(expected_json.str()) != std::string::npos)
            << "Lane " << lane << " should have changed=true" 
            << ", current_status=" << ((lane % 2 == 0) ?"true" : "false");
    }

    std::string fecAlignmentValue;
    found = countersTable.hget(expectedKey, "SAI_PORT_ATTR_FEC_ALIGNMENT_LOCK", fecAlignmentValue);
    EXPECT_TRUE(found) << "SAI_PORT_ATTR_FEC_ALIGNMENT_LOCK not found in COUNTERS_DB";

    for (uint32_t lane = 0; lane < MAX_LANES_PER_PORT; lane++) {
        std::ostringstream expected_json;
        //expected pattern : {"lane":<lane_no>,"value":"<changed>:<current_status>"}
        expected_json << "{\"lane\":" << lane << ",\"value\":\""
                     <<((lane % 2 == 0) ? "true" : "false") << ":false"
                     << "\"}";
        EXPECT_TRUE(fecAlignmentValue.find(expected_json.str()) != std::string::npos)
            << "FEC Lane " << lane << " should have changed="<<((lane % 2 == 0) ?"true" : "false") <<" , current_status=false";
    }

    std::string rxSnrValue;
    found = countersTable.hget(expectedKey, "SAI_PORT_ATTR_RX_SNR", rxSnrValue);
    EXPECT_TRUE(found) << "SAI_PORT_ATTR_RX_SNR not found in COUNTERS_DB";
    EXPECT_TRUE(rxSnrValue.find("lane") != std::string::npos) << "RX_SNR missing lane information";
    EXPECT_TRUE(rxSnrValue.find("snr") != std::string::npos) << "RX_SNR missing SNR values";
    EXPECT_TRUE(rxSnrValue.find("count") != std::string::npos) << "RX_SNR missing count field";

    // Validate all lanes (0-7) with SNR values: 145, 150, 155, 160, 165, 170, 175, 180
    for (uint32_t lane = 0; lane < MAX_LANES_PER_PORT; lane++) {
        uint32_t expected_snr = 145 + (lane * 5);
        std::ostringstream expected_json;
        expected_json << "{\"lane\":\"" << lane << "\",\"snr\":\"" << expected_snr << "\"}";
        EXPECT_TRUE(rxSnrValue.find(expected_json.str()) != std::string::npos)
            << "Lane " << lane << " SNR should be " << expected_snr;
    }

    flexCounter->removeCounter(testPortOid);
}
