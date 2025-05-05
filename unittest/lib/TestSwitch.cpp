#include "Switch.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;

TEST(Switch, ctr)
{
    EXPECT_THROW(std::make_shared<Switch>(SAI_NULL_OBJECT_ID), std::runtime_error);
}

TEST(Switch, getSwitchNotifications)
{
    auto s = std::make_shared<Switch>(7);

    s->getSwitchNotifications();
}

TEST(Switch, updateNotifications)
{
    auto s = std::make_shared<Switch>(7);

    sai_attribute_t attrs[12];

    attrs[0].id = 10000;

    EXPECT_NO_THROW(s->updateNotifications(1, attrs));

    attrs[0].value.ptr = (void*)1;
    attrs[1].value.ptr = (void*)1;
    attrs[2].value.ptr = (void*)1;
    attrs[3].value.ptr = (void*)1;
    attrs[4].value.ptr = (void*)1;
    attrs[5].value.ptr = (void*)1;
    attrs[6].value.ptr = (void*)1;
    attrs[7].value.ptr = (void*)1;
    attrs[8].value.ptr = (void*)1;
    attrs[9].value.ptr = (void*)1;
    attrs[10].value.ptr = (void*)1;
    attrs[11].value.ptr = (void *)1;

    attrs[0].id = SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY;
    attrs[1].id = SAI_SWITCH_ATTR_SHUTDOWN_REQUEST_NOTIFY;
    attrs[2].id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;
    attrs[3].id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;
    attrs[4].id = SAI_SWITCH_ATTR_PACKET_EVENT_NOTIFY;
    attrs[5].id = SAI_SWITCH_ATTR_QUEUE_PFC_DEADLOCK_NOTIFY;
    attrs[6].id = SAI_SWITCH_ATTR_BFD_SESSION_STATE_CHANGE_NOTIFY;
    attrs[7].id = SAI_SWITCH_ATTR_NAT_EVENT_NOTIFY;
    attrs[8].id = SAI_SWITCH_ATTR_PORT_HOST_TX_READY_NOTIFY;
    attrs[9].id = SAI_SWITCH_ATTR_SWITCH_ASIC_SDK_HEALTH_EVENT_NOTIFY;
    attrs[10].id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attrs[11].id = SAI_SWITCH_ATTR_TAM_TEL_TYPE_CONFIG_CHANGE_NOTIFY;

    s->updateNotifications(12, attrs);

    auto sn = s->getSwitchNotifications();

    EXPECT_EQ((void*)1, sn.on_bfd_session_state_change);
    EXPECT_EQ((void*)1, sn.on_fdb_event);
    EXPECT_EQ((void*)1, sn.on_packet_event);
    EXPECT_EQ((void*)1, sn.on_port_state_change);
    EXPECT_EQ((void*)1, sn.on_queue_pfc_deadlock);
    EXPECT_EQ((void*)1, sn.on_switch_shutdown_request);
    EXPECT_EQ((void*)1, sn.on_switch_state_change);
    EXPECT_EQ((void*)1, sn.on_nat_event);
    EXPECT_EQ((void*)1, sn.on_port_host_tx_ready);
    EXPECT_EQ((void*)1, sn.on_switch_asic_sdk_health_event);
    EXPECT_EQ((void*)1, sn.on_tam_tel_type_config_change);
}
