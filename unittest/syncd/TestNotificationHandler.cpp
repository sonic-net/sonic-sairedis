#include "NotificationHandler.h"
#include "SwitchNotifications.h"
#include "RedisNotificationProducer.h"
#include "RedisClient.h"

#include "swss/dbconnector.h"

#include <gtest/gtest.h>

#include <memory>

using namespace syncd;
using namespace std::placeholders;

static std::shared_ptr<NotificationProcessor> g_processor;

static void syncProcessNotification(
        _In_ const swss::KeyOpFieldsValuesTuple& item)
{
    SWSS_LOG_ENTER();

    g_processor->syncProcessNotification(item);
}

TEST(NotificationHandler, onFdbEvent)
{
    sai_fdb_event_notification_data_t data;

    sai_attribute_t attr;

    attr.id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attr.value.oid = 0x3a00000001;

    data.fdb_entry.switch_id = 0x2100000000;
    data.fdb_entry.bv_id = 0x2600000000;
    data.fdb_entry.mac_address[0] = 0x00;
    data.fdb_entry.mac_address[1] = 0x22;
    data.fdb_entry.mac_address[2] = 0x48;
    data.fdb_entry.mac_address[3] = 0x58;
    data.fdb_entry.mac_address[4] = 0xD1;
    data.fdb_entry.mac_address[5] = 0x29;

    data.event_type = SAI_FDB_EVENT_LEARNED;

    data.attr_count = 1;
    data.attr = &attr;

    auto dbAsic = std::make_shared<swss::DBConnector>("ASIC_DB", 0);
    auto client = std::make_shared<RedisClient>(dbAsic);
    auto notifications = std::make_shared<RedisNotificationProducer>("ASIC_DB");
    g_processor = std::make_shared<NotificationProcessor>(notifications, client, std::bind(syncProcessNotification, _1));
    auto handler = std::make_shared<NotificationHandler>(g_processor);

    SwitchNotifications sn;

    sn.onFdbEvent = std::bind(&NotificationHandler::onFdbEvent, handler.get(), _1, _2);

    handler->setSwitchNotifications(sn.getSwitchNotifications());

    handler->onFdbEvent(1, &data);

    sleep(1);
}
