#include "FlowDump.h"
#include "meta/sai_serialize.h"
#include "swss/logger.h"

#include <gtest/gtest.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <zlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <arpa/inet.h>
#include <thread>
#include <chrono>
#include <algorithm>

using namespace syncd;

class FlowDumpTest : public ::testing::Test
{
    protected:

        virtual void SetUp()
        {
            SWSS_LOG_ENTER();
            // Reset FlowDumpWriter base path to default for each test
            FlowDumpWriter& writer = FlowDumpWriter::getInstance();
            writer.setBasePath(FlowDumpWriter::DEFAULT_BASE_PATH);

            // Set up test data
            memset(&m_flow_entry, 0, sizeof(m_flow_entry));
            memset(&m_event_data, 0, sizeof(m_event_data));

            // Set up a test flow entry
            m_flow_entry.vnet_id = 1; // uint16_t, so use small values
            m_flow_entry.ip_proto = 6; // TCP
            m_flow_entry.src_port = 12345;
            m_flow_entry.dst_port = 80;

            // Set up MAC address
            uint8_t eni_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
            memcpy(m_flow_entry.eni_mac, eni_mac, 6);

            // Set up source IP (IPv4) - use network byte order
            m_flow_entry.src_ip.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
            in_addr_t src_ip = inet_addr("10.10.10.10");
            m_flow_entry.src_ip.addr.ip4 = src_ip;

            // Set up destination IP (IPv4) - use network byte order
            m_flow_entry.dst_ip.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
            in_addr_t dst_ip = inet_addr("192.168.0.1");
            m_flow_entry.dst_ip.addr.ip4 = dst_ip;

            // Set up event data
            m_event_data.event_type = SAI_FLOW_BULK_GET_SESSION_EVENT_FLOW_ENTRY;
            m_event_data.flow_entry = m_flow_entry;
            m_event_data.attr_count = 0;
            m_event_data.attr = nullptr;
        }

        virtual void TearDown()
        {
            SWSS_LOG_ENTER();
            // Clean up test files
            removeTestDirectory(m_test_dir);
        }

        void removeTestDirectory(const std::string& dir_path)
        {
            SWSS_LOG_ENTER();
            DIR* dir = opendir(dir_path.c_str());
            if (dir != nullptr)
            {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr)
                {
                    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
                    {
                        std::string file_path = dir_path + "/" + entry->d_name;
                        unlink(file_path.c_str());
                    }
                }
                closedir(dir);
                rmdir(dir_path.c_str());
            }
        }

        void clearTestDirectory(const std::string& dir_path)
        {
            SWSS_LOG_ENTER();
            // Clear any existing files in the directory
            DIR* dir = opendir(dir_path.c_str());
            if (dir != nullptr)
            {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr)
                {
                    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
                    {
                        std::string file_path = dir_path + "/" + entry->d_name;
                        unlink(file_path.c_str());
                    }
                }
                closedir(dir);
            }
            else
            {
                // Directory doesn't exist, create it
                mkdir(dir_path.c_str(), 0755);
            }
        }

        sai_flow_entry_t m_flow_entry;
        sai_flow_bulk_get_session_event_data_t m_event_data;
        std::string m_test_dir = "/tmp/test_flow_dump";
};

// Test FlowDumpSerializer with a single flow entry
TEST_F(FlowDumpTest, FlowDumpSerializer_SingleFlowEntry)
{
    SWSS_LOG_ENTER();
    nlohmann::json json_line = FlowDumpSerializer::serializeFlowEntryToJson(m_event_data);

    // Verify flow entry fields are serialized correctly
    EXPECT_EQ(json_line["vn"], 1);
    EXPECT_EQ(json_line["pr"], 6);
    EXPECT_EQ(json_line["sp"], 12345);
    EXPECT_EQ(json_line["dp"], 80);
    EXPECT_EQ(json_line["em"], "00:11:22:33:44:55");
    EXPECT_EQ(json_line["si"], "10.10.10.10");
    EXPECT_EQ(json_line["di"], "192.168.0.1");
}

// Test epoch difference when serializing with 1 sec gap
TEST_F(FlowDumpTest, FlowDumpSerializer_EpochDifferenceAfterOneSecondGap)
{
    SWSS_LOG_ENTER();
    nlohmann::json json_line1 = FlowDumpSerializer::serializeFlowEntryToJson(m_event_data);
    ASSERT_TRUE(json_line1.contains("epoch"));
    int64_t epoch1 = json_line1["epoch"].get<int64_t>();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    nlohmann::json json_line2 = FlowDumpSerializer::serializeFlowEntryToJson(m_event_data);
    ASSERT_TRUE(json_line2.contains("epoch"));
    int64_t epoch2 = json_line2["epoch"].get<int64_t>();

    int64_t epoch_diff = epoch2 - epoch1;
    // Epoch difference should be <= 1 sec (gap) + 1 sec (delta tolerance)
    EXPECT_LE(epoch_diff, 2) << "epoch difference " << epoch_diff << "s should be <= 2 (1s gap + 1s delta)";
}

