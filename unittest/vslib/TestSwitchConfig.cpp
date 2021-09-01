#include "SwitchConfig.h"

#include "saivs.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(SwitchConfig, parseSaiSwitchType)
{
    sai_switch_type_t type;

    EXPECT_FALSE(SwitchConfig::parseSaiSwitchType(nullptr, type));

    EXPECT_FALSE(SwitchConfig::parseSaiSwitchType("foo", type));

    EXPECT_TRUE(SwitchConfig::parseSaiSwitchType(SAI_VALUE_SAI_SWITCH_TYPE_NPU, type));
    EXPECT_EQ(type, SAI_SWITCH_TYPE_NPU);

    EXPECT_TRUE(SwitchConfig::parseSaiSwitchType(SAI_VALUE_SAI_SWITCH_TYPE_PHY, type));
    EXPECT_EQ(type, SAI_SWITCH_TYPE_PHY);
}

TEST(SwitchConfig, parseSwitchType)
{
    sai_vs_switch_type_t type;

    EXPECT_FALSE(SwitchConfig::parseSwitchType(nullptr, type));
    EXPECT_FALSE(SwitchConfig::parseSwitchType("foo", type));

    EXPECT_TRUE(SwitchConfig::parseSwitchType(SAI_VALUE_VS_SWITCH_TYPE_BCM56850, type));
    EXPECT_EQ(type, SAI_VS_SWITCH_TYPE_BCM56850);

    EXPECT_TRUE(SwitchConfig::parseSwitchType(SAI_VALUE_VS_SWITCH_TYPE_BCM81724, type));
    EXPECT_EQ(type, SAI_VS_SWITCH_TYPE_BCM81724);

    EXPECT_TRUE(SwitchConfig::parseSwitchType(SAI_VALUE_VS_SWITCH_TYPE_MLNX2700, type));
    EXPECT_EQ(type, SAI_VS_SWITCH_TYPE_MLNX2700);
}


TEST(SwitchConfig, parseBootType)
{
    sai_vs_boot_type_t type;

    EXPECT_TRUE(SwitchConfig::parseBootType(nullptr, type));
    EXPECT_EQ(type, SAI_VS_BOOT_TYPE_COLD);

    EXPECT_FALSE(SwitchConfig::parseBootType("foo", type));

    EXPECT_TRUE(SwitchConfig::parseBootType("cold", type));
    EXPECT_EQ(type, SAI_VS_BOOT_TYPE_COLD);

    EXPECT_TRUE(SwitchConfig::parseBootType(SAI_VALUE_VS_BOOT_TYPE_COLD, type));
    EXPECT_EQ(type, SAI_VS_BOOT_TYPE_COLD);

    EXPECT_TRUE(SwitchConfig::parseBootType("warm", type));
    EXPECT_EQ(type, SAI_VS_BOOT_TYPE_WARM);

    EXPECT_TRUE(SwitchConfig::parseBootType(SAI_VALUE_VS_BOOT_TYPE_WARM, type));
    EXPECT_EQ(type, SAI_VS_BOOT_TYPE_WARM);

    EXPECT_TRUE(SwitchConfig::parseBootType("fast", type));
    EXPECT_EQ(type, SAI_VS_BOOT_TYPE_FAST);

    EXPECT_TRUE(SwitchConfig::parseBootType(SAI_VALUE_VS_BOOT_TYPE_FAST, type));
    EXPECT_EQ(type, SAI_VS_BOOT_TYPE_FAST);
}

TEST(SwitchConfig, parseUseTapDevice)
{
    EXPECT_FALSE(SwitchConfig::parseUseTapDevice(nullptr));

    EXPECT_FALSE(SwitchConfig::parseUseTapDevice("foo"));

    EXPECT_FALSE(SwitchConfig::parseUseTapDevice("false"));

    EXPECT_TRUE(SwitchConfig::parseUseTapDevice("true"));
}
