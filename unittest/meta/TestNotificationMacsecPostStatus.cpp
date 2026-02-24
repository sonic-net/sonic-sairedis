#include "NotificationMacsecPostStatus.h"
#include "Meta.h"
#include "MetaTestSaiInterface.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;
using namespace saimeta;

static std::string s = "{\"macsec_post_status\":\"SAI_MACSEC_POST_STATUS_PASS\",\"macsec_id\":\"oid:0x5800000000\"}";
static std::string null = "{\"macsec_post_status\":\"SAI_MACSEC_POST_STATUS_PASS\",\"macsec_id\":\"oid:0x0\"}";

TEST(NotificationMacsecPostStatus, ctr)
{
    NotificationMacsecPostStatus n(s);
}

TEST(NotificationMacsecPostStatus, getSwitchId)
{
    NotificationMacsecPostStatus n(s);

    EXPECT_EQ(n.getSwitchId(), SAI_NULL_OBJECT_ID);

    NotificationMacsecPostStatus n2(null);

    EXPECT_EQ(n2.getSwitchId(), SAI_NULL_OBJECT_ID);
}

TEST(NotificationMacsecPostStatus, getAnyObjectId)
{
    NotificationMacsecPostStatus n(s);

    EXPECT_EQ(n.getAnyObjectId(), 0x5800000000);

    NotificationMacsecPostStatus n2(null);

    EXPECT_EQ(n2.getAnyObjectId(), 0);
}

static void on_macsec_post_status(
        _In_ sai_object_id_t macsec_id,
        _In_ sai_macsec_post_status_t post_status)
{
    SWSS_LOG_ENTER();
}

TEST(NotificationMacsecPostStatus, executeCallback)
{
    NotificationMacsecPostStatus n(s);

    sai_switch_notifications_t ntfs;

    ntfs.on_macsec_post_status = &on_macsec_post_status;

    n.executeCallback(ntfs);
}
