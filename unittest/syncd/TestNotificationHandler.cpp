#include "NotificationProcessor.h"
#include "NotificationHandler.h"
#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

using namespace syncd;

static std::string natData =
"[{\"nat_entry\":\"{\\\"nat_data\\\":{\\\"key\\\":{\\\"dst_ip\\\":\\\"10.10.10.10\\\",\\\"l4_dst_port\\\":\\\"20006\\\",\\\"l4_src_port\\\":\\\"0\\\",\\\"proto\\\":\\\"6\\\",\\\"src_ip\\\":\\\"0.0.0.0\\\"},\\\"mask\\\":{\\\"dst_ip\\\":\\\"255.255.255.255\\\",\\\"l4_dst_port\\\":\\\"65535\\\",\\\"l4_src_port\\\":\\\"0\\\",\\\"proto\\\":\\\"255\\\",\\\"src_ip\\\":\\\"0.0.0.0\\\"}},\\\"nat_type\\\":\\\"SAI_NAT_TYPE_DESTINATION_NAT\\\",\\\"switch_id\\\":\\\"oid:0x21000000000000\\\",\\\"vr\\\":\\\"oid:0x3000000000048\\\"}\",\"nat_event\":\"SAI_NAT_EVENT_AGED\"}]";

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
}
