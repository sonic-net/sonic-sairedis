#include "PortMapParser.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

#include <fstream>
#include <cstdio>

using namespace syncd;

static void writeTestFile(const std::string& path, const std::string& content)
{
    SWSS_LOG_ENTER();

    std::ofstream ofs(path);
    ofs << content;
    ofs.close();
}

TEST(PortMapParser, parsePortMapEmptyFile)
{
    auto portMap = PortMapParser::parsePortMap("");

    EXPECT_EQ(portMap->size(), 0);
}

TEST(PortMapParser, parsePortMapIniFormat)
{
    std::string path = "test_port_config.ini";

    writeTestFile(path,
            "# comment line\n"
            "; another comment\n"
            "Ethernet0 0,1,2,3 Ethernet0\n"
            "Ethernet4 4,5,6,7 Ethernet4\n");

    auto portMap = PortMapParser::parsePortMap(path);

    EXPECT_EQ(portMap->size(), 2);

    auto raw = portMap->getRawPortMap();

    std::set<int> lanes0 = {0, 1, 2, 3};
    std::set<int> lanes4 = {4, 5, 6, 7};

    EXPECT_EQ(raw.at(lanes0), "Ethernet0");
    EXPECT_EQ(raw.at(lanes4), "Ethernet4");

    std::remove(path.c_str());
}

TEST(PortMapParser, parsePortMapJsonValid)
{
    std::string path = "test_config_db.json";

    writeTestFile(path,
            "{\n"
            "    \"PORT\": {\n"
            "        \"Ethernet0\": {\n"
            "            \"lanes\": \"0,1,2,3\",\n"
            "            \"speed\": \"100000\"\n"
            "        },\n"
            "        \"Ethernet4\": {\n"
            "            \"lanes\": \"4,5,6,7\",\n"
            "            \"speed\": \"100000\"\n"
            "        }\n"
            "    }\n"
            "}\n");

    auto portMap = PortMapParser::parsePortMap(path);

    EXPECT_EQ(portMap->size(), 2);

    auto raw = portMap->getRawPortMap();

    std::set<int> lanes0 = {0, 1, 2, 3};
    std::set<int> lanes4 = {4, 5, 6, 7};

    EXPECT_EQ(raw.at(lanes0), "Ethernet0");
    EXPECT_EQ(raw.at(lanes4), "Ethernet4");

    std::remove(path.c_str());
}

TEST(PortMapParser, parsePortMapJsonSingleLane)
{
    std::string path = "test_single_lane.json";

    writeTestFile(path,
            "{\n"
            "    \"PORT\": {\n"
            "        \"Ethernet0\": {\n"
            "            \"lanes\": \"25\"\n"
            "        }\n"
            "    }\n"
            "}\n");

    auto portMap = PortMapParser::parsePortMap(path);

    EXPECT_EQ(portMap->size(), 1);

    auto raw = portMap->getRawPortMap();

    std::set<int> lanes = {25};

    EXPECT_EQ(raw.at(lanes), "Ethernet0");

    std::remove(path.c_str());
}

TEST(PortMapParser, parsePortMapJsonSkipsMissingLanes)
{
    std::string path = "test_missing_lanes.json";

    writeTestFile(path,
            "{\n"
            "    \"PORT\": {\n"
            "        \"Ethernet0\": {\n"
            "            \"lanes\": \"0,1,2,3\"\n"
            "        },\n"
            "        \"Ethernet4\": {\n"
            "            \"speed\": \"100000\"\n"
            "        },\n"
            "        \"Ethernet8\": {\n"
            "            \"lanes\": \"8,9,10,11\"\n"
            "        }\n"
            "    }\n"
            "}\n");

    auto portMap = PortMapParser::parsePortMap(path);

    EXPECT_EQ(portMap->size(), 2);

    auto raw = portMap->getRawPortMap();

    std::set<int> lanes0 = {0, 1, 2, 3};
    std::set<int> lanes8 = {8, 9, 10, 11};

    EXPECT_EQ(raw.count(lanes0), 1);
    EXPECT_EQ(raw.count(lanes8), 1);

    std::remove(path.c_str());
}

TEST(PortMapParser, parsePortMapJsonSkipsNonStringLanes)
{
    std::string path = "test_nonstring_lanes.json";

    writeTestFile(path,
            "{\n"
            "    \"PORT\": {\n"
            "        \"Ethernet0\": {\n"
            "            \"lanes\": 12345\n"
            "        },\n"
            "        \"Ethernet4\": {\n"
            "            \"lanes\": \"4,5,6,7\"\n"
            "        }\n"
            "    }\n"
            "}\n");

    auto portMap = PortMapParser::parsePortMap(path);

    EXPECT_EQ(portMap->size(), 1);

    auto raw = portMap->getRawPortMap();

    std::set<int> lanes4 = {4, 5, 6, 7};

    EXPECT_EQ(raw.at(lanes4), "Ethernet4");

    std::remove(path.c_str());
}

