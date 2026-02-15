#include "NotificationIcmpEchoSessionStateChange.h"
#include "Meta.h"
#include "MetaTestSaiInterface.h"

#include "sairediscommon.h"
#include "sai_serialize.h"
#include "VirtualObjectIdManager.h"
#include "NumberOidIndexGenerator.h"
#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;
using namespace saimeta;

std::string s = "[{\"icmp_echo_session_id\":\"oid:0x100000000001a\",\"session_state\":\"SAI_ICMP_ECHO_SESSION_STATE_DOWN\"}]";
static std::string invalid_id_str = "[{\"icmp_echo_session_id\":\"oid:0x100000000004a\",\"session_state\":\"SAI_ICMP_ECHO_SESSION_STATE_DOWN\"}]";
static std::string null = "[{\"icmp_echo_session_id\":\"oid:0x0\",\"session_state\":\"SAI_ICMP_ECHO_SESSION_STATE_DOWN\"}]";
static std::string fullnull = "[]";
static sai_object_id_t icmp_echo_session_oid;

sai_object_id_t generateOid()
{
    SWSS_LOG_ENTER();
    auto hw_info = "asic0";
    auto scc = std::make_shared<SwitchConfigContainer>();
    scc->insert(std::make_shared<SwitchConfig>(0, hw_info));
    auto vidManager = VirtualObjectIdManager(0, scc, std::make_shared<NumberOidIndexGenerator>());
    auto sid = vidManager.allocateNewSwitchObjectId("asic0");
    return vidManager.allocateNewObjectId(SAI_OBJECT_TYPE_ICMP_ECHO_SESSION, sid);
}

void setUp()
{
    SWSS_LOG_ENTER();
    icmp_echo_session_oid = generateOid();
    auto oidStr = sai_serialize_object_id(icmp_echo_session_oid);
    s = "[{\"icmp_echo_session_id\":\"" + oidStr + "\",\"session_state\":\"SAI_ICMP_ECHO_SESSION_STATE_DOWN\"}]";
}

TEST(NotificationIcmpEchoSessionStateChange, ctr)
{
    setUp();
    NotificationIcmpEchoSessionStateChange n(s);
}

TEST(NotificationIcmpEchoSessionStateChange, getSwitchId)
{
    NotificationIcmpEchoSessionStateChange n(s);

    EXPECT_EQ(n.getSwitchId(), 0);

    NotificationIcmpEchoSessionStateChange n2(null);

    EXPECT_EQ(n2.getSwitchId(), 0);
}

TEST(NotificationIcmpEchoSessionStateChange, getAnyObjectId)
{
    NotificationIcmpEchoSessionStateChange n(s);

    EXPECT_EQ(n.getAnyObjectId(), icmp_echo_session_oid);

    NotificationIcmpEchoSessionStateChange n2(null);

    EXPECT_EQ(n2.getAnyObjectId(), 0);

    NotificationIcmpEchoSessionStateChange n3(fullnull);

    EXPECT_EQ(n3.getSwitchId(), 0);

    EXPECT_EQ(n3.getAnyObjectId(), 0);
}

TEST(NotificationIcmpEchoSessionStateChange, processMetadata)
{
    NotificationIcmpEchoSessionStateChange n(s);

    auto sai = std::make_shared<MetaTestSaiInterface>();
    auto meta = std::make_shared<Meta>(sai);

    n.processMetadata(meta);
}

TEST(NotificationIcmpEchoSessionStateChange, processInvlaidMetadata)
{
    NotificationIcmpEchoSessionStateChange n(invalid_id_str);

    auto sai = std::make_shared<MetaTestSaiInterface>();
    auto meta = std::make_shared<Meta>(sai);

    n.processMetadata(meta);
}

static void on_icmp_echo_session_state_change(
        _In_ uint32_t count,
        _In_ const sai_icmp_echo_session_state_notification_t *data)
{
    SWSS_LOG_ENTER();
}

TEST(NotificationIcmpEchoSessionStateChange, executeCallback)
{
    NotificationIcmpEchoSessionStateChange n(s);

    sai_switch_notifications_t ntfs;

    ntfs.on_icmp_echo_session_state_change = &on_icmp_echo_session_state_change;

    n.executeCallback(ntfs);
}
