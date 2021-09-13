#include "SkipRecordAttrContainer.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;

TEST(SkipRecordAttrContainer, add)
{
    auto srac = std::make_shared<SkipRecordAttrContainer>();

    EXPECT_FALSE(srac->add(SAI_OBJECT_TYPE_PORT, 10000));

    EXPECT_FALSE(srac->add(SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_INGRESS_ACL));

    EXPECT_TRUE(srac->add(SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_TYPE));
}

TEST(SkipRecordAttrContainer, remove)
{
    auto srac = std::make_shared<SkipRecordAttrContainer>();

    EXPECT_FALSE(srac->remove(SAI_OBJECT_TYPE_PORT, 10000));

    EXPECT_TRUE(srac->add(SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_TYPE));

    EXPECT_FALSE(srac->remove(SAI_OBJECT_TYPE_PORT, 10000));

    EXPECT_TRUE(srac->remove(SAI_OBJECT_TYPE_PORT, SAI_PORT_ATTR_TYPE));
}

TEST(SkipRecordAttrContainer, clear)
{
    auto srac = std::make_shared<SkipRecordAttrContainer>();

    EXPECT_TRUE(srac->remove(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_ACL_TABLE_GROUP));

    srac->clear();

    EXPECT_FALSE(srac->remove(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_ACL_TABLE));
}

TEST(SkipRecordAttrContainer, canSkipRecording)
{
    auto srac = std::make_shared<SkipRecordAttrContainer>();

    EXPECT_FALSE(srac->canSkipRecording(SAI_OBJECT_TYPE_SWITCH, 0, nullptr));

    EXPECT_FALSE(srac->canSkipRecording(SAI_OBJECT_TYPE_SWITCH, 1, nullptr));
}