// Test FlowDumpSerializer with multiple flow entries
TEST_F(FlowDumpTest, FlowDumpSerializer_MultipleFlowEntries)
{
    SWSS_LOG_ENTER();
    // First flow entry
    sai_flow_bulk_get_session_event_data_t event_data1 = m_event_data;

    // Second flow entry with different values
    sai_flow_bulk_get_session_event_data_t event_data2 = m_event_data;
    event_data2.flow_entry.vnet_id = 2; // uint16_t, so use small values
    event_data2.flow_entry.src_port = 54321;
    event_data2.flow_entry.dst_port = 443;

    nlohmann::json json_line1 = FlowDumpSerializer::serializeFlowEntryToJson(event_data1);
    nlohmann::json json_line2 = FlowDumpSerializer::serializeFlowEntryToJson(event_data2);

    // Verify first entry
    EXPECT_EQ(json_line1["vn"], 1);
    EXPECT_EQ(json_line1["sp"], 12345);

    // Verify second entry
    EXPECT_EQ(json_line2["vn"], 2);
    EXPECT_EQ(json_line2["sp"], 54321);
    EXPECT_EQ(json_line2["dp"], 443);
}

// Test FlowDumpSerializer with FINISHED event (should still serialize, event_type is not checked)
TEST_F(FlowDumpTest, FlowDumpSerializer_FinishedEvent)
{
    SWSS_LOG_ENTER();
    sai_flow_bulk_get_session_event_data_t event_data = m_event_data;
    event_data.event_type = SAI_FLOW_BULK_GET_SESSION_EVENT_FINISHED;

    // serializeFlowEntryToJson doesn't check event_type, it just serializes the flow_entry
    nlohmann::json json_line = FlowDumpSerializer::serializeFlowEntryToJson(event_data);

    // Should still serialize the flow entry data
    EXPECT_EQ(json_line["vn"], 1);
    EXPECT_EQ(json_line["sp"], 12345);
}

// Test FlowDumpSerializer with flow attributes
TEST_F(FlowDumpTest, FlowDumpSerializer_WithAttributes)
{
    SWSS_LOG_ENTER();
    sai_flow_bulk_get_session_event_data_t event_data = m_event_data;

    // Set up attributes
    sai_attribute_t attrs[4];
    memset(attrs, 0, sizeof(attrs));

    // Attribute 1: Version (uint32)
    attrs[0].id = SAI_FLOW_ENTRY_ATTR_VERSION;
    attrs[0].value.u32 = 12345;

    // Attribute 2: Is unidirectional flow (bool)
    attrs[1].id = SAI_FLOW_ENTRY_ATTR_IS_UNIDIRECTIONAL_FLOW;
    attrs[1].value.booldata = true;

    // Attribute 3: Destination MAC
    attrs[2].id = SAI_FLOW_ENTRY_ATTR_DST_MAC;
    uint8_t dst_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    memcpy(attrs[2].value.mac, dst_mac, 6);

    // Attribute 4: Source IP (sai_ip_address_t)
    attrs[3].id = SAI_FLOW_ENTRY_ATTR_SIP;
    attrs[3].value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    attrs[3].value.ipaddr.addr.ip4 = inet_addr("172.16.0.1");

    event_data.attr_count = 4;
    event_data.attr = attrs;

    nlohmann::json json_line = FlowDumpSerializer::serializeFlowEntryToJson(event_data);

    // Verify flow entry fields are serialized correctly
    EXPECT_EQ(json_line["vn"], 1);
    EXPECT_EQ(json_line["pr"], 6);
    EXPECT_EQ(json_line["sp"], 12345);
    EXPECT_EQ(json_line["dp"], 80);
    EXPECT_EQ(json_line["em"], "00:11:22:33:44:55");
    EXPECT_EQ(json_line["si"], "10.10.10.10");
    EXPECT_EQ(json_line["di"], "192.168.0.1");

    // Verify attributes are serialized correctly (using attribute ID as key)
    // Note: serializeAttributeValue returns strings for all types
    std::string attr_version_key = std::to_string(SAI_FLOW_ENTRY_ATTR_VERSION);
    std::string attr_unidirectional_key = std::to_string(SAI_FLOW_ENTRY_ATTR_IS_UNIDIRECTIONAL_FLOW);
    std::string attr_dst_mac_key = std::to_string(SAI_FLOW_ENTRY_ATTR_DST_MAC);
    std::string attr_sip_key = std::to_string(SAI_FLOW_ENTRY_ATTR_SIP);

    ASSERT_TRUE(json_line.contains(attr_version_key));
    EXPECT_EQ(json_line[attr_version_key], "12345");  // Serialized as string

    ASSERT_TRUE(json_line.contains(attr_unidirectional_key));
    EXPECT_EQ(json_line[attr_unidirectional_key], "true");  // Serialized as string

    ASSERT_TRUE(json_line.contains(attr_dst_mac_key));
    EXPECT_EQ(json_line[attr_dst_mac_key], "AA:BB:CC:DD:EE:FF");

    ASSERT_TRUE(json_line.contains(attr_sip_key));
    EXPECT_EQ(json_line[attr_sip_key], "172.16.0.1");
}

