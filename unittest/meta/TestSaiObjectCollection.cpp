#include "SaiObjectCollection.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saimeta;

TEST(SaiObjectCollection, createObject)
{
    sai_object_meta_key_t mk = { .objecttype = SAI_OBJECT_TYPE_SWITCH, .objectkey = { .key = { .object_id = 1 } } };

    SaiObjectCollection oc;

    oc.createObject(mk);

    EXPECT_NO_THROW(oc.createObject(mk));
}

TEST(SaiObjectCollection, removeObject)
{
    sai_object_meta_key_t mk = { .objecttype = SAI_OBJECT_TYPE_SWITCH, .objectkey = { .key = { .object_id = 1 } } };

    SaiObjectCollection oc;

    EXPECT_NO_THROW(oc.removeObject(mk));
}

TEST(SaiObjectCollection, setObjectAttr)
{
    sai_object_meta_key_t mk = { .objecttype = SAI_OBJECT_TYPE_SWITCH, .objectkey = { .key = { .object_id = 1 } } };

    SaiObjectCollection oc;

    auto meta = sai_metadata_get_attr_metadata(
            SAI_OBJECT_TYPE_SWITCH,
            SAI_SWITCH_ATTR_INIT_SWITCH);

    EXPECT_NO_THROW(oc.setObjectAttr(mk, *meta, nullptr));
}

TEST(SaiObjectCollection, getObjectAttr)
{
    sai_object_meta_key_t mk = { .objecttype = SAI_OBJECT_TYPE_SWITCH, .objectkey = { .key = { .object_id = 1 } } };

    SaiObjectCollection oc;

    EXPECT_EQ(oc.getObjectAttr(mk, 0), nullptr);
}

TEST(SaiObjectCollection, getObject)
{
    sai_object_meta_key_t mk = { .objecttype = SAI_OBJECT_TYPE_SWITCH, .objectkey = { .key = { .object_id = 1 } } };

    SaiObjectCollection oc;

    EXPECT_NO_THROW(oc.getObject(mk));
}
