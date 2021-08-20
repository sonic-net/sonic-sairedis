#include "Switch.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(Switch, ctr)
{
    EXPECT_THROW(std::make_shared<Switch>(0), std::runtime_error);

    EXPECT_THROW(std::make_shared<Switch>(0, 1, nullptr), std::runtime_error);

    Switch s(0x2100000000);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY;

    Switch s2(0x2100000000, 1, &attr);
}

TEST(Switch, getSwitchId)
{
    Switch s(0x2100000000);

    EXPECT_EQ(s.getSwitchId(), 0x2100000000);
}

TEST(Switch, updateNotifications)
{
    Switch s(0x2100000000);

    EXPECT_THROW(s.updateNotifications(1, nullptr), std::runtime_error);

    s.updateNotifications(0, nullptr);

    sai_attribute_t attr;

    attr.id = -1;

    EXPECT_THROW(s.updateNotifications(1, &attr), std::runtime_error);

    attr.id = SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY;
    s.updateNotifications(1, &attr);

    attr.id = SAI_SWITCH_ATTR_SHUTDOWN_REQUEST_NOTIFY;
    s.updateNotifications(1, &attr);

    attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;
    s.updateNotifications(1, &attr);

    attr.id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;
    s.updateNotifications(1, &attr);

    attr.id = SAI_SWITCH_ATTR_PACKET_EVENT_NOTIFY;
    s.updateNotifications(1, &attr);

    attr.id = SAI_SWITCH_ATTR_QUEUE_PFC_DEADLOCK_NOTIFY;
    s.updateNotifications(1, &attr);

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    s.updateNotifications(1, &attr);
}

TEST(Switch, getSwitchNotifications)
{
    Switch s(0x2100000000);

    s.getSwitchNotifications();
}