// Test FlowDumpWriter - write and read back flow dump data
TEST_F(FlowDumpTest, FlowDumpWriter_WriteAndRead)
{
    SWSS_LOG_ENTER();
    // Clear any existing files in test directory
    clearTestDirectory(m_test_dir);

    // Set base path to test directory
    FlowDumpWriter& writer = FlowDumpWriter::getInstance();
    writer.setBasePath(m_test_dir + "/");

    // Create flow dump data
    FlowDumpDataPtr data = std::make_shared<FlowDumpData>();
    nlohmann::json json_line1;
    json_line1["em"] = "00:11:22:33:44:55";
    json_line1["vn"] = 1;
    json_line1["pr"] = 6;
    json_line1["si"] = "10.10.10.10";
    json_line1["di"] = "192.168.0.1";
    json_line1["sp"] = 12345;
    json_line1["dp"] = 80;

    nlohmann::json json_line2;
    json_line2["em"] = "AA:BB:CC:DD:EE:FF";
    json_line2["vn"] = 2;
    json_line2["pr"] = 17; // UDP
    json_line2["si"] = "192.168.1.1";
    json_line2["di"] = "10.0.0.1";
    json_line2["sp"] = 54321;
    json_line2["dp"] = 53;

    data->json_lines.push_back(json_line1);
    data->json_lines.push_back(json_line2);

    // Write flow dump data
    sai_object_id_t test_vid = 0x2000000000001;
    bool success = writer.writeFlowDumpData(data, test_vid);
    ASSERT_TRUE(success);

    // Verify file was created - find the actual file that was created
    std::string expected_file;
    DIR* dir = opendir(m_test_dir.c_str());
    ASSERT_NE(dir, nullptr);
    struct dirent* entry;
    bool found = false;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (strncmp(entry->d_name, "flow_dump_", 10) == 0)
        {
            expected_file = m_test_dir + "/" + entry->d_name;
            found = true;
            break;
        }
    }
    closedir(dir);
    ASSERT_TRUE(found) << "No flow dump file found in " << m_test_dir;

    // Verify file exists and is readable
    struct stat file_stat;
    ASSERT_EQ(stat(expected_file.c_str(), &file_stat), 0);

    // Read and decompress the file
    gzFile gz_file = gzopen(expected_file.c_str(), "rb");
    ASSERT_NE(gz_file, nullptr);

    char buffer[4096];
    std::string decompressed_data;
    int bytes_read;
    while ((bytes_read = gzread(gz_file, buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytes_read] = '\0';
        decompressed_data += buffer;
    }
    gzclose(gz_file);

    // Parse the decompressed data
    std::istringstream iss(decompressed_data);
    std::string line;
    std::vector<nlohmann::json> read_lines;

    while (std::getline(iss, line))
    {
        if (!line.empty())
        {
            read_lines.push_back(nlohmann::json::parse(line));
        }
    }

    // Verify we read back 2 lines
    ASSERT_EQ(read_lines.size(), 2);

    // Verify first line
    EXPECT_EQ(read_lines[0]["em"], "00:11:22:33:44:55");
    EXPECT_EQ(read_lines[0]["vn"], 1);
    EXPECT_EQ(read_lines[0]["pr"], 6);
    EXPECT_EQ(read_lines[0]["si"], "10.10.10.10");
    EXPECT_EQ(read_lines[0]["di"], "192.168.0.1");
    EXPECT_EQ(read_lines[0]["sp"], 12345);
    EXPECT_EQ(read_lines[0]["dp"], 80);

    // Verify second line
    EXPECT_EQ(read_lines[1]["em"], "AA:BB:CC:DD:EE:FF");
    EXPECT_EQ(read_lines[1]["vn"], 2);
    EXPECT_EQ(read_lines[1]["pr"], 17);
    EXPECT_EQ(read_lines[1]["si"], "192.168.1.1");
    EXPECT_EQ(read_lines[1]["di"], "10.0.0.1");
    EXPECT_EQ(read_lines[1]["sp"], 54321);
    EXPECT_EQ(read_lines[1]["dp"], 53);
}

// Test FlowDumpWriter - verify file path generation
TEST_F(FlowDumpTest, FlowDumpWriter_FilePathGeneration)
{
    SWSS_LOG_ENTER();
    std::string test_dir = "/tmp/test";

    // Clear any existing files in test directory
    clearTestDirectory(test_dir);

    FlowDumpWriter& writer = FlowDumpWriter::getInstance();
    writer.setBasePath(test_dir + "/");

    sai_object_id_t test_vid = 0x3000000000001;
    FlowDumpDataPtr data = std::make_shared<FlowDumpData>();
    nlohmann::json json_line;
    json_line["em"] = "00:11:22:33:44:55";
    data->json_lines.push_back(json_line);

    bool success = writer.writeFlowDumpData(data, test_vid);
    ASSERT_TRUE(success);

    // Verify file was created - find the actual file that was created
    std::string expected_file;
    DIR* dir = opendir("/tmp/test");
    ASSERT_NE(dir, nullptr);
    struct dirent* entry;
    bool found = false;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (strncmp(entry->d_name, "flow_dump_", 10) == 0)
        {
            expected_file = test_dir + "/" + std::string(entry->d_name);
            found = true;
            break;
        }
    }
    closedir(dir);
    ASSERT_TRUE(found) << "No flow dump file found in " << test_dir;

    // Verify file exists
    struct stat file_stat;
    ASSERT_EQ(stat(expected_file.c_str(), &file_stat), 0);

    // Clean up
    unlink(expected_file.c_str());
    rmdir(test_dir.c_str());
}

