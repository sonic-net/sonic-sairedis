#include <gtest/gtest.h>

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"

#include "DummySaiInterface.h"


extern std::shared_ptr<saimeta::DummySaiInterface> sai;

TEST(libsaistub, sai_log_set)
{
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai_log_set(SAI_API_VLAN, SAI_LOG_LEVEL_NOTICE));
}

TEST(libsaistub, sai_api_query)
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

TEST(libsaistub, sai_query_attribute_capability)
{
    EXPECT_EQ(SAI_STATUS_FAILURE, sai_query_attribute_capability(0,SAI_OBJECT_TYPE_NULL,0,0));
}

TEST(libsaistub, sai_query_attribute_enum_values_capability)
{
    EXPECT_EQ(SAI_STATUS_FAILURE, sai_query_attribute_enum_values_capability(0,SAI_OBJECT_TYPE_NULL,0,0));
}

TEST(libsaistub, sai_object_type_get_availability)
{
    EXPECT_EQ(SAI_STATUS_FAILURE, sai_object_type_get_availability(0,SAI_OBJECT_TYPE_NULL,0,0,0));
}

TEST(libsaistub, sai_dbg_generate_dump)
{
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai_dbg_generate_dump(nullptr));
}

TEST(libsaistub, sai_object_type_query)
{
    sai->setStatus(SAI_STATUS_SUCCESS);

    EXPECT_EQ(SAI_OBJECT_TYPE_NULL, sai_object_type_query(SAI_NULL_OBJECT_ID));

    sai->setStatus(SAI_STATUS_FAILURE);
}

TEST(libsaistub, sai_switch_id_query)
{
    sai->setStatus(SAI_STATUS_SUCCESS);

    EXPECT_EQ(SAI_NULL_OBJECT_ID, sai_switch_id_query(SAI_NULL_OBJECT_ID));

    sai->setStatus(SAI_STATUS_FAILURE);
}

TEST(libsaistub, sai_bulk_get_attribute)
{
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai_bulk_get_attribute(0,SAI_OBJECT_TYPE_NULL,0,0,0,0,0));
}

TEST(libsaistub, sai_get_maximum_attribute_count)
{
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai_get_maximum_attribute_count(0, SAI_OBJECT_TYPE_NULL,0));
}

TEST(libsaistub, sai_get_object_count)
{
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai_get_object_count(0,SAI_OBJECT_TYPE_NULL,0));
}

TEST(libsaistub, sai_get_object_key)
{
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai_get_object_key(0,SAI_OBJECT_TYPE_NULL,0,0));
}

TEST(libsaistub, sai_query_stats_capability)
{
    EXPECT_EQ(SAI_STATUS_FAILURE, sai_query_stats_capability(0,SAI_OBJECT_TYPE_NULL,0));
}

TEST(libsaistub, sai_bulk_object_get_stats)
{
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai_bulk_object_get_stats(SAI_NULL_OBJECT_ID,
                                                                    SAI_OBJECT_TYPE_PORT,
                                                                    0,
                                                                    nullptr,
                                                                    0,
                                                                    nullptr,
                                                                    SAI_STATS_MODE_BULK_READ,
                                                                    nullptr,
                                                                    nullptr));
}

TEST(libsaistub, sai_bulk_object_clear_stats)
{
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai_bulk_object_clear_stats(SAI_NULL_OBJECT_ID,
                                                                      SAI_OBJECT_TYPE_PORT,
                                                                      0,
                                                                      nullptr,
                                                                      0,
                                                                      nullptr,
                                                                      SAI_STATS_MODE_BULK_CLEAR,
                                                                      nullptr));
}

TEST(libsaistub, sai_query_api_version)
{
    sai_api_version_t version;

    EXPECT_EQ(SAI_STATUS_FAILURE, sai_query_api_version(nullptr));

    sai->setStatus(SAI_STATUS_SUCCESS);

    EXPECT_EQ(SAI_STATUS_SUCCESS, sai_query_api_version(&version));

    sai->setStatus(SAI_STATUS_FAILURE);
}
