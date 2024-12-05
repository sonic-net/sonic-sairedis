#include "NotificationSwitchAsicSdkHealthEvent.h"
#include "Meta.h"
#include "MetaTestSaiInterface.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

#include <inttypes.h>

#include <gtest/gtest.h>
#include <memory>

using namespace sairedis;
using namespace saimeta;

//static std::string s = "[{\"host_tx_ready_status\":\"SAI_PORT_HOST_TX_READY_STATUS_READY\",\"port_id\":\"oid:0x100000000001a\",\"switch_id\":\"oid:0x2100000000\"}]";
static std::string s = "{"
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
static std::string null = "[{\"host_tx_ready_status\":\"SAI_PORT_HOST_TX_READY_STATUS_READY\",\"port_id\":\"oid:0x0\",\"switch_id\":\"oid:0x0\"}]";
static std::string fullnull = "[]";

TEST(NotificationSwitchAsicSdkHealthEvent, ctr)
{
    NotificationSwitchAsicSdkHealthEvent n(s);
}

TEST(NotificationSwitchAsicSdkHealthEvent, getSwitchId)
{
    NotificationSwitchAsicSdkHealthEvent n(s);

    EXPECT_EQ(n.getSwitchId(), 0x21000000000000);
}

TEST(NotificationSwitchAsicSdkHealthEvent, getAnyObjectId)
{
    NotificationSwitchAsicSdkHealthEvent n(s);

    EXPECT_EQ(n.getAnyObjectId(), 0x21000000000000);
}

TEST(NotificationSwitchAsicSdkHealthEvent, processMetadata)
{
    NotificationSwitchAsicSdkHealthEvent n(s);

    auto sai = std::make_shared<MetaTestSaiInterface>();
    auto meta = std::make_shared<Meta>(sai);

    n.processMetadata(meta);
}

static void on_switch_asic_sdk_health_event(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_asic_sdk_health_severity_t severity,
        _In_ sai_timespec_t timestamp,
        _In_ sai_switch_asic_sdk_health_category_t category,
        _In_ sai_switch_health_data_t data,
        _In_ const sai_u8_list_t description)
{
    SWSS_LOG_ENTER();

    printf("swtch: %" PRIx64 " %" PRIu64 " %d\n", switch_id, timestamp.tv_sec, timestamp.tv_nsec);

    // this code is from swss and it crashes on put_time because of tv_sec == 172479515853275099

    // 67767710400000001
    uint64_t y1000 = 1000;
    y1000 *= 365;
    y1000 *= 24;
    y1000 *= 60;
    y1000 *= 60;

    timestamp.tv_sec %= y1000;

    std::stringstream ss;
    const std::time_t t = (std::time_t)timestamp.tv_sec;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");// THIS WILL THROW if seconds is too big
}

TEST(NotificationSwitchAsicSdkHealthEvent, executeCallback)
{
    NotificationSwitchAsicSdkHealthEvent n(s);

    sai_switch_notifications_t ntfs;

    ntfs.on_switch_asic_sdk_health_event = &on_switch_asic_sdk_health_event;

    n.executeCallback(ntfs);

    auto str = "{\"category\":\"SAI_SWITCH_ASIC_SDK_HEALTH_CATEGORY_ASIC_HW\",\"data.data_type\":\"SAI_HEALTH_DATA_TYPE_GENERAL\",\"description\":\"16:123,10,9,34,100,97,116,97,34,58,32,34,48,34,10,125\",\"severity\":\"SAI_SWITCH_ASIC_SDK_HEALTH_SEVERITY_WARNING\",\"switch_id\":\"oid:0x21000000000000\",\"timestamp\":\"{\\\"tv_nsec\\\":\\\"12\\\",\\\"tv_sec\\\":\\\"172479515853275099\\\"}\"}";

    NotificationSwitchAsicSdkHealthEvent nn(str);

    ntfs.on_switch_asic_sdk_health_event = &on_switch_asic_sdk_health_event;

    nn.executeCallback(ntfs);
}

typedef struct _sai_switch_health_data_t_v14
{
    sai_health_data_type_t data_type;

    sai_health_data_t data;
} sai_switch_health_data_t_v14;

typedef struct _sai_switch_health_data_t_v13
{
    sai_health_data_type_t data_type;
} sai_switch_health_data_t_v13;