TEST_F(FlowDumpTest, FlowDumpWriter_BasePathGetterSetter)
{
    SWSS_LOG_ENTER();
    FlowDumpWriter& writer = FlowDumpWriter::getInstance();

    // Test default path
    const std::string& default_path = writer.getBasePath();
    EXPECT_EQ(default_path, FlowDumpWriter::DEFAULT_BASE_PATH);

    // Set new path
    std::string new_path = "/tmp/custom/path/";
    writer.setBasePath(new_path);

    // Verify new path
    const std::string& retrieved_path = writer.getBasePath();
    EXPECT_EQ(retrieved_path, new_path);
}

// Test FlowDumpWriter - null data handling
TEST_F(FlowDumpTest, FlowDumpWriter_NullData)
{
    SWSS_LOG_ENTER();
    FlowDumpWriter& writer = FlowDumpWriter::getInstance();
    bool success = writer.writeFlowDumpData(nullptr, 0x1000000000001);
    ASSERT_FALSE(success);
}

// Test FlowDumpWriter - write multiple flows and verify
TEST_F(FlowDumpTest, FlowDumpWriter_MultipleFlows)
{
    SWSS_LOG_ENTER();
    std::string test_dir = "/tmp/test_multiple_flows";

    // Clear any existing files in test directory
    clearTestDirectory(test_dir);

    // Set base path to test directory
    FlowDumpWriter& writer = FlowDumpWriter::getInstance();
    writer.setBasePath(test_dir + "/");

    // Create flow dump data with multiple flows
    FlowDumpDataPtr data = std::make_shared<FlowDumpData>();

    // Flow 1: TCP flow
    nlohmann::json flow1;
    flow1["em"] = "AA:BB:CC:DD:EE:01";
    flow1["vn"] = 10;
    flow1["pr"] = 6; // TCP
    flow1["si"] = "192.168.1.10";
    flow1["di"] = "10.0.0.10";
    flow1["sp"] = 8080;
    flow1["dp"] = 443;
    data->json_lines.push_back(flow1);

    // Flow 2: UDP flow
    nlohmann::json flow2;
    flow2["em"] = "AA:BB:CC:DD:EE:02";
    flow2["vn"] = 20;
    flow2["pr"] = 17; // UDP
    flow2["si"] = "172.16.0.1";
    flow2["di"] = "8.8.8.8";
    flow2["sp"] = 5353;
    flow2["dp"] = 53;
    data->json_lines.push_back(flow2);

    // Flow 3: Another TCP flow
    nlohmann::json flow3;
    flow3["em"] = "AA:BB:CC:DD:EE:03";
    flow3["vn"] = 30;
    flow3["pr"] = 6; // TCP
    flow3["si"] = "10.1.1.1";
    flow3["di"] = "10.2.2.2";
    flow3["sp"] = 12345;
    flow3["dp"] = 80;
    data->json_lines.push_back(flow3);

    // Flow 4: ICMP-like flow (protocol 1)
    nlohmann::json flow4;
    flow4["em"] = "AA:BB:CC:DD:EE:04";
    flow4["vn"] = 40;
    flow4["pr"] = 1; // ICMP
    flow4["si"] = "192.168.100.1";
    flow4["di"] = "192.168.100.2";
    flow4["sp"] = 0;
    flow4["dp"] = 0;
    data->json_lines.push_back(flow4);

    // Flow 5: Another UDP flow
    nlohmann::json flow5;
    flow5["em"] = "AA:BB:CC:DD:EE:05";
    flow5["vn"] = 50;
    flow5["pr"] = 17; // UDP
    flow5["si"] = "1.1.1.1";
    flow5["di"] = "1.1.1.2";
    flow5["sp"] = 50000;
    flow5["dp"] = 50001;
    data->json_lines.push_back(flow5);

    // Write flow dump data
    sai_object_id_t test_vid = 0x4000000000001;
    bool success = writer.writeFlowDumpData(data, test_vid);
    ASSERT_TRUE(success);

    // Verify file was created
    std::string expected_file;
    DIR* dir = opendir(test_dir.c_str());
    ASSERT_NE(dir, nullptr);
    struct dirent* entry;
    bool found = false;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (strncmp(entry->d_name, "flow_dump_", 10) == 0)
        {
            expected_file = test_dir + "/" + entry->d_name;
            found = true;
            break;
        }
    }
    closedir(dir);
    ASSERT_TRUE(found) << "No flow dump file found in " << test_dir;

    // Verify file exists
    struct stat file_stat;
    ASSERT_EQ(stat(expected_file.c_str(), &file_stat), 0);

    // Read and decompress the file
    gzFile gz_file = gzopen(expected_file.c_str(), "rb");
    ASSERT_NE(gz_file, nullptr);

    char buffer[4096];
    std::string decompressed_data;
    int bytes_read;
    while ((bytes_read = gzread(gz_file, buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytes_read] = '\0';
        decompressed_data += buffer;
    }
    gzclose(gz_file);

    // Parse the decompressed data
    std::istringstream iss(decompressed_data);
    std::string line;
    std::vector<nlohmann::json> read_lines;

    while (std::getline(iss, line))
    {
        if (!line.empty())
        {
            read_lines.push_back(nlohmann::json::parse(line));
        }
    }

    // Verify we read back 5 flows
    ASSERT_EQ(read_lines.size(), 5);

    // Verify Flow 1 (TCP)
    EXPECT_EQ(read_lines[0]["em"], "AA:BB:CC:DD:EE:01");
    EXPECT_EQ(read_lines[0]["vn"], 10);
    EXPECT_EQ(read_lines[0]["pr"], 6);
    EXPECT_EQ(read_lines[0]["si"], "192.168.1.10");
    EXPECT_EQ(read_lines[0]["di"], "10.0.0.10");
    EXPECT_EQ(read_lines[0]["sp"], 8080);
    EXPECT_EQ(read_lines[0]["dp"], 443);

    // Verify Flow 2 (UDP)
    EXPECT_EQ(read_lines[1]["em"], "AA:BB:CC:DD:EE:02");
    EXPECT_EQ(read_lines[1]["vn"], 20);
    EXPECT_EQ(read_lines[1]["pr"], 17);
    EXPECT_EQ(read_lines[1]["si"], "172.16.0.1");
    EXPECT_EQ(read_lines[1]["di"], "8.8.8.8");
    EXPECT_EQ(read_lines[1]["sp"], 5353);
    EXPECT_EQ(read_lines[1]["dp"], 53);

    // Verify Flow 3 (TCP)
    EXPECT_EQ(read_lines[2]["em"], "AA:BB:CC:DD:EE:03");
    EXPECT_EQ(read_lines[2]["vn"], 30);
    EXPECT_EQ(read_lines[2]["pr"], 6);
    EXPECT_EQ(read_lines[2]["si"], "10.1.1.1");
    EXPECT_EQ(read_lines[2]["di"], "10.2.2.2");
    EXPECT_EQ(read_lines[2]["sp"], 12345);
    EXPECT_EQ(read_lines[2]["dp"], 80);

    // Verify Flow 4 (ICMP)
    EXPECT_EQ(read_lines[3]["em"], "AA:BB:CC:DD:EE:04");
    EXPECT_EQ(read_lines[3]["vn"], 40);
    EXPECT_EQ(read_lines[3]["pr"], 1);
    EXPECT_EQ(read_lines[3]["si"], "192.168.100.1");
    EXPECT_EQ(read_lines[3]["di"], "192.168.100.2");
    EXPECT_EQ(read_lines[3]["sp"], 0);
    EXPECT_EQ(read_lines[3]["dp"], 0);

    // Verify Flow 5 (UDP)
    EXPECT_EQ(read_lines[4]["em"], "AA:BB:CC:DD:EE:05");
    EXPECT_EQ(read_lines[4]["vn"], 50);
    EXPECT_EQ(read_lines[4]["pr"], 17);
    EXPECT_EQ(read_lines[4]["si"], "1.1.1.1");
    EXPECT_EQ(read_lines[4]["di"], "1.1.1.2");
    EXPECT_EQ(read_lines[4]["sp"], 50000);
    EXPECT_EQ(read_lines[4]["dp"], 50001);

    // Clean up
    unlink(expected_file.c_str());
    rmdir(test_dir.c_str());
}

