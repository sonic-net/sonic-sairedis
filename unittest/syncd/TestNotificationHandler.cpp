#include "NotificationProcessor.h"
#include "NotificationHandler.h"
#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

using namespace syncd;

static std::string natData =
"[{\"nat_entry\":\"{\\\"nat_data\\\":{\\\"key\\\":{\\\"dst_ip\\\":\\\"10.10.10.10\\\",\\\"l4_dst_port\\\":\\\"20006\\\",\\\"l4_src_port\\\":\\\"0\\\",\\\"proto\\\":\\\"6\\\",\\\"src_ip\\\":\\\"0.0.0.0\\\"},\\\"mask\\\":{\\\"dst_ip\\\":\\\"255.255.255.255\\\",\\\"l4_dst_port\\\":\\\"65535\\\",\\\"l4_src_port\\\":\\\"0\\\",\\\"proto\\\":\\\"255\\\",\\\"src_ip\\\":\\\"0.0.0.0\\\"}},\\\"nat_type\\\":\\\"SAI_NAT_TYPE_DESTINATION_NAT\\\",\\\"switch_id\\\":\\\"oid:0x21000000000000\\\",\\\"vr\\\":\\\"oid:0x3000000000048\\\"}\",\"nat_event\":\"SAI_NAT_EVENT_AGED\"}]";
static std::string icmp_echo_sesion_state_str = "[{\"icmp_echo_session_id\":\"oid:0x100000000003a\",\"session_state\":\"SAI_ICMP_ECHO_SESSION_STATE_DOWN\"}]";

// Test ASIC/SDK health event
std::string asheData = "{"
    "\"category\":\"SAI_SWITCH_ASIC_SDK_HEALTH_CATEGORY_FW\","
    "\"data.data_type\":\"SAI_HEALTH_DATA_TYPE_GENERAL\","
    "\"description\":\"2:30,30\","
    "\"severity\":\"SAI_SWITCH_ASIC_SDK_HEALTH_SEVERITY_FATAL\","
    "\"switch_id\":\"oid:0x21000000000000\","
    "\"timestamp\":\"{"
        "\\\"tv_nsec\\\":\\\"28715881\\\","
        "\\\"tv_sec\\\":\\\"1700042919\\\""
    "}\""
"}";


TEST(NotificationHandler, NotificationHandlerTest)
{
    std::vector<sai_attribute_t> attrs;
    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_NAT_EVENT_NOTIFY;
    attr.value.ptr = (void *) 1;

    attrs.push_back(attr);

    auto notificationProcessor =
      std::make_shared<NotificationProcessor>(nullptr, nullptr, nullptr);
    auto notificationHandler =
      std::make_shared<NotificationHandler>(notificationProcessor);

    notificationHandler->updateNotificationsPointers(1, attrs.data());
    uint32_t count;
    sai_nat_event_notification_data_t *natevent = NULL;

    sai_deserialize_nat_event_ntf(natData, count, &natevent);
    notificationHandler->onNatEvent(count, natevent);

    attrs.clear();
    sai_attribute_t icmp_echo_attr;
    icmp_echo_attr.id = SAI_SWITCH_ATTR_ICMP_ECHO_SESSION_STATE_CHANGE_NOTIFY;
    icmp_echo_attr.value.ptr = (void *) 1;

    attrs.push_back(icmp_echo_attr);
    notificationHandler->updateNotificationsPointers(1, attrs.data());

    sai_icmp_echo_session_state_notification_t *icmp_echo_session_state_ntf = NULL;
    sai_deserialize_icmp_echo_session_state_ntf(icmp_echo_sesion_state_str, count, &icmp_echo_session_state_ntf);
    notificationHandler->onIcmpEchoSessionStateChange(count, icmp_echo_session_state_ntf);

    sai_object_id_t switch_id;
    sai_switch_asic_sdk_health_severity_t severity;
    sai_timespec_t timestamp;
    sai_switch_asic_sdk_health_category_t category;
    sai_switch_health_data_t data;
    sai_u8_list_t description;
    sai_deserialize_switch_asic_sdk_health_event(asheData,
                                                 switch_id,
                                                 severity,
                                                 timestamp,
                                                 category,
                                                 data,
                                                 description);
    assert(switch_id == 0x21000000000000);
    assert(severity == SAI_SWITCH_ASIC_SDK_HEALTH_SEVERITY_FATAL);
    assert(category == SAI_SWITCH_ASIC_SDK_HEALTH_CATEGORY_FW);
    notificationHandler->onSwitchAsicSdkHealthEvent(switch_id,
                                                    severity,
                                                    timestamp,
                                                    category,
                                                    data,
                                                    description);
}

TEST(NotificationHandler, setApiVersion)
{
    auto np = std::make_shared<NotificationProcessor>(nullptr, nullptr, nullptr);

    auto nh = std::make_shared<NotificationHandler>(np);

    EXPECT_EQ(SAI_VERSION(0,0,0), nh->getApiVersion());

    nh->setApiVersion(SAI_VERSION(1,15,0));

    EXPECT_EQ(SAI_VERSION(1,15,0), nh->getApiVersion());
}

TEST(NotificationHandler, NotificationMacsecPostStatusTest)
{
    auto notificationProcessor =
      std::make_shared<NotificationProcessor>(nullptr, nullptr, nullptr);
    auto notificationHandler =
      std::make_shared<NotificationHandler>(notificationProcessor);

    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_SWITCH_MACSEC_POST_STATUS_NOTIFY;
    attr.value.ptr = (void *) 1;
    notificationHandler->updateNotificationsPointers(1, &attr);
    sai_object_id_t switch_id;
    sai_switch_macsec_post_status_t switch_macsec_post_status;
    std::string switchPostStatusData = "{\"switch_id\":\"oid:0x21000000000000\",\"macsec_post_status\":\"SAI_SWITCH_MACSEC_POST_STATUS_PASS\"}";
    sai_deserialize_switch_macsec_post_status_ntf(switchPostStatusData, switch_id, switch_macsec_post_status);
    notificationHandler->onSwitchMacsecPostStatus(switch_id, switch_macsec_post_status);

    attr.id = SAI_SWITCH_ATTR_MACSEC_POST_STATUS_NOTIFY;
    attr.value.ptr = (void *) 1;
    notificationHandler->updateNotificationsPointers(1, &attr);
    sai_object_id_t macsec_id;
    sai_macsec_post_status_t macsec_post_status;
    std::string macsecPostStatusData = "{\"macsec_id\":\"oid:0x5800000000\",\"macsec_post_status\":\"SAI_MACSEC_POST_STATUS_PASS\"}";
    sai_deserialize_macsec_post_status_ntf(macsecPostStatusData, macsec_id, macsec_post_status);
    notificationHandler->onMacsecPostStatus(macsec_id, macsec_post_status);
}
