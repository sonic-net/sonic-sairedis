#include "NotificationPortStateChange.h"
#include "Meta.h"
#include "MetaTestSaiInterface.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;
using namespace saimeta;

static std::string s = "[{\"port_id\":\"oid:0x100000000001a\",\"port_state\":\"SAI_PORT_OPER_STATUS_UP\"}]";
static std::string null =  "[{\"port_id\":\"oid:0x0\",\"port_state\":\"SAI_PORT_OPER_STATUS_UP\"}]";
static std::string fullnull = "[]";

TEST(NotificationPortStateChange, ctr)
{
    NotificationPortStateChange n(s);
}

TEST(NotificationPortStateChange, getSwitchId)
{
    NotificationPortStateChange n(s);

    EXPECT_EQ(n.getSwitchId(), 0);

    NotificationPortStateChange n2(null);

    EXPECT_EQ(n2.getSwitchId(), 0);
}

TEST(NotificationPortStateChange, getAnyObjectId)
{
    NotificationPortStateChange n(s);

    EXPECT_EQ(n.getAnyObjectId(), 0x100000000001a);

    NotificationPortStateChange n2(null);

    EXPECT_EQ(n2.getAnyObjectId(), 0);

    NotificationPortStateChange n3(fullnull);

    EXPECT_EQ(n3.getSwitchId(), 0);

    EXPECT_EQ(n3.getAnyObjectId(), 0);
}

TEST(NotificationPortStateChange, processMetadata)
{
    NotificationPortStateChange n(s);

    auto sai = std::make_shared<MetaTestSaiInterface>();
    auto meta = std::make_shared<Meta>(sai);

    n.processMetadata(meta);
}

static void on_port_state_change_notification(
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();
}

TEST(NotificationPortStateChange, executeCallback)
{
    NotificationPortStateChange n(s);

    sai_switch_notifications_t ntfs;

    ntfs.on_port_state_change = &on_port_state_change_notification;

    n.executeCallback(ntfs);
}