// Test FlowDumpWriter - write flows with attributes and verify
TEST_F(FlowDumpTest, FlowDumpWriter_WithAttributes)
{
    SWSS_LOG_ENTER();
    std::string test_dir = "/tmp/test_flows_with_attributes";

    // Clear any existing files in test directory
    clearTestDirectory(test_dir);

    // Set base path to test directory
    FlowDumpWriter& writer = FlowDumpWriter::getInstance();
    writer.setBasePath(test_dir + "/");

    // Create flow dump data with attributes using FlowDumpSerializer
    sai_flow_bulk_get_session_event_data_t event_data[2];

    // Flow 1: With attributes
    event_data[0] = m_event_data;
    sai_attribute_t attrs1[3];
    memset(attrs1, 0, sizeof(attrs1));

    attrs1[0].id = SAI_FLOW_ENTRY_ATTR_VERSION;
    attrs1[0].value.u32 = 100;

    attrs1[1].id = SAI_FLOW_ENTRY_ATTR_IS_UNIDIRECTIONAL_FLOW;
    attrs1[1].value.booldata = true;

    attrs1[2].id = SAI_FLOW_ENTRY_ATTR_DST_MAC;
    uint8_t dst_mac1[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    memcpy(attrs1[2].value.mac, dst_mac1, 6);

    event_data[0].attr_count = 3;
    event_data[0].attr = attrs1;

    // Flow 2: With different attributes
    event_data[1] = m_event_data;
    event_data[1].flow_entry.vnet_id = 100;
    event_data[1].flow_entry.src_port = 9999;
    event_data[1].flow_entry.dst_port = 8888;

    sai_attribute_t attrs2[2];
    memset(attrs2, 0, sizeof(attrs2));

    attrs2[0].id = SAI_FLOW_ENTRY_ATTR_VERSION;
    attrs2[0].value.u32 = 200;

    attrs2[1].id = SAI_FLOW_ENTRY_ATTR_SIP;
    attrs2[1].value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    attrs2[1].value.ipaddr.addr.ip4 = inet_addr("1.2.3.4");

    event_data[1].attr_count = 2;
    event_data[1].attr = attrs2;

    // Serialize to JSON lines
    FlowDumpDataPtr data = std::make_shared<FlowDumpData>();
    nlohmann::json json_line1 = FlowDumpSerializer::serializeFlowEntryToJson(event_data[0]);
    nlohmann::json json_line2 = FlowDumpSerializer::serializeFlowEntryToJson(event_data[1]);
    data->json_lines.push_back(json_line1);
    data->json_lines.push_back(json_line2);

    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->json_lines.size(), 2);

    // Write flow dump data
    sai_object_id_t test_vid = 0x5000000000001;
    bool success = writer.writeFlowDumpData(data, test_vid);
    ASSERT_TRUE(success);

    // Verify file was created
    std::string expected_file;
    DIR* dir = opendir(test_dir.c_str());
    ASSERT_NE(dir, nullptr);
    struct dirent* entry;
    bool found = false;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (strncmp(entry->d_name, "flow_dump_", 10) == 0)
        {
            expected_file = test_dir + "/" + entry->d_name;
            found = true;
            break;
        }
    }
    closedir(dir);
    ASSERT_TRUE(found) << "No flow dump file found in " << test_dir;

    // Verify file exists
    struct stat file_stat;
    ASSERT_EQ(stat(expected_file.c_str(), &file_stat), 0);

    // Read and decompress the file
    gzFile gz_file = gzopen(expected_file.c_str(), "rb");
    ASSERT_NE(gz_file, nullptr);

    char buffer[4096];
    std::string decompressed_data;
    int bytes_read;
    while ((bytes_read = gzread(gz_file, buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytes_read] = '\0';
        decompressed_data += buffer;
    }
    gzclose(gz_file);

    // Parse the decompressed data
    std::istringstream iss(decompressed_data);
    std::string line;
    std::vector<nlohmann::json> read_lines;

    while (std::getline(iss, line))
    {
        if (!line.empty())
        {
            read_lines.push_back(nlohmann::json::parse(line));
        }
    }

    // Verify we read back 2 flows
    ASSERT_EQ(read_lines.size(), 2);

    // Verify Flow 1 with attributes
    EXPECT_EQ(read_lines[0]["vn"], 1);
    EXPECT_EQ(read_lines[0]["sp"], 12345);
    EXPECT_EQ(read_lines[0]["dp"], 80);

    std::string attr_version_key1 = std::to_string(SAI_FLOW_ENTRY_ATTR_VERSION);
    std::string attr_unidirectional_key = std::to_string(SAI_FLOW_ENTRY_ATTR_IS_UNIDIRECTIONAL_FLOW);
    std::string attr_dst_mac_key = std::to_string(SAI_FLOW_ENTRY_ATTR_DST_MAC);

    ASSERT_TRUE(read_lines[0].contains(attr_version_key1));
    EXPECT_EQ(read_lines[0][attr_version_key1], "100");  // Serialized as string

    ASSERT_TRUE(read_lines[0].contains(attr_unidirectional_key));
    EXPECT_EQ(read_lines[0][attr_unidirectional_key], "true");  // Serialized as string

    ASSERT_TRUE(read_lines[0].contains(attr_dst_mac_key));
    EXPECT_EQ(read_lines[0][attr_dst_mac_key], "11:22:33:44:55:66");

    // Verify Flow 2 with attributes
    EXPECT_EQ(read_lines[1]["vn"], 100);
    EXPECT_EQ(read_lines[1]["sp"], 9999);
    EXPECT_EQ(read_lines[1]["dp"], 8888);

    std::string attr_version_key2 = std::to_string(SAI_FLOW_ENTRY_ATTR_VERSION);
    std::string attr_sip_key = std::to_string(SAI_FLOW_ENTRY_ATTR_SIP);

    ASSERT_TRUE(read_lines[1].contains(attr_version_key2));
    EXPECT_EQ(read_lines[1][attr_version_key2], "200");  // Serialized as string

    ASSERT_TRUE(read_lines[1].contains(attr_sip_key));
    EXPECT_EQ(read_lines[1][attr_sip_key], "1.2.3.4");

    // Clean up
    unlink(expected_file.c_str());
    rmdir(test_dir.c_str());
}

