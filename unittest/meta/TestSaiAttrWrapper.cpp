#include "SaiAttrWrapper.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saimeta;

TEST(SaiAttrWrapper, ctr)
{
    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    EXPECT_THROW(std::make_shared<SaiAttrWrapper>(nullptr, attr), std::runtime_error);
}

TEST(SaiAttrWrapper, getAttrId)
{
    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    auto meta = sai_metadata_get_attr_metadata(
            SAI_OBJECT_TYPE_SWITCH,
            SAI_SWITCH_ATTR_INIT_SWITCH);

    SaiAttrWrapper w(meta, attr);

    EXPECT_EQ(w.getAttrId(), SAI_SWITCH_ATTR_INIT_SWITCH);
}

