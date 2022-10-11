#include "NotificationNatEvent.h"
#include "Meta.h"
#include "MetaTestSaiInterface.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;
using namespace saimeta;

static std::string s =
"[{\"nat_entry\":\"{\\\"nat_data\\\":{\\\"key\\\":{\\\"dst_ip\\\":\\\"10.10.10.10\\\",\\\"l4_dst_port\\\":\\\"20006\\\",\\\"l4_src_port\\\":\\\"0\\\",\\\"proto\\\":\\\"6\\\",\\\"src_ip\\\":\\\"0.0.0.0\\\"},\\\"mask\\\":{\\\"dst_ip\\\":\\\"255.255.255.255\\\",\\\"l4_dst_port\\\":\\\"65535\\\",\\\"l4_src_port\\\":\\\"0\\\",\\\"proto\\\":\\\"255\\\",\\\"src_ip\\\":\\\"0.0.0.0\\\"}},\\\"nat_type\\\":\\\"SAI_NAT_TYPE_DESTINATION_NAT\\\",\\\"switch_id\\\":\\\"oid:0x21000000000000\\\",\\\"vr\\\":\\\"oid:0x3000000000048\\\"}\",\"nat_event\":\"SAI_NAT_EVENT_AGED\"}]";

static std::string null =
"[{\"nat_entry\":\"{\\\"nat_data\\\":{\\\"key\\\":{\\\"dst_ip\\\":\\\"10.10.10.10\\\",\\\"l4_dst_port\\\":\\\"20006\\\",\\\"l4_src_port\\\":\\\"0\\\",\\\"proto\\\":\\\"6\\\",\\\"src_ip\\\":\\\"0.0.0.0\\\"},\\\"mask\\\":{\\\"dst_ip\\\":\\\"255.255.255.255\\\",\\\"l4_dst_port\\\":\\\"65535\\\",\\\"l4_src_port\\\":\\\"0\\\",\\\"proto\\\":\\\"255\\\",\\\"src_ip\\\":\\\"0.0.0.0\\\"}},\\\"nat_type\\\":\\\"SAI_NAT_TYPE_DESTINATION_NAT\\\",\\\"switch_id\\\":\\\"oid:0x0\\\",\\\"vr\\\":\\\"oid:0x3000000000048\\\"}\",\"nat_event\":\"SAI_NAT_EVENT_AGED\"}]";

static std::string fullnull = "[]";

static std::string none =
"[{\"nat_entry\":\"{\\\"nat_data\\\":{\\\"key\\\":{\\\"dst_ip\\\":\\\"10.10.10.10\\\",\\\"l4_dst_port\\\":\\\"20006\\\",\\\"l4_src_port\\\":\\\"0\\\",\\\"proto\\\":\\\"6\\\",\\\"src_ip\\\":\\\"0.0.0.0\\\"},\\\"mask\\\":{\\\"dst_ip\\\":\\\"255.255.255.255\\\",\\\"l4_dst_port\\\":\\\"65535\\\",\\\"l4_src_port\\\":\\\"0\\\",\\\"proto\\\":\\\"255\\\",\\\"src_ip\\\":\\\"0.0.0.0\\\"}},\\\"nat_type\\\":\\\"SAI_NAT_TYPE_DESTINATION_NAT\\\",\\\"switch_id\\\":\\\"oid:0x21000000000000\\\",\\\"vr\\\":\\\"oid:0x3000000000048\\\"}\",\"nat_event\":\"SAI_NAT_EVENT_NONE\"}]";

TEST(NotificationNatEvent, ctr)
{
    NotificationNatEvent n(s);
}

TEST(NotificationNatEvent, getSwitchId)
{
    NotificationNatEvent n(s);

    EXPECT_EQ(n.getSwitchId(), 0x21000000000000);

    NotificationNatEvent n2(null);

    EXPECT_EQ(n2.getSwitchId(), 0);
}

TEST(NotificationNatEvent, getAnyObjectId)
{
    NotificationNatEvent n(s);

    EXPECT_EQ(n.getAnyObjectId(), 0x21000000000000);

    NotificationNatEvent n2(null);

    EXPECT_EQ(n2.getAnyObjectId(), 0x3000000000048);

    NotificationNatEvent n3(fullnull);

    EXPECT_EQ(n3.getSwitchId(), 0x0);

    EXPECT_EQ(n3.getAnyObjectId(), 0x0);
}

TEST(NotificationNatEvent, processMetadata)
{
    NotificationNatEvent n(s);

    auto sai = std::make_shared<MetaTestSaiInterface>();
    auto meta = std::make_shared<Meta>(sai);

    n.processMetadata(meta);

    NotificationNatEvent n4(none);
    n4.processMetadata(meta);
}

static void on_nat_event(
        _In_ uint32_t count,
        _In_ const sai_nat_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();
}

TEST(NotificationNatEvent, executeCallback)
{
    NotificationNatEvent n(s);

    sai_switch_notifications_t ntfs;

    ntfs.on_nat_event = &on_nat_event;

    n.executeCallback(ntfs);
}