// Test FlowDumpWriter - logrotate functionality
TEST_F(FlowDumpTest, FlowDumpWriter_LogRotate)
{
    SWSS_LOG_ENTER();
    std::string test_dir = "/tmp/test_logrotate";

    // Clear any existing files in test directory
    clearTestDirectory(test_dir);

    // Set base path to test directory
    FlowDumpWriter& writer = FlowDumpWriter::getInstance();
    writer.setBasePath(test_dir + "/");

    // Create flow dump data
    FlowDumpDataPtr data = std::make_shared<FlowDumpData>();
    nlohmann::json json_line;
    json_line["em"] = "00:11:22:33:44:55";
    json_line["vn"] = 1;
    json_line["pr"] = 6;
    json_line["si"] = "10.10.10.10";
    json_line["di"] = "192.168.0.1";
    json_line["sp"] = 12345;
    json_line["dp"] = 80;
    data->json_lines.push_back(json_line);

    // Create first file - should work fine
    sai_object_id_t vid1 = 0x6000000000001;
    bool success1 = writer.writeFlowDumpData(data, vid1);
    ASSERT_TRUE(success1);

    // Small delay to ensure different modification times
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Create second file - should work fine (we have MAX_FILES = 2)
    sai_object_id_t vid2 = 0x6000000000002;
    bool success2 = writer.writeFlowDumpData(data, vid2);
    ASSERT_TRUE(success2);

    // Get the actual file paths that were created
    std::vector<std::pair<std::string, std::time_t>> files_before;

    DIR* dir = opendir(test_dir.c_str());
    ASSERT_NE(dir, nullptr);
    struct dirent* entry;
    struct stat st;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        std::string filename = entry->d_name;
        if (filename.find(FlowDumpWriter::FLOW_DUMP_FILE_PREFIX) == 0 && filename.find(FlowDumpWriter::FLOW_DUMP_FILE_SUFFIX) != std::string::npos)
        {
            std::string full_path = test_dir;
            if (full_path.back() != '/')
            {
                full_path += '/';
            }
            full_path += filename;

            if (stat(full_path.c_str(), &st) == 0 && S_ISREG(st.st_mode))
            {
                files_before.push_back(std::make_pair(full_path, st.st_mtime));
            }
        }
    }
    closedir(dir);
    ASSERT_EQ(files_before.size(), 2) << "Expected 2 files after creating second file";

    // Sort files by modification time to identify oldest
    std::sort(files_before.begin(), files_before.end(),
              [](const std::pair<std::string, std::time_t>& a, const std::pair<std::string, std::time_t>& b) {
                  return a.second < b.second;
              });

    // The first file in the sorted list is the oldest (vid1)
    std::string file1_path = files_before[0].first;
    std::string file2_path = files_before[1].first;

    // Small delay to ensure different modification times
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Create third file - should delete the first file (oldest)
    sai_object_id_t vid3 = 0x6000000000003;
    bool success3 = writer.writeFlowDumpData(data, vid3);
    ASSERT_TRUE(success3);

    // Verify only 2 files exist now (second and third, first should be deleted)
    std::vector<std::string> files_after;
    dir = opendir(test_dir.c_str());
    ASSERT_NE(dir, nullptr);
    while ((entry = readdir(dir)) != nullptr)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        std::string filename = entry->d_name;
        if (filename.find(FlowDumpWriter::FLOW_DUMP_FILE_PREFIX) == 0 && filename.find(FlowDumpWriter::FLOW_DUMP_FILE_SUFFIX) != std::string::npos)
        {
            std::string full_path = test_dir;
            if (full_path.back() != '/')
            {
                full_path += '/';
            }
            full_path += filename;

            if (stat(full_path.c_str(), &st) == 0 && S_ISREG(st.st_mode))
            {
                files_after.push_back(full_path);
            }
        }
    }
    closedir(dir);
    ASSERT_EQ(files_after.size(), 2) << "Expected 2 files after creating third file (logrotate should have deleted oldest)";

    // Verify the first file (oldest) was deleted
    struct stat file_stat;
    ASSERT_NE(stat(file1_path.c_str(), &file_stat), 0) << "First file (oldest) should have been deleted";

    // Verify the second file still exists
    ASSERT_EQ(stat(file2_path.c_str(), &file_stat), 0) << "Second file should still exist";

    // Verify file1_path is not in the remaining files
    ASSERT_EQ(std::find(files_after.begin(), files_after.end(), file1_path), files_after.end())
        << "First file should not be in remaining files";

    // Verify file2_path is in the remaining files
    ASSERT_NE(std::find(files_after.begin(), files_after.end(), file2_path), files_after.end())
        << "Second file should be in remaining files";

    // Clean up
    for (const auto& file : files_after)
    {
        unlink(file.c_str());
    }
    rmdir(test_dir.c_str());
}

