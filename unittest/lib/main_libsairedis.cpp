#include <gtest/gtest.h>

#include "sairedis.h"

#include "swss/logger.h"

#include <iostream>

static std::map<std::string, std::string> g_profileMap;
static std::map<std::string, std::string>::iterator g_profileIter;

static const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return NULL;
    }

    auto it = g_profileMap.find(variable);

    if (it == g_profileMap.end())
    {
        SWSS_LOG_NOTICE("%s: NULL", variable);
        return NULL;
    }

    SWSS_LOG_NOTICE("%s: %s", variable, it->second.c_str());

    return it->second.c_str();
}

static int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    if (value == NULL)
    {
        SWSS_LOG_INFO("resetting profile map iterator");

        g_profileIter = g_profileMap.begin();
        return 0;
    }

    if (variable == NULL)
    {
        SWSS_LOG_WARN("variable is null");
        return -1;
    }

    if (g_profileIter == g_profileMap.end())
    {
        SWSS_LOG_INFO("iterator reached end");
        return -1;
    }

    *variable = g_profileIter->first.c_str();
    *value = g_profileIter->second.c_str();

    SWSS_LOG_INFO("key: %s:%s", *variable, *value);

    g_profileIter++;

    return 0;
}

static sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

class sairedisEnvironment: 
    public ::testing::Environment
{
    public:

        virtual void SetUp() override 
        {
            SWSS_LOG_ENTER();

            g_profileIter = g_profileMap.begin();

            auto status = sai_api_initialize(0, (sai_service_method_table_t*)&test_services);

            EXPECT_EQ(status, SAI_STATUS_SUCCESS);
        }
        
        virtual void TearDown() override
        {
            SWSS_LOG_ENTER();

            auto status = sai_api_uninitialize();

            EXPECT_EQ(status, SAI_STATUS_SUCCESS);
        }
};

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);

    const auto env = new sairedisEnvironment();

    testing::AddGlobalTestEnvironment(env);

    return RUN_ALL_TESTS();
}
