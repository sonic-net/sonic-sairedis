#include "NotificationSwitchStateChange.h"
#include "Meta.h"
#include "MetaTestSaiInterface.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;
using namespace saimeta;

static std::string s = "{\"status\":\"SAI_SWITCH_OPER_STATUS_UP\",\"switch_id\":\"oid:0x2100000000\"}";
static std::string null = "{\"status\":\"SAI_SWITCH_OPER_STATUS_UP\",\"switch_id\":\"oid:0x0\"}";

TEST(NotificationSwitchStateChange, ctr)
{
    NotificationSwitchStateChange n(s);
}

TEST(NotificationSwitchStateChange, getSwitchId)
{
    NotificationSwitchStateChange n(s);

    EXPECT_EQ(n.getSwitchId(), 0x2100000000);

    NotificationSwitchStateChange n2(null);

    EXPECT_EQ(n2.getSwitchId(), 0);
}

TEST(NotificationSwitchStateChange, getAnyObjectId)
{
    NotificationSwitchStateChange n(s);

    EXPECT_EQ(n.getAnyObjectId(), 0x2100000000);

    NotificationSwitchStateChange n2(null);

    EXPECT_EQ(n2.getAnyObjectId(), 0);
}

TEST(NotificationSwitchStateChange, processMetadata)
{
    NotificationSwitchStateChange n(s);

    auto sai = std::make_shared<MetaTestSaiInterface>();
    auto meta = std::make_shared<Meta>(sai);

    n.processMetadata(meta);
}

static void on_switch_state_change_notification(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    SWSS_LOG_ENTER();
}

TEST(NotificationSwitchStateChange, executeCallback)
{
    NotificationSwitchStateChange n(s);

    sai_switch_notifications_t ntfs;

    ntfs.on_switch_state_change = &on_switch_state_change_notification;

    n.executeCallback(ntfs);
}
