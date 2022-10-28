#include <gtest/gtest.h>
#include "VendorSai.h"
#include "swss/logger.h"

#ifdef HAVE_SAI_BULK_OBJECT_GET_STATS
#undef HAVE_SAI_BULK_OBJECT_GET_STATS
#endif

using namespace syncd;

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

TEST(VendorSai, bulkGetStats)
{
    VendorSai sai;
    sai.initialize(0, &test_services);
    ASSERT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai.bulkGetStats(SAI_NULL_OBJECT_ID,
                                                           SAI_OBJECT_TYPE_PORT,
                                                           0,
                                                           nullptr,
                                                           0,
                                                           nullptr,
                                                           SAI_STATS_MODE_BULK_READ_AND_CLEAR,
                                                           nullptr,
                                                           nullptr));
    ASSERT_EQ(SAI_STATUS_NOT_IMPLEMENTED, sai.bulkClearStats(SAI_NULL_OBJECT_ID,
                                                             SAI_OBJECT_TYPE_PORT,
                                                             0,
                                                             nullptr,
                                                             0,
                                                             nullptr,
                                                             SAI_STATS_MODE_BULK_READ_AND_CLEAR,
                                                             nullptr));
}
