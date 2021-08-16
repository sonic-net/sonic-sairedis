#include "AttrKeyMap.h"

#include <gtest/gtest.h>

using namespace saimeta;

TEST(AttrKeyMap, constructKey)
{
    auto akm = std::make_shared<AttrKeyMap>();

    sai_object_meta_key_t mk;

    EXPECT_THROW(
            akm->constructKey(SAI_NULL_OBJECT_ID, mk, 0, nullptr),
            std::runtime_error);
}

TEST(AttrKeyMap, clear)
{
    AttrKeyMap akm;

    EXPECT_EQ(akm.getAllKeys().size(), 0);

    akm.insert("foo", "bar");

    EXPECT_EQ(akm.getAllKeys().size(), 1);

    akm.clear();

    EXPECT_EQ(akm.getAllKeys().size(), 0);
}
