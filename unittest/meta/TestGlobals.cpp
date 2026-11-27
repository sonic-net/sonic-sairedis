#include "Globals.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saimeta;

static_assert(SAI_MAX_HARDWARE_ID_LEN <= 256, "max is 256");

TEST(Globals, getHardwareInfo)
{
    EXPECT_EQ("", Globals::getHardwareInfo(0, nullptr));

    EXPECT_EQ("", Globals::getHardwareInfo(1, nullptr));

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO;
    attr.value.s8list.count = 0;
    attr.value.s8list.list = nullptr;

    EXPECT_EQ("", Globals::getHardwareInfo(1, &attr));

    attr.value.s8list.count = 1;
    attr.value.s8list.list = nullptr;

    EXPECT_EQ("", Globals::getHardwareInfo(1, &attr));

    char hwinfo[SAI_MAX_HARDWARE_ID_LEN + 2];

    memset(hwinfo, '0', SAI_MAX_HARDWARE_ID_LEN + 2);

    hwinfo[SAI_MAX_HARDWARE_ID_LEN + 1] = 0;

    attr.value.s8list.count = SAI_MAX_HARDWARE_ID_LEN + 1;
    attr.value.s8list.list = (int8_t*)hwinfo;

    EXPECT_EQ(std::string(SAI_MAX_HARDWARE_ID_LEN, '0'), Globals::getHardwareInfo(1, &attr));

    attr.value.s8list.count = 10;
    attr.value.s8list.list = (int8_t*)hwinfo;

    hwinfo[3] = 0;

    EXPECT_EQ("000", Globals::getHardwareInfo(1, &attr));
}

TEST(Globals, getApiFailureLogPriority)
{
    EXPECT_EQ(swss::Logger::SWSS_NOTICE, Globals::getApiFailureLogPriority(
                SAI_COMMON_API_CREATE, SAI_OBJECT_TYPE_FDB_ENTRY, SAI_STATUS_ITEM_ALREADY_EXISTS));

    EXPECT_EQ(swss::Logger::SWSS_ERROR, Globals::getApiFailureLogPriority(
                SAI_COMMON_API_CREATE, SAI_OBJECT_TYPE_FDB_ENTRY, SAI_STATUS_FAILURE));

    EXPECT_EQ(swss::Logger::SWSS_ERROR, Globals::getApiFailureLogPriority(
                SAI_COMMON_API_CREATE, SAI_OBJECT_TYPE_FDB_ENTRY, SAI_STATUS_TABLE_FULL));

    EXPECT_EQ(swss::Logger::SWSS_ERROR, Globals::getApiFailureLogPriority(
                SAI_COMMON_API_REMOVE, SAI_OBJECT_TYPE_FDB_ENTRY, SAI_STATUS_ITEM_ALREADY_EXISTS));

    EXPECT_EQ(swss::Logger::SWSS_ERROR, Globals::getApiFailureLogPriority(
                SAI_COMMON_API_SET, SAI_OBJECT_TYPE_FDB_ENTRY, SAI_STATUS_ITEM_ALREADY_EXISTS));

    EXPECT_EQ(swss::Logger::SWSS_ERROR, Globals::getApiFailureLogPriority(
                SAI_COMMON_API_BULK_CREATE, SAI_OBJECT_TYPE_FDB_ENTRY, SAI_STATUS_ITEM_ALREADY_EXISTS));

    EXPECT_EQ(swss::Logger::SWSS_ERROR, Globals::getApiFailureLogPriority(
                SAI_COMMON_API_CREATE, SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, SAI_STATUS_ITEM_ALREADY_EXISTS));

    EXPECT_EQ(swss::Logger::SWSS_ERROR, Globals::getApiFailureLogPriority(
                SAI_COMMON_API_CREATE, SAI_OBJECT_TYPE_MCAST_FDB_ENTRY, SAI_STATUS_ITEM_ALREADY_EXISTS));
}
