#include "Sai.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;

static const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();
    return NULL;
}

static int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();
    return -1;
}

static sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

TEST(Sai, queryApiVersion)
{
    Sai sai;

    sai_api_version_t version;

    sai.initialize(0,&test_services);

    EXPECT_EQ(sai.queryApiVersion(NULL), SAI_STATUS_INVALID_PARAMETER);
    EXPECT_EQ(sai.queryApiVersion(&version), SAI_STATUS_SUCCESS);
}