TEST(PortMapParser, parsePortMapJsonEmptyPortTable)
{
    std::string path = "test_empty_port.json";

    writeTestFile(path,
            "{\n"
            "    \"PORT\": {}\n"
            "}\n");

    auto portMap = PortMapParser::parsePortMap(path);

    EXPECT_EQ(portMap->size(), 0);

    std::remove(path.c_str());
}

TEST(PortMapParser, parsePortMapJsonFileNotFound)
{
    EXPECT_EXIT(
            PortMapParser::parsePortMap("nonexistent_file.json"),
            ::testing::ExitedWithCode(EXIT_FAILURE),
            "");
}

TEST(PortMapParser, parsePortMapJsonInvalidJson)
{
    std::string path = "test_invalid.json";

    writeTestFile(path, "{ this is not valid json }");

    EXPECT_EXIT(
            PortMapParser::parsePortMap(path),
            ::testing::ExitedWithCode(EXIT_FAILURE),
            "");

    std::remove(path.c_str());
}

TEST(PortMapParser, parsePortMapJsonMissingPortKey)
{
    std::string path = "test_no_port_key.json";

    writeTestFile(path,
            "{\n"
            "    \"DEVICE_METADATA\": {}\n"
            "}\n");

    EXPECT_EXIT(
            PortMapParser::parsePortMap(path),
            ::testing::ExitedWithCode(EXIT_FAILURE),
            "");

    std::remove(path.c_str());
}

TEST(PortMapParser, parsePortMapJsonPortNotObject)
{
    std::string path = "test_port_not_obj.json";

    writeTestFile(path,
            "{\n"
            "    \"PORT\": \"not_an_object\"\n"
            "}\n");

    EXPECT_EXIT(
            PortMapParser::parsePortMap(path),
            ::testing::ExitedWithCode(EXIT_FAILURE),
            "");

    std::remove(path.c_str());
}

TEST(PortMapParser, parsePortMapIniFileNotFound)
{
    EXPECT_EXIT(
            PortMapParser::parsePortMap("nonexistent_file.ini"),
            ::testing::ExitedWithCode(EXIT_FAILURE),
            "");
}

TEST(PortMapParser, parsePortMapJsonMultiplePorts)
{
    std::string path = "test_many_ports.json";

    writeTestFile(path,
            "{\n"
            "    \"PORT\": {\n"
            "        \"Ethernet0\": { \"lanes\": \"0,1,2,3\" },\n"
            "        \"Ethernet4\": { \"lanes\": \"4,5,6,7\" },\n"
            "        \"Ethernet8\": { \"lanes\": \"8,9,10,11\" },\n"
            "        \"Ethernet12\": { \"lanes\": \"12,13,14,15\" }\n"
            "    }\n"
            "}\n");

    auto portMap = PortMapParser::parsePortMap(path);

    EXPECT_EQ(portMap->size(), 4);

    std::remove(path.c_str());
}

TEST(PortMapParser, parsePortMapJsonExtraFieldsIgnored)
{
    std::string path = "test_extra_fields.json";

    writeTestFile(path,
            "{\n"
            "    \"DEVICE_METADATA\": { \"localhost\": {} },\n"
            "    \"PORT\": {\n"
            "        \"Ethernet0\": {\n"
            "            \"admin_status\": \"up\",\n"
            "            \"alias\": \"fortyGigE0/0\",\n"
            "            \"index\": \"0\",\n"
            "            \"lanes\": \"0,1,2,3\",\n"
            "            \"mtu\": \"9100\",\n"
            "            \"speed\": \"40000\"\n"
            "        }\n"
            "    },\n"
            "    \"VLAN\": {}\n"
            "}\n");

    auto portMap = PortMapParser::parsePortMap(path);

    EXPECT_EQ(portMap->size(), 1);

    auto raw = portMap->getRawPortMap();

    std::set<int> lanes0 = {0, 1, 2, 3};

    EXPECT_EQ(raw.at(lanes0), "Ethernet0");

    std::remove(path.c_str());
}

TEST(PortMapParser, parsePortMapNonJsonExtension)
{
    std::string path = "test_portmap.txt";

    writeTestFile(path,
            "Ethernet0 0,1 Ethernet0\n");

    auto portMap = PortMapParser::parsePortMap(path);

    EXPECT_EQ(portMap->size(), 1);

    auto raw = portMap->getRawPortMap();

    std::set<int> lanes0 = {0, 1};

    EXPECT_EQ(raw.at(lanes0), "Ethernet0");

    std::remove(path.c_str());
}
