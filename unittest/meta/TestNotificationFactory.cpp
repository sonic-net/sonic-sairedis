#include "NotificationFactory.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <memory>

using namespace sairedis;

TEST(NotificationFactory, deserialize)
{
    EXPECT_THROW(NotificationFactory::deserialize("foo", "bar"), std::runtime_error);

    EXPECT_THROW(NotificationFactory::deserialize(SAI_SWITCH_NOTIFICATION_NAME_FDB_EVENT, "bar"), nlohmann::detail::parse_error);
}

TEST(NotificationFactory, deserialize_fdb_event)
{
    auto ntf = NotificationFactory::deserialize(
            SAI_SWITCH_NOTIFICATION_NAME_FDB_EVENT,
            "[{\"fdb_entry\":\"{\\\"bvid\\\":\\\"oid:0x260000000005be\\\",\\\"mac\\\":\\\"52:54:00:86:DD:7A\\\",\\\"switch_id\\\":\\\"oid:0x21000000000000\\\"}\","
            "\"fdb_event\":\"SAI_FDB_EVENT_LEARNED\","
            "\"list\":[{\"id\":\"SAI_FDB_ENTRY_ATTR_TYPE\",\"value\":\"SAI_FDB_ENTRY_TYPE_DYNAMIC\"},{\"id\":\"SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID\",\"value\":\"oid:0x3a000000000660\"}]}]");

    EXPECT_EQ(ntf->getNotificationType(), SAI_SWITCH_NOTIFICATION_TYPE_FDB_EVENT);
}

TEST(NotificationFactory, deserialize_nat_event)
{
    auto ntf = NotificationFactory::deserialize(
            SAI_SWITCH_NOTIFICATION_NAME_NAT_EVENT,
            "[{\"nat_entry\":\"{\\\"nat_data\\\":{\\\"key\\\":{\\\"dst_ip\\\":\\\"10.10.10.10\\\",\\\"l4_dst_port\\\":\\\"20006\\\",\\\"l4_src_port\\\":\\\"0\\\",\\\"proto\\\":\\\"6\\\",\\\"src_ip\\\":\\\"0.0.0.0\\\"},\\\"mask\\\":{\\\"dst_ip\\\":\\\"255.255.255.255\\\",\\\"l4_dst_port\\\":\\\"65535\\\",\\\"l4_src_port\\\":\\\"0\\\",\\\"proto\\\":\\\"255\\\",\\\"src_ip\\\":\\\"0.0.0.0\\\"}},\\\"nat_type\\\":\\\"SAI_NAT_TYPE_DESTINATION_NAT\\\",\\\"switch_id\\\":\\\"oid:0x21000000000000\\\",\\\"vr\\\":\\\"oid:0x3000000000048\\\"}\",\"nat_event\":\"SAI_NAT_EVENT_AGED\"}]");

    EXPECT_EQ(ntf->getNotificationType(), SAI_SWITCH_NOTIFICATION_TYPE_NAT_EVENT);
}

TEST(NotificationFactory, deserialize_port_state_change)
{
    auto ntf = NotificationFactory::deserialize(
            SAI_SWITCH_NOTIFICATION_NAME_PORT_STATE_CHANGE,
            "[{\"port_id\":\"oid:0x100000000001a\",\"port_state\":\"SAI_PORT_OPER_STATUS_UP\",\"port_error_status\":\"0\"}]");

    EXPECT_EQ(ntf->getNotificationType(), SAI_SWITCH_NOTIFICATION_TYPE_PORT_STATE_CHANGE);
}

TEST(NotificationFactory, deserialize_port_host_tx_ready_status)
{
    auto ntf = NotificationFactory::deserialize(
            SAI_SWITCH_NOTIFICATION_NAME_PORT_HOST_TX_READY,
            "[{\"host_tx_ready_status\":\"SAI_PORT_HOST_TX_READY_STATUS_READY\",\"port_id\":\"oid:0x100000000001a\",\"switch_id\":\"oid:0x2100000000\"}]");

    EXPECT_EQ(ntf->getNotificationType(), SAI_SWITCH_NOTIFICATION_TYPE_PORT_HOST_TX_READY);
}