// Test FlowDumpSerializer::serializeAttributeValue with enum types
TEST_F(FlowDumpTest, FlowDumpSerializer_AttributeValue_EnumTypes)
{
    SWSS_LOG_ENTER();
    sai_flow_bulk_get_session_event_data_t event_data = m_event_data;

    // Set up attributes with enum types
    sai_attribute_t attrs[5];
    memset(attrs, 0, sizeof(attrs));

    // Attribute 1: DASH direction (enum - sai_dash_direction_t)
    attrs[0].id = SAI_FLOW_ENTRY_ATTR_DASH_DIRECTION;
    attrs[0].value.s32 = SAI_DASH_DIRECTION_OUTBOUND;

    // Attribute 2: DASH flow action (bit mask - sai_dash_flow_action_t, serialized as numeric)
    attrs[1].id = SAI_FLOW_ENTRY_ATTR_DASH_FLOW_ACTION;
    attrs[1].value.s32 = SAI_DASH_FLOW_ACTION_ENCAP_U1;  // 1 << 1 = 2

    // Attribute 3: DASH encapsulation (enum - sai_dash_encapsulation_t)
    attrs[2].id = SAI_FLOW_ENTRY_ATTR_UNDERLAY0_DASH_ENCAPSULATION;
    attrs[2].value.s32 = SAI_DASH_ENCAPSULATION_VXLAN;

    // Attribute 4: DASH flow sync state (enum - sai_dash_flow_sync_state_t)
    attrs[3].id = SAI_FLOW_ENTRY_ATTR_DASH_FLOW_SYNC_STATE;
    attrs[3].value.s32 = SAI_DASH_FLOW_SYNC_STATE_FLOW_PENDING_DELETE;

    // Attribute 5: IP address family (enum - sai_ip_addr_family_t)
    attrs[4].id = SAI_FLOW_ENTRY_ATTR_IP_ADDR_FAMILY;
    attrs[4].value.s32 = SAI_IP_ADDR_FAMILY_IPV4;

    event_data.attr_count = 5;
    event_data.attr = attrs;

    nlohmann::json json_line = FlowDumpSerializer::serializeFlowEntryToJson(event_data);

    // Verify enum attributes are serialized as strings (not numbers)
    std::string attr_direction_key = std::to_string(SAI_FLOW_ENTRY_ATTR_DASH_DIRECTION);
    std::string attr_flow_action_key = std::to_string(SAI_FLOW_ENTRY_ATTR_DASH_FLOW_ACTION);
    std::string attr_encap_key = std::to_string(SAI_FLOW_ENTRY_ATTR_UNDERLAY0_DASH_ENCAPSULATION);
    std::string attr_sync_state_key = std::to_string(SAI_FLOW_ENTRY_ATTR_DASH_FLOW_SYNC_STATE);
    std::string attr_ip_family_key = std::to_string(SAI_FLOW_ENTRY_ATTR_IP_ADDR_FAMILY);

    // Check that attributes exist and are strings (enum serialization returns strings)
    ASSERT_TRUE(json_line.contains(attr_direction_key));
    EXPECT_TRUE(json_line[attr_direction_key].is_string());
    EXPECT_EQ(json_line[attr_direction_key], "SAI_DASH_DIRECTION_OUTBOUND");

    ASSERT_TRUE(json_line.contains(attr_flow_action_key));
    EXPECT_TRUE(json_line[attr_flow_action_key].is_string());
    // DASH flow action is serialized as numeric bit mask (no sai_serialize_attr_value)
    EXPECT_EQ(json_line[attr_flow_action_key], "2");

    ASSERT_TRUE(json_line.contains(attr_encap_key));
    EXPECT_TRUE(json_line[attr_encap_key].is_string());
    EXPECT_EQ(json_line[attr_encap_key], "SAI_DASH_ENCAPSULATION_VXLAN");

    ASSERT_TRUE(json_line.contains(attr_sync_state_key));
    EXPECT_TRUE(json_line[attr_sync_state_key].is_string());
    EXPECT_EQ(json_line[attr_sync_state_key], "SAI_DASH_FLOW_SYNC_STATE_FLOW_PENDING_DELETE");

    ASSERT_TRUE(json_line.contains(attr_ip_family_key));
    EXPECT_TRUE(json_line[attr_ip_family_key].is_string());
    EXPECT_EQ(json_line[attr_ip_family_key], "SAI_IP_ADDR_FAMILY_IPV4");
}

