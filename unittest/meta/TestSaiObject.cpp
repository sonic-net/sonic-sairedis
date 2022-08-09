#include "SaiObject.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saimeta;

TEST(SaiObject, ctr)
{
    sai_object_meta_key_t meta_key = { .objecttype = SAI_OBJECT_TYPE_NULL, .objectkey = { .key = { .object_id = 0 } } };

    EXPECT_THROW(std::make_shared<SaiObject>(meta_key), std::runtime_error);
}

TEST(SaiObject, hasAttr)
{
    sai_object_meta_key_t mk = { .objecttype = SAI_OBJECT_TYPE_SWITCH, .objectkey = { .key = { .object_id = 0 } } };

    SaiObject so(mk);

    EXPECT_FALSE(so.hasAttr(1));
}

TEST(SaiObject, setAttr)
{
    sai_object_meta_key_t mk = { .objecttype = SAI_OBJECT_TYPE_SWITCH, .objectkey = { .key = { .object_id = 0 } } };

    SaiObject so(mk);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    auto meta = sai_metadata_get_attr_metadata(
            SAI_OBJECT_TYPE_SWITCH,
            SAI_SWITCH_ATTR_INIT_SWITCH);

    auto a = std::make_shared<SaiAttrWrapper>(meta, attr);

    so.setAttr(a);
}
