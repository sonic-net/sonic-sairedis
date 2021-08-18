#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

TEST(libsairedis, sai_log_set)
{
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai_log_set(SAI_API_VLAN, SAI_LOG_LEVEL_NOTICE));
}

TEST(libsairedis, sai_api_query)
{
    sai_vlan_api_t *api = nullptr;

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, sai_api_query(SAI_API_VLAN, nullptr));
    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, sai_api_query(SAI_API_UNSPECIFIED, (void**)&api));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, sai_api_query((sai_api_t)(1000), (void**)&api));
#pragma GCC diagnostic pop

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai_api_query(SAI_API_VLAN, (void**)&api));
}

TEST(libsairedis, sai_query_attribute_capability)
{
    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, sai_query_attribute_capability(0,SAI_OBJECT_TYPE_NULL,0,0));
}

TEST(libsairedis, sai_query_attribute_enum_values_capability)
{
    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, sai_query_attribute_enum_values_capability(0,SAI_OBJECT_TYPE_NULL,0,0));
}

TEST(libsairedis, sai_object_type_get_availability)
{
    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, sai_object_type_get_availability(0,SAI_OBJECT_TYPE_NULL,0,0,0));
}

TEST(libsairedis, sai_dbg_generate_dump)
{
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai_dbg_generate_dump(nullptr));
}

TEST(libsairedis, sai_object_type_query)
{
    EXPECT_EQ(SAI_OBJECT_TYPE_NULL, sai_object_type_query(SAI_NULL_OBJECT_ID));
}

TEST(libsairedis, sai_switch_id_query)
{
    EXPECT_EQ(SAI_NULL_OBJECT_ID, sai_switch_id_query(SAI_NULL_OBJECT_ID));
}

TEST(libsairedis, sai_bulk_get_attribute)
{
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai_bulk_get_attribute(0,SAI_OBJECT_TYPE_NULL,0,0,0,0,0));
}

TEST(libsairedis, sai_get_maximum_attribute_count)
{
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai_get_maximum_attribute_count(0, SAI_OBJECT_TYPE_NULL,0));
}

TEST(libsairedis, sai_get_object_count)
{
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai_get_object_count(0,SAI_OBJECT_TYPE_NULL,0));
}

TEST(libsairedis, sai_get_object_key)
{
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai_get_object_key(0,SAI_OBJECT_TYPE_NULL,0,0));
}

TEST(libsairedis, sai_query_stats_capability)
{
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai_query_stats_capability(0,SAI_OBJECT_TYPE_NULL,0));
}