// Test FlowDumpSerializer::serializeAttributeValue with sai_u8_list_t
TEST_F(FlowDumpTest, FlowDumpSerializer_AttributeValue_U8List)
{
    SWSS_LOG_ENTER();
    sai_flow_bulk_get_session_event_data_t event_data = m_event_data;

    // Set up attributes with u8_list type
    sai_attribute_t attrs[2];
    memset(attrs, 0, sizeof(attrs));

    // Attribute 1: Vendor metadata (sai_u8_list_t)
    uint8_t vendor_metadata_buffer[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
    attrs[0].id = SAI_FLOW_ENTRY_ATTR_VENDOR_METADATA;
    attrs[0].value.u8list.count = 5;
    attrs[0].value.u8list.list = vendor_metadata_buffer;

    // Attribute 2: Flow data protocol buffer (sai_u8_list_t)
    uint8_t flow_data_buffer[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    attrs[1].id = SAI_FLOW_ENTRY_ATTR_FLOW_DATA_PB;
    attrs[1].value.u8list.count = 4;
    attrs[1].value.u8list.list = flow_data_buffer;

    event_data.attr_count = 2;
    event_data.attr = attrs;

    nlohmann::json json_line = FlowDumpSerializer::serializeFlowEntryToJson(event_data);

    std::string attr_vendor_metadata_key = std::to_string(SAI_FLOW_ENTRY_ATTR_VENDOR_METADATA);
    std::string attr_flow_data_pb_key = std::to_string(SAI_FLOW_ENTRY_ATTR_FLOW_DATA_PB);

    ASSERT_TRUE(json_line.contains(attr_vendor_metadata_key));
    EXPECT_TRUE(json_line[attr_vendor_metadata_key].is_string());
    EXPECT_EQ(json_line[attr_vendor_metadata_key], "5:1,2,3,4,5");

    ASSERT_TRUE(json_line.contains(attr_flow_data_pb_key));
    EXPECT_TRUE(json_line[attr_flow_data_pb_key].is_string());
    EXPECT_EQ(json_line[attr_flow_data_pb_key], "4:170,187,204,221");
}

// Test FlowDumpSerializer::serializeAttributeValue exception handling
TEST_F(FlowDumpTest, FlowDumpSerializer_AttributeValue_ExceptionHandling)
{
    SWSS_LOG_ENTER();
    // Test with null metadata
    sai_attribute_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.id = SAI_FLOW_ENTRY_ATTR_VERSION;
    attr.value.u32 = 12345;

    std::string result = FlowDumpSerializer::serializeAttributeValue(attr, nullptr);
    EXPECT_EQ(result, "");

    // Test with valid metadata
    auto meta = sai_metadata_get_attr_metadata(static_cast<sai_object_type_t>(SAI_OBJECT_TYPE_FLOW_ENTRY), SAI_FLOW_ENTRY_ATTR_VERSION);
    ASSERT_NE(meta, nullptr);
    result = FlowDumpSerializer::serializeAttributeValue(attr, meta);
    EXPECT_EQ(result, "12345");
}

