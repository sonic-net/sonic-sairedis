#include "NotificationSwitchMacsecPostStatus.h"
#include "Meta.h"
#include "MetaTestSaiInterface.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;
using namespace saimeta;

static std::string s = "{\"macsec_post_status\":\"SAI_SWITCH_MACSEC_POST_STATUS_PASS\",\"switch_id\":\"oid:0x2100000000\"}";
static std::string null = "{\"macsec_post_status\":\"SAI_SWITCH_MACSEC_POST_STATUS_PASS\",\"switch_id\":\"oid:0x0\"}";

TEST(NotificationSwitchMacsecPostStatus, ctr)
{
    NotificationSwitchMacsecPostStatus n(s);
}

TEST(NotificationSwitchMacsecPostStatus, getSwitchId)
{
    NotificationSwitchMacsecPostStatus n(s);

    EXPECT_EQ(n.getSwitchId(), 0x2100000000);

    NotificationSwitchMacsecPostStatus n2(null);

    EXPECT_EQ(n2.getSwitchId(), 0);
}

TEST(NotificationSwitchMacsecPostStatus, getAnyObjectId)
{
    NotificationSwitchMacsecPostStatus n(s);

    EXPECT_EQ(n.getAnyObjectId(), SAI_NULL_OBJECT_ID);

    NotificationSwitchMacsecPostStatus n2(null);

    EXPECT_EQ(n2.getAnyObjectId(), SAI_NULL_OBJECT_ID);
}

static void on_switch_macsec_post_status(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_macsec_post_status_t post_status)
{
    SWSS_LOG_ENTER();
}

TEST(NotificationSwitchMacsecPostStatus, executeCallback)
{
    NotificationSwitchMacsecPostStatus n(s);

    sai_switch_notifications_t ntfs;

    ntfs.on_switch_macsec_post_status = &on_switch_macsec_post_status;

    n.executeCallback(ntfs);
}
