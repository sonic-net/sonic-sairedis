#include "LaneMapFileParser.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(LaneMapFileParser, isInterfaceNameValid)
{
    EXPECT_FALSE(LaneMapFileParser::isInterfaceNameValid(""));
    EXPECT_FALSE(LaneMapFileParser::isInterfaceNameValid("111111111111111111111111111111111111111"));
    EXPECT_FALSE(LaneMapFileParser::isInterfaceNameValid("ab09AZ:"));
}


TEST(LaneMapFileParser, parseLaneMapFile)
{
    EXPECT_NE(LaneMapFileParser::parseLaneMapFile(nullptr), nullptr);

    EXPECT_NE(LaneMapFileParser::parseLaneMapFile("not_existing"), nullptr);

    EXPECT_NE(LaneMapFileParser::parseLaneMapFile("files/lane_map_empty.txt"), nullptr);

    EXPECT_NE(LaneMapFileParser::parseLaneMapFile("files/lane_nap_ok.txt"), nullptr);
}
