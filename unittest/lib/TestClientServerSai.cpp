#include "ClientServerSai.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

using namespace sairedis;

static const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
        return NULL;

    return nullptr;
}

static int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    return 0;
}

static sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

TEST(ClientServerSai, ctr)
{
    auto css = std::make_shared<ClientServerSai>();
}

TEST(ClientServerSai, initialize)
{
    auto css = std::make_shared<ClientServerSai>();

    EXPECT_NE(SAI_STATUS_SUCCESS, css->initialize(1, nullptr));

    EXPECT_NE(SAI_STATUS_SUCCESS, css->initialize(0, nullptr));

    EXPECT_EQ(SAI_STATUS_SUCCESS, css->initialize(0, &test_services));

    css = nullptr; // invoke uninitialize in destructor

    css = std::make_shared<ClientServerSai>();

    EXPECT_EQ(SAI_STATUS_SUCCESS, css->initialize(0, &test_services));

    EXPECT_NE(SAI_STATUS_SUCCESS, css->initialize(0, &test_services));
}

TEST(ClientServerSai, objectTypeQuery)
{
    auto css = std::make_shared<ClientServerSai>();

    EXPECT_EQ(SAI_OBJECT_TYPE_NULL, css->objectTypeQuery(0x1111111111111111L));
}

TEST(ClientServerSai, switchIdQuery)
{
    auto css = std::make_shared<ClientServerSai>();

    EXPECT_EQ(SAI_NULL_OBJECT_ID, css->switchIdQuery(0x1111111111111111L));
}

TEST(ClientServerSai, logSet)
{
    auto css = std::make_shared<ClientServerSai>();

    EXPECT_NE(SAI_STATUS_SUCCESS, css->logSet(SAI_API_PORT, SAI_LOG_LEVEL_NOTICE));

    EXPECT_EQ(SAI_STATUS_SUCCESS, css->initialize(0, &test_services));

    EXPECT_EQ(SAI_STATUS_SUCCESS, css->logSet(SAI_API_PORT, SAI_LOG_LEVEL_NOTICE));
}
