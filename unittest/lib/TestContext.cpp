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
