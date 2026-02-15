#include "Context.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

using namespace sairedis;

static sai_switch_notifications_t handle_notification(
        _In_ std::shared_ptr<Notification> notification,
        _In_ Context* context)
{
    SWSS_LOG_ENTER();

    sai_switch_notifications_t ntf;

    memset(&ntf, 0, sizeof(ntf));

    return ntf;
}

TEST(Context, populateMetadata)
{
    auto recorder = std::make_shared<Recorder>();

    auto cc = std::make_shared<ContextConfig>(0, "syncd", "ASIC_DB", "COUNTERS_DB","FLEX_DB", "STATE_DB");

    auto ctx = std::make_shared<Context>(cc, recorder,handle_notification);

    ctx->populateMetadata(0x212121212212121L);
}

TEST(Context, bulkGetClearStats)
{
    auto recorder = std::make_shared<Recorder>();

    auto cc = std::make_shared<ContextConfig>(0, "syncd", "ASIC_DB", "COUNTERS_DB","FLEX_DB", "STATE_DB");

    auto ctx = std::make_shared<Context>(cc, recorder,handle_notification);

    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, ctx->m_redisSai->bulkGetStats(SAI_NULL_OBJECT_ID,
                                                                        SAI_OBJECT_TYPE_PORT,
                                                                        0,
                                                                        nullptr,
                                                                        0,
                                                                        nullptr,
                                                                        SAI_STATS_MODE_BULK_READ,
                                                                        nullptr,
                                                                        nullptr));
    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED, ctx->m_redisSai->bulkClearStats(SAI_NULL_OBJECT_ID,
                                                                          SAI_OBJECT_TYPE_PORT,
                                                                          0,
                                                                          nullptr,
                                                                          0,
                                                                          nullptr,
                                                                          SAI_STATS_MODE_BULK_CLEAR,
                                                                          nullptr));
}
