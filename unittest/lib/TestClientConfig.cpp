#include "ClientConfig.h"

#include <gtest/gtest.h>

using namespace sairedis;

TEST(ClientConfig, loadFromFile)
{
    EXPECT_NE(ClientConfig::loadFromFile("non_existing"), nullptr);

    EXPECT_NE(ClientConfig::loadFromFile("files/client_config_bad.txt"), nullptr);

    EXPECT_NE(ClientConfig::loadFromFile("files/client_config_ok.txt"), nullptr);
}
