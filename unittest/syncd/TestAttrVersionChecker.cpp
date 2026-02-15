#include <cstdint>

#include <memory>
#include <vector>
#include <array>

#include <gtest/gtest.h>

#include "AttrVersionChecker.h"
#include "swss/logger.h"

using namespace syncd;

TEST(AttrVersionChecker, ctr)
{
    AttrVersionChecker avc;
}

TEST(AttrVersionChecker, enable)
{
    AttrVersionChecker avc;

    avc.enable(true);

    avc.enable(false);
}

TEST(AttrVersionChecker, setSaiApiVersion)
{
    AttrVersionChecker avc;

    avc.setSaiApiVersion(SAI_VERSION(1,13,0));
}

TEST(AttrVersionChecker, reset)
{
    AttrVersionChecker avc;

    avc.reset();
}

#define MD(x,v,n) \
    const sai_attr_metadata_t x = {\
        .objecttype                    = (sai_object_type_t)SAI_OBJECT_TYPE_BRIDGE,\
        .attrid                        = SAI_BRIDGE_ATTR_PORT_LIST,\
        .attridname                    = "SAI_BRIDGE_ATTR_PORT_LIST",\
        .brief                         = "List of bridge ports associated to this bridge.",\
        .attrvaluetype                 = SAI_ATTR_VALUE_TYPE_OBJECT_LIST,\
        .flags                         = (sai_attr_flags_t)(SAI_ATTR_FLAGS_READ_ONLY),\
        .allowedobjecttypes            = NULL,\
        .allowedobjecttypeslength      = 0,\
        .allowrepetitiononlist         = false,\
        .allowmixedobjecttypes         = false,\
        .allowemptylist                = false,\
        .allownullobjectid             = false,\
        .isoidattribute                = (1 > 0),\
        .defaultvaluetype              = SAI_DEFAULT_VALUE_TYPE_NONE,\
        .defaultvalue                  = NULL,\
        .defaultvalueobjecttype        = SAI_OBJECT_TYPE_NULL,\
        .defaultvalueattrid            = SAI_INVALID_ATTRIBUTE_ID,\
        .storedefaultvalue             = false,\
        .isenum                        = false,\
        .isenumlist                    = false,\
        .enummetadata                  = NULL,\
        .conditiontype                 = SAI_ATTR_CONDITION_TYPE_NONE,\
        .conditions                    = NULL,\
        .conditionslength              = 0,\
        .isconditional                 = (0 != 0),\
        .validonlytype                 = SAI_ATTR_CONDITION_TYPE_NONE,\
        .validonly                     = NULL,\
        .validonlylength               = 0,\
        .isvalidonly                   = (0 != 0),\
        .getsave                       = false,\
        .isvlan                        = false,\
        .isaclfield                    = false,\
        .isaclaction                   = false,\
        .isaclmask                     = false,\
        .ismandatoryoncreate           = false,\
        .iscreateonly                  = false,\
        .iscreateandset                = false,\
        .isreadonly                    = true,\
        .iskey                         = false,\
        .isprimitive                   = false,\
        .notificationtype              = -1,\
        .iscallback                    = false,\
        .pointertype                   = -1,\
        .capability                    = NULL,\
        .capabilitylength              = 0,\
        .isextensionattr               = false,\
        .isresourcetype                = false,\
        .isdeprecated                  = false,\
        .isconditionrelaxed            = false,\
        .iscustom                      = false,\
        .apiversion                    = (v),\
        .nextrelease                   = (n),\
    };\


TEST(AttrVersionChecker, isSufficientVersion)
{
    AttrVersionChecker avc;

    avc.enable(true);
    EXPECT_EQ(avc.isSufficientVersion(nullptr), false);

    avc.enable(false);
    avc.setSaiApiVersion(SAI_VERSION(1,10,0));
    avc.enable(true);

    MD(md,SAI_VERSION(1,9,0),false);
    EXPECT_EQ(avc.isSufficientVersion(&md), true);

    MD(md1,SAI_VERSION(1,11,0),false);
    EXPECT_EQ(avc.isSufficientVersion(&md1),false);

    avc.enable(false);
    EXPECT_EQ(avc.isSufficientVersion(&md1),true);

    avc.enable(true);
    avc.setSaiApiVersion(SAI_VERSION(1,10,0));
    EXPECT_EQ(avc.isSufficientVersion(&md1),false);

    avc.setSaiApiVersion(SAI_VERSION(1,12,0));
    EXPECT_EQ(avc.isSufficientVersion(&md1),true);

    avc.setSaiApiVersion(SAI_VERSION(1,11,0));
    EXPECT_EQ(avc.isSufficientVersion(&md1),true);

    MD(md2,SAI_VERSION(1,11,0),true);
    avc.setSaiApiVersion(SAI_VERSION(1,11,0));
    EXPECT_EQ(avc.isSufficientVersion(&md2),false);
}
