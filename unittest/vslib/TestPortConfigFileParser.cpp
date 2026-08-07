#include "PortConfigFileParser.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

#include <fstream>
#include <cstdio>
#include <set>
#include <string>

using namespace saivs;

namespace
{
    class PortConfigFileParserTest : public ::testing::Test
    {
        protected:
            void SetUp() override
            {
                file = "/tmp/saivs-port-config-test.ini";
            }

            void TearDown() override
            {
                std::remove(file.c_str());
            }

            void write(const std::string& content)
            {
                SWSS_LOG_ENTER();

                std::ofstream output(file);
                ASSERT_TRUE(output.is_open());
                output << content;
            }

            std::string file;
    };
}

TEST_F(PortConfigFileParserTest, MatchesCompleteLaneSetIndependentOfOrder)
{
    write("# name lanes alias index speed\n"
          "Ethernet0 25,26,27,28 fortyGigE0/0 0 40000\n");

    auto port_config = PortConfigFileParser::parse(file);

    EXPECT_EQ("Ethernet0", port_config->getPortName({28, 26, 25, 27}));
    EXPECT_EQ("", port_config->getPortName({25, 26, 27}));
}

TEST_F(PortConfigFileParserTest, IgnoresMalformedRowsAndRejectsDuplicates)
{
    write("Ethernet0 1,2,3,4 alias 0 40000\n"
          "Ethernet1 bad,lanes alias 1 40000\n"
          "Ethernet2 1,2,3,4 alias 2 40000\n");

    auto port_config = PortConfigFileParser::parse(file);

    EXPECT_EQ(1U, port_config->size());
    EXPECT_EQ("Ethernet0", port_config->getPortName({1, 2, 3, 4}));
}

TEST(PortConfigFileParser, MissingFileReturnsEmptyMap)
{
    auto port_config = PortConfigFileParser::parse(
            "/tmp/saivs-port-config-file-does-not-exist.ini");

    EXPECT_EQ(0U, port_config->size());
}

TEST(PortConfigFileParser, ProductStyleNameResolvesFromLanes)
{
    const std::string file = "/tmp/saivs-product-port-config.ini";
    {
        std::ofstream output(file);
        ASSERT_TRUE(output.is_open());
        output << "# name lanes alias index speed\n"
                  "Ethernet0 25,26,27,28 fortyGigE0/0 0 40000\n";
    }

    auto port_config = PortConfigFileParser::parse(file);

    EXPECT_EQ("Ethernet0", port_config->getPortName({25, 26, 27, 28}));
    std::remove(file.c_str());
}