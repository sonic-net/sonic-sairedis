#include "NotificationSwitchAsicSdkHealthEvent.h"
#include "Meta.h"
#include "MetaTestSaiInterface.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

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
}

TEST(NotificationSwitchAsicSdkHealthEvent, executeCallback)
{
    NotificationSwitchAsicSdkHealthEvent n(s);

    sai_switch_notifications_t ntfs;

    ntfs.on_switch_asic_sdk_health_event = &on_switch_asic_sdk_health_event;

    n.executeCallback(ntfs);
}