TEST(NotificationFactory, deserialize_queue_pfc_deadlock)
{
    auto str = "[{\"event\":\"SAI_QUEUE_PFC_DEADLOCK_EVENT_TYPE_DETECTED\",\"queue_id\":\"oid:0x1500000000020a\"}]";

    auto ntf = NotificationFactory::deserialize(
            SAI_SWITCH_NOTIFICATION_NAME_QUEUE_PFC_DEADLOCK,
            str);

    EXPECT_EQ(ntf->getNotificationType(), SAI_SWITCH_NOTIFICATION_TYPE_QUEUE_PFC_DEADLOCK);

    EXPECT_EQ(str, ntf->getSerializedNotification());
}

TEST(NotificationFactory, deserialize_switch_asic_sdk_health_event)
{
    auto ntf = NotificationFactory::deserialize(
        SAI_SWITCH_NOTIFICATION_NAME_SWITCH_ASIC_SDK_HEALTH_EVENT,
        "{"
            "\"category\":\"SAI_SWITCH_ASIC_SDK_HEALTH_CATEGORY_FW\","
            "\"data.data_type\":\"SAI_HEALTH_DATA_TYPE_GENERAL\","
            "\"description\":\"2:30,30\","
            "\"severity\":\"SAI_SWITCH_ASIC_SDK_HEALTH_SEVERITY_FATAL\","
            "\"switch_id\":\"oid:0x21000000000000\","
            "\"timestamp\":\"{"
                "\\\"tv_nsec\\\":\\\"28715881\\\","
                "\\\"tv_sec\\\":\\\"1700042919\\\""
            "}\""
        "}");

    EXPECT_EQ(ntf->getNotificationType(), SAI_SWITCH_NOTIFICATION_TYPE_SWITCH_ASIC_SDK_HEALTH_EVENT);
}

TEST(NotificationFactory, deserialize_shutdown_request)
{
    auto ntf = NotificationFactory::deserialize(
            SAI_SWITCH_NOTIFICATION_NAME_SWITCH_SHUTDOWN_REQUEST,
            "{\"switch_id\":\"oid:0x2100000000\"}");

    EXPECT_EQ(ntf->getNotificationType(), SAI_SWITCH_NOTIFICATION_TYPE_SWITCH_SHUTDOWN_REQUEST);

    EXPECT_EQ("{\"switch_id\":\"oid:0x2100000000\"}", ntf->getSerializedNotification());
}

TEST(NotificationFactory, deserialize_switch_state_change)
{
    sai_switch_oper_status_t status = SAI_SWITCH_OPER_STATUS_UP;

    auto str = sai_serialize_switch_oper_status(0x2100000000, status);

    // {"status":"SAI_SWITCH_OPER_STATUS_UP","switch_id":"oid:0x2100000000"}

    auto ntf = NotificationFactory::deserialize(
            SAI_SWITCH_NOTIFICATION_NAME_SWITCH_STATE_CHANGE,
            str);

    EXPECT_EQ(ntf->getNotificationType(), SAI_SWITCH_NOTIFICATION_TYPE_SWITCH_STATE_CHANGE);

    EXPECT_EQ(str, ntf->getSerializedNotification());
}

TEST(NotificationFactory, deserialize_switch_macsec_post_status)
{
    auto str = sai_serialize_switch_macsec_post_status_ntf(0x2100000000, SAI_SWITCH_MACSEC_POST_STATUS_PASS);
    auto ntf = NotificationFactory::deserialize(SAI_SWITCH_NOTIFICATION_NAME_SWITCH_MACSEC_POST_STATUS, str);
    EXPECT_EQ(ntf->getNotificationType(), SAI_SWITCH_NOTIFICATION_TYPE_SWITCH_MACSEC_POST_STATUS);
    EXPECT_EQ(str, ntf->getSerializedNotification());
}

TEST(NotificationFactory, deserialize_macsec_post_status)
{
    auto str = sai_serialize_macsec_post_status_ntf(0x2100000000, SAI_MACSEC_POST_STATUS_PASS);
    auto ntf = NotificationFactory::deserialize(SAI_SWITCH_NOTIFICATION_NAME_MACSEC_POST_STATUS, str);
    EXPECT_EQ(ntf->getNotificationType(), SAI_SWITCH_NOTIFICATION_TYPE_MACSEC_POST_STATUS);
    EXPECT_EQ(str, ntf->getSerializedNotification());
}
