#include "NotificationBfdSessionStateChange.h"
#include "Meta.h"
#include "MetaTestSaiInterface.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;
using namespace saimeta;

static std::string s = "[{\"bfd_session_id\":\"oid:0x100000000001a\",\"session_state\":\"SAI_BFD_SESSION_STATE_ADMIN_DOWN\"}]";
static std::string null = "[{\"bfd_session_id\":\"oid:0x0\",\"session_state\":\"SAI_BFD_SESSION_STATE_ADMIN_DOWN\"}]";
static std::string fullnull = "[]";

TEST(NotificationBfdSessionStateChange, ctr)
{
    NotificationBfdSessionStateChange n(s);
}

TEST(NotificationBfdSessionStateChange, getSwitchId)
{
    NotificationBfdSessionStateChange n(s);

    EXPECT_EQ(n.getSwitchId(), 0);

    NotificationBfdSessionStateChange n2(null);

    EXPECT_EQ(n2.getSwitchId(), 0);
}

TEST(NotificationBfdSessionStateChange, getAnyObjectId)
{
    NotificationBfdSessionStateChange n(s);

    EXPECT_EQ(n.getAnyObjectId(), 0x100000000001a);

    NotificationBfdSessionStateChange n2(null);

    EXPECT_EQ(n2.getAnyObjectId(), 0);

    NotificationBfdSessionStateChange n3(fullnull);

    EXPECT_EQ(n3.getSwitchId(), 0);

    EXPECT_EQ(n3.getAnyObjectId(), 0);
}

TEST(NotificationBfdSessionStateChange, processMetadata)
{
    NotificationBfdSessionStateChange n(s);

    auto sai = std::make_shared<MetaTestSaiInterface>();
    auto meta = std::make_shared<Meta>(sai);

    n.processMetadata(meta);
}

static void on_bfd_session_state_change(
        _In_ uint32_t count,
        _In_ const sai_bfd_session_state_notification_t *data)
{
    SWSS_LOG_ENTER();
}

TEST(NotificationBfdSessionStateChange, executeCallback)
{
    NotificationBfdSessionStateChange n(s);

    sai_switch_notifications_t ntfs;

    ntfs.on_bfd_session_state_change = &on_bfd_session_state_change;

    n.executeCallback(ntfs);
}

