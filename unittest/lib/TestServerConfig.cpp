#include "ServerConfig.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;

TEST(ServerConfig, loadFromFile)
{
    EXPECT_NE(ServerConfig::loadFromFile("/not_existing"), nullptr);

    EXPECT_NE(ServerConfig::loadFromFile("files/server_config_ok.json"), nullptr);
    EXPECT_NE(ServerConfig::loadFromFile("files/server_config_bad.json"), nullptr);
}
