#include "CorePortIndexMapFileParser.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(CorePortIndexMapFileParser, isInterfaceNameValid)
{
    EXPECT_EQ(CorePortIndexMapFileParser::isInterfaceNameValid(""), false);

    EXPECT_EQ(CorePortIndexMapFileParser::isInterfaceNameValid("1111111111111111111111111111111111111111111111111111111"), false);

    EXPECT_EQ(CorePortIndexMapFileParser::isInterfaceNameValid("rer/;"), false);

    EXPECT_EQ(CorePortIndexMapFileParser::isInterfaceNameValid("foo"), true);
}


TEST(CorePortIndexMapFileParser, parseCorePortIndexMapFile)
{
    // should load default

    EXPECT_NE(CorePortIndexMapFileParser::parseCorePortIndexMapFile(nullptr), nullptr);

    EXPECT_NE(CorePortIndexMapFileParser::parseCorePortIndexMapFile("not_existing"), nullptr);

    EXPECT_NE(CorePortIndexMapFileParser::parseCorePortIndexMapFile("files/core_port_empty.txt"), nullptr);

    EXPECT_NE(CorePortIndexMapFileParser::parseCorePortIndexMapFile("files/core_port_ok.txt"), nullptr);
}

