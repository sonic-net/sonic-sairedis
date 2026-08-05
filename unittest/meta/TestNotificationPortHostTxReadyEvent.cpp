#include "NotificationPortHostTxReadyEvent.h"
#include "Meta.h"
#include "MetaTestSaiInterface.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

#include <gtest/gtest.h>
#include <memory>

using namespace sairedis;
using namespace saimeta;

static std::string s = "[{\"host_tx_ready_status\":\"SAI_PORT_HOST_TX_READY_STATUS_READY\",\"port_id\":\"oid:0x100000000001a\",\"switch_id\":\"oid:0x2100000000\"}]";
static std::string null = "[{\"host_tx_ready_status\":\"SAI_PORT_HOST_TX_READY_STATUS_READY\",\"port_id\":\"oid:0x0\",\"switch_id\":\"oid:0x0\"}]";
static std::string fullnull = "[]";

TEST(NotificationPortHostTxReadyEvent, ctr)
{
    NotificationPortHostTxReady n(s);
}

TEST(NotificationPortHostTxReadyEvent, getSwitchId)
{
    NotificationPortHostTxReady n(s);

    EXPECT_EQ(n.getSwitchId(), 0x2100000000);

    NotificationPortHostTxReady n2(null);

    EXPECT_EQ(n2.getSwitchId(), 0);
}

TEST(NotificationPortHostTxReadyEvent, getAnyObjectId)
{
    NotificationPortHostTxReady n(s);

    EXPECT_EQ(n.getAnyObjectId(), 0x100000000001a);

    NotificationPortHostTxReady n2(null);

    EXPECT_EQ(n2.getAnyObjectId(), 0);

    EXPECT_THROW(NotificationPortHostTxReady n3(fullnull), std::exception);
}

TEST(NotificationPortHostTxReadyEvent, processMetadata)
{
    NotificationPortHostTxReady n(s);

    auto sai = std::make_shared<MetaTestSaiInterface>();
    auto meta = std::make_shared<Meta>(sai);

    n.processMetadata(meta);
}

static void on_port_host_tx_ready_change_notification(
        _In_ sai_object_id_t port_id,
        _In_ sai_object_id_t switch_id,
        _In_ sai_port_host_tx_ready_status_t host_tx_ready_status)
{
    SWSS_LOG_ENTER();
}

TEST(NotificationPortHostTxReadyEvent, executeCallback)
{
    NotificationPortHostTxReady n(s);

    sai_switch_notifications_t ntfs;

    ntfs.on_port_host_tx_ready = &on_port_host_tx_ready_change_notification;

    n.executeCallback(ntfs);
}