typedef void (*on_switch_asic_sdk_health_event_v13_fn)(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_asic_sdk_health_severity_t severity,
        _In_ sai_timespec_t timestamp,
        _In_ sai_switch_asic_sdk_health_category_t category,
        _In_ sai_switch_health_data_t_v13 data,
        _In_ const sai_u8_list_t description);

static void on_switch_asic_sdk_health_event_v13(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_asic_sdk_health_severity_t severity,
        _In_ sai_timespec_t timestamp,
        _In_ sai_switch_asic_sdk_health_category_t category,
        _In_ sai_switch_health_data_t_v13 data,
        _In_ const sai_u8_list_t description)
{
    SWSS_LOG_ENTER();

    printf("switch v13: 0x%" PRIx64 " %" PRIu64 " %d, count = %d\n", switch_id, timestamp.tv_sec,timestamp.tv_nsec,
            description.count);

    EXPECT_TRUE(timestamp.tv_sec == 1731848814);

    const std::time_t &t = (std::time_t)timestamp.tv_sec;
    std::stringstream time_ss;
    time_ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
}

typedef void (*on_switch_asic_sdk_health_event_v14_fn)(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_asic_sdk_health_severity_t severity,
        _In_ sai_timespec_t timestamp,
        _In_ sai_switch_asic_sdk_health_category_t category,
        _In_ sai_switch_health_data_t_v14 data,
        _In_ const sai_u8_list_t description);

static void on_switch_asic_sdk_health_event_v14(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_asic_sdk_health_severity_t severity,
        _In_ sai_timespec_t timestamp,
        _In_ sai_switch_asic_sdk_health_category_t category,
        _In_ sai_switch_health_data_t_v14 data,
        _In_ const sai_u8_list_t description)
{
    SWSS_LOG_ENTER();

    printf("switch v14: 0x%" PRIx64 " %" PRIu64 " %d, count = %d\n", switch_id, timestamp.tv_sec, timestamp.tv_nsec,
            description.count);

    EXPECT_TRUE(timestamp.tv_sec == 1731848814);

    const std::time_t &t = (std::time_t)timestamp.tv_sec;
    std::stringstream time_ss;
    time_ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
}

TEST(NotificationSwitchAsicSdkHealthEvent, version)
{
    SWSS_LOG_ENTER();

    sai_object_id_t switchId = 0x21000000000000;
    sai_switch_asic_sdk_health_severity_t severity = SAI_SWITCH_ASIC_SDK_HEALTH_SEVERITY_WARNING;
    sai_switch_asic_sdk_health_category_t category = SAI_SWITCH_ASIC_SDK_HEALTH_CATEGORY_ASIC_HW;
    sai_timespec_t timestamp = {1731848814, 123456};
    sai_switch_health_data_t_v13 data13 = { };
    sai_switch_health_data_t_v14 data14 = { };
    data13.data_type = SAI_HEALTH_DATA_TYPE_GENERAL;
    data14.data_type = SAI_HEALTH_DATA_TYPE_GENERAL;

    uint8_t list[] = {123,10,9,34,100,97,116,97,34,58,32,34,48,34,10,125};
    sai_u8_list_t desc;
    desc.count = 16;
    desc.list = list;

    {
        on_switch_asic_sdk_health_event_v13_fn v13 = &on_switch_asic_sdk_health_event_v13;
        v13(switchId, severity, timestamp, category, data13, desc); // v13 call v13

        on_switch_asic_sdk_health_event_v14_fn v14 = &on_switch_asic_sdk_health_event_v14;
        v14(switchId, severity, timestamp, category, data14, desc); // v14 call v14
    }

    {
        printf("call other\n");

        on_switch_asic_sdk_health_event_v13_fn v13 = reinterpret_cast<on_switch_asic_sdk_health_event_v13_fn>((void*)&on_switch_asic_sdk_health_event_v14);
        v13(switchId, severity, timestamp, category, data13, desc); // v13 call v14

        on_switch_asic_sdk_health_event_v14_fn v14 = reinterpret_cast<on_switch_asic_sdk_health_event_v14_fn>((void*)&on_switch_asic_sdk_health_event_v13);
        v14(switchId, severity, timestamp, category, data14, desc); // v14 call v13
    }
}
